//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_API_PTI_METRICS_SCOPE_HELPER_H_
#define SRC_API_PTI_METRICS_SCOPE_HELPER_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <pti/pti_metrics_scope.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "metrics_handler.h"
#include "pti/pti_callback.h"
#include "pti_assert.h"
#include "pti_metrics_scope_buffer.h"
#include "pti_metrics_scope_buffer_handler.h"

// Forward declarations
struct _pti_scope_collection_handle_t;
struct kernel_metric_data;

namespace {
// Global mutex to serialize Level Zero API calls
std::mutex g_level_zero_api_mutex;
}  // namespace

// Complete handle type
struct _pti_scope_collection_handle_t {
  _pti_scope_collection_handle_t() = default;
  _pti_scope_collection_handle_t(const _pti_scope_collection_handle_t&) = delete;
  _pti_scope_collection_handle_t& operator=(const _pti_scope_collection_handle_t&) = delete;
  _pti_scope_collection_handle_t(_pti_scope_collection_handle_t&&) = delete;
  _pti_scope_collection_handle_t& operator=(_pti_scope_collection_handle_t&&) = delete;

  // Multi-device support
  std::unordered_set<pti_device_handle_t>
      requested_devices_;  // Devices user requested to profile, empty in auto-detect mode
  std::unordered_set<pti_device_handle_t>
      failed_devices_;  // Devices that failed configuration or activation
  bool auto_detect_mode_ =
      false;  // True when using auto-detect mode (devices=nullptr, device_count=0)

  // Per-device resources
  std::unordered_map<pti_device_handle_t, pti_metrics_group_handle_t> metrics_group_handles_;
  std::unordered_map<pti_device_handle_t, std::unique_ptr<PtiQueryMetricsProfiler>>
      query_profilers_;
  std::unordered_map<pti_device_handle_t, uint64_t> next_buffer_ids_;

  // Shared across all devices
  const char* collected_metrics_group_name_ = nullptr;  // Single group name for all devices
  std::vector<int32_t> requested_metric_indices_;       // Single set of indices for all devices
  std::vector<pti_metric_properties_t> requested_metric_properties_;
  std::vector<pti_metric_value_type> requested_value_types_;
  std::vector<const char*> requested_metric_names_;
  std::vector<const char*> requested_metric_units_;

  std::atomic<bool> is_configured_ = false;
  std::atomic<bool> is_collection_active_ = false;

  pti_callback_subscriber_handle callback_subscriber_ = 0;
  std::mutex data_mutex_;       // Thread safety for callback access
  std::mutex buffer_id_mutex_;  // Guards next_buffer_ids_ only

  std::unique_ptr<PtiMetricsScopeBufferHandler<PtiMetricsScopeBuffer>> buffer_manager_;
  size_t configured_buffer_size_ = 0;

  // Destructor to handle cleanup
  ~_pti_scope_collection_handle_t() {
    try {
      // Finalize all device buffers
      if (buffer_manager_) {
        buffer_manager_->FinalizeAllCurrentBuffers();
      }

      // Stop all device profilers
      if (is_configured_) {
        for (auto& [device, profiler] : query_profilers_) {
          if (profiler) {
            pti_result stop_result = profiler->StopProfiling();
            if (stop_result != PTI_SUCCESS) {
              SPDLOG_WARN("Failed to stop profiler for device {}: {}", static_cast<void*>(device),
                          static_cast<int>(stop_result));
            }
          }
        }
      }

      // Log cleanup info
      if (buffer_manager_) {
        [[maybe_unused]] size_t buffer_count = buffer_manager_->GetBufferCount();
        SPDLOG_TRACE("Cleaning up scope handle with {} collection buffers", buffer_count);
      }

      // Clean up collected data
      requested_metric_properties_.clear();
      query_profilers_.clear();

      SPDLOG_TRACE("Scope metrics collection handle destroyed");
    } catch (const std::exception& e) {
      SPDLOG_DEBUG("Exception during scope handle destruction: {}", e.what());
    } catch (...) {
      SPDLOG_DEBUG("Unknown exception during scope handle destruction");
    }
  }
};

/**
 * @brief Helper to get all system devices
 * This function retrieves all devices available on the machine.
 * @param[out] devices Vector to store device properties
 * @return PTI_SUCCESS if devices retrieved, error code otherwise
 */
inline pti_result GetAllAvailableDevices(std::vector<pti_device_properties_t>& devices) {
  uint32_t device_count = 0;
  pti_result result = ptiMetricsGetDevices(nullptr, &device_count);
  if (result != PTI_SUCCESS) {
    SPDLOG_DEBUG(
        "GetAllAvailableDevices: Failed to enumerate devices (error: {}). "
        "Ensure Level Zero drivers are properly installed",
        static_cast<int>(result));
    return result;
  }

  if (device_count == 0) {
    SPDLOG_DEBUG(
        "GetAllAvailableDevices: No devices available for metrics collection. "
        "Ensure ZET_ENABLE_METRICS=1 and compatible GPU hardware is present");
    return PTI_ERROR_BAD_ARGUMENT;
  }

  devices.resize(device_count);
  result = ptiMetricsGetDevices(devices.data(), &device_count);
  if (result != PTI_SUCCESS) {
    SPDLOG_DEBUG("GetAllAvailableDevices: Failed to get device properties (error: {})",
                 static_cast<int>(result));
    return result;
  }

  return PTI_SUCCESS;
}

