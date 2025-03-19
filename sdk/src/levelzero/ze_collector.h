//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
#define PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_

/*
 * Level zero collection methods:  hook into the level zero api to capture
 * kernel/memory movement, collect relevent timing and handles key structures
 * and issue callbacks to buffer,etc interfaces to capture this data in view
 * records.
 */

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/layers/zel_tracing_register_cb.h>
#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>
#include <pti/pti_driver_levelzero_api_ids.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "collector_options.h"
#include "lz_api_tracing_api_loader.h"
#include "overhead_kinds.h"
#include "pti/pti_view.h"
#include "unikernel.h"
#include "utils.h"
#include "ze_event_cache.h"
#include "ze_local_collection_helpers.h"
#include "ze_timer_helper.h"
#include "ze_utils.h"
#include "ze_wrappers.h"

struct CallbacksEnabled {
  std::atomic<bool> acallback = false;
  std::atomic<bool> fcallback = false;
};

inline std::atomic<uint64_t> global_ref_count =
    0;  // Keeps track of zelEnable/zelDisable TracingLayer()
        // calls issued. 0 => truely disabled tracing.

struct ZeInstanceData {
  uint64_t start_time_host;
  uint64_t timestamp_host;    // in ns
  uint64_t timestamp_device;  // in ticks
  uint64_t end_time_host;
  uint64_t kid;  // passing kid from enter callback to exit callback
  uint32_t callback_id_;
};

inline thread_local ZeInstanceData ze_instance_data;

// Used for CPU/GPU sync points. See description to ZeCollector::GetDeviceTimestamps function
inline thread_local std::map<ze_device_handle_t, std::unique_ptr<CPUGPUTimeInterpolationHelper>>
    timer_helpers_;

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

enum class KernelCommandType { kInvalid = 0, kKernel = 1, kMemory = 2, kCommand = 3 };

struct ZeKernelCommandProps {
  std::string name;
  KernelCommandType type = KernelCommandType::kInvalid;
  ZeMemoryCommandRoute route;
  size_t simd_width;
  size_t bytes_transferred;
  std::array<uint32_t, 3> group_count;
  std::array<uint32_t, 3> group_size;
  size_t value_size;
  std::byte* value_array;
  ze_device_handle_t src_device = nullptr;  // Device for p2p memcpy, source of copy data
  ze_device_handle_t dst_device = nullptr;  // Device for p2p memcpy, destination of copy data
  void* dst = nullptr;                      // Addressess for MemorCopy or Fill
  void* src = nullptr;
};

struct ZeKernelCommand {
  ZeKernelCommandProps props;
  uint64_t device_timer_frequency_;
  uint64_t device_timer_mask_;
  ze_event_handle_t event_self = nullptr;  // in Local mode this evet goes to the Bridge kernel
  ze_event_handle_t event_swap = nullptr;  // event created in Local collection mode
  ze_device_handle_t device =
      nullptr;  // Device where the operation is submitted, associated with command list
  uint64_t kernel_id = 0;
  uint64_t append_time = 0;
  ze_context_handle_t context = nullptr;
  ze_command_list_handle_t command_list = nullptr;
  ze_command_queue_handle_t queue = nullptr;
  ze_fence_handle_t fence;
  uint64_t submit_time = 0;          // in ns
  uint64_t submit_time_device_ = 0;  // in ticks
  uint32_t tid = 0;
  uint64_t sycl_node_id_ = 0;
  uint64_t sycl_queue_id_ =
      PTI_INVALID_QUEUE_ID;  // default to invalid till we determine otherwise.
  uint32_t sycl_invocation_id_ = 0;
  uint64_t sycl_task_begin_time_ = 0;
  uint64_t sycl_enqk_begin_time_ = 0;
  std::string source_file_name_;
  uint32_t source_line_number_ = 0;
  uint32_t corr_id_ = 0;
  uint32_t callback_id_ = 0;
  uint64_t api_start_time_ = 0;  // in ns
  uint64_t api_end_time_ = 0;    // in ns
  uint64_t num_wait_events = 0;  // tracks wait event count for synchronization activity commands
  ze_result_t result_ = ZE_RESULT_SUCCESS;
};

struct ZeCommandQueue {
  ze_command_queue_handle_t queue_;
  ze_context_handle_t context_;
  ze_device_handle_t device_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;
};

struct ZeCommandListInfo {
  std::vector<std::unique_ptr<ZeKernelCommand>> kernel_commands;
  ze_context_handle_t context;
  ze_device_handle_t device;
  bool immediate;
  std::pair<uint32_t, uint32_t> oi_pair;
};

struct ZeDeviceDescriptor {
  uint64_t host_time_origin = 0;
  uint64_t device_time_origin = 0;
  uint64_t device_timer_frequency = 0;
  uint64_t device_timer_mask = 0;
  uint64_t device_sync_delta = 50'000'000ULL;  // 50ms
  ze_driver_handle_t driver = nullptr;
  ze_context_handle_t context = nullptr;
  ze_pci_ext_properties_t pci_properties{};
  ze_device_uuid_t uuid;
};

using ZeKernelGroupSizeMap = std::map<ze_kernel_handle_t, ZeKernelGroupSize>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;
using ZeImageSizeMap = std::map<ze_image_handle_t, size_t>;
using ZeDeviceMap = std::map<ze_device_handle_t, std::vector<ze_device_handle_t>>;

using OnZeKernelFinishCallback = void (*)(void*, std::vector<ZeKernelCommandExecutionRecord>&);
using OnZeApiCallsFinishCallback = void (*)(void*, ZeKernelCommandExecutionRecord&);

class ZeCollector {
 public:  // Interface
  ZeCollector(const ZeCollector&) = delete;
  ZeCollector& operator=(const ZeCollector&) = delete;
  ZeCollector(ZeCollector&&) = delete;
  ZeCollector& operator=(ZeCollector&&) = delete;

