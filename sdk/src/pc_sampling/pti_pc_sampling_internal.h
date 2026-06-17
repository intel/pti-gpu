//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Internal implementation details for PC Sampling feature.
// This header provides:
//   - State machine for PC sampling collection lifecycle
//   - Handle storage and validation utilities
//   - Kernel callback infrastructure to track kernel binary info
//
// Note: This is an internal header and should not be included by users.
//
// =============================================================

#ifndef PTI_PC_SAMPLING_INTERNAL_H_
#define PTI_PC_SAMPLING_INTERNAL_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_pc_sampling.h"
#include "pti_pc_sampling_aggregator.h"
#include "pti_pc_sampling_collector.h"
#include "pti_pc_sampling_l0.h"
#include "pti_pc_sampling_raw_data_file.h"
#include "utils/pti_string_pool.h"

namespace pti::pc_sampling {

constexpr uint64_t kInstructionPointerAddressMask = 0xFFFFFFFFull;
constexpr size_t kInvalidMetricIndex = (std::numeric_limits<size_t>::max)();

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

/** Default sampling period in nanoseconds (100 µs) */
constexpr uint32_t kDefaultSamplingPeriodNs = 100'000;

/** Maximum number of devices that can be configured for PC sampling */
constexpr size_t kMaxConfiguredDevices = 1;

//-----------------------------------------------------------------------------
// Enumerations
//-----------------------------------------------------------------------------

/**
 * @brief State machine for PC sampling collection lifecycle.
 *
 * Valid transitions:
 *   ENABLED     -> CONFIGURED (via Configure)
 *   CONFIGURED  -> STARTED    (via StartCollection)
 *   STARTED     -> STOPPED    (via StopCollection)
 */
enum class PcSamplingState : uint32_t {
  kEnabled = 0,    /**< Handle created but not configured yet */
  kConfigured = 1, /**< Configuration accepted; collection not started */
  kStarted = 2,    /**< Collection is currently running */
  kStopped = 3,    /**< Collection stopped; query APIs now valid */
  kForceUint32 = 0x7fffffff,
};

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

class PtiPcSamplingDataCollector;

/**
 * @brief Information about a kernel binary collected during kernel creation callback.
 *
 * Used to correlate PC samples with kernel source code.
 */
struct KernelBinaryInfo {
  uint64_t kernel_base_address_ = 0;             /**< Base address of kernel in device memory */
  size_t kernel_binary_size_ = 0;                /**< Size of the kernel binary in bytes */
  std::string kernel_name_;                      /**< Name of the kernel (owned copy) */
  pti_backend_kernel_t kernel_handle_ = nullptr; /**< Device kernel handle */
  pti_backend_module_t module_handle_ = nullptr; /**< Module handle containing this kernel */
  pti_device_handle_t device_handle_ = nullptr; /**< Device handle this kernel is associated with */
};

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

/**
 * @brief Convert PC sampling state to human-readable string.
 *
 * @param[in] state   The state to convert
 *
 * @return String representation of the state
 */
inline const char* PcSamplingStateToString(PcSamplingState state) {
  switch (state) {
    case PcSamplingState::kEnabled:
      return "ENABLED";
    case PcSamplingState::kConfigured:
      return "CONFIGURED";
    case PcSamplingState::kStarted:
      return "STARTED";
    case PcSamplingState::kStopped:
      return "STOPPED";
    case PcSamplingState::kForceUint32:
      return "FORCE_UINT32";
  }
  return "UNKNOWN";
}

}  // namespace pti::pc_sampling

//-----------------------------------------------------------------------------
// External Handle Structure (C API compatibility)
//-----------------------------------------------------------------------------

/**
 * @brief PC sampling handle structure exposed through the C API.
 *
 * Contains the state machine and configuration for a sampling session.
 */
struct _pti_pc_sampling_handle_t {
  pti::pc_sampling::PcSamplingState state_ = pti::pc_sampling::PcSamplingState::kEnabled;
  uint32_t sampling_period_ns_ = pti::pc_sampling::kDefaultSamplingPeriodNs;
  std::unordered_map<pti_device_handle_t, zet_metric_group_handle_t>
      supported_device_metric_group_map_;
  // Supported devices in Level Zero discovery order (zeDriverGet/zeDeviceGet).
  // Used to pick a deterministic profiling device that matches the one the
  // runtime (e.g. SYCL gpu_selector) executes kernels on.
  std::vector<pti_device_handle_t> supported_devices_;
  std::vector<pti_device_handle_t> configured_devices_;
  zet_metric_group_handle_t collected_metric_group_ = nullptr;
  pti::pc_sampling::TempRawDataFile collected_raw_data_;
  bool samples_dropped_ = false;
  std::unique_ptr<pti::pc_sampling::PtiPcSamplingDataCollector> collector;