/**
 * @brief Validate configuration arguments for metrics scope setup
 * This function validates all input arguments for the metrics scope configuration
 * including device count, metric names array, and scope collection handle
 *
 * @param[in] devices_to_profile         Array of device handles
 * @param[in] device_count               Number of devices in the array
 * @param[in] metric_names               Array of metric names
 *
 * @return PTI_SUCCESS if all arguments are valid, error code otherwise
 */
inline pti_result ValidateConfigurationArguments(pti_device_handle_t* devices_to_profile,
                                                 uint32_t device_count, const char** metric_names,
                                                 size_t metrics_count) {
  if (metric_names == nullptr) {
    SPDLOG_DEBUG("{}: Metric names array is null", __func__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (metrics_count == 0) {
    SPDLOG_DEBUG("{}: No metrics provided for configuration", __func__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  // Auto detect mode
  if (device_count == 0) {
    if (devices_to_profile != nullptr) {
      SPDLOG_DEBUG("{}: Auto-detect mode requires devices_to_profile == nullptr", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }
    // Valid auto mode
    return PTI_SUCCESS;
  }

  // Explicit mode validation
  if (devices_to_profile == nullptr) {
    SPDLOG_DEBUG("{}: devices_to_profile cannot be nullptr with explicit device_count", __func__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_SUCCESS;
}

/**
 * @brief Validate that a target device supports metrics collection
 * This function checks if the specified device handle is valid and supports
 * metrics collection by verifying it exists in the available devices list
 * and has metric groups available
 *
 * @param[in] target_device    Device handle to validate
 *
 * @return PTI_SUCCESS if device is valid and supports metrics, error code otherwise
 */
inline pti_result ValidateTargetDevice(
    pti_device_handle_t target_device,
    const std::unordered_set<pti_device_handle_t>& available_device_handles) {
  if (available_device_handles.find(target_device) == available_device_handles.end()) {
    SPDLOG_DEBUG(
        "ValidateTargetDevice: Specified device handle is not valid or not available for metrics "
        "collection");
    return PTI_ERROR_BAD_ARGUMENT;
  }

  {
    std::lock_guard<std::mutex> lock(g_level_zero_api_mutex);

    // Additional validation: check if device supports metrics
    ze_device_handle_t ze_device = static_cast<ze_device_handle_t>(target_device);
    uint32_t metric_group_count = 0;
    ze_result_t status = zetMetricGroupGet(ze_device, &metric_group_count, nullptr);
    if (status != ZE_RESULT_SUCCESS || metric_group_count == 0) {
      SPDLOG_DEBUG(
          "ValidateTargetDevice: Device does not support metrics collection or no metric groups "
          "available");
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }
  }

  return PTI_SUCCESS;
}

/**
 * @brief Set up metric properties in the scope collection handle
 * This function initializes the requested_metric_properties vector with
 * the provided metric names, preparing them for group resolution
 *
 * @param[in] scope_collection_handle    Scope collection handle to configure
 * @param[in] metric_names               Array of metric names to set up
 * @param[in] metric_count               Number of metric names in the array
 *
 * @return PTI_SUCCESS if setup successful, error code otherwise
 */
inline pti_result SetupMetricProperties(pti_scope_collection_handle_t scope_collection_handle,
                                        const char** metric_names, size_t metric_count) {
  if (metric_count == 0) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  scope_collection_handle->requested_metric_properties_.clear();
  scope_collection_handle->requested_metric_properties_.reserve(metric_count);

  for (size_t i = 0; i < metric_count; ++i) {
    const char* name = metric_names[i];
    if (name == nullptr || name[0] == '\0') {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    // Create basic property structure - will be filled in ResolveGroupFromMetricNames
    pti_metric_properties_t prop = {};
    prop._name = name;  // Store the name for now
    scope_collection_handle->requested_metric_properties_.push_back(prop);
  }

  return PTI_SUCCESS;
}

/**
 * @brief Validate that all devices are the same type
 * This ensures all devices have the same model name, which means they share
 * identical metric groups and indices.
 *
 * @param devices_to_validate Set of device handles to validate for uniformity
 * @return PTI_SUCCESS if all devices are uniform,
 *         PTI_ERROR_METRICS_SCOPE_DEVICE_TYPE_NOT_UNIFORM if mixed types
 */
inline pti_result ValidateDevicesUniform(
    const std::unordered_set<pti_device_handle_t>& devices_to_validate,
    const std::vector<pti_device_properties_t>& all_devices) {
  if (devices_to_validate.empty()) return PTI_ERROR_BAD_ARGUMENT;
  if (devices_to_validate.size() == 1) return PTI_SUCCESS;  // Single device always uniform

  // Create lookup map for requested devices
  std::unordered_map<pti_device_handle_t, const char*> device_models;
  for (const auto& device : all_devices) {
    if (devices_to_validate.find(device._handle) != devices_to_validate.end()) {
      device_models[device._handle] = device._model_name;
    }
  }

  // Get reference model from first device
  auto reference_device = *devices_to_validate.begin();
  auto it = device_models.find(reference_device);
  if (it == device_models.end()) {
    SPDLOG_DEBUG("Reference device handle {} not found in available devices",
                 static_cast<void*>(reference_device));
    return PTI_ERROR_BAD_ARGUMENT;
  }
  const char* reference_model = it->second;

  // Compare all other devices
  for (auto device : devices_to_validate) {
    if (device == reference_device) continue;

    auto it = device_models.find(device);
    if (it == device_models.end()) {
      SPDLOG_DEBUG("Device handle {} not found in available devices", static_cast<void*>(device));
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (std::strcmp(it->second, reference_model) != 0) {
      SPDLOG_DEBUG("Device type mismatch: reference='{}', device='{}'", reference_model,
                   it->second);
      return PTI_ERROR_METRICS_SCOPE_DEVICE_TYPE_NOT_UNIFORM;
    }
  }

  SPDLOG_DEBUG("All {} devices are uniform (model: {})", devices_to_validate.size(),
               reference_model);
  return PTI_SUCCESS;
}

inline pti_result GetMetricGroups(pti_device_handle_t device_handle,
                                  std::vector<pti_metrics_group_properties_t>& groups) {
  uint32_t group_count = 0;
  {
    std::lock_guard<std::mutex> lock(g_level_zero_api_mutex);
    pti_result result = ptiMetricsGetMetricGroups(device_handle, nullptr, &group_count);
    if (result != PTI_SUCCESS || group_count == 0) {
      return (result == PTI_SUCCESS) ? PTI_ERROR_BAD_ARGUMENT : result;
    }
  }
  SPDLOG_TRACE("GetMetricGroups: group count: {}", group_count);

  groups.resize(group_count);
  for (auto& g : groups) {
    g._metric_properties = nullptr;
  }

  {
    std::lock_guard<std::mutex> lock(g_level_zero_api_mutex);
    return ptiMetricsGetMetricGroups(device_handle, groups.data(), &group_count);
  }
}

inline bool CheckMetricsInGroup(pti_scope_collection_handle_t scope_collection_handle,
                                pti_metrics_group_properties_t& group_props,
                                uint32_t metric_count) {
  // Skip empty or non-event-based groups
  if (group_props._metric_count == 0 || group_props._type != PTI_METRIC_GROUP_TYPE_EVENT_BASED) {
    return false;
  }

  std::vector<pti_metric_properties_t> metrics(group_props._metric_count);
  group_props._metric_properties = metrics.data();

  pti_result result;
  {
    std::lock_guard<std::mutex> lock(g_level_zero_api_mutex);
    result = ptiMetricsGetMetricsProperties(group_props._handle, group_props._metric_properties);
  }
  if (result != PTI_SUCCESS) {
    return false;  // Skip groups we can't introspect
  }

  bool all_in_one_group = true;
  for (uint32_t mi = 0; mi < metric_count; ++mi) {
    const char* name = scope_collection_handle->requested_metric_properties_[mi]._name;
    if (name == nullptr || name[0] == '\0') {
      return false;
    }

    bool found_in_current_group = false;
    for (uint32_t k = 0; k < group_props._metric_count; ++k) {
      const char* n = group_props._metric_properties[k]._name;
      if (n != nullptr && std::strcmp(n, name) == 0) {
        found_in_current_group = true;

        scope_collection_handle->requested_metric_indices_[mi] = k;
        scope_collection_handle->requested_metric_properties_[mi] =
            group_props._metric_properties[k];
        break;
      }
    }

    if (!found_in_current_group) {
      all_in_one_group = false;
    }
  }

  return all_in_one_group;
}

inline pti_result ValidateMetricAvailability(pti_scope_collection_handle_t scope_collection_handle,
                                             uint32_t metric_count) {
  for (uint32_t mi = 0; mi < metric_count; ++mi) {
    if (scope_collection_handle->requested_metric_indices_[mi] == -1) {
      return PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND;
    }
  }
  return PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP;
}

/**
 * @brief Helper function to resolve a metric group that contains all provided metric names on the
 * given device Returns PTI_SUCCESS and sets metrics_group_handle_ if a single group contains all
 * names. Returns PTI_ERROR_BAD_ARGUMENT if any name is null/empty or no matching metric is found.
 * Returns PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION if names span multiple groups
 *
 * @param[in] scope_collection_handle    Scope collection handle containing device and metric names
 * @return pti_result
 */
inline pti_result ResolveGroupFromMetricNames(pti_scope_collection_handle_t scope_collection_handle,
                                              pti_device_handle_t device) {
  if (scope_collection_handle == nullptr) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (!scope_collection_handle->auto_detect_mode_ &&
      scope_collection_handle->requested_devices_.find(device) ==
          scope_collection_handle->requested_devices_.end()) {
    SPDLOG_DEBUG("ResolveGroupFromMetricNames: Device {} not in requested_devices_",
                 static_cast<void*>(device));
    return PTI_ERROR_BAD_ARGUMENT;
  }

  pti_metrics_group_handle_t found_group = nullptr;

  // Get all metric groups for the device
  std::vector<pti_metrics_group_properties_t> groups;
  pti_result result = GetMetricGroups(device, groups);
  if (result != PTI_SUCCESS) return result;

  uint32_t metric_count =
      static_cast<uint32_t>(scope_collection_handle->requested_metric_properties_.size());
  scope_collection_handle->requested_metric_indices_.resize(metric_count, -1);
  scope_collection_handle->requested_value_types_.resize(metric_count);
  scope_collection_handle->requested_metric_names_.resize(metric_count);
  scope_collection_handle->requested_metric_units_.resize(metric_count);

  // Find a group that contains all requested metrics
  for (auto& group_props : groups) {
    if (CheckMetricsInGroup(scope_collection_handle, group_props, metric_count)) {
      found_group = group_props._handle;
      if (scope_collection_handle->collected_metrics_group_name_ == nullptr) {
        scope_collection_handle->collected_metrics_group_name_ = group_props._name;
      }
      break;  // Choose the first matched group
    }
  }

  if (found_group == nullptr) {
    SPDLOG_TRACE(
        "ResolveGroupFromMetricNames: No single metric group contains all {} requested metrics",
        metric_count);
    return ValidateMetricAvailability(scope_collection_handle, metric_count);
  }

  scope_collection_handle->metrics_group_handles_[device] = found_group;

  // Fill other requested properties
  if (scope_collection_handle->requested_metric_names_[0] == nullptr) {
    for (size_t i = 0; i < metric_count; ++i) {
      scope_collection_handle->requested_metric_names_[i] =
          scope_collection_handle->requested_metric_properties_[i]._name;
      scope_collection_handle->requested_value_types_[i] =
          scope_collection_handle->requested_metric_properties_[i]._value_type;
      scope_collection_handle->requested_metric_units_[i] =
          scope_collection_handle->requested_metric_properties_[i]._units;
    }
  }

  return PTI_SUCCESS;
}

// Look up the metric-group handle on `device` by the group name already
// resolved at Configure.
inline pti_result ResolveGroupHandleByName(pti_scope_collection_handle_t scope_collection_handle,
                                           pti_device_handle_t device) {
  PTI_ASSERT(scope_collection_handle != nullptr);
  PTI_ASSERT(scope_collection_handle->is_configured_);
  if (scope_collection_handle->collected_metrics_group_name_ == nullptr) {
    return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
  }

  std::vector<pti_metrics_group_properties_t> groups;
  pti_result result = GetMetricGroups(device, groups);
  if (result != PTI_SUCCESS) return result;

  const char* target_name = scope_collection_handle->collected_metrics_group_name_;
  for (auto& g : groups) {
    if (g._type != PTI_METRIC_GROUP_TYPE_EVENT_BASED) {
      continue;
    }
    if (g._name != nullptr && std::strcmp(g._name, target_name) == 0) {
      scope_collection_handle->metrics_group_handles_[device] = g._handle;
      return PTI_SUCCESS;
    }
  }

  SPDLOG_DEBUG("ResolveGroupHandleByName: group '{}' not found on device {}", target_name,
               static_cast<void*>(device));
  return PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND;
}

/**
 * @brief Align a size value up to the specified alignment boundary
 * This utility function rounds up a size to the next multiple of the alignment value
 *
 * @param[in] size        Size value to align
 * @param[in] alignment   Alignment boundary (bytes)
 *
 * @return Aligned size value
 */
inline size_t AlignUp(size_t size, size_t alignment) {
  if (alignment == 0) return size;
  return ((size + alignment - 1) / alignment) * alignment;
}

/**
 * @brief Convert Level Zero typed value to PTI value format
 * This function converts a zet_typed_value_t to a pti_value_t based on the source type
 *
 * @param[in] src     Source typed value from Level Zero
 * @param[out] dst    Destination PTI value structure
 */
inline void ConvertTypedValueToPtiValue(const zet_typed_value_t& src, pti_value_t& dst) {
  switch (src.type) {
    case ZET_VALUE_TYPE_UINT32:
      dst.ui32 = src.value.ui32;
      break;
    case ZET_VALUE_TYPE_UINT64:
      dst.ui64 = src.value.ui64;
      break;
    case ZET_VALUE_TYPE_FLOAT32:
      dst.fp32 = src.value.fp32;
      break;
    case ZET_VALUE_TYPE_FLOAT64:
      dst.fp64 = src.value.fp64;
      break;
    case ZET_VALUE_TYPE_BOOL8:
      dst.b8 = src.value.b8;
      break;
    default:
      dst.ui64 = 0;
      break;
  }
}

inline uint8_t* ProcessMetricValues(pti_scope_collection_handle_t scope_collection_handle,
                                    const kernel_metric_data* kernel_data,
                                    pti_metrics_scope_record_t* record,
                                    zet_metric_group_handle_t metric_group, uint8_t* current_pos) {
  if (!kernel_data->raw_data || kernel_data->data_size == 0) {
    record->_metrics_values = nullptr;
    return current_pos;
  }

  uint32_t total_metric_values = 0;
  zet_metric_group_calculation_type_t calculation_type =
      ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES;

  ze_result_t status = zetMetricGroupCalculateMetricValues(
      metric_group, calculation_type, kernel_data->data_size, kernel_data->raw_data.get(),
      &total_metric_values, nullptr);

  if (status != ZE_RESULT_SUCCESS || total_metric_values == 0) {
    record->_metrics_values = nullptr;
    return current_pos;
  }

  std::vector<zet_typed_value_t> all_metric_values(total_metric_values);
  status = zetMetricGroupCalculateMetricValues(metric_group, calculation_type,
                                               kernel_data->data_size, kernel_data->raw_data.get(),
                                               &total_metric_values, all_metric_values.data());

  if (status != ZE_RESULT_SUCCESS) {
    record->_metrics_values = nullptr;
    return current_pos;
  }

  // Store only requested metric values
  uint32_t requested_count =
      static_cast<uint32_t>(scope_collection_handle->requested_metric_properties_.size());
  record->_metrics_values = reinterpret_cast<pti_value_t*>(current_pos);

  for (uint32_t j = 0; j < requested_count; ++j) {
    uint32_t index = scope_collection_handle->requested_metric_indices_[j];
    if (index < total_metric_values) {
      ConvertTypedValueToPtiValue(all_metric_values[index], record->_metrics_values[j]);
    }
  }
  current_pos += AlignUp(requested_count * sizeof(pti_value_t), 8);

  return current_pos;
}

inline uint8_t* ProcessSingleRecord(pti_scope_collection_handle_t scope_collection_handle,
                                    PtiMetricsScopeBuffer* found_buffer,
                                    pti_metrics_scope_record_t* records,
                                    zet_metric_group_handle_t metric_group, size_t record_index,
                                    uint8_t* current_pos) {
  const kernel_metric_data* kernel_data = found_buffer->GetRecord(record_index);
  if (!kernel_data) return current_pos;

  pti_metrics_scope_record_t* record = &records[record_index];

  // Set basic record fields
  record->_kernel_id = kernel_data->kernel_id;
  record->_queue = kernel_data->queue;

  // Point directly to kernel_data's kernel_name
  record->_kernel_name = kernel_data->kernel_name;

  // Process metric values
  current_pos =
      ProcessMetricValues(scope_collection_handle, kernel_data, record, metric_group, current_pos);

  return current_pos;
}

// Helper function to get metric name from properties
inline const char* GetMetricName(pti_scope_collection_handle_t scope_collection_handle,
                                 size_t index) {
  return (index < scope_collection_handle->requested_metric_properties_.size())
             ? scope_collection_handle->requested_metric_properties_[index]._name
             : nullptr;
}

// Helper to create kernel_metric_data with dynamic name allocation
inline std::unique_ptr<kernel_metric_data> CreateKernelMetricData(
    uint64_t kernel_id, const char* kernel_name, uint64_t start_time, uint64_t end_time,
    const uint8_t* raw_data, size_t data_size, ze_context_handle_t context,
    ze_command_queue_handle_t queue, pti_backend_command_list_type submit_type) {
  auto data = std::make_unique<kernel_metric_data>();
  if (!data) {
    SPDLOG_DEBUG("CreateKernelMetricData: Failed to allocate kernel_metric_data structure");
    return nullptr;
  }

  data->kernel_id = kernel_id;
  data->start_time = start_time;
  data->end_time = end_time;
  data->data_size = data_size;
  data->context = context;
  data->queue = queue;
  data->submit_type = submit_type;

  // Handle kernel name
  if (kernel_name) {
    size_t name_len = strlen(kernel_name) + 1;
    data->kernel_name = new char[name_len];
    if (!data->kernel_name) {
      SPDLOG_WARN("CreateKernelMetricData: Kernel name allocation failed.");
      return nullptr;
    }
    snprintf(data->kernel_name, name_len, "%s", kernel_name);
    data->kernel_name[name_len - 1] = '\0';
  } else {
    data->kernel_name = nullptr;
  }

  // Handle raw data
  if (raw_data && data_size > 0) {
    try {
      data->raw_data = std::make_unique<uint8_t[]>(data_size);
      std::memcpy(data->raw_data.get(), raw_data, data_size);
    } catch (const std::bad_alloc&) {
      SPDLOG_DEBUG("CreateKernelMetricData: Raw data allocation failed.");
      return nullptr;
    }
  }

  return data;
}

/**
 * @brief Create a new metrics collection buffer
 */
inline pti_result CreateNewBuffer(pti_scope_collection_handle_t scope_collection_handle,
                                  pti_device_handle_t device) {
  SPDLOG_TRACE(
      "CreateNewBuffer: Creating new buffer for device {} About to acquire buffer_id_mutex_",
      static_cast<void*>(device));
  uint64_t buffer_id;
  {
    std::lock_guard<std::mutex> lock(scope_collection_handle->buffer_id_mutex_);
    buffer_id = scope_collection_handle->next_buffer_ids_[device]++;
  }
  return scope_collection_handle->buffer_manager_->CreateBuffer(
      scope_collection_handle->configured_buffer_size_, device, buffer_id);
}

// Helper function to extract GPU operation context data
struct GpuOperationContext {
  ze_context_handle_t context;
  ze_command_queue_handle_t queue;
  ze_command_list_handle_t cmd_list;
  pti_backend_command_list_type submit_type;
  ze_device_handle_t device;
};

inline GpuOperationContext ExtractGpuOperationContext(pti_callback_gpu_op_data* callback_data,
                                                      pti_backend_ctx_t backend_context) {
  return GpuOperationContext{static_cast<ze_context_handle_t>(backend_context),
                             static_cast<ze_command_queue_handle_t>(callback_data->_queue_handle),
                             static_cast<ze_command_list_handle_t>(callback_data->_cmd_list_handle),
                             callback_data->_cmd_list_properties,
                             static_cast<ze_device_handle_t>(callback_data->_device_handle)};
}

/**
 * @brief Log GPU operation context information for debugging
 */
inline void LogGpuOperationContext([[maybe_unused]] const GpuOperationContext& ctx) {
  SPDLOG_DEBUG("\tContext:     {}", static_cast<void*>(ctx.context));
  SPDLOG_DEBUG("\tDevice:      {}", static_cast<void*>(ctx.device));
  SPDLOG_DEBUG("\tQueue:       {}", static_cast<void*>(ctx.queue));
  SPDLOG_DEBUG("\tCommandList: {}", static_cast<void*>(ctx.cmd_list));
}

/**
 * @brief Validate metric query and completion event state
 * This function checks if a metric query and its associated completion event are valid
 * and ready for data collection
 *
 * @param[in] query               Metric query handle to validate
 * @param[in] completion_event    Completion event handle to check
 * @param[in] kernel_id          Kernel ID for logging purposes
 *
 * @return true if query and event are valid and ready, false otherwise
 */
inline bool ValidateQueryAndEvent(zet_metric_query_handle_t query,
                                  ze_event_handle_t completion_event, uint64_t kernel_id) {
  if (!query) {
    SPDLOG_DEBUG("ValidateQueryAndEvent: No query found for kernel ID: {}", kernel_id);
    return false;
  }

  if (!completion_event) {
    SPDLOG_WARN(
        "ValidateQueryAndEvent: No completion event found for query associated with kernel ID: {}",
        kernel_id);
    return false;
  }

  ze_result_t event_status = zeEventQueryStatus(completion_event);
  if (event_status != ZE_RESULT_SUCCESS) {
    if (event_status == ZE_RESULT_NOT_READY) {
      SPDLOG_TRACE(
          "ValidateQueryAndEvent: Query for kernel ID {} is not yet completed, skipping data "
          "collection",
          kernel_id);
    } else {
      SPDLOG_TRACE("ValidateQueryAndEvent: Error checking event status for kernel ID {}: 0x{:x}",
                   kernel_id, static_cast<uint32_t>(event_status));
    }
    return false;
  }

  SPDLOG_TRACE(
      "ValidateQueryAndEvent: Query completion event is signaled, proceeding with data collection");
  return true;
}

/**
 * @brief Retrieve raw metric data from a completed query
 * This function extracts the raw metric data from a Level Zero metric query
 * and stores it in the provided vector
 *
 * @param[in] query         Metric query handle to get data from
 * @param[out] raw_data     Vector to store the retrieved raw data
 * @param[in] kernel_name   Kernel name for logging purposes
 *
 * @return true if data was successfully retrieved, false otherwise
 */
inline bool GetRawMetricData(zet_metric_query_handle_t query, std::vector<uint8_t>& raw_data,
                             [[maybe_unused]] const char* kernel_name) {
  SPDLOG_TRACE("GetRawMetricData: Attempting to retrieve raw metric data for kernel: {}",
               kernel_name);
  size_t raw_data_size = 0;
  ze_result_t status = zetMetricQueryGetData(query, &raw_data_size, nullptr);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_DEBUG("GetRawMetricData: Failed to get query data size: {}",
                 static_cast<uint64_t>(status));
    return false;
  }
  SPDLOG_TRACE("GetRawMetricData: Raw data size {} bytes", raw_data_size);

  if (raw_data_size == 0) {
    SPDLOG_TRACE("GetRawMetricData: No metric data available for kernel: {}", kernel_name);
    return false;
  }

  raw_data.resize(raw_data_size);
  status = zetMetricQueryGetData(query, &raw_data_size, raw_data.data());
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_TRACE("GetRawMetricData: Failed to get query data: {}", static_cast<uint64_t>(status));
    return false;
  }
  SPDLOG_TRACE("GetRawMetricData: Successfully retrieved Query raw data");

  return true;
}

/**
 * @brief Store kernel metric data in the collection buffer
 * This function manages buffer allocation and stores the kernel metric data.
 * If the current buffer is full, it creates a new buffer automatically.
 *
 * @param[in] scope_collection_handle    Scope collection handle containing buffer manager
 * @param[in] kernel_data               Kernel metric data to store
 * @param[in] raw_data_size             Size of the raw metric data
 *
 * @return true if data was successfully stored, false otherwise
 */
inline bool StoreKernelData(pti_scope_collection_handle_t scope_collection_handle,
                            pti_device_handle_t device,
                            std::unique_ptr<kernel_metric_data> kernel_data, size_t raw_data_size) {
  SPDLOG_DEBUG("StoreKernelData: Storing data for kernel ID: {}, raw data size: {} bytes",
               kernel_data->kernel_id, raw_data_size);
  PtiMetricsScopeBuffer* current_buffer =
      scope_collection_handle->buffer_manager_->GetCurrentBuffer(device);

  // Check if current buffer has space
  size_t required_space = raw_data_size + sizeof(struct kernel_metric_data);
  if (!current_buffer || !current_buffer->HasSpace(required_space)) {
    // Current buffer is full, finalize it and create new one
    if (current_buffer) {
      scope_collection_handle->buffer_manager_->FinalizeCurrentBuffer(device);
    }

    // Create new buffer
    SPDLOG_DEBUG(
        "StoreKernelData: Current buffer full or not found for device {}, creating new buffer",
        static_cast<void*>(device));
    pti_result result = CreateNewBuffer(scope_collection_handle, device);
    if (result != PTI_SUCCESS) {
      return false;
    }

    current_buffer = scope_collection_handle->buffer_manager_->GetCurrentBuffer(device);
    SPDLOG_DEBUG("StoreKernelData: After CreateNewBuffer, GetCurrentBuffer -> {}",
                 static_cast<void*>(current_buffer));
  }

  // Add data to current buffer
  return current_buffer && current_buffer->AddKernelRecord(std::move(kernel_data));
}

inline bool ValidateAppendedOperation(pti_callback_gpu_op_data* callback_data,
                                      pti_gpu_op_details* op_details) {
  if (callback_data->_operation_count > 1) {
    SPDLOG_WARN(
        "ValidateAppendedOperation: Metrics Scope: Multiple operations appended in a single "
        "append. "
        "Can not process such cases.");
    return false;
  }

  // Check operation type and handle appropriately
  switch (op_details->_operation_kind) {
    case PTI_GPU_OPERATION_KIND_KERNEL:
      // Process kernel operations normally
      return true;
    case PTI_GPU_OPERATION_KIND_MEMORY:
    default:
      // For now, skip memory operations due to known issue with query submission
      SPDLOG_TRACE(
          "ValidateAppendedOperation: Skipping memory (known limitation) or other operation type: "
          "{}",
          static_cast<uint32_t>(op_details->_operation_kind));
      return false;
  }
}

// Forward declaration for the helper function
void CollectQueryDataForKernel(pti_scope_collection_handle_t scope_collection_handle,
                               uint64_t kernel_id, const char* kernel_name,
                               ze_context_handle_t context, ze_command_queue_handle_t queue,
                               pti_backend_command_list_type submit_type,
                               pti_device_handle_t device);

inline void HandleGpuOperationCompleted(pti_scope_collection_handle_t scope_collection_handle,
                                        pti_callback_gpu_op_data* callback_data,
                                        const GpuOperationContext& ctx,
                                        [[maybe_unused]] pti_device_handle_t device) {
  SPDLOG_TRACE("GPU OP COMPLETED");
  SPDLOG_TRACE("\tSubmit type: {}", static_cast<int>(ctx.submit_type));

  // Process several GPU ops - they might come in a batch
  if (callback_data->_operation_count > 0 && callback_data->_operation_details != nullptr) {
    auto* op_details_array = static_cast<pti_gpu_op_details*>(callback_data->_operation_details);

    for (uint32_t i = 0; i < callback_data->_operation_count; ++i) {
      const pti_gpu_op_details* op_details = &op_details_array[i];

      uint64_t kernel_id = op_details->_operation_id;
      const char* kernel_name = op_details->_name;

      SPDLOG_TRACE("\tKernel ID: {}, Name: {}", kernel_id, kernel_name);

      // Collect query data for this completed kernel
      CollectQueryDataForKernel(scope_collection_handle, kernel_id, kernel_name, ctx.context,
                                ctx.queue, ctx.submit_type,
                                static_cast<pti_device_handle_t>(ctx.device));
    }
  }
}

inline PtiQueryMetricsProfiler* FindProfiler(pti_scope_collection_handle_t scope,
                                             pti_device_handle_t device) {
  auto it = scope->query_profilers_.find(device);
  if (it == scope->query_profilers_.end() || !it->second) {
    return nullptr;
  }
  return it->second.get();
}

inline bool ShouldLazyCreateProfiler(pti_scope_collection_handle_t scope,
                                     pti_device_handle_t device) {
  if (scope->failed_devices_.count(device)) {
    return false;
  }
  if (scope->auto_detect_mode_) {
    return true;
  }
  return scope->requested_devices_.count(device) != 0;
}

inline PtiQueryMetricsProfiler* CreateProfilerForDevice(pti_scope_collection_handle_t scope,
                                                        pti_device_handle_t device) {
  PTI_ASSERT(scope != nullptr);
  PTI_ASSERT(scope->is_configured_);
  PTI_ASSERT(scope->buffer_manager_ != nullptr);

  pti_result res = ResolveGroupHandleByName(scope, device);
  if (res != PTI_SUCCESS) {
    SPDLOG_WARN("CreateProfilerForDevice: ResolveGroupHandleByName failed for device {}: {}",
                static_cast<void*>(device), static_cast<int>(res));
    scope->failed_devices_.insert(device);
    return nullptr;
  }

  PTI_ASSERT(scope->metrics_group_handles_[device] != nullptr);
  auto new_profiler =
      std::make_unique<PtiQueryMetricsProfiler>(device, scope->metrics_group_handles_[device]);
  pti_result start_res = new_profiler->StartProfiling(false);
  if (start_res != PTI_SUCCESS) {
    SPDLOG_WARN(
        "CreateProfilerForDevice: StartProfiling failed for device {} ({}); "
        "skipping metrics for this device (likely held by another process)",
        static_cast<void*>(device), static_cast<int>(start_res));
    scope->failed_devices_.insert(device);
    return nullptr;
  }

  PTI_ASSERT(scope->configured_buffer_size_ >= kMinCollectionBufferSize);

  scope->buffer_manager_->RegisterDevice(device);

  uint64_t buffer_id = 0;
  {
    std::lock_guard<std::mutex> id_lock(scope->buffer_id_mutex_);
    scope->next_buffer_ids_[device] = 0;
    buffer_id = scope->next_buffer_ids_[device]++;
  }
  pti_result buf_res =
      scope->buffer_manager_->CreateBuffer(scope->configured_buffer_size_, device, buffer_id);
  if (buf_res != PTI_SUCCESS && buf_res != PTI_WARN_BUFFER_NOT_FINALIZED) {
    SPDLOG_WARN("CreateProfilerForDevice: failed to pre-allocate buffer for device {}: {}",
                static_cast<void*>(device), static_cast<int>(buf_res));
    scope->failed_devices_.insert(device);
    return nullptr;
  }

  PtiQueryMetricsProfiler* raw = new_profiler.get();
  scope->query_profilers_[device] = std::move(new_profiler);
  SPDLOG_DEBUG("CreateProfilerForDevice: lazily created profiler for device {}",
               static_cast<void*>(device));
  return raw;
}

inline void HandleAppendEnter(pti_scope_collection_handle_t scope,
                              pti_callback_gpu_op_data* callback_data,
                              pti_gpu_op_details* op_details, const GpuOperationContext& ctx,
                              pti_device_handle_t device) {
  PTI_ASSERT(scope != nullptr);
  PTI_ASSERT(callback_data != nullptr);
  PTI_ASSERT(callback_data->_phase == PTI_CB_PHASE_API_ENTER);
  std::lock_guard<std::mutex> lock(scope->data_mutex_);

  uint64_t kernel_id = 0;
  if (callback_data->_operation_count > 0 && callback_data->_operation_details != nullptr) {
    if (!ValidateAppendedOperation(callback_data, op_details)) {
      return;
    }
    kernel_id = op_details->_operation_id;
  }

  PtiQueryMetricsProfiler* profiler = FindProfiler(scope, device);
  if (profiler == nullptr) {
    if (!ShouldLazyCreateProfiler(scope, device)) {
      SPDLOG_DEBUG("HandleAppendEnter: skipping device {} (not requested or previously failed)",
                   static_cast<void*>(device));
      return;
    }
    profiler = CreateProfilerForDevice(scope, device);
    if (profiler == nullptr) {
      return;
    }
  }

  SPDLOG_TRACE("API Enter phase - injecting query begin (kernel ID: {})", kernel_id);
  profiler->HandleKernelAppendEnter(ctx.cmd_list, ctx.device, kernel_id);
}

// TODO(JIRA): EXIT only reads query_profilers_; could use a per-device
// read/shared lock instead of data_mutex_.
inline void HandleAppendExit(pti_scope_collection_handle_t scope,
                             pti_callback_gpu_op_data* callback_data,
                             pti_gpu_op_details* op_details, const GpuOperationContext& ctx,
                             pti_device_handle_t device) {
  PTI_ASSERT(scope != nullptr);
  PTI_ASSERT(callback_data != nullptr);
  PTI_ASSERT(callback_data->_phase == PTI_CB_PHASE_API_EXIT);
  std::lock_guard<std::mutex> lock(scope->data_mutex_);

  PtiQueryMetricsProfiler* profiler = FindProfiler(scope, device);
  if (profiler == nullptr) {
    SPDLOG_DEBUG("HandleAppendExit: no profiler for device {} (not requested or skipped)",
                 static_cast<void*>(device));
    return;
  }

  uint64_t kernel_id = 0;
  if (callback_data->_operation_count > 0 && callback_data->_operation_details != nullptr) {
    if (!ValidateAppendedOperation(callback_data, op_details)) {
      return;
    }
    kernel_id = op_details->_operation_id;
  }

  SPDLOG_TRACE("API Exit phase - injecting query end (kernel ID: {})", kernel_id);
  profiler->HandleKernelAppendExit(ctx.cmd_list, ctx.device, kernel_id);
}

inline void HandleGpuOperationAppended(pti_scope_collection_handle_t scope_collection_handle,
                                       pti_callback_gpu_op_data* callback_data,
                                       pti_gpu_op_details* op_details,
                                       const GpuOperationContext& ctx, pti_device_handle_t device) {
  SPDLOG_TRACE("GPU OP APPENDED");
  if (callback_data->_phase == PTI_CB_PHASE_API_ENTER) {
    HandleAppendEnter(scope_collection_handle, callback_data, op_details, ctx, device);
    return;
  }
  if (callback_data->_phase == PTI_CB_PHASE_API_EXIT) {
    HandleAppendExit(scope_collection_handle, callback_data, op_details, ctx, device);
  }
}

inline void HandleKernelEvent(pti_scope_collection_handle_t scope_collection_handle,
                              pti_callback_gpu_op_data* callback_data,
                              pti_backend_ctx_t backend_context, pti_callback_domain domain) {
  SPDLOG_TRACE("In {}: {}", __func__, "Processing GPU operation appending or completion");

  auto* op_details = static_cast<pti_gpu_op_details*>(callback_data->_operation_details);
  if (op_details == nullptr) {
    SPDLOG_DEBUG("MetricsScope: Operation details are null. Skipping domain {}",
                 ptiCallbackDomainTypeToString(domain));
    return;
  }

  GpuOperationContext ctx = ExtractGpuOperationContext(callback_data, backend_context);
  LogGpuOperationContext(ctx);

  pti_device_handle_t callback_device = static_cast<pti_device_handle_t>(ctx.device);
  if (domain == PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
    HandleGpuOperationCompleted(scope_collection_handle, callback_data, ctx, callback_device);
  } else if (domain == PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED) {
    HandleGpuOperationAppended(scope_collection_handle, callback_data, op_details, ctx,
                               static_cast<pti_device_handle_t>(ctx.device));
  }
}

#endif  // SRC_API_PTI_METRICS_SCOPE_HELPER_H_