  static std::unique_ptr<ZeCollector> Create(std::atomic<pti_result>* pti_state,
                                             CollectorOptions options,
                                             OnZeKernelFinishCallback acallback = nullptr,
                                             OnZeApiCallsFinishCallback fcallback = nullptr,
                                             void* callback_data = nullptr) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(nullptr != pti_state);
    ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_CRITICAL(
          "zeInit() returned: {}. There might be Level-Zero Loader "
          "and Tracing library mismatch. Cannot continue",
          static_cast<uint32_t>(status));
      *pti_state = pti_result::PTI_ERROR_DRIVER;
    }

    ze_api_version_t version = utils::ze::GetVersion();
    PTI_ASSERT(ZE_MAJOR_VERSION(version) >= 1 && ZE_MINOR_VERSION(version) >= 3);
    SPDLOG_DEBUG("Driver version major: {}, minor: {}", ZE_MAJOR_VERSION(version),
                 ZE_MINOR_VERSION(version));
    if ((*pti_state) != pti_result::PTI_SUCCESS) {
      // zeInit returned not SUCCESS but we want to know version of driver in any case
      return nullptr;
    }

    auto collector =
        std::unique_ptr<ZeCollector>(new ZeCollector(options, acallback, fcallback, callback_data));
    PTI_ASSERT(collector != nullptr);
    collector->parent_state_ = pti_state;

    zel_tracer_desc_t tracer_desc = {ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector.get()};
    zel_tracer_handle_t tracer = nullptr;
    overhead::Init();
    status = zelTracerCreate(&tracer_desc, &tracer);
    overhead_fini(zelTracerCreate_id);

    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_CRITICAL(
          "Unable to create Level Zero tracer, error code {0:#x}\n"
          "It could be due to old driver installed where tracing enabled with "
          "setting env variable ZE_ENABLE_TRACING_LAYER to 1.",
          static_cast<std::size_t>(status));
      *pti_state = pti_result::PTI_ERROR_TRACING_NOT_INITIALIZED;
      return nullptr;
    }

    collector->collection_mode_ =
        SelectZeCollectionMode(collector->driver_introspection_capable_,
                               collector->options_.disabled_mode, collector->options_.hybrid_mode);
    SPDLOG_DEBUG("\tCollection_mode: {}", (uint32_t)collector->collection_mode_);

    collector->options_.api_tracing = true;
    collector->EnableTracer(tracer);

    status = collector->l0_wrapper_.w_zelEnableTracingLayer();
    if (ZE_RESULT_SUCCESS == status) {
      global_ref_count++;
    }

    if (collector->options_.disabled_mode) {
      SPDLOG_DEBUG("\tRunning in disabled mode");
      status = collector->l0_wrapper_.w_zelDisableTracingLayer();
      if (ZE_RESULT_SUCCESS == status) {
        global_ref_count--;
      }
    } else {
      SPDLOG_DEBUG("\tRunning in enabled mode");
    }

    collector->tracer_ = tracer;
    return collector;
  }

  ~ZeCollector() {
#if !defined(_WIN32)
    // TODO Looks on Windows due to not specified DLLs unload order we hit assert here
    if (tracer_ != nullptr) {
      [[maybe_unused]] ze_result_t status = zelTracerDestroy(tracer_);
      // PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      tracer_ = nullptr;
    }
#endif
  }
  enum class ZeCollectionMode { Full = 0, Hybrid = 1, Local = 2 };
  enum class ZeCollectionState { Normal = 0, Abnormal = 1 };

  /**
   * \internal
   * \brief Returns the current device timestamps, CPU in nanoceconds and GPU in ticks
   *
   * \param [in] device - the device to get the timestamps from
   * \param [out] host_time - the CPU time in nanoseconds
   * \param [out] device_time - the GPU time in ticks
   * \return ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR on failure
   *
   * Previously, zeDeviceGetGlobalTimestamps was called every time CPU and GPU timestamps
   * needed to be synced (via utils::ze::GetDeviceTimestamps(device, host_time, device_time);)
   * GPU cycles were then converted to CPU (aka host) timescale.
   * However, zeDeviceGetGlobalTimestamps has a high latency,
   * so it is not sutable for frequent calls (e.g. each dozen of micro-secs), especially in profiler
   *
   * The current implementation calls zeDeviceGetGlobalTimestamp less often:
   * once in ~ dozens (or hundreds) of milliseconds
   * (see CPUGPUTimeInterpolationHelper.delta) - for sync CPU/GPU point
   * per thread per device. (per thread - to avoid any synchronization between threads)
   * The delta between sycn points is selected  empirically, but it is important keep in mind that
   * on systems with 32 GPU time counter - this counter would wrap around every few seconds.
   * The delta should be less than this wrap around time.
   *
   * Another change - the GPU timer frequency is not interpolated anymore but rather
   * taken from the device descriptor. This assumes that it is constant.
   *
   * The function is called synchroniously in a profiled thread, once per device.
   *
   * InterpolationHelper keeps the sync point from some recent past.
   * If CPU time, from the recent sych point, exceeded delta - makes a new sych point.
   * The sync point data are returned to a caller. The caller ueses the sync point data
   * to convert GPU cycles to CPU time or do other ops with them.
   *
   */
  ze_result_t GetDeviceTimestamps(ze_device_handle_t device, uint64_t* host_time,
                                  uint64_t* device_time) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(host_time != nullptr);
    PTI_ASSERT(device_time != nullptr);
    if (timer_helpers_.find(device) == timer_helpers_.end()) {
      timer_helpers_[device] = std::make_unique<CPUGPUTimeInterpolationHelper>(
          device, device_descriptors_[device].device_timer_frequency,
          device_descriptors_[device].device_timer_mask,
          device_descriptors_[device].device_sync_delta);
    }
    uint64_t anchor_host_time = 0;
    uint64_t anchor_device_time = 0;
    auto helper = timer_helpers_[device].get();
    uint64_t current_host_time = utils::GetTime();
    if (current_host_time - helper->cpu_timestamp_ > helper->delta_) {
      ze_result_t res =
          utils::ze::GetDeviceTimestamps(device, &anchor_host_time, &anchor_device_time);
      PTI_ASSERT(res == ZE_RESULT_SUCCESS);
      helper->cpu_timestamp_ = anchor_host_time;
      helper->gpu_timestamp_ = anchor_device_time;
    } else {
      anchor_host_time = helper->cpu_timestamp_;
      anchor_device_time = helper->gpu_timestamp_;
    }
    current_host_time = utils::GetTime();
    uint64_t current_device_time = anchor_device_time + ((current_host_time - anchor_host_time) /
                                                         timer_helpers_[device]->coeff_);

    *host_time = current_host_time;
    *device_time = current_device_time;
    return ZE_RESULT_SUCCESS;
  }

  static ZeCollectionMode SelectZeCollectionMode(bool introspection_capable, bool& disabled_mode,
                                                 bool& hybrid_mode) {
    ZeCollector::ZeCollectionMode mode = ZeCollectionMode::Full;
    disabled_mode = false;
    hybrid_mode = false;
    SPDLOG_TRACE("In {}", __FUNCTION__);

    SPDLOG_INFO("\tIntrospectable?: {}", introspection_capable);
    SPDLOG_INFO("\tChecking if the mode enforced by PTI_COLLECTION_MODE environment variable");
    try {
      std::string env_string = utils::GetEnv("PTI_COLLECTION_MODE");
      int32_t env_value = -1;
      if (!env_string.empty()) {
        env_value = std::stoi(env_string);
        SPDLOG_DEBUG("\tDetected var: {}", env_value);
        switch (env_value) {
          case 0:  // FullAPI collection mode
            SPDLOG_DEBUG("\tForced Full collection");
            disabled_mode = false;
            hybrid_mode = false;
            mode = ZeCollectionMode::Full;
            break;
          case 1:  // Asking for Hybrid collection mode
            if (introspection_capable) {
              SPDLOG_INFO(
                  "\tLevel-Zero Introspection API available: Forced fallback to hybrid mode.");
              disabled_mode = false;
              hybrid_mode = true;
              mode = ZeCollectionMode::Hybrid;
              break;
            } else {
              SPDLOG_WARN("\tLevel-Zero Introspection API not available: Cannot do Hybrid mode.");
            }
            break;
          case 2:  // Asking for Local collection mode
            if (introspection_capable) {
              SPDLOG_INFO("\tForced fallback to Local mode.");
              disabled_mode = true;
              hybrid_mode = false;
              mode = ZeCollectionMode::Local;
            } else {
              SPDLOG_WARN("\tLevel-Zero Introspection API not available: Cannot do Local mode.");
            }
            break;
        }
      } else {
        if (introspection_capable) {
          mode = ZeCollectionMode::Local;
          disabled_mode = true;
          hybrid_mode = false;
        }
      }
    } catch (std::invalid_argument const& /*ex*/) {
      hybrid_mode = false;
      disabled_mode = false;
      mode = ZeCollectionMode::Full;
    } catch (std::out_of_range const& /*ex*/) {
      hybrid_mode = false;
      disabled_mode = false;
      mode = ZeCollectionMode::Full;
    }
    return mode;
  }

  bool IsIntrospectionCapable() { return driver_introspection_capable_; }

  bool IsDynamicTracingCapable() { return loader_dynamic_tracing_capable_; }

  // We get here on StartTracing/enable of L0 related view kinds.
  // The caller needs to ensure duplicated enable of view_kinds do not happen on a per thread basis.
  void EnableTracing() {
    // switches to full/hybrid api mode - only if we are not already in full/hybrid api mode.  Else
    // records another view_kind active in region.
    startstop_mode_changer.ToStartTracing();
  }

  // We get here on StopTracing/disable of L0 related view kinds.
  // The caller needs to ensure duplicated disables of view_kinds do not happen on a per thread
  // basis.
  void DisableTracing() {
    // disables full/hybrid api mode - only if all previously active view_kinds are disabled
    // across all threads. Else records another view_kind deactivated in region.
    startstop_mode_changer.ToStopTracing();
  }

  /**
   * @brief Stop Tracing if in case of any abnormal collection situation
   *
   * this could be no L0 Introspection API while dynamic tracing enabled
   * so application called PTI after context, or queue created
   */
  void AbnormalStopTracing() {
    ze_result_t status = l0_wrapper_.w_zelDisableTracingLayer();
    if (ZE_RESULT_SUCCESS == status) {
      global_ref_count--;
      collection_state_ = ZeCollectionState::Abnormal;

      PTI_ASSERT(global_ref_count == 0);
      SPDLOG_DEBUG("In {}, L0 Tracing OFF, tid: {}", __FUNCTION__, thread_local_pid_tid_info.tid);
      return;
    }
    SPDLOG_CRITICAL("In {}, Cannot stop L0 Tracing, tid: {}", __FUNCTION__,
                    thread_local_pid_tid_info.tid);
    PTI_ASSERT(false);
    return;
  }

  void DisableTracer() {
    // PTI_ASSERT(tracer_ != nullptr);
#if !defined(_WIN32)
    overhead::Init();
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    overhead_fini(zelTracerSetEnabled_id);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
  }

 private:  // Implementation
  ZeCollector(CollectorOptions options, OnZeKernelFinishCallback acallback,
              OnZeApiCallsFinishCallback fcallback, void* callback_data)
      : options_(options),
        acallback_(acallback),
        fcallback_(fcallback),
        callback_data_(callback_data),
        event_cache_(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP),
        swap_event_pool_(512),
        startstop_mode_changer(this) {
    CreateDeviceMap();
    UpdateDeviceSyncDelta();
    ze_result_t res = l0_wrapper_.InitDynamicTracingWrappers();
    if (ZE_RESULT_SUCCESS == res) {
      loader_dynamic_tracing_capable_ = true;
      MarkIntrospection();
    }
  }

  int32_t trace_all_env_value = utils::IsSetEnv("PTI_VIEW_DRIVER_API");

  ze_result_t DetectIntrospectionApis(const ze_driver_handle_t& driver) {
    SPDLOG_TRACE("In {}", __FUNCTION__);

    ze_result_t res = l0_wrapper_.InitIntrospectionWrappers();
    if (ZE_RESULT_SUCCESS != res) {
      return res;
    }

    // Create Context
    ze_context_handle_t context = nullptr;
    ze_context_desc_t cdesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    overhead::Init();
    ze_result_t status = zeContextCreate(driver, &cdesc, &context);
    overhead_fini(zeContextCreate_id);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    // Create Event Pool
    ze_event_pool_handle_t event_pool = nullptr;
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};

    overhead::Init();
    status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
    overhead_fini(zeEventPoolCreate_id);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    // IntrospectionAPI --- return status determines if api's available on this driver.
    ze_event_pool_flags_t event_pool_flags;
    overhead::Init();
    status = l0_wrapper_.w_zeEventPoolGetFlags(event_pool, &event_pool_flags);
    overhead_fini(zeEventPoolGetFlags_id);

    // Cleanup
    overhead::Init();
    ze_result_t status1 = zeEventPoolDestroy(event_pool);
    overhead_fini(zeEventPoolDestroy_id);
    PTI_ASSERT(status1 == ZE_RESULT_SUCCESS);

    overhead::Init();
    status1 = zeContextDestroy(context);
    overhead_fini(zeContextDestroy_id);
    PTI_ASSERT(status1 == ZE_RESULT_SUCCESS);

    return status;
  }

  void CreateDeviceMap() {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    const auto drivers = utils::ze::GetDriverList();
    for (auto* const driver : drivers) {
      const auto devices = utils::ze::GetDeviceList(driver);
      for (auto* const device : devices) {
        device_descriptors_[device] = GetZeDeviceDescriptor(device);
        SPDLOG_DEBUG("\tdevice: {}", (void*)device);
        const auto sub_devices = utils::ze::GetSubDeviceList(device);
        device_map_[device] = sub_devices;
        for (auto* const sub_device : sub_devices) {
          SPDLOG_DEBUG("\tsub-device: {}", (void*)sub_device);
          device_descriptors_[sub_device] = GetZeDeviceDescriptor(sub_device);
        }
      }
    }
  }

  void UpdateDeviceSyncDelta() {
    // in future we can try make it per device
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    uint64_t delta = 0;
    auto env_string = utils::GetEnv("PTI_DEVICE_SYNC_DELTA");
    SPDLOG_INFO("Checking DeviceSyncDelta by PTI_DEVICE_SYNC_DELTA environment variable");
    if (!env_string.empty()) {
      try {
        delta = std::stoll(env_string);
        SPDLOG_INFO("\tPTI_DEVICE_SYNC_DELTA is {} ns, will use it in device tracing", delta);
      } catch (std::invalid_argument const& /*ex*/) {
        delta = CPUGPUTimeInterpolationHelper::kSycnDeltaDefault;  // fallback to default
      } catch (std::out_of_range const& /*ex*/) {
        delta = CPUGPUTimeInterpolationHelper::kSycnDeltaDefault;  // fallback to default
      }
    }
    for (auto& [device, descriptor] : device_descriptors_) {
      descriptor.device_sync_delta = delta;
    }
  }

  void MarkIntrospection() {
    const auto drivers = utils::ze::GetDriverList();
    for (auto const driver : drivers) {
      const auto devices = utils::ze::GetDeviceList(driver);
      for (auto const device : devices) {
        ze_device_properties_t device_properties = {};
        device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        overhead::Init();
        ze_result_t status = zeDeviceGetProperties(device, &device_properties);
        overhead_fini(zeDeviceGetProperties_id);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        // Checking only on one driver for GPU device
        const ze_device_type_t type = ZE_DEVICE_TYPE_GPU;
        if (type == device_properties.type) {
          // Issue api call here and detect if introspection apis are supported by underlying
          // rolling driver.
          status = DetectIntrospectionApis(driver);
          if (status == ze_result_t::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            driver_introspection_capable_ = false;
          } else if (status == ze_result_t::ZE_RESULT_SUCCESS) {
            driver_introspection_capable_ = true;
          }
          break;
        }
      }
    }
  }

  static ZeDeviceDescriptor GetZeDeviceDescriptor(const ze_device_handle_t device) {
    ZeDeviceDescriptor desc = {};

    bool ret = utils::ze::GetDeviceTimerFrequency_TimestampMask_UUID(
        device, desc.device_timer_frequency, desc.device_timer_mask, desc.uuid);
    PTI_ASSERT(ret);

    ze_pci_ext_properties_t pci_device_properties{};
    pci_device_properties.pNext = nullptr;
    pci_device_properties.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;

    overhead::Init();
    ze_result_t status = zeDevicePciGetPropertiesExt(device, &pci_device_properties);
    overhead_fini(zeDevicePciGetPropertiesExt_id);

    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN("Unable to get PCI properties for device {} driver returned 0x{:x}",
                  static_cast<const void*>(device), static_cast<std::size_t>(status));
      std::memset(&pci_device_properties.address, 0, sizeof(pci_device_properties.address));
      std::memset(&pci_device_properties.maxSpeed, 0, sizeof(pci_device_properties.maxSpeed));
    }

    desc.pci_properties = pci_device_properties;
    return desc;
  }

  ze_result_t ReBuildCommandListInfo([[maybe_unused]] ze_command_list_handle_t command_list) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_bool_t isImmediate = true;
    ze_context_handle_t hContext;
    ze_device_handle_t hDevice;
    uint32_t ordinal = static_cast<uint32_t>(-1);
    uint32_t index = static_cast<uint32_t>(-1);

    status = l0_wrapper_.w_zeCommandListGetDeviceHandle(command_list, &hDevice);

    if (ZE_RESULT_SUCCESS != status) {
      // as this function is called from many places - makes sense to communicate an issue here
      SPDLOG_WARN(
          "Level-Zero Introspection API is not present. Local Collection not possible."
          " Disabling Level-Zero Tracing.");
      if (nullptr != parent_state_) {
        *(parent_state_) = pti_result::PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED;
      }
      return status;
    }

    status = l0_wrapper_.w_zeCommandListGetContextHandle(command_list, &hContext);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = l0_wrapper_.w_zeCommandListIsImmediate(command_list, &isImmediate);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    SPDLOG_DEBUG("\tIs CmdList immediate?  {}", isImmediate);
    if (isImmediate) {
      status = l0_wrapper_.w_zeCommandListImmediateGetIndex(command_list, &index);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      status = l0_wrapper_.w_zeCommandListGetOrdinal(command_list, &ordinal);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    std::pair<uint32_t, uint32_t> oi(ordinal, index);
    CreateCommandListInfo(command_list, hContext, hDevice, oi, isImmediate);

    return status;
  }

  bool CommandListInfoExists(ze_command_list_handle_t clist_handle) {
    std::shared_lock lock(command_list_map_mutex_);
    return command_list_map_.find(clist_handle) != command_list_map_.end();
  }

  const ZeCommandListInfo& GetCommandListInfoConst(ze_command_list_handle_t clist_handle) {
    if (!CommandListInfoExists(clist_handle)) {
      auto result = ReBuildCommandListInfo(clist_handle);
      PTI_ASSERT(result == ZE_RESULT_SUCCESS);  // Can't happen - unsupported mode (non-local).
    }

    std::shared_lock lock(command_list_map_mutex_);
    return command_list_map_[clist_handle];
  }

  ZeCommandListInfo& GetCommandListInfo(ze_command_list_handle_t clist_handle) {
    if (!CommandListInfoExists(clist_handle)) {
      auto result = ReBuildCommandListInfo(clist_handle);
      PTI_ASSERT(result == ZE_RESULT_SUCCESS);  // Can't happen - unsupported mode (non-local).
    }

    std::shared_lock lock(command_list_map_mutex_);
    return command_list_map_[clist_handle];
  }

  auto FindCommandListInfo(ze_command_list_handle_t clist_handle) {
    std::shared_lock lock(command_list_map_mutex_);
    return command_list_map_.find(clist_handle);
  }

  void CopyDeviceUUIDTo(ze_device_handle_t device_handle, uint8_t* ptr) {
    SPDLOG_TRACE("In {} device_handle: {}", __FUNCTION__, (void*)device_handle);
    if (device_descriptors_.find(device_handle) != device_descriptors_.end()) {
      std::copy_n(device_descriptors_[device_handle].uuid.id, ZE_MAX_DEVICE_UUID_SIZE, ptr);
      return;
    }
    // in some cases for CCL Copy GetMemoryAllocProperties returns new,
    // not seen so far, device handle that looks like a parent device to two tile devices
    // TODO: this is not a good solution, need to investigate why this happens
    // and how to handle it properly
    // Specifically - bad that here write to device_descriptors_ without lock
    SPDLOG_DEBUG("In {} detected new device: device_handle: {}", __FUNCTION__,
                 (void*)device_handle);
    device_descriptors_[device_handle] = GetZeDeviceDescriptor(device_handle);
    std::copy_n(device_descriptors_[device_handle].uuid.id, ZE_MAX_DEVICE_UUID_SIZE, ptr);
    return;
  }

  /**
   *  \internal
   *  \warning To be called only after the corresponding Command is processed
   *  \warning lock to be acquired in caller chain
   */
  void CleanDestroyedEvent(bool destroyed, ze_event_handle_t event) {
    if (destroyed) {
      destroyed_events_.erase(event);
      ze_event_handle_t swap_event = swap_event_pool_.RemoveKeyEventFromShadowCache(event);
      SPDLOG_TRACE("--- RemoveKeyEventFromShadowCache returned: {}",
                   static_cast<const void*>(swap_event));
      if (swap_event != nullptr) {
        swap_event_pool_.ReturnSwapEvent(swap_event);
      }
    }
  }

  /**
   *  \internal
   *  \warning lock to be acquired in caller chain
   */
  bool QueryEventStatusUnlessDestroyed(ze_event_handle_t event, ze_result_t& status) {
    bool destroyed = destroyed_events_.find(event) != destroyed_events_.end();
    SPDLOG_TRACE("\tPrior checking event status event: {}, destroyed: {} ",
                 static_cast<const void*>(event), destroyed);
    if (!destroyed) {
      overhead::Init();
      status = zeEventQueryStatus(event);
      overhead_fini(zeEventQueryStatus_id);
    }
    return destroyed;
  }

  void ProcessCallEvent(ze_event_handle_t event, std::vector<uint64_t>* kids,
                        std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in caller
    // const std::lock_guard<std::mutex> lock(lock_);
    SPDLOG_TRACE("In {}, event: {}", __FUNCTION__, (void*)event);

    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeEventQueryStatus(event);
    overhead_fini(zeEventQueryStatus_id);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_DEBUG("\tIn {} EventQueryStatus returned: {}, Returning...", __FUNCTION__,
                   static_cast<uint32_t>(status));
      return;
    }

    [[maybe_unused]] uint32_t idx = 0;
    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = (*it).get();
      PTI_ASSERT(command != nullptr);

      if (command->event_self != nullptr) {
        status = ZE_RESULT_NOT_READY;
        bool destroyed = QueryEventStatusUnlessDestroyed(command->event_self, status);
        idx++;
        if (status == ZE_RESULT_SUCCESS || destroyed) {
          // if this the event passed to the function or another event we met at the way
          if (command->event_self == event) {
            done = true;
          }
          SPDLOG_TRACE("\tIs Passed event? {}, idx: {}", done, idx);

          // TODO: this could be good check in Debug
          /*if (collection_mode_ == Local && command->event_swap != nullptr) {
            status = zeEventHostReset(command->event_swap);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          }*/

          SPDLOG_TRACE("\tEvent SIGNALED or destroyed!");
          ProcessCallCommand(command, kids, kcexecrec);
          CleanDestroyedEvent(destroyed, command->event_self);
          it = kernel_command_list_.erase(it);
        } else {  // command not ready yet
          it++;
        }
      } else {
        PTI_ASSERT(command->event_self == nullptr);
        SPDLOG_WARN("\tDeleting of unexpected command {} containing zero event.", (void*)command);
        it = kernel_command_list_.erase(it);
      }

      if (done) {
        break;
      }
    }
  }

  void ProcessCallFence(ze_fence_handle_t fence, std::vector<uint64_t>* kids,
                        std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);
    SPDLOG_TRACE("In {}, fence: {}", __FUNCTION__, static_cast<const void*>(fence));
    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeFenceQueryStatus(fence);
    overhead_fini(zeFenceQueryStatus_id);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN("\tFence Query Status unsuccessful.");
      return;
    }

    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = (*it).get();
      PTI_ASSERT(command != nullptr);

      SPDLOG_TRACE("\tFence Query: {} - {}.", (void*)command, (void*)fence);
      if ((command->fence != nullptr) && (command->fence == fence)) {
        SPDLOG_TRACE("\tFence Query2 {} - {}.", (void*)command, (void*)fence);
        ProcessCallCommand(command, kids, kcexecrec);
        // TODO - check if we need to clean up the fence event
        // CleanDestroyedEvent(destroyed, command->event_self);
        it = kernel_command_list_.erase(it);
        done = true;
      } else if (command->event_self != nullptr) {
        status = ZE_RESULT_NOT_READY;
        bool destroyed = QueryEventStatusUnlessDestroyed(command->event_self, status);
        if (destroyed || status == ZE_RESULT_SUCCESS) {
          ProcessCallCommand(command, nullptr, kcexecrec);
          CleanDestroyedEvent(destroyed, command->event_self);
          it = kernel_command_list_.erase(it);
        } else {  // command not ready yet
          it++;
        }
      } else {
        PTI_ASSERT(command->event_self == nullptr);
        SPDLOG_WARN("\tDeleting of unexpected command {} containing zero event.", (void*)command);
        it = kernel_command_list_.erase(it);
      }

      if (done) {
        break;
      }
    }
  }

  constexpr uint64_t ComputeDuration(uint64_t start, uint64_t end, uint64_t freq, uint64_t mask) {
    uint64_t duration = 0;
    if (start <= end) {
      duration = (end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    } else {  // Timer Overflow
      duration = ((mask + 1ULL) + end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    }
    return duration;
  }

  inline void GetHostTime(const ZeKernelCommand* command, const ze_kernel_timestamp_result_t& ts,
                          uint64_t& start, uint64_t& end) {
    uint64_t device_freq = command->device_timer_frequency_;
    uint64_t device_mask = command->device_timer_mask_;

    uint64_t device_start = ts.global.kernelStart & device_mask;
    uint64_t device_end = ts.global.kernelEnd & device_mask;

    // Why submit_time_device_ and time_shift ?
    //
    // - All times reported by PTI_VIEW in CPU (aka Host) timescale
    // - However GPU "commands" (kernel & memory transfers) start/end reported in GPU timescale
    // - There is significant time drift between CPU and GPU, so to cope wth it - need to
    // "sync" often calling zeDeviceGetGlobalTimestamps,
    //  where command->submit_time_device_ comes with GPU time
    //        command->submit_time         comes with CPU time
    //
    // "sync" points are made around "command(s)" submit to GPU:
    // - at Enter to CommandListAppendLaunch<...>  time for an Immediate Command List
    // - at Enter to CommandQueueExecuteCommandLists for not Immediate CommandLists

    //  GPU time mask applied to the GPU time to remove some spiritous bits (in case they made
    //  there)
    uint64_t device_submit_time = (command->submit_time_device_ & device_mask);

    // time_shift calculated in GPU scale between sync point and GPU command start,
    // then it recalculated to CPU timescale units
    uint64_t time_shift = 0;

    if (device_start > device_submit_time) {
      time_shift = (device_start - device_submit_time) * NSEC_IN_SEC / device_freq;
    } else {
      // overflow
      time_shift =
          (device_mask - device_submit_time + 1 + device_start) * NSEC_IN_SEC / device_freq;
    }

    // GPU command duration recalculated to CPU time scale units
    uint64_t duration = ComputeDuration(device_start, device_end, device_freq, device_mask);

    // here GPU command start and end (on GPU) are calculated in CPU timescale
    start = command->submit_time + time_shift;
    end = start + duration;
  }

  void ProcessCallTimestamp(const ZeKernelCommand* command,
                            const ze_kernel_timestamp_result_t& timestamp, int tile,
                            bool /*in_summary*/,
                            std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    uint64_t host_start = 0;
    uint64_t host_end = 0;
    GetHostTime(command, timestamp, host_start, host_end);
    PTI_ASSERT(host_start <= host_end);

    std::string name = command->props.name;

    PTI_ASSERT(!name.empty());

    if (kcexecrec && acallback_) {
      SPDLOG_TRACE("Processing callback rec: {}", name);
      ZeKernelCommandExecutionRecord rec = {};

      rec.kid_ = command->kernel_id;
      rec.tid_ = command->tid;
      rec.cid_ = command->corr_id_;
      rec.callback_id_ = command->callback_id_;
      rec.append_time_ = command->append_time;
      rec.submit_time_ = command->submit_time;
      rec.start_time_ = host_start;
      rec.end_time_ = host_end;
      PTI_ASSERT(queue_ordinal_index_map_.count(command->queue) != 0);
      std::pair<uint32_t, uint32_t> oi;
      oi = queue_ordinal_index_map_[command->queue];
      rec.engine_ordinal_ = oi.first;
      rec.engine_index_ = oi.second;
      rec.tile_ = tile;
      auto it = device_descriptors_.find(command->device);
      PTI_ASSERT(it != device_descriptors_.end());
      rec.pci_prop_ = it->second.pci_properties;
      rec.name_ = std::move(name);
      rec.queue_ = command->queue;
      rec.device_ = command->device;
      rec.route_ = command->props.route;
      rec.num_wait_events_ = command->num_wait_events;
      rec.result_ = command->result_;
      if (command->props.src_device != nullptr) {
        CopyDeviceUUIDTo(command->props.src_device, static_cast<uint8_t*>(rec.src_device_uuid));
      }
      if (command->props.dst_device != nullptr) {
        CopyDeviceUUIDTo(command->props.dst_device, static_cast<uint8_t*>(rec.dst_device_uuid));
      }

      if ((tile >= 0) && (device_map_.count(command->device) == 1) &&
          !device_map_[command->device].empty()) {  // Implicit Scaling
        rec.implicit_scaling_ = true;
      } else {
        rec.implicit_scaling_ = false;
      }

      if (command->props.type == KernelCommandType::kMemory) {
        rec.device_ = command->props.src_device;
        rec.dst_device_ = command->props.dst_device;
        if (command->props.src_device != nullptr) {
          auto dev_it = device_descriptors_.find(command->props.src_device);
          PTI_ASSERT(dev_it != device_descriptors_.end());
          rec.pci_prop_ = dev_it->second.pci_properties;
        }
        if (command->props.dst_device != nullptr) {
          auto dev_it = device_descriptors_.find(command->props.dst_device);
          PTI_ASSERT(dev_it != device_descriptors_.end());
          rec.dst_pci_prop_ = dev_it->second.pci_properties;
        }
        if (command->props.bytes_transferred > 0) {
          rec.bytes_xfered_ = command->props.bytes_transferred;
        } else if (command->props.value_size > 0) {
          rec.value_set_ = command->props.value_size;
        }
      }

      rec.context_ = command->context;

      if (command->props.type == KernelCommandType::kKernel) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_queue_id_ = command->sycl_queue_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.sycl_enqk_begin_time_ = command->sycl_enqk_begin_time_;

        // rec.sycl_enqk_end_time_ = sycl_data_kview.sycl_enqk_end_time_;

        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
        if (command->device != nullptr) {
          CopyDeviceUUIDTo(command->device, static_cast<uint8_t*>(rec.src_device_uuid));
        }
      }
      if (command->props.type == KernelCommandType::kMemory) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_queue_id_ = command->sycl_queue_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
      }

      kcexecrec->push_back(rec);
    }
  }

  void ProcessCallCommand(ZeKernelCommand* command, std::vector<uint64_t>* kids,
                          std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    SPDLOG_TRACE("In {} command kid: {}", __FUNCTION__, command->kernel_id);
    if (kids) {
      kids->push_back(command->kernel_id);
    }

    ze_kernel_timestamp_result_t timestamp{};

    ze_event_handle_t event_to_query = command->event_self;
    if (ZeCollectionMode::Local == collection_mode_ && command->event_swap != nullptr) {
      event_to_query = command->event_swap;
    }
    SPDLOG_TRACE("\tQuery KernelTimestamp on event: {}", static_cast<const void*>(event_to_query));
    overhead::Init();
    ze_result_t status = zeEventQueryKernelTimestamp(event_to_query, &timestamp);
    overhead_fini(zeEventQueryKernelTimestamp_id);
    if (status != ZE_RESULT_SUCCESS) {
      // sporadic - smth wrong with event from time to time
      SPDLOG_WARN("In {}, zeEventQueryKernelTimestamp returned: {} for event: {}", __FUNCTION__,
                  static_cast<uint32_t>(status), static_cast<const void*>(event_to_query));
    }

    ProcessCallTimestamp(command, timestamp, -1, true, kcexecrec);

    if (ZeCollectionMode::Local != collection_mode_) {
      event_cache_.ReleaseEvent(command->event_self);
      command->event_self = nullptr;
    }
  }

  void ProcessCalls(std::vector<uint64_t>* kids,
                    std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    SPDLOG_TRACE("In {} Kernel command list size: {}", __FUNCTION__, kernel_command_list_.size());
    ze_result_t status = ZE_RESULT_SUCCESS;
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_command_list_.begin();
    while (it != kernel_command_list_.end()) {
      ZeKernelCommand* command = (*it).get();

      if (command->event_self != nullptr) {
        SPDLOG_TRACE("\tChecking status of event {}", (void*)command->event_self);
        overhead::Init();
        status = zeEventQueryStatus(command->event_self);
        overhead_fini(zeEventQueryStatus_id);
        if (status == ZE_RESULT_SUCCESS) {
          /* if (collection_mode_ == Local) { // TODO this should be in Debug only
            if (command->event_swap != nullptr) {
              overhead::Init();
              status = zeEventQueryStatus(command->event_swap);
              overhead_fini(zeEventQueryStatus_id);
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            }
          }*/
          ProcessCallCommand(command, kids, kcexecrec);
          it = kernel_command_list_.erase(it);
        } else {
          ++it;
        }
      } else {
        SPDLOG_WARN("\tDeleting of unexpected command {} containing zero event.", (void*)command);
        it = kernel_command_list_.erase(it);
      }
    }
  }

  void CreateCommandListInfo(ze_command_list_handle_t command_list, ze_context_handle_t context,
                             ze_device_handle_t device, std::pair<uint32_t, uint32_t>& oi_pair,
                             bool immediate) {
    const std::lock_guard<std::mutex> lock(lock_);

    // exclusive lock of command_list_map_   as we are changing it ("writing" to it)
    // all other accesses to it ("reading") would be protected by shared_lock
    {
      const std::lock_guard<std::shared_mutex> cl_list_lock(command_list_map_mutex_);
      if (command_list_map_.count(command_list)) {
        ZeCommandListInfo& command_list_info = command_list_map_[command_list];
        if (command_list_info.immediate) {
          queue_ordinal_index_map_.erase(reinterpret_cast<ze_command_queue_handle_t>(command_list));
        }
        command_list_map_.erase(command_list);
      }

      PTI_ASSERT(device_descriptors_.count(device) != 0);

      command_list_map_[command_list] = {std::vector<std::unique_ptr<ZeKernelCommand>>(), context,
                                         device, immediate, oi_pair};
    }

    if (immediate) {
      if (queue_ordinal_index_map_.count(
              reinterpret_cast<ze_command_queue_handle_t>(command_list)) == 0) {
        queue_ordinal_index_map_[reinterpret_cast<ze_command_queue_handle_t>(command_list)] =
            oi_pair;
      }
    }
  }

  void PrepareToExecuteCommandLists(ze_command_list_handle_t* command_lists,
                                    uint32_t command_list_count, ze_command_queue_handle_t queue,
                                    ze_fence_handle_t fence) {
    uint32_t q_index;
    uint32_t q_ordinal;
    uint64_t host_time_sync = 0;
    uint64_t device_time_sync = 0;

    // TODO Consider taking only one Timestamp for all command lists
    // as all those are in one queue and on one device
    /*
        auto it = command_queues_.find(queue);
        PTI_ASSERT(it != command_queues_.end());
        ze_device_handle_t device = it->second.device_;
        PTI_ASSERT(nullptr != device);
        ze_result_t status = zeDeviceGetGlobalTimestamps(device, &host_time_sync,
       &device_time_sync); PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    */

    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_command_list_handle_t clist = command_lists[i];
      PTI_ASSERT(clist != nullptr);

      const ZeCommandListInfo& info = GetCommandListInfoConst(clist);

      // as all command lists submitted to the execution into queue - they are not immediate
      PTI_ASSERT(!info.immediate);
      PTI_ASSERT(info.device != nullptr);
      ze_result_t status = GetDeviceTimestamps(info.device, &host_time_sync, &device_time_sync);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      const std::lock_guard<std::mutex> lock(lock_);
      if (queue_ordinal_index_map_.count(queue) == 0) {
        ze_result_t res = l0_wrapper_.w_zeCommandQueueGetIndex(queue, &q_index);
        ze_result_t res2 = l0_wrapper_.w_zeCommandQueueGetOrdinal(queue, &q_ordinal);
        if (ZE_RESULT_SUCCESS != res || ZE_RESULT_SUCCESS != res2) {
          if (nullptr != parent_state_) {
            *(parent_state_) = pti_result::PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED;
          }
          AbnormalStopTracing();
        }
        queue_ordinal_index_map_[queue] = std::make_pair(q_ordinal, q_index);
      }

      for (auto it = info.kernel_commands.begin(); it != info.kernel_commands.end(); it++) {
        ZeKernelCommand* command = (*it).get();
        if (!command->tid) {
          command->tid = thread_local_pid_tid_info.tid;
        }
        command->queue = queue;
        command->submit_time = host_time_sync;
        command->submit_time_device_ = device_time_sync;

        PTI_ASSERT(command->append_time <= command->submit_time);
        command->fence = fence;
      }
    }
  }

  void PostSubmitKernelCommands(ze_command_list_handle_t* command_lists,
                                uint32_t command_list_count, std::vector<uint64_t>* kids) {
    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_command_list_handle_t clist = command_lists[i];
      PTI_ASSERT(clist != nullptr);
      ZeCommandListInfo& info = GetCommandListInfo(clist);

      // Needs to go after GetCommandListInfo and before cl_list_lock to avoid deadlocks.
      const std::lock_guard<std::mutex> lock(lock_);

      // info is a reference to an element in this map, which we're modifying here.
      const std::lock_guard<std::shared_mutex> cl_list_lock(command_list_map_mutex_);
      // as all command lists submitted to the execution into queue - they are not immediate
      // TODO: does this effect zeCommandListImmediateAppendCommandListsExp ?
      PTI_ASSERT(!info.immediate);
      for (auto it = info.kernel_commands.begin(); it != info.kernel_commands.end(); it++) {
        ZeKernelCommand* command = (*it).get();
        if (kids) {
          kids->push_back(command->kernel_id);
        }
        kernel_command_list_.push_back(std::move(*it));
      }
      info.kernel_commands.clear();
    }
  }

  ze_context_handle_t GetCommandListContext(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.context;
  }

  ze_device_handle_t GetCommandListDevice(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.device;
  }

  bool IsCommandListImmediate(ze_command_list_handle_t command_list) {
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.immediate;
  }

  void AddImage(ze_image_handle_t image, size_t size) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 0);
    image_size_map_[image] = size;
  }

  void RemoveImage(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 1);
    image_size_map_.erase(image);
  }

  size_t GetImageSize(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (image_size_map_.count(image) == 1) {
      return image_size_map_[image];
    }
    return 0;
  }

  void AddKernelGroupSize(ze_kernel_handle_t kernel, const ZeKernelGroupSize& group_size) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_[kernel] = group_size;
  }

  void RemoveKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_.erase(kernel);
  }

  ZeKernelGroupSize GetKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_group_size_map_.count(kernel) == 0) {
      return {0, 0, 0};
    }
    return kernel_group_size_map_[kernel];
  }

  // Callbacks
  static void OnEnterEventPoolCreate(ze_event_pool_create_params_t* params, void* global_data,
                                     void** instance_data) {
    const ze_event_pool_desc_t* desc = *(params->pdesc);
    if (desc == nullptr) {
      return;
    }
    if (desc->flags & ZE_EVENT_POOL_FLAG_IPC) {
      return;
    }

    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (ZeCollectionMode::Local == collector->collection_mode_) {
      return;
    }

    ze_event_pool_desc_t* profiling_desc = new ze_event_pool_desc_t;
    PTI_ASSERT(profiling_desc != nullptr);
    profiling_desc->stype = desc->stype;
    // PTI_ASSERT(profiling_desc->stype == ZE_STRUCTURE_TYPE_EVENT_POOL_DESC);
    profiling_desc->pNext = desc->pNext;
    profiling_desc->flags = desc->flags;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    profiling_desc->count = desc->count;

    *(params->pdesc) = profiling_desc;
    *instance_data = profiling_desc;
    SPDLOG_DEBUG("In {} over-wrote profiling_desc -- onenter", __FUNCTION__);
  }

  static void OnExitEventPoolCreate(ze_event_pool_create_params_t* /*params*/,
                                    ze_result_t /*result*/, void* global_data,
                                    void** instance_data) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (ZeCollectionMode::Local == collector->collection_mode_) {
      return;
    }
    ze_event_pool_desc_t* desc = static_cast<ze_event_pool_desc_t*>(*instance_data);
    SPDLOG_DEBUG("In {} cleaned up profiling_desc -- onexit", __FUNCTION__);
    delete desc;
  }

  static void OnEnterEventDestroy(ze_event_destroy_params_t* params, void* global_data,
                                  void** /*instance_data*/, std::vector<uint64_t>* /*kids*/) {
    SPDLOG_TRACE("In {} event: {}", __FUNCTION__, (void*)*(params->phEvent));
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      // only events that managed by collector should be taken care
      if (ZeCollectionMode::Local == collector->collection_mode_) {
        const std::lock_guard<std::mutex> lock(collector->lock_);
        collector->destroyed_events_.insert(*(params->phEvent));
      }
    }
  }

  static void OnEnterEventHostReset(ze_event_host_reset_params_t* params, void* global_data,
                                    void** /*instance_data*/,
                                    [[maybe_unused]] std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {} event: {}", __FUNCTION__, (void*)*(params->phEvent));
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }

      if (ZeCollectionMode::Local == collector->collection_mode_) {
        ze_event_handle_t swap_event =
            collector->swap_event_pool_.GetSwapEventFromShadowCache(*(params->phEvent));
        SPDLOG_TRACE("--- In {} , self_event: {}, swap_event: {}", __FUNCTION__,
                     (void*)(*(params->phEvent)), (void*)(swap_event));
        if (nullptr != swap_event) {
          ze_result_t status = zeEventHostReset(swap_event);
          if (status != ZE_RESULT_SUCCESS) {
            SPDLOG_WARN("\tIn {} zeEventHostReset returned: {}, ", __FUNCTION__, (uint32_t)status);
          }
        }
      }
    }
  }

  static void OnExitEventHostSynchronize(ze_event_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids,
                                         uint64_t synch_corrid) {
    SPDLOG_TRACE("In {} event: {}", __FUNCTION__, (void*)*(params->phEvent));
    ze_result_t status;
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (result == ZE_RESULT_SUCCESS) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->ProcessCallEvent(*(params->phEvent), kids, &kcexec);
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
    // Process generation of synch record even if result is not successful.
    if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      ZeKernelCommandExecutionRecord rec = {};
      ze_event_handle_t event_h = *params->phEvent;
      ze_event_pool_handle_t epool_h = nullptr;
      ze_context_handle_t ctxt_h = nullptr;
      rec.context_ = nullptr;
      if (collector->IsIntrospectionCapable()) {
        status = collector->l0_wrapper_.w_zeEventGetEventPool(event_h, &epool_h);
        if (status == ZE_RESULT_SUCCESS) {
          status = collector->l0_wrapper_.w_zeEventPoolGetContextHandle(epool_h, &ctxt_h);
          if (status == ZE_RESULT_SUCCESS) {
            rec.context_ = ctxt_h;
          } else {
            SPDLOG_WARN(
                "\tLevel-Zero Introspection API: zeEventPoolGetContextHandle return unsuccessful "
                "-- inserting null context handle in synch. record..");
          };
        };
      }
      rec.name_ = "zeEventHostSynchronize";
      rec.tid_ = thread_local_pid_tid_info.tid;
      rec.start_time_ = ze_instance_data.start_time_host;
      rec.end_time_ = utils::GetTime();
      rec.event_ = event_h;
      rec.cid_ = synch_corrid;
      rec.result_ = result;
      rec.callback_id_ = zeEventHostSynchronize_id;
      kcexec.push_back(rec);
      collector->acallback_(collector->callback_data_, kcexec);
    }
  }

  static void OnExitCommandListHostSynchronize(
      [[maybe_unused]] ze_command_list_host_synchronize_params_t* params, ze_result_t result,
      void* global_data, void** /*instance_data*/, std::vector<uint64_t>* kids,
      [[maybe_unused]] uint64_t synch_corrid) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (result == ZE_RESULT_SUCCESS) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->ProcessCalls(kids, &kcexec);
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
    // Process generation of synch record even if result is not successful.
    if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      ze_result_t status;
      ZeKernelCommandExecutionRecord rec = {};
      ze_command_list_handle_t clist_h = *params->phCommandList;
      ze_context_handle_t ctxt_h = nullptr;
      rec.context_ = nullptr;
      if (collector->IsIntrospectionCapable()) {
        status = collector->l0_wrapper_.w_zeCommandListGetContextHandle(clist_h, &ctxt_h);
        if (status == ZE_RESULT_SUCCESS) {
          rec.context_ = ctxt_h;
        } else {
          SPDLOG_WARN(
              "\tLevel-Zero Introspection API: zeCommandListGetContextHandle return unsuccessful "
              "-- "
              "inserting null context handle in synch. record..");
        };
      }
      rec.name_ = "zeCommandListHostSynchronize";
      rec.tid_ = thread_local_pid_tid_info.tid;
      rec.start_time_ = ze_instance_data.start_time_host;
      rec.end_time_ = utils::GetTime();
      rec.event_ = nullptr;
      rec.cid_ = synch_corrid;
      rec.result_ = result;
      rec.callback_id_ = zeCommandListHostSynchronize_id;
      kcexec.push_back(rec);
      collector->acallback_(collector->callback_data_, kcexec);
    }
  }

  static void OnExitEventQueryStatus([[maybe_unused]] ze_event_query_status_params_t* params,
                                     [[maybe_unused]] ze_result_t result, void* /*global_data*/,
                                     void** /*instance_data*/, std::vector<uint64_t>* /*kids*/) {
    SPDLOG_TRACE("In {}, result {} event: {}", __FUNCTION__, static_cast<uint32_t>(result),
                 static_cast<const void*>(*(params->phEvent)));
    // this call-back is useful to see if we are re-entering to it via Tracing level
    // this should not happen when we are inside of Tracing layer..
    // but things can get weird..
  }

  static void OnExitFenceCreate(ze_fence_create_params_t* params,
                                [[maybe_unused]] ze_result_t result, void* global_data,
                                void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    const std::lock_guard<std::mutex> lock(collector->lock_);
    ze_command_queue_handle_t fence_queue = *(params->phCommandQueue);
    ze_fence_handle_t* fence_h = *params->pphFence;
    collector->fence_queue_map_.insert({*fence_h, fence_queue});
    SPDLOG_TRACE("In {}, fence_h {}, queue_h {}", __FUNCTION__, static_cast<void*>(*fence_h),
                 static_cast<void*>(fence_queue));
  }

  static void OnExitFenceHostSynchronize(ze_fence_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids,
                                         uint64_t synch_corrid) {
    SPDLOG_TRACE("In {}, result {} ", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phFence) != nullptr);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->ProcessCallFence(*(params->phFence), kids, &kcexec);

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
    // Process generation of synch record even if result is not successful.
    if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      ZeKernelCommandExecutionRecord rec = {};
      ze_fence_handle_t fence_h = *params->phFence;
      rec.context_ = nullptr;
      rec.queue_ = nullptr;
      auto it_fence = collector->fence_queue_map_.find(fence_h);
      if (it_fence != collector->fence_queue_map_.end()) {
        rec.queue_ = it_fence->second;
        auto it = collector->command_queues_.find(it_fence->second);
        if (it != collector->command_queues_.end()) {
          rec.context_ = it->second.context_;
        }
      }
      rec.name_ = "zeFenceHostSynchronize";
      rec.tid_ = thread_local_pid_tid_info.tid;
      rec.start_time_ = ze_instance_data.start_time_host;
      rec.end_time_ = utils::GetTime();
      rec.event_ = nullptr;
      rec.cid_ = synch_corrid;
      rec.result_ = result;
      rec.callback_id_ = zeFenceHostSynchronize_id;
      kcexec.push_back(rec);
      collector->acallback_(collector->callback_data_, kcexec);
    }
  }

  static void OnExitImageCreate(ze_image_create_params_t* params, ze_result_t result,
                                void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result {} ", __FUNCTION__, (uint32_t)result);
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);

      ze_image_desc_t image_desc = **(params->pdesc);
      size_t image_size = image_desc.width;
      switch (image_desc.type) {
        case ZE_IMAGE_TYPE_2D:
        case ZE_IMAGE_TYPE_2DARRAY:
          image_size *= image_desc.height;
          break;
        case ZE_IMAGE_TYPE_3D:
          image_size *= image_desc.height * image_desc.depth;
          break;
        default:
          break;
      }

      switch (image_desc.format.type) {
        case ZE_IMAGE_FORMAT_TYPE_UINT:
        case ZE_IMAGE_FORMAT_TYPE_UNORM:
        case ZE_IMAGE_FORMAT_TYPE_FORCE_UINT32:
          image_size *= sizeof(unsigned int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_SINT:
        case ZE_IMAGE_FORMAT_TYPE_SNORM:
          image_size *= sizeof(int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_FLOAT:
          image_size *= sizeof(float);
          break;
        default:
          break;
      }

      collector->AddImage(**(params->pphImage), image_size);
    }
  }

  static void OnExitImageDestroy(ze_image_destroy_params_t* params, ze_result_t result,
                                 void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result {} ", __FUNCTION__, (uint32_t)result);
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      collector->RemoveImage(*(params->phImage));
    }
  }

  static void PrepareToAppendKernelCommand(ZeCollector* collector,
                                           ze_command_list_handle_t command_list,
                                           KernelCommandType kernel_type,
                                           ze_event_handle_t& signal_event, void** instance_data) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(instance_data != nullptr);
    SPDLOG_TRACE("In {} Collection mode: {}, Cmdl: {}, signal_event: {}, kernel_type: {}",
                 __FUNCTION__, (uint32_t)collector->collection_mode_, (void*)command_list,
                 (void*)signal_event, (uint32_t)kernel_type);

    if (!collector->CommandListInfoExists(command_list)) {
      ze_result_t res = collector->ReBuildCommandListInfo(command_list);
      if (res != ZE_RESULT_SUCCESS) {
        collector->AbnormalStopTracing();
        return;
      }
    }
    const std::lock_guard<std::mutex> lock(collector->lock_);
    ze_context_handle_t context = collector->GetCommandListContext(command_list);
    ze_device_handle_t device = collector->GetCommandListDevice(command_list);

    ZeKernelCommand* command = new ZeKernelCommand;

    SPDLOG_TRACE("\tCreated New ZeKernelCommand: {}, passes via instance data", (void*)command);
    PTI_ASSERT(command != nullptr);
    *instance_data = command;

    command->props.type = kernel_type;
    command->command_list = command_list;
    command->device = device;
    command->context = context;
    SPDLOG_TRACE("\tcontext: {}, device: {}", (void*)context, (void*)device);

    command->event_swap = nullptr;
    if (ZeCollectionMode::Local != collector->collection_mode_) {
      if (signal_event == nullptr) {
        signal_event = collector->event_cache_.GetEvent(context);
        PTI_ASSERT(signal_event != nullptr);
        SPDLOG_DEBUG("\tIn {} created Signal event from event_cache", __FUNCTION__);
      }
      command->event_self = signal_event;
    } else {
      // Setting up data for later submission Bridge Kernel (or Memory Op)
      // the Bridge kernel will be submitted after the Target Kernel
      // Swapping the events:
      // Target kernel will signal the new ("swap") event with Timestamp enabled
      // Bridge Kernel will signal the Target Kernel initial event
      if (signal_event != nullptr) {
        ze_event_handle_t swap_event =
            collector->swap_event_pool_.GetSwapEventFromShadowCache(signal_event);
        SPDLOG_TRACE("\t\tContext: {}, Device: {}, self_event: {}, swap_event: {}", (void*)context,
                     (void*)device, (void*)(signal_event), (void*)(swap_event));
        command->event_self = signal_event;
        if (nullptr == swap_event) {
          swap_event = collector->swap_event_pool_.GetEvent(context);
          PTI_ASSERT(swap_event != nullptr);
          collector->swap_event_pool_.StoreEventsToShadowCache(command->event_self, swap_event);
          SPDLOG_TRACE("\t\tCreated swap_event: {}", static_cast<const void*>(swap_event));
        }
        // both should not be signalled.
        // this verifies that EventReset handled properly, as a lot of events might re-used
        //
        // However with UR the behaviour changed - events come signalled
        // TODO: clarify it with UR
        // PTI_ASSERT(ZE_RESULT_NOT_READY == zeEventQueryStatus(signal_event));
        // PTI_ASSERT(ZE_RESULT_NOT_READY == zeEventQueryStatus(swap_event));

        command->event_swap = swap_event;
        signal_event = command->event_swap;
        SPDLOG_TRACE("\t\t swap event: {}, self_event: {}",
                     static_cast<const void*>(command->event_swap),
                     static_cast<const void*>(command->event_self));
      } else {
        signal_event = collector->event_cache_.GetEvent(context);
        PTI_ASSERT(signal_event != nullptr);
        SPDLOG_DEBUG("\tLocal collection mode, created Signal event from event_cache: {}",
                     static_cast<const void*>(signal_event));
        command->event_self = signal_event;
      }
    }

    uint64_t host_timestamp = 0;
    uint64_t device_timestamp = 0;  // in ticks

    ze_result_t status = collector->GetDeviceTimestamps(device, &host_timestamp, &device_timestamp);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_instance_data.timestamp_host = host_timestamp;
    ze_instance_data.timestamp_device = device_timestamp;
  }

  void PostAppendKernelCommandCommon(ZeCollector* /*collector*/, ZeKernelCommand* command,
                                     ZeKernelCommandProps& props, ze_event_handle_t& signal_event,
                                     ZeCommandListInfo& command_list_info,
                                     std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, command: {}, kernel name {}", __FUNCTION__,
                 static_cast<const void*>(command), props.name.c_str());
    if (ZeCollectionState::Abnormal == collection_state_) {
      return;
    }
    PTI_ASSERT(command != nullptr);
    command->props = props;

    PTI_ASSERT(signal_event != nullptr);
    command->tid = thread_local_pid_tid_info.tid;
    uint64_t host_timestamp = ze_instance_data.start_time_host;
    command->append_time = host_timestamp;
    command->kernel_id = UniKernelId::GetKernelId();
    command->device_timer_frequency_ = device_descriptors_[command->device].device_timer_frequency;
    command->device_timer_mask_ = device_descriptors_[command->device].device_timer_mask;
    if (command->props.type == KernelCommandType::kKernel) {
      command->sycl_node_id_ = sycl_data_kview.sycl_node_id_;
      command->sycl_queue_id_ = sycl_data_kview.sycl_queue_id_;
      command->sycl_invocation_id_ = sycl_data_kview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_kview.sycl_task_begin_time_;
      command->sycl_enqk_begin_time_ = sycl_data_kview.sycl_enqk_begin_time_;
      sycl_data_kview.kid_ = command->kernel_id;
      sycl_data_kview.tid_ = command->tid;
      command->source_file_name_ = sycl_data_kview.source_file_name_;
      command->source_line_number_ = sycl_data_kview.source_line_number_;
      if (sycl_data_kview.cid_) {
        command->corr_id_ = sycl_data_kview.cid_;
      } else {
        command->corr_id_ = UniCorrId::GetUniCorrId();
        sycl_data_kview.cid_ = command->corr_id_;
      }
    } else if (command->props.type == KernelCommandType::kMemory) {
      command->props.src_device = props.src_device;
      command->props.dst_device = props.dst_device;

      sycl_data_mview.kid_ = command->kernel_id;
      sycl_data_mview.tid_ = command->tid;
      if (sycl_data_mview.cid_) {
        command->corr_id_ = sycl_data_mview.cid_;
      } else {
        command->corr_id_ = UniCorrId::GetUniCorrId();
        sycl_data_mview.cid_ = command->corr_id_;
      }

      command->sycl_node_id_ = sycl_data_mview.sycl_node_id_;
      command->sycl_queue_id_ = sycl_data_mview.sycl_queue_id_;
      command->sycl_invocation_id_ = sycl_data_mview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_mview.sycl_task_begin_time_;
      command->source_file_name_ = sycl_data_mview.source_file_name_;
      command->source_line_number_ = sycl_data_mview.source_line_number_;
    } else {
      if (!command
               ->corr_id_) {  // for synchronization activity commands the corrid is the api corrid.
        command->corr_id_ =
            UniCorrId::GetUniCorrId();  // setting here for non-synchronization activity corr_id.
      }
    }

    SPDLOG_TRACE("\tcorr_id: {}", command->corr_id_);

    // creating unique ptr here so command will be properly deleted when removed from
    // the container it stored
    std::unique_ptr<ZeKernelCommand> p_command(command);

    {
      const std::lock_guard<std::mutex> lock(lock_);

      if (command_list_info.immediate) {
        command->submit_time = command->append_time;
        command->submit_time_device_ =
            ze_instance_data.timestamp_device;  // append time and submit time are the same
        command->queue = reinterpret_cast<ze_command_queue_handle_t>(command->command_list);
        kernel_command_list_.push_back(std::move(p_command));
        SPDLOG_TRACE("\tImmediate CmdList, command: {} pushed to kernel_command_list_, queue: {}",
                     static_cast<void*>(command), static_cast<void*>(command->queue));
        kids->push_back(command->kernel_id);
      } else {
        command_list_info.kernel_commands.push_back(std::move(p_command));
        SPDLOG_TRACE("\tcommand: {} pushed to command_list_info", (void*)command);
      }
    }

    // it could be that event swap was not needed  - in this case event_swap would be 0
    // and we can not append Bridge kernel
    if (nullptr != command->event_swap && ZeCollectionMode::Local == collection_mode_) {
      SPDLOG_DEBUG("\t\t Will be appending Bridge command!");
      bool append_res = true;
      if (command->props.type == KernelCommandType::kKernel) {
        ze_kernel_handle_t kernel =
            bridge_kernel_pool_.GetMarkKernel(command->context, command->device);
        PTI_ASSERT(kernel != nullptr);
        append_res = A2AppendBridgeKernel(kernel, command->command_list, command->event_self,
                                          command->event_swap);
      } else if (command->props.type == KernelCommandType::kMemory) {
        SPDLOG_TRACE("\t\tDevices in Memory command: src: {}, dst {}",
                     (void*)command->props.src_device, (void*)command->props.dst_device);

        auto buffer = device_buffer_pool_.GetBuffers(command->context, command->device);
        PTI_ASSERT(buffer != nullptr);
        append_res = A2AppendBridgeMemoryCopyOrFillEx(command->command_list, command->event_self,
                                                      command->event_swap, buffer,
                                                      device_buffer_pool_.buffer_size_);
      } else if (command->props.type == KernelCommandType::kCommand) {
        append_res =
            A2AppendBridgeBarrier(command->command_list, command->event_self, command->event_swap);
      }
      PTI_ASSERT(append_res);
    }
  }

  void PostAppendKernel(ZeCollector* collector, ze_kernel_handle_t kernel,
                        const ze_group_count_t* group_count, ze_event_handle_t& signal_event,
                        ze_command_list_handle_t command_list, void** instance_data,
                        std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(kernel != nullptr);
    SPDLOG_TRACE("In {}", __FUNCTION__);

    ZeKernelCommandProps props{};

    props.name = utils::ze::GetKernelName(kernel, options_.demangle);
    props.type = KernelCommandType::kKernel;
    props.simd_width = utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelGroupSize group_size{};
    {
      const std::lock_guard<std::mutex> lock(lock_);
      if (kernel_group_size_map_.count(kernel) == 0) {
        group_size = {0, 0, 0};
      } else {
        group_size = kernel_group_size_map_[kernel];
      }
    }

    props.group_size[0] = group_size.x;
    props.group_size[1] = group_size.y;
    props.group_size[2] = group_size.z;

    if (group_count != nullptr) {
      props.group_count[0] = group_count->groupCountX;
      props.group_count[1] = group_count->groupCountY;
      props.group_count[2] = group_count->groupCountZ;
    }

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    PostAppendKernelCommandCommon(collector, static_cast<ZeKernelCommand*>(*instance_data), props,
                                  signal_event, command_list_info, kids);
  }

  void PostAppendMemoryCommand(ZeCollector* collector, std::string command_name,
                               size_t bytes_transferred, const void* src, const void* dst,
                               ze_event_handle_t& signal_event,
                               ze_command_list_handle_t command_list, void** instance_data,
                               std::vector<uint64_t>* kids, size_t pattern_size = 0) {
    SPDLOG_TRACE(
        "In: {}, CmdList: {}, Signal event: {}, dst: {}, src: {}, \
                 bytes_transferred: {}, pattern_size: {}",
        __FUNCTION__, (void*)command_list, (void*)signal_event, dst, src, bytes_transferred,
        pattern_size);
    if (ZeCollectionState::Abnormal == collection_state_) {
      return;
    }
    PTI_ASSERT(command_list != nullptr);

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command_name), bytes_transferred, (src ? context : nullptr), src,
                         (dst ? context : nullptr), dst, pattern_size);

    PostAppendKernelCommandCommon(collector, static_cast<ZeKernelCommand*>(*instance_data), props,
                                  signal_event, command_list_info, kids);
  }

  void AppendMemoryCommandContext(ZeCollector* collector, std::string command,
                                  size_t bytes_transferred, ze_context_handle_t src_context,
                                  const void* src, ze_context_handle_t dst_context, const void* dst,
                                  ze_event_handle_t& signal_event,
                                  ze_command_list_handle_t command_list, void** instance_data,
                                  std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    PTI_ASSERT(command_list != nullptr);
    // TODO: How to protect command_list_info reference?
    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, src_context, src,
                         (dst_context ? dst_context : context), dst);

    PostAppendKernelCommandCommon(collector, static_cast<ZeKernelCommand*>(*instance_data), props,
                                  signal_event, command_list_info, kids);
  }

  void AppendImageMemoryCopyCommand(ZeCollector* collector, std::string command,
                                    ze_image_handle_t image, const void* src, const void* dst,
                                    ze_event_handle_t& signal_event,
                                    ze_command_list_handle_t command_list, void** instance_data,
                                    std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    PTI_ASSERT(command_list != nullptr);

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);
    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    size_t bytes_transferred = GetImageSize(image);

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, context, src, context, dst);

    // TODO implement image copy support in Local collection model
    if (collector->collection_mode_ != ZeCollectionMode::Local) {
      PostAppendKernelCommandCommon(collector, static_cast<ZeKernelCommand*>(*instance_data), props,
                                    signal_event, command_list_info, kids);
    }
  }

  void PostAppendCommand(ZeCollector* collector, std::string command,
                         ze_event_handle_t& signal_event, ze_command_list_handle_t command_list,
                         void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    if (ZeCollectionState::Abnormal == collection_state_) {
      return;
    }
    PTI_ASSERT(command_list != nullptr);

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props{};
    props.name = std::move(command);
    props.type = KernelCommandType::kCommand;

    PostAppendKernelCommandCommon(collector, static_cast<ZeKernelCommand*>(*instance_data), props,
                                  signal_event, command_list_info, kids);
  }

  static ZeKernelCommandProps GetTransferProps(std::string name, size_t bytes_transferred,
                                               ze_context_handle_t src_context, const void* src,
                                               ze_context_handle_t dst_context, const void* dst,
                                               size_t pattern_size = 0) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    PTI_ASSERT(!name.empty());

    ZeMemoryCommandRoute route;
    ze_device_handle_t hSrcDevice = nullptr;
    ze_device_handle_t hDstDevice = nullptr;

    if (src_context != nullptr && src != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(src_context, src, &props, &hSrcDevice);
      overhead_fini(zeMemGetAllocProperties_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
          route.peer_2_peer = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
          route.peer_2_peer = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
          route.peer_2_peer = true;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
          route.peer_2_peer = true;
          break;
        default:
          route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
          route.peer_2_peer = false;
          break;
      }
    }

    if (dst_context != nullptr && dst != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(dst_context, dst, &props, &hDstDevice);
      overhead_fini(zeMemGetAllocProperties_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
          route.peer_2_peer = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
          route.peer_2_peer = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
          break;
        default:
          route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
          route.peer_2_peer = false;
          break;
      }
    }

    //
    // TODO:  Redo the stringified -P2P propagation.
    //
    name += "(" + route.StringifyTypesCompact();
    ze_bool_t p2p_access = 0;
    if (route.peer_2_peer && hSrcDevice && hDstDevice && (hSrcDevice != hDstDevice)) {
      auto status = zeDeviceCanAccessPeer(hSrcDevice, hDstDevice, &p2p_access);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (p2p_access) {
        name += route.StringifyPeer2PeerCompact();
      }
    }
    name += ")";
    SPDLOG_TRACE("\t\tIn {}, ops name: {}, p2p: {}", __FUNCTION__, name.c_str(), route.peer_2_peer);

    ZeKernelCommandProps props{};
    props.name = std::move(name);
    props.route = route;
    props.bytes_transferred = bytes_transferred;
    props.value_size = pattern_size;
    props.type = KernelCommandType::kMemory;
    props.src_device = hSrcDevice;
    props.dst_device = hDstDevice;
    props.dst = const_cast<void*>(dst);
    props.src = const_cast<void*>(src);
    return props;
  }

  static ZeKernelCommandProps GetCommandProps(std::string name) {
    PTI_ASSERT(!name.empty());

    ZeKernelCommandProps props{};
    props.name = name;
    props.type = KernelCommandType::kCommand;
    return props;
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kKernel,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, ze_result_t result, void* global_data,
      void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->PostAppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                                  *(params->phSignalEvent), *(params->phCommandList), instance_data,
                                  kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kKernel,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->PostAppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                                  *(params->phSignalEvent), *(params->phCommandList), instance_data,
                                  kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kKernel,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->PostAppendKernel(collector, *(params->phKernel),
                                  *(params->ppLaunchArgumentsBuffer), *(params->phSignalEvent),
                                  *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
    SPDLOG_TRACE("In {}, new (swapped)  signal event: {}", __FUNCTION__,
                 (void*)(*(params->phSignalEvent)));
  }

  static void OnExitCommandListAppendMemoryCopy(ze_command_list_append_memory_copy_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->PostAppendMemoryCommand(collector, "zeCommandListAppendMemoryCopy",
                                         *(params->psize), *(params->psrcptr), *(params->pdstptr),
                                         *(params->phSignalEvent), *(params->phCommandList),
                                         instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendMemoryFill(ze_command_list_append_memory_fill_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->PostAppendMemoryCommand(collector, "zeCommandListAppendMemoryFill",
                                         *(params->psize), *(params->ppattern), *(params->pptr),
                                         *(params->phSignalEvent), *(params->phCommandList),
                                         instance_data, kids, *(params->ppattern_size));
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                              void* global_data, void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kCommand,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                             ze_result_t result, void* global_data,
                                             void** instance_data, std::vector<uint64_t>* kids,
                                             uint64_t synch_corrid) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, (uint32_t)result);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCommand* command = static_cast<ZeKernelCommand*>(*instance_data);
      command->corr_id_ = synch_corrid;
      command->num_wait_events = *params->pnumWaitEvents;
      command->callback_id_ = zeCommandListAppendBarrier_id;
      command->result_ = result;
      collector->PostAppendCommand(collector, "zeCommandListAppendBarrier",
                                   *(params->phSignalEvent), *(params->phCommandList),
                                   instance_data, kids);
    } else {
      // Process generation of synch record even if result is not successful.
      // TODO barrier sync based on api start/end times here if needed.  For now no gen if result
      // shows unsuccess. rec.name_ = "zeCommandListAppendBarrier";
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kCommand,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids, uint64_t synch_corrid) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCommand* command = static_cast<ZeKernelCommand*>(*instance_data);
      command->corr_id_ = synch_corrid;
      command->num_wait_events = *params->pnumWaitEvents;
      command->callback_id_ = zeCommandListAppendMemoryRangesBarrier_id;
      command->result_ = result;
      collector->PostAppendCommand(collector, "zeCommandListAppendMemoryRangesBarrier",
                                   *(params->phSignalEvent), *(params->phCommandList),
                                   instance_data, kids);
    } else {
      // Process generation of synch record even if result is not successful.
      // TODO barrier sync based on api start/end times here if needed.  For now no gen if result
      // shows unsuccess. rec.name_ = "zeCommandListAppendMemoryRangesBarrier";
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  // TODO -- uncomment this method and corresponding gen portion, for support of context barriers.
  /*
    static void OnExitContextTodoSystemBarrier(ze_context_system_barrier_params_t* params,
                                           ze_result_t result, void* global_data,
                                           void** instance_data, uint64_t synch_corrid) {
      SPDLOG_TRACE("In {}, result {} ", __FUNCTION__, static_cast<uint32_t>(result));
      if (result == ZE_RESULT_SUCCESS) {
        PTI_ASSERT(*(params->phContext) != nullptr);
        ZeCollector* collector = static_cast<ZeCollector*>(global_data);
        std::vector<ZeKernelCommandExecutionRecord> kcexec;

        if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
          ZeKernelCommandExecutionRecord rec = {};
          ze_context_handle_t ctxt = *params->phContext;
          rec.name_ = "zeContextSystemBarrier";
          rec.tid_ = thread_local_pid_tid_info.tid;
          rec.start_time_ = ze_instance_data.start_time_host;
          rec.end_time_ = ze_instance_data.end_time_host;
          rec.context_ = ctxt;
          rec.cid_ = synch_corrid;
          rec.callback_id_ = zeContextSystemBarrier_id;
          SPDLOG_TRACE("In {}, context_h {}, corrId {}", __FUNCTION__, static_cast<void*>(ctxt),
                       rec.cid_);
          kcexec.push_back(rec);
          collector->acallback_(collector->callback_data_, kcexec);
        }
      }
    }
  */

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      size_t bytes_transferred = 0;
      const ze_copy_region_t* region = *(params->psrcRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height * (*(params->psrcPitch));
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      collector->PostAppendMemoryCommand(collector, "zeCommandListAppendMemoryCopyRegion",
                                         bytes_transferred, *(params->psrcptr), *(params->pdstptr),
                                         *(params->phSignalEvent), *(params->phCommandList),
                                         instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      ze_context_handle_t src_context = *(params->phContextSrc);
      // ze_context_handle_t dst_context = nullptr;
      collector->AppendMemoryCommandContext(collector, "zeCommandListAppendMemoryCopyFromContext",
                                            *(params->psize), src_context, *(params->psrcptr),
                                            nullptr, *(params->pdstptr), *(params->phSignalEvent),
                                            *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                                void* global_data, void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopy", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopyRegion", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(collector, "zeCommandListAppendImageCopyRegion",
                                              *(params->phSrcImage), nullptr, *(params->pdstptr),
                                              *(params->phSignalEvent), *(params->phCommandList),
                                              instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, void* global_data,
      void** instance_data) {
    SPDLOG_TRACE("In {} --->", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phCommandList), KernelCommandType::kMemory,
                                 *(params->phSignalEvent), instance_data);
  }

  static void OnExitCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      size_t bytes_transferred = 0;
      const ze_image_region_t* region = *(params->ppDstRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height;
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      // TODO implement image copy support in Local collection model
      if (collector->collection_mode_ != ZeCollectionMode::Local) {
        collector->PostAppendMemoryCommand(collector, "zeCommandListAppendImageCopyFromMemory",
                                           bytes_transferred, *(params->psrcptr), nullptr,
                                           *(params->phSignalEvent), *(params->phCommandList),
                                           instance_data, kids);
      }
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnExitCommandListCreate(ze_command_list_create_params_t* params, ze_result_t result,
                                      void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);

      // dummy pair
      std::pair<uint32_t, uint32_t> oi(-1, -1);
      collector->CreateCommandListInfo(**(params->pphCommandList), *(params->phContext),
                                       *(params->phDevice), oi, false);
    }
  }

  static void OnExitCommandListCreateImmediate(ze_command_list_create_immediate_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      ze_device_handle_t* hDevice = params->phDevice;
      if (hDevice == nullptr) {
        return;
      }

      const ze_command_queue_desc_t* clq_desc = *params->paltdesc;
      if (clq_desc == nullptr) {
        return;
      }

      ze_command_list_handle_t* command_list = *params->pphCommandList;
      if (command_list == nullptr) {
        return;
      }

      std::pair<uint32_t, uint32_t> oi(clq_desc->ordinal, clq_desc->index);

      collector->CreateCommandListInfo(**(params->pphCommandList), *(params->phContext),
                                       *(params->phDevice), oi, true);
    }
  }

  static void OnExitCommandListDestroy(ze_command_list_destroy_params_t* params, ze_result_t result,
                                       void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;

      {
        const std::lock_guard<std::mutex> lock(collector->lock_);
        collector->ProcessCalls(nullptr, &kcexec);
      }

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandListReset(ze_command_list_reset_params_t* params, ze_result_t result,
                                     void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      {
        // collector->ResetCommandList(*params->phCommandList);
        const std::lock_guard<std::mutex> lock(collector->lock_);
        collector->ProcessCalls(nullptr, &kcexec);
      }

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnEnterCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params, void* global_data,
      void** /*instance_data*/) {
    SPDLOG_TRACE("In {}", __FUNCTION__);
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);

    uint32_t command_list_count = *params->pnumCommandLists;
    if (command_list_count == 0) {
      return;
    }

    ze_command_list_handle_t* command_lists = *params->pphCommandLists;
    if (command_lists == nullptr) {
      return;
    }

    collector->PrepareToExecuteCommandLists(command_lists, command_list_count,
                                            *(params->phCommandQueue), *(params->phFence));
  }

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params, ze_result_t result,
      void* global_data, void** /*instance_data*/, std::vector<uint64_t>* kids) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      uint32_t command_list_count = *params->pnumCommandLists;
      if (command_list_count == 0) {
        return;
      }

      ze_command_list_handle_t* command_lists = *params->pphCommandLists;
      if (command_lists == nullptr) {
        return;
      }

      collector->PostSubmitKernelCommands(command_lists, command_list_count, kids);
    }
  }

  static void OnExitCommandQueueSynchronize(
      [[maybe_unused]] ze_command_queue_synchronize_params_t* params, ze_result_t result,
      void* global_data, void** /*instance_data*/, std::vector<uint64_t>* kids,
      [[maybe_unused]] uint64_t synch_corrid) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (result == ZE_RESULT_SUCCESS) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->ProcessCalls(kids, &kcexec);
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }

    // Process generation of synch record even if result is not successful.
    if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      ZeKernelCommandExecutionRecord rec = {};
      auto it = collector->command_queues_.find(*params->phCommandQueue);
      rec.context_ = nullptr;
      if (it != collector->command_queues_.end()) {
        rec.context_ = it->second.context_;
      };
      rec.name_ = "zeCommandQueueSynchronize";
      rec.tid_ = thread_local_pid_tid_info.tid;
      rec.start_time_ = ze_instance_data.start_time_host;
      rec.end_time_ = ze_instance_data.end_time_host;
      rec.context_ = nullptr;
      rec.queue_ = *params->phCommandQueue;
      rec.event_ = nullptr;
      rec.cid_ = synch_corrid;
      rec.callback_id_ = zeCommandQueueSynchronize_id;
      rec.result_ = result;
      kcexec.push_back(rec);
      collector->acallback_(collector->callback_data_, kcexec);
    }
  }

  static void OnExitCommandQueueCreate(ze_command_queue_create_params_t* params,
                                       [[maybe_unused]] ze_result_t result, void* global_data,
                                       void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    ze_device_handle_t* device = params->phDevice;
    if (device == nullptr) {
      return;
    }
    const ze_command_queue_desc_t* queue_desc = *params->pdesc;
    if (queue_desc == nullptr) {
      return;
    }
    ze_command_queue_handle_t* command_queue = *params->pphCommandQueue;
    if (command_queue == nullptr) {
      return;
    }

    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (collector->queue_ordinal_index_map_.count(*command_queue) == 0) {
      collector->queue_ordinal_index_map_[*command_queue] =
          std::make_pair(queue_desc->ordinal, queue_desc->index);
    }

    ZeCommandQueue desc{};
    desc.queue_ = *command_queue;
    desc.context_ = *(params->phContext);
    desc.device_ = *device;
    desc.engine_ordinal_ = queue_desc->ordinal;
    desc.engine_index_ = queue_desc->index;

    collector->command_queues_.erase(*command_queue);
    collector->command_queues_.insert({*command_queue, std::move(desc)});
  }

  static void OnExitCommandQueueDestroy(ze_command_queue_destroy_params_t* params,
                                        ze_result_t result, void* global_data,
                                        void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      {
        const std::lock_guard<std::mutex> lock(collector->lock_);
        collector->ProcessCalls(nullptr, &kcexec);
        collector->queue_ordinal_index_map_.erase(*params->phCommandQueue);
        collector->command_queues_.erase(*params->phCommandQueue);
      }

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitKernelSetGroupSize(ze_kernel_set_group_size_params_t* params,
                                       ze_result_t result, void* global_data,
                                       void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      ZeKernelGroupSize group_size{*(params->pgroupSizeX), *(params->pgroupSizeY),
                                   *(params->pgroupSizeZ)};
      collector->AddKernelGroupSize(*(params->phKernel), group_size);
    }
  }

  static void OnExitKernelDestroy(ze_kernel_destroy_params_t* params, ze_result_t result,
                                  void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      collector->RemoveKernelGroupSize(*(params->phKernel));
    }
  }

  static void OnExitContextDestroy(ze_context_destroy_params_t* params, ze_result_t result,
                                   void* global_data, void** /*instance_data*/) {
    SPDLOG_TRACE("In {}, result: {}", __FUNCTION__, static_cast<uint32_t>(result));
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      {
        const std::lock_guard<std::mutex> lock(collector->lock_);
        collector->ProcessCalls(nullptr, nullptr);
      }
      collector->event_cache_.ReleaseContext(*(params->phContext));
      if (collector->collection_mode_ == ZeCollectionMode::Local) {
        collector->swap_event_pool_.Clean(*(params->phContext));
      }
    }
  }

  zel_tracer_handle_t tracer_ = nullptr;
  CollectorOptions options_ = {};
  bool driver_introspection_capable_ = false;
  bool loader_dynamic_tracing_capable_ = false;
  CallbacksEnabled cb_enabled_ = {};
  OnZeKernelFinishCallback acallback_ = nullptr;
  OnZeApiCallsFinishCallback fcallback_ = nullptr;
  void* callback_data_ = nullptr;
  std::mutex lock_;