  // Lazily-computed, cached results. Built once on the first results query that
  // needs them and reused for all subsequent queries (no recalculation).
  bool aggregated_ = false;
  pti::pc_sampling::DeviceAggregate device_aggregate_;
  // Stall reasons in metric-group order, pooled so the pointers stay alive for
  // the API caller. Size is the stall-reason count (IP metric excluded).
  std::vector<std::pair<const char*, const char*>> stall_reasons_;
  size_t ip_metric_index_ = pti::pc_sampling::kInvalidMetricIndex;
};

namespace pti::pc_sampling {

inline void ClearProfiledDeviceData(pti_pc_sampling_handle_t handle) {
  if (handle == nullptr) {
    return;
  }

  handle->device_aggregate_.Clear();
  handle->stall_reasons_.clear();
  handle->ip_metric_index_ = pti::pc_sampling::kInvalidMetricIndex;
  handle->aggregated_ = false;
}

inline pti_result GetConfiguredDevice(const pti_pc_sampling_handle_t handle,
                                      pti_device_handle_t* device) {
  if (handle == nullptr || device == nullptr) {
    SPDLOG_ERROR("{}: handle or output device pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (handle->configured_devices_.empty()) {
    SPDLOG_WARN("{}: no configured device is available on the handle", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  *device = handle->configured_devices_.front();
  return PTI_SUCCESS;
}

pti_result EnsureStallReasons(pti_pc_sampling_handle_t handle);
pti_result EnsureAggregatedResults(pti_pc_sampling_handle_t handle);
void ResetCollectionSession(pti_pc_sampling_handle_t handle);

//-----------------------------------------------------------------------------
// Kernel Info Storage (Thread-Safe)
//-----------------------------------------------------------------------------

/**
 * @brief Thread-safe storage for kernel binary information.
 *
 * Collects kernel metadata during kernel creation callbacks.
 */
class KernelInfoStorage {
 public:
  /**
   * @brief Get the singleton instance.
   */
  static KernelInfoStorage& Instance() {
    static KernelInfoStorage instance;
    return instance;
  }

  /**
   * @brief Add kernel information to the storage.
   *
   * Thread-safe: uses write lock.
   *
   * @param[in] info   The kernel binary info to store
   */
  void Add(KernelBinaryInfo info) {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    // Use kernel base address as key since it's guaranteed unique
    auto key = static_cast<uintptr_t>(info.kernel_base_address_);
    kernel_info_map_[key] = std::make_unique<KernelBinaryInfo>(std::move(info));
  }

  /**
   * @brief Find kernel info by ip address.
   *
   * Thread-safe: uses read lock. The returned pointer remains valid until
   * Clear() is called (typically at ptiPcSamplingDisable).
   *
   * @param[in] ip_address   The instruction pointer address to search for
   *
   * @return Pointer to info if found, nullptr otherwise
   */
  const KernelBinaryInfo* FindByIpAddress(uint64_t ip_address) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [key, info_ptr] : kernel_info_map_) {
      if (info_ptr->kernel_base_address_ <= ip_address &&
          ip_address < info_ptr->kernel_base_address_ + info_ptr->kernel_binary_size_) {
        return info_ptr.get();
      }
    }
    return nullptr;
  }

  /**
   * @brief Get the number of stored kernel infos.
   */
  size_t Size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return kernel_info_map_.size();
  }

  /**
   * @brief Clear all stored kernel information.
   */
  void Clear() {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    kernel_info_map_.clear();
  }

 private:
  KernelInfoStorage() = default;
  ~KernelInfoStorage() = default;
  KernelInfoStorage(const KernelInfoStorage&) = delete;
  KernelInfoStorage& operator=(const KernelInfoStorage&) = delete;

  mutable std::shared_mutex mutex_;
  std::unordered_map<uintptr_t, std::unique_ptr<KernelBinaryInfo>> kernel_info_map_;
};

//-----------------------------------------------------------------------------
// Kernel Callback Implementation
//-----------------------------------------------------------------------------

/**
 * @brief Callback function invoked when a kernel is created.
 *
 * Extracts kernel binary information (base address, module size, name)
 * and stores it for later correlation with PC samples.
 *
 * @param[in] domain              Callback domain (must be PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED)
 * @param[in] api_group           API group identifier (unused)
 * @param[in] api_id              API identifier (unused)
 * @param[in] ctx                 Backend context (unused)
 * @param[in] cb_data             Callback data, must be cast to pti_callback_kernel_data*
 * @param[in] user_data           User data passed during subscription (unused)
 * @param[in] instance_user_data  Per-instance user data (unused)
 */
inline void KernelCreatedCallback(pti_callback_domain domain, pti_api_group_id /*api_group*/,
                                  uint32_t /*api_id*/, pti_backend_ctx_t /*ctx*/, void* cb_data,
                                  void* /*user_data*/, void** /*instance_user_data*/) {
  // Filter for kernel created domain only
  if (domain != PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED) {
    return;
  }

  auto* data = static_cast<pti_callback_kernel_data*>(cb_data);
  if (data == nullptr) {
    SPDLOG_ERROR("{}: callback data is null", __FUNCTION__);
    return;
  }

  // Only process EXIT phase where the kernel handle is valid and call succeeded
  if (data->_phase != PTI_CB_PHASE_API_EXIT || data->_return_code != ZE_RESULT_SUCCESS) {
    return;
  }

  // Initialize kernel info with data from callback
  KernelBinaryInfo kernel_info;
  kernel_info.kernel_handle_ = data->_device_kernel_handle;
  kernel_info.module_handle_ = data->_module_handle;
  kernel_info.device_handle_ = data->_device_handle;
  kernel_info.kernel_name_ = data->_name ? data->_name : "<unknown>";

  SPDLOG_DEBUG("{}: kernel created - handle: {}, name: {}, module: {}, device: {}", __FUNCTION__,
               static_cast<void*>(data->_device_kernel_handle), kernel_info.kernel_name_,
               static_cast<void*>(data->_module_handle), static_cast<void*>(data->_device_handle));

  // Query kernel base address using extension function
  auto kernel_handle = reinterpret_cast<ze_kernel_handle_t>(data->_device_kernel_handle);
  if (kernel_handle != nullptr) {
    using ZexKernelGetBaseAddressFn = ze_result_t (*)(ze_kernel_handle_t, uint64_t*);

    // Initialize extension function pointer only once (thread-safe in C++11+)
    static ZexKernelGetBaseAddressFn get_base_addr = []() -> ZexKernelGetBaseAddressFn {
      ze_driver_handle_t driver = nullptr;
      uint32_t count = 1;
      if (zeDriverGet(&count, &driver) != ZE_RESULT_SUCCESS || driver == nullptr) {
        SPDLOG_WARN("KernelCreatedCallback: Failed to get driver handle");
        return nullptr;
      }

      ZexKernelGetBaseAddressFn fn = nullptr;
      ze_result_t status = zeDriverGetExtensionFunctionAddress(driver, "zexKernelGetBaseAddress",
                                                               reinterpret_cast<void**>(&fn));

      if (status != ZE_RESULT_SUCCESS || fn == nullptr) {
        SPDLOG_WARN("KernelCreatedCallback: zexKernelGetBaseAddress extension not available");
        return nullptr;
      }
      return fn;
    }();

    if (get_base_addr != nullptr) {
      uint64_t base_address = 0;
      ze_result_t status = get_base_addr(kernel_handle, &base_address);
      if (status != ZE_RESULT_SUCCESS) {
        SPDLOG_WARN("{}: zexKernelGetBaseAddress failed with status {:#x}", __FUNCTION__,
                    static_cast<uint32_t>(status));
      } else {
        kernel_info.kernel_base_address_ = base_address & kInstructionPointerAddressMask;
        SPDLOG_DEBUG("{}: kernel base address {:#x} for '{}'", __FUNCTION__, base_address,
                     kernel_info.kernel_name_);
      }
    } else {
      SPDLOG_WARN("{}: zexKernelGetBaseAddress extension not available, skipping kernel '{}'",
                  __FUNCTION__, kernel_info.kernel_name_);
    }

    size_t kernel_size = 0;
    auto status = zeKernelGetBinaryExp(kernel_handle, &kernel_size, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_DEBUG("{}: zeKernelGetBinaryExp failed with status {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
    } else {
      kernel_info.kernel_binary_size_ = kernel_size;
    }
  }

  // Store the kernel info for later use
  KernelInfoStorage::Instance().Add(std::move(kernel_info));
}

//-----------------------------------------------------------------------------
// Handle Storage and Management
//-----------------------------------------------------------------------------

/**
 * @brief Singleton storage for PC sampling handle.
 *
 * Ensures only one PC sampling session is active at a time.
 * Manages the lifecycle of the sampling handle and callback subscriptions.
 */
class PtiPcSamplingHandleStorage {
 public:
  /**
   * @brief Get the singleton instance.
   */
  static PtiPcSamplingHandleStorage& Instance() {
    static PtiPcSamplingHandleStorage storage;
    return storage;
  }

  /**
   * @brief Check if the given handle is the currently active handle.
   *
   * @param[in] handle   Handle to check
   *
   * @return true if handle matches the active handle
   */
  bool Contains(pti_pc_sampling_handle_t handle) const {
    return handle_ != nullptr && handle_.get() == handle;
  }

  /**
   * @brief Create a new PC sampling handle.
   *
   * Fails if a handle already exists or no compatible devices found.
   *
   * @param[out] handle   Pointer to receive the new handle
   *
   * @return PTI_SUCCESS or appropriate error code
   */
  pti_result Create(pti_pc_sampling_handle_t* handle) {
    if (handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (handle_ != nullptr) {
      SPDLOG_ERROR("{}: only one PC sampling handle can be enabled at a time", __FUNCTION__);
      return PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED;
    }

    std::unordered_map<pti_device_handle_t, ze_driver_handle_t> supported_device_driver_map;
    std::unordered_map<pti_device_handle_t, zet_metric_group_handle_t>
        supported_device_metric_group_map;
    std::vector<pti_device_handle_t> supported_devices_ordered;

    // Initialize Level Zero (idempotent if already initialized) and enumerate
    // drivers/devices through the shared utils so this stays in sync with the
    // rest of the SDK. Driver enumeration uses zeDriverGet (via GetDriverList)
    // to match the handles the SYCL runtime collects on.
    ZeDriverInit init_drivers{};
    if (!init_drivers.Success()) {
      SPDLOG_ERROR("{}: failed to initialize Level Zero", __FUNCTION__);
      return PTI_ERROR_PC_SAMPLING_UNSUPPORTED;
    }

    for (ze_driver_handle_t driver : ::utils::ze::GetDriverList()) {
      for (ze_device_handle_t device : ::utils::ze::GetDeviceList(driver)) {
        ze_device_properties_t props{};
        props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

        ze_result_t status = zeDeviceGetProperties(device, &props);
        if (status != ZE_RESULT_SUCCESS) {
          SPDLOG_ERROR("{}: zeDeviceGetProperties failed, status: {:#x}", __FUNCTION__,
                       static_cast<uint32_t>(status));
          continue;
        }

        if (props.type != ZE_DEVICE_TYPE_GPU) {
          continue;
        }

        zet_metric_group_handle_t metric_group = FindEUStallMetricGroupHandle(device);
        if (metric_group == nullptr) {
          continue;
        }

        supported_device_driver_map.emplace(reinterpret_cast<pti_device_handle_t>(device), driver);
        supported_device_metric_group_map.emplace(reinterpret_cast<pti_device_handle_t>(device),
                                                  metric_group);
        supported_devices_ordered.push_back(reinterpret_cast<pti_device_handle_t>(device));
        SPDLOG_INFO("{}: device {} supports {}", __FUNCTION__, props.name,
                    kPcSamplingMetricGroupName);
      }
    }

    if (supported_device_metric_group_map.empty()) {
      std::string err_msg = "";
#ifndef _WIN32
      err_msg +=
          "/proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid is set "
          "to 0 and";
#endif /* _WIN32 */
      SPDLOG_ERROR(
          "{}: no devices with EUStallSampling support found. "
          "Ensure {} ZET_ENABLE_METRICS=1 is set in the environment for PC sampling to work.",
          __FUNCTION__, err_msg);
      return PTI_ERROR_PC_SAMPLING_UNSUPPORTED;
    }

    // Allocate the handle
    auto collection_handle =
        std::unique_ptr<_pti_pc_sampling_handle_t>(new (std::nothrow) _pti_pc_sampling_handle_t());
    if (collection_handle == nullptr) {
      return PTI_ERROR_INTERNAL;
    }

    {
      std::lock_guard lock(device_metric_group_map_mutex_);
      device_driver_map_ = std::move(supported_device_driver_map);
    }
    collection_handle->supported_device_metric_group_map_ =
        std::move(supported_device_metric_group_map);
    collection_handle->supported_devices_ = std::move(supported_devices_ordered);

    pti_pc_sampling_handle_t new_handle = collection_handle.get();
    handle_ = std::move(collection_handle);

    // Subscribe to kernel creation callbacks to track kernel binary info
    pti_result callback_result =
        ptiCallbackSubscribe(&callback_subscriber_, &KernelCreatedCallback, nullptr);
    if (callback_result != PTI_SUCCESS) {
      SPDLOG_ERROR("{}: ptiCallbackSubscribe failed with result {}", __FUNCTION__,
                   static_cast<uint32_t>(callback_result));
      handle_.reset();
      return callback_result;
    }

    // Enable the kernel created domain (EXIT phase only)
    callback_result =
        ptiCallbackEnableDomain(callback_subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED,
                                0,   // api_enter = false
                                1);  // api_exit = true
    if (callback_result != PTI_SUCCESS) {
      SPDLOG_ERROR("{}: ptiCallbackEnableDomain failed with result {}", __FUNCTION__,
                   static_cast<uint32_t>(callback_result));
      ptiCallbackUnsubscribe(callback_subscriber_);
      callback_subscriber_ = nullptr;
      handle_.reset();
      return callback_result;
    }

    *handle = new_handle;
    return PTI_SUCCESS;
  }

  /**
   * @brief Destroy the PC sampling handle and release all resources.
   *
   * @param[in] handle   Handle to destroy
   *
   * @return PTI_SUCCESS or appropriate error code
   */
  pti_result Destroy(pti_pc_sampling_handle_t handle) {
    if (handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (handle_ == nullptr || handle_.get() != handle) {
      SPDLOG_ERROR("{}: there is no active PC sampling handle", __FUNCTION__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;
    // Cleanup callback subscription
    if (callback_subscriber_ != nullptr) {
      result = ptiCallbackDisableDomain(callback_subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED);
      if (result != PTI_SUCCESS) {
        SPDLOG_ERROR("{}: ptiCallbackDisableDomain failed with result {}", __FUNCTION__,
                     static_cast<uint32_t>(result));
      }
      result = ptiCallbackUnsubscribe(callback_subscriber_);
      if (result != PTI_SUCCESS) {
        SPDLOG_ERROR("{}: ptiCallbackUnsubscribe failed with result {}", __FUNCTION__,
                     static_cast<uint32_t>(result));
      }
      callback_subscriber_ = nullptr;
    }

    // Clear kernel info storage
    KernelInfoStorage::Instance().Clear();

    {
      std::lock_guard lock(device_metric_group_map_mutex_);
      handle_->supported_device_metric_group_map_.clear();
      device_driver_map_.clear();
    }

    handle_.reset();
    return result;
  }

  ze_driver_handle_t GetDriver(pti_device_handle_t device) const {
    std::shared_lock lock(device_metric_group_map_mutex_);
    auto it = device_driver_map_.find(device);
    return (it != device_driver_map_.end()) ? it->second : nullptr;
  }

  /**
   * @brief Get the metric group handle for a device.
   *
   * @param[in] device   Device handle
   *
   * @return Metric group handle or nullptr if not found
   */
  zet_metric_group_handle_t GetMetricGroup(pti_device_handle_t device) const {
    std::shared_lock lock(device_metric_group_map_mutex_);
    if (handle_ == nullptr) {
      return nullptr;
    }

    auto it = handle_->supported_device_metric_group_map_.find(device);
    return (it != handle_->supported_device_metric_group_map_.end()) ? it->second : nullptr;
  }

  bool IsSupported(pti_device_handle_t device) const {
    std::shared_lock lock(device_metric_group_map_mutex_);
    return handle_ != nullptr && handle_->supported_device_metric_group_map_.find(device) !=
                                     handle_->supported_device_metric_group_map_.end();
  }

  std::vector<pti_device_handle_t> GetSupportedDevices() const {
    std::shared_lock lock(device_metric_group_map_mutex_);
    if (handle_ == nullptr) {
      return {};
    }
    return handle_->supported_devices_;
  }

 private:
  PtiPcSamplingHandleStorage() = default;
  ~PtiPcSamplingHandleStorage() = default;
  PtiPcSamplingHandleStorage(const PtiPcSamplingHandleStorage&) = delete;
  PtiPcSamplingHandleStorage& operator=(const PtiPcSamplingHandleStorage&) = delete;

  std::unique_ptr<_pti_pc_sampling_handle_t> handle_;
  pti_callback_subscriber_handle callback_subscriber_ = nullptr;
  mutable std::shared_mutex device_metric_group_map_mutex_;
  std::unordered_map<pti_device_handle_t, ze_driver_handle_t> device_driver_map_;
};

inline zet_metric_group_handle_t ResolveCollectedMetricGroup(
    const pti_pc_sampling_handle_t handle, pti_device_handle_t configured_device) {
  if (handle->collected_metric_group_ != nullptr) {
    return handle->collected_metric_group_;
  }

  return PtiPcSamplingHandleStorage::Instance().GetMetricGroup(configured_device);
}

//-----------------------------------------------------------------------------
// Handle Validation Utilities
//-----------------------------------------------------------------------------

/**
 * @brief Check if the handle belongs to this module.
 */
inline bool IsOurHandle(pti_pc_sampling_handle_t handle) {
  return PtiPcSamplingHandleStorage::Instance().Contains(handle);
}

inline bool IsPCSamplingSupportedDevice(pti_device_handle_t device) {
  return PtiPcSamplingHandleStorage::Instance().IsSupported(device);
}

inline std::vector<pti_device_handle_t> GetAllDevices() {
  return PtiPcSamplingHandleStorage::Instance().GetSupportedDevices();
}

class StringCache {
 public:
  static StringCache& Instance() {
    static StringCache cache;
    return cache;
  }

  const char* Get(const std::string& value) { return string_pool_.Get(value); }

 private:
  StringCache() = default;
  ~StringCache() = default;
  StringCache(const StringCache&) = delete;
  StringCache& operator=(const StringCache&) = delete;

  StringPool string_pool_;
};

inline pti_result EnsureStallReasons(pti_pc_sampling_handle_t handle) {
  if (handle == nullptr) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (handle->ip_metric_index_ != pti::pc_sampling::kInvalidMetricIndex) {
    return PTI_SUCCESS;
  }

  pti_device_handle_t configured_device = nullptr;
  if (GetConfiguredDevice(handle, &configured_device) != PTI_SUCCESS) {
    configured_device =
        handle->supported_devices_.empty() ? nullptr : handle->supported_devices_.front();
  }
  const zet_metric_group_handle_t metric_group =
      ResolveCollectedMetricGroup(handle, configured_device);

  if (metric_group == nullptr) {
    // No collection happened; an empty reason table is valid.
    handle->ip_metric_index_ = 0;
    return PTI_SUCCESS;
  }

  size_t ip_metric_index = pti::pc_sampling::kInvalidMetricIndex;
  std::vector<std::pair<std::string, std::string>> metric_names =
      GetAllSupportedStallMetricNames(metric_group, &ip_metric_index);
  if (metric_names.empty()) {
    handle->ip_metric_index_ = pti::pc_sampling::kInvalidMetricIndex;
    handle->stall_reasons_.clear();
    SPDLOG_ERROR("{}: failed to collect metric names", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  std::vector<std::pair<const char*, const char*>> stall_reasons;
  stall_reasons.reserve(metric_names.size());
  for (const auto& [name, description] : metric_names) {
    stall_reasons.emplace_back(StringCache::Instance().Get(name),
                               StringCache::Instance().Get(description));
  }

  handle->stall_reasons_ = std::move(stall_reasons);
  handle->ip_metric_index_ = ip_metric_index;

  return PTI_SUCCESS;
}

inline pti_result EnsureAggregatedResults(pti_pc_sampling_handle_t handle) {
  if (handle->aggregated_) {
    return PTI_SUCCESS;
  }

  const pti_result reasons_status = EnsureStallReasons(handle);
  if (reasons_status != PTI_SUCCESS) {
    return reasons_status;
  }

  pti_device_handle_t configured_device = nullptr;
  const pti_result configured_device_status = GetConfiguredDevice(handle, &configured_device);
  if (configured_device_status != PTI_SUCCESS) {
    return configured_device_status;
  }

  const zet_metric_group_handle_t metric_group =
      ResolveCollectedMetricGroup(handle, configured_device);
  const size_t reason_count = handle->stall_reasons_.size();
  const pti_result aggregate_status = AggregateCollectedData(
      configured_device, metric_group, handle->samples_dropped_, handle->collected_raw_data_,
      handle->ip_metric_index_, reason_count, &handle->device_aggregate_);
  if (aggregate_status != PTI_SUCCESS) {
    return aggregate_status;
  }

  handle->aggregated_ = true;
  return PTI_SUCCESS;
}

/**
 * @brief Check if a device is configured for the given handle.
 */
inline bool IsConfiguredDevice(const pti_pc_sampling_handle_t handle, pti_device_handle_t device) {
  if (handle == nullptr || device == nullptr) {
    return false;
  }
  return std::find(handle->configured_devices_.begin(), handle->configured_devices_.end(),
                   device) != handle->configured_devices_.end();
}

/**
 * @brief Validate that the handle is valid and belongs to this module.
 */
inline pti_result ValidateHandle(const pti_pc_sampling_handle_t handle) {
  if (handle == nullptr) {
    SPDLOG_ERROR("{}: handle is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (!PtiPcSamplingHandleStorage::Instance().Contains(handle)) {
    SPDLOG_ERROR("{}: handle is invalid", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_SUCCESS;
}

/**
 * @brief Validate handle is configured and ready to start collection.
 */
inline pti_result ValidateConfiguredHandle(const pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state_ == PcSamplingState::kStarted) {
    SPDLOG_ERROR("{}: collection is already running, current state is {}", __FUNCTION__,
                 PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_ALREADY_STARTED;
  }

  if (handle->state_ == PcSamplingState::kStopped) {
    SPDLOG_ERROR("{}: collection is already stopped, current state is {}", __FUNCTION__,
                 PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED;
  }

  if (handle->state_ != PcSamplingState::kConfigured) {
    SPDLOG_ERROR("{}: collection not configured, current state is {}", __FUNCTION__,
                 PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED;
  }

  return PTI_SUCCESS;
}

/**
 * @brief Validate handle is in stopped state for query operations.
 */
inline pti_result ValidateStoppedCollectionHandle(const pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state_ != PcSamplingState::kStopped) {
    SPDLOG_ERROR("{}: collection must be stopped, current state is {}", __FUNCTION__,
                 PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_NOT_STOPPED;
  }

  return PTI_SUCCESS;
}

inline pti_result ValidateConfiguredProfiledDevice(const pti_pc_sampling_handle_t handle,
                                                   pti_device_handle_t device) {
  if (!IsConfiguredDevice(handle, device)) {
    SPDLOG_ERROR("{}: device does not match the configured PC sampling device", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_SUCCESS;
}

inline pti_result FindKernelAggregate(const pti_pc_sampling_handle_t handle, uint64_t kernel_handle,
                                      const KernelAggregate** kernel_aggregate) {
  if (kernel_aggregate == nullptr) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  auto it = handle->device_aggregate_.kernels.find(kernel_handle);
  if (it == handle->device_aggregate_.kernels.end()) {
    SPDLOG_ERROR("{}: kernel handle {:#x} was not observed during collection", __FUNCTION__,
                 kernel_handle);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  *kernel_aggregate = &it->second;
  return PTI_SUCCESS;
}

inline pti_result FillProfiledDevices(const pti_pc_sampling_handle_t handle,
                                      pti_device_handle_t* devices, size_t* device_count) {
  const size_t configured_device_count = handle->configured_devices_.size();
  if (devices != nullptr && configured_device_count != 0 && *device_count != 0) {
    const size_t copied_device_count = (std::min)(*device_count, configured_device_count);
    std::copy_n(handle->configured_devices_.begin(), copied_device_count, devices);
    *device_count = copied_device_count;
  } else {
    *device_count = configured_device_count;
  }
  return PTI_SUCCESS;
}

inline pti_result FillObservedKernelHandles(const pti_pc_sampling_handle_t handle,
                                            uint64_t* kernel_handles, size_t* kernel_count) {
  const auto& kernels = handle->device_aggregate_.kernels;
  const size_t available_count = kernels.size();
  if (kernel_handles == nullptr || *kernel_count == 0) {
    *kernel_count = available_count;
    return PTI_SUCCESS;
  }

  const size_t copy_count = (std::min)(*kernel_count, available_count);
  size_t index = 0;
  for (const auto& entry : kernels) {
    if (index >= copy_count) {
      break;
    }
    kernel_handles[index++] = entry.first;
  }
  *kernel_count = copy_count;
  return PTI_SUCCESS;
}

inline pti_result FillObservedKernelInfo(pti_device_handle_t device,
                                         const pti_pc_sampling_handle_t handle,
                                         uint64_t kernel_handle,
                                         pti_pc_sampling_kernel_info_t* kernel_info) {
  const KernelAggregate* kernel_aggregate = nullptr;
  const pti_result kernel_status = FindKernelAggregate(handle, kernel_handle, &kernel_aggregate);
  if (kernel_status != PTI_SUCCESS) {
    return kernel_status;
  }

  kernel_info->_device = device;
  kernel_info->_kernel_handle = kernel_aggregate->kernel_handle;
  kernel_info->_kernel_name = kernel_aggregate->kernel_name_.c_str();
  kernel_info->_reason_count = kernel_aggregate->reason_count;
  kernel_info->_instructions_with_samples_count = kernel_aggregate->instruction_count;

  if (kernel_info->_aggregated_samples != nullptr) {
    const size_t copy_count =
        (std::min)(kernel_aggregate->reason_count, kernel_aggregate->aggregated_samples.size());
    std::copy_n(kernel_aggregate->aggregated_samples.begin(), copy_count,
                kernel_info->_aggregated_samples);
  }

  return PTI_SUCCESS;
}

inline pti_result FillSamplesPerInstruction(const pti_pc_sampling_handle_t handle,
                                            uint64_t kernel_handle,
                                            pti_pc_sampling_instruction_t* instruction_buffer,
                                            size_t instruction_buffer_count,
                                            uint64_t* samples_buffer, size_t samples_buffer_count) {
  const KernelAggregate* kernel_aggregate = nullptr;
  const pti_result kernel_status = FindKernelAggregate(handle, kernel_handle, &kernel_aggregate);
  if (kernel_status != PTI_SUCCESS) {
    return kernel_status;
  }

  const size_t persisted_instruction_count = kernel_aggregate->instruction_count;
  const size_t reason_count = kernel_aggregate->reason_count;
  const size_t offsets_bytes = persisted_instruction_count * sizeof(uint64_t);
  if (reason_count != 0 &&
      persisted_instruction_count > ((std::numeric_limits<size_t>::max)() / reason_count)) {
    SPDLOG_ERROR("{}: sample matrix size overflows for kernel handle {:#x}", __FUNCTION__,
                 kernel_aggregate->kernel_handle);
    return PTI_ERROR_INTERNAL;
  }

  const size_t sample_count = persisted_instruction_count * reason_count;
  const size_t samples_bytes = sample_count * sizeof(uint64_t);
  const size_t expected_size = offsets_bytes + samples_bytes;
  if (expected_size != kernel_aggregate->samples_file.size()) {
    SPDLOG_ERROR("{}: kernel samples file size is invalid for kernel handle {:#x}", __FUNCTION__,
                 kernel_aggregate->kernel_handle);
    return PTI_ERROR_INTERNAL;
  }

  const size_t instruction_count =
      (std::min)(instruction_buffer_count, persisted_instruction_count);
  std::vector<uint64_t> instruction_offsets(instruction_count);
  if (instruction_count != 0 &&
      !kernel_aggregate->samples_file.ReadRange(0, instruction_count * sizeof(uint64_t),
                                                instruction_offsets.data())) {
    SPDLOG_ERROR("{}: failed to read instruction offsets for kernel handle {:#x}", __FUNCTION__,
                 kernel_aggregate->kernel_handle);
    return PTI_ERROR_INTERNAL;
  }

  for (size_t i = 0; i < instruction_count; ++i) {
    instruction_buffer[i]._instruction_offset = instruction_offsets[i];
    instruction_buffer[i]._source_info = nullptr;
  }

  const size_t returned_sample_count = instruction_count * reason_count;
  const size_t copied_sample_count = (std::min)(samples_buffer_count, returned_sample_count);
  if (copied_sample_count != 0 &&
      !kernel_aggregate->samples_file.ReadRange(
          offsets_bytes, copied_sample_count * sizeof(*samples_buffer), samples_buffer)) {
    SPDLOG_ERROR("{}: failed to read instruction samples for kernel handle {:#x}", __FUNCTION__,
                 kernel_aggregate->kernel_handle);
    return PTI_ERROR_INTERNAL;
  }

  return PTI_SUCCESS;
}

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_INTERNAL_H_