#include <tracing.gen>  // Auto-generated callbacks

  // mode=0 implies full apis; mode=1 implies hybrid apis only (eventpool); mode=2 is Local
  ZeCollectionMode collection_mode_ = ZeCollectionMode::Full;

  std::list<std::unique_ptr<ZeKernelCommand>> kernel_command_list_;
  // keep track of destroyed events, not request their status
  // CCL workloads often destroy events
  std::unordered_set<ze_event_handle_t> destroyed_events_;

  mutable std::shared_mutex command_list_map_mutex_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;
  ZeKernelGroupSizeMap kernel_group_size_map_;
  ZeDeviceMap device_map_;
  std::map<ze_device_handle_t, ZeDeviceDescriptor> device_descriptors_;

  ZeEventCache event_cache_;

  std::map<ze_command_queue_handle_t, std::pair<uint32_t, uint32_t>> queue_ordinal_index_map_;

  std::map<ze_command_queue_handle_t, ZeCommandQueue> command_queues_;
  std::map<ze_fence_handle_t, ze_command_queue_handle_t> fence_queue_map_;

  A2BridgeKernelPool bridge_kernel_pool_;
  A2DeviceBufferPool device_buffer_pool_;
  A2EventPool swap_event_pool_;

  Level0Wrapper l0_wrapper_;

  std::atomic<ZeCollectionState> collection_state_ = ZeCollectionState::Normal;

  // pointer to state of an object that created ZeCollector
  // a way to communicate abnormal situations
  std::atomic<pti_result>* parent_state_ = nullptr;

  class ZeStartStopModeChanger {
   private:
    // Track enable/disable tracing layer calls on a global basis - in order to swap apis.
    // zelEnableTracingLayer and zelDisableTracingLayer are not thread specific -- and act globally.
    //      We use ref_count to track how many L0 view_kinds are enabled/disabled on a global basis.

    std::atomic<uint64_t> ref_count = 0;
    ZeCollector* parent_collector_;
    std::mutex ss_lock_;

   public:
    ZeStartStopModeChanger(const ZeStartStopModeChanger&) = delete;
    ZeStartStopModeChanger& operator=(const ZeStartStopModeChanger&) = delete;
    ZeStartStopModeChanger(ZeStartStopModeChanger&&) = delete;
    ZeStartStopModeChanger& operator=(ZeStartStopModeChanger&&) = delete;
    ZeStartStopModeChanger() = delete;
    ZeStartStopModeChanger(ZeCollector* collector) {
      parent_collector_ = collector;
      ref_count = 0;
    }

    // switches to fully start tracing mode - only if we are not already in start mode.  Else
    // records another view_kind active in region.
    inline uint64_t ToStartTracing() {
      const std::lock_guard<std::mutex> lock(ss_lock_);
      if (ref_count) {
        ref_count++;
        return ref_count;
      }
      if (parent_collector_->options_.disabled_mode) {
        ze_result_t status = parent_collector_->l0_wrapper_.w_zelEnableTracingLayer();
        if (ZE_RESULT_SUCCESS == status) {
          PTI_ASSERT(global_ref_count == 0);
          global_ref_count++;
          SPDLOG_DEBUG(" --- In {}, Tracing ON, tid: {}", __FUNCTION__,
                       thread_local_pid_tid_info.tid);
        }
      }
      parent_collector_->cb_enabled_.acallback = true;
      parent_collector_->cb_enabled_.fcallback = true;
      if (ZeCollectionMode::Hybrid == parent_collector_->collection_mode_)
        parent_collector_->options_.hybrid_mode = false;
      ref_count++;
      return ref_count;
    }

    // switches to fully stopped tracing mode - only if all previously active view_kinds are
    // disabled across all threads(ref_count drops to 0). Else records another view_kind deactivated
    // in region.
    inline uint64_t ToStopTracing() {
      SPDLOG_TRACE("In {}", __FUNCTION__);
      const std::lock_guard<std::mutex> lock(ss_lock_);
      if (ref_count > 0) ref_count--;
      if (ref_count) {
        return ref_count;
      }

      // ref_count hit 0 -- we need to ensure tracing is fully disabled
      if (parent_collector_->options_.disabled_mode) {
        // no any collector ProcessCalls or similar here -
        // all finsihed tasks data should be captured and handled by proper callbacks by this point
        ze_result_t status = parent_collector_->l0_wrapper_.w_zelDisableTracingLayer();
        if (ZE_RESULT_SUCCESS == status) {
          global_ref_count--;
          PTI_ASSERT(global_ref_count == 0);
          SPDLOG_DEBUG(" --- In {}, Tracing OFF, tid: {}", __FUNCTION__,
                       thread_local_pid_tid_info.tid);
        }
      }
      parent_collector_->cb_enabled_.fcallback = false;
      if (ZeCollectionMode::Hybrid == parent_collector_->collection_mode_)
        parent_collector_->options_.hybrid_mode = true;
      return ref_count;
    }
  };
  ZeStartStopModeChanger startstop_mode_changer;
};

#endif  // PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
