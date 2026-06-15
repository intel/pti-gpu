//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Helper functions for PC Sampling feature.
// Provides utilities for:
//   - Level Zero driver enumeration
//   - Device discovery
//   - EU Stall Sampling metric group lookup
//
// These helpers abstract the Level Zero API calls needed to
// discover devices that support PC sampling via the EuStallSampling
// metric group.
//
// =============================================================

#ifndef SRC_PTI_PC_SAMPLING_HELPER_H_
#define SRC_PTI_PC_SAMPLING_HELPER_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/layers/zel_tracing_register_cb.h>
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "levelzero/ze_driver_init.h"
#include "pc_sampling/pti_pc_sampling_raw_data_file.h"
#include "utils/ze_utils.h"

namespace pti::pc_sampling {

constexpr size_t kMinMetricStreamerReadSize = 4 * 1024;
/** Name of the EU Stall Sampling metric group used for PC sampling */
constexpr const char* kPcSamplingMetricGroupName = "EuStallSampling";

inline void DestroyMetricStreamerContext(ze_context_handle_t* context);

struct MetricStreamerHandles {
  ze_context_handle_t context = nullptr;
  ze_device_handle_t device = nullptr;
  ze_event_pool_handle_t event_pool = nullptr;
  ze_event_handle_t event = nullptr;
  zet_metric_streamer_handle_t streamer = nullptr;

  void Reset() {
    if (streamer != nullptr) {
      zetMetricStreamerClose(streamer);
      streamer = nullptr;
    }
    if (event != nullptr) {
      zeEventDestroy(event);
      event = nullptr;
    }
    if (event_pool != nullptr) {
      zeEventPoolDestroy(event_pool);
      event_pool = nullptr;
    }
    if (context != nullptr && device != nullptr) {
      ze_result_t status = zetContextActivateMetricGroups(context, device, 0, nullptr);
      if (status != ZE_RESULT_SUCCESS) {
        SPDLOG_WARN("{}: metric group deactivation failed with status=0x{:x}", __FUNCTION__,
                    static_cast<uint32_t>(status));
      }
    }
    DestroyMetricStreamerContext(&context);
    device = nullptr;
  }

  ~MetricStreamerHandles() { Reset(); }
};

//-----------------------------------------------------------------------------
// Metric Streamer Helpers
//-----------------------------------------------------------------------------

inline bool CreateNotificationEvent(ze_context_handle_t context, ze_event_pool_handle_t* event_pool,
                                    ze_event_handle_t* event) {
  if (event_pool == nullptr || event == nullptr) {
    return false;
  }

  ze_event_pool_desc_t event_pool_desc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                       ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
  ze_result_t status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, event_pool);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zeEventPoolCreate failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  ze_event_desc_t event_desc{ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, ZE_EVENT_SCOPE_FLAG_HOST,
                             ZE_EVENT_SCOPE_FLAG_HOST};
  status = zeEventCreate(*event_pool, &event_desc, event);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zeEventCreate failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    zeEventPoolDestroy(*event_pool);
    *event_pool = nullptr;
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Metric Group Discovery
//-----------------------------------------------------------------------------

/**
 * @brief Find the EuStallSampling metric group for a device.
 *
 * This metric group is required for PC sampling functionality.
 *
 * @param[in] device   Device handle to search metric groups for
 *
 * @return Metric group handle if found, nullptr otherwise
 */
inline zet_metric_group_handle_t FindEUStallMetricGroupHandle(ze_device_handle_t device) {
  std::vector<zet_metric_group_handle_t> group_list;
  ::utils::ze::FindMetricGroups(device, group_list);

  if (group_list.empty()) {
    SPDLOG_WARN("{}: No metric groups found for device, cannot support PC sampling", __FUNCTION__);
    return nullptr;
  }

  // Search for the EuStallSampling metric group with time-based sampling
  for (zet_metric_group_handle_t group : group_list) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;

    ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("{}: zetMetricGroupGetProperties failed, status: {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
      return nullptr;
    }

    // Check for EuStallSampling with time-based sampling support
    if (std::string(group_props.name) == kPcSamplingMetricGroupName &&
        (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED)) {
      return group;
    }
  }

  return nullptr;
}

inline bool CreateMetricStreamerContext(ze_driver_handle_t driver, ze_context_handle_t* context) {
  if (context == nullptr) {
    return false;
  }

  ze_context_desc_t contextDesc{ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
  ze_result_t status = zeContextCreate(driver, &contextDesc, context);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zeContextCreate failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  return true;
}

inline void DestroyMetricStreamerContext(ze_context_handle_t* context) {
  if (context == nullptr || *context == nullptr) {
    return;
  }

  ze_result_t status = zeContextDestroy(*context);
  *context = nullptr;
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("{}: zeContextDestroy failed with status=0x{:x}", __FUNCTION__,
                static_cast<uint32_t>(status));
  }
}

inline bool OpenMetricStreamer(ze_context_handle_t context, ze_device_handle_t device,
                               zet_metric_group_handle_t metric_group, uint32_t sampling_period_ns,
                               uint32_t notify_every_n_reports, MetricStreamerHandles* handles) {
  if (handles == nullptr) {
    return false;
  }

  ze_result_t status = zetContextActivateMetricGroups(context, device, 1, &metric_group);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: metric group activation failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  handles->context = context;
  handles->device = device;
  if (!CreateNotificationEvent(context, &handles->event_pool, &handles->event)) {
    return false;
  }

  zet_metric_streamer_desc_t streamerDesc{ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr,
                                          notify_every_n_reports, sampling_period_ns};
  status = zetMetricStreamerOpen(context, device, metric_group, &streamerDesc, handles->event,
                                 &handles->streamer);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zetMetricStreamerOpen failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Metric Enumeration
//-----------------------------------------------------------------------------

inline bool CalculateMetricValues(zet_metric_group_handle_t metric_group,
                                  const std::vector<uint8_t>& raw_data,
                                  std::vector<uint32_t>* reports,
                                  std::vector<zet_typed_value_t>* values, bool* samples_dropped) {
  if (metric_group == nullptr || reports == nullptr || values == nullptr ||
      samples_dropped == nullptr) {
    return false;
  }

  reports->clear();
  values->clear();

  uint32_t report_count = 0;
  uint32_t total_value_count = 0;
  ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
      metric_group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, raw_data.size(),
      raw_data.data(), &report_count, &total_value_count, nullptr, nullptr);
  SPDLOG_DEBUG("{}: raw_data_size={}, report_count={}, total_value_count={}, status=0x{:x}",
               __FUNCTION__, raw_data.size(), report_count, total_value_count,
               static_cast<uint32_t>(status));
  if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
    *samples_dropped = true;
  } else if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: metric value count query failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  if (report_count == 0 || total_value_count == 0) {
    return true;
  }

  reports->resize(report_count);
  values->resize(total_value_count);
  status = zetMetricGroupCalculateMultipleMetricValuesExp(
      metric_group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, raw_data.size(),
      raw_data.data(), &report_count, &total_value_count, reports->data(), values->data());
  SPDLOG_DEBUG("{}: decoded report_count={}, total_value_count={}, status=0x{:x}", __FUNCTION__,
               report_count, total_value_count, static_cast<uint32_t>(status));
  if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
    *samples_dropped = true;
  } else if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: metric value decode failed with status=0x{:x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return false;
  }

  reports->resize(report_count);
  values->resize(total_value_count);
  return true;
}

inline void WaitForMetricStreamerReport(ze_event_handle_t event, uint64_t timeout_ns) {
  if (event == nullptr) {
    return;
  }

  ze_result_t status = zeEventHostSynchronize(event, timeout_ns);
  if (status != ZE_RESULT_SUCCESS) {
    if (status != ZE_RESULT_NOT_READY) {
      SPDLOG_WARN("{}: zeEventHostSynchronize failed with status=0x{:x}", __FUNCTION__,
                  static_cast<uint32_t>(status));
    }
    return;
  }

  status = zeEventHostReset(event);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("{}: zeEventHostReset failed with status=0x{:x}", __FUNCTION__,
                static_cast<uint32_t>(status));
  }
}

inline void ReadMetricStreamerData(zet_metric_streamer_handle_t streamer,
                                   std::vector<uint8_t>* local_buffer,
                                   TempRawDataFile* raw_data_file, bool* samples_dropped) {
  if (streamer == nullptr || local_buffer == nullptr || raw_data_file == nullptr ||
      samples_dropped == nullptr || !raw_data_file->IsOpen()) {
    return;
  }

  size_t data_size = 0;
  ze_result_t status = zetMetricStreamerReadData(streamer, UINT32_MAX, &data_size, nullptr);
  SPDLOG_DEBUG("{}: size query returned {} bytes with status=0x{:x}", __FUNCTION__, data_size,
               static_cast<uint32_t>(status));
  if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
    *samples_dropped = true;
  } else if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("{}: size query failed with status=0x{:x}", __FUNCTION__,
                static_cast<uint32_t>(status));
    return;
  }

  if (data_size == 0) {
    return;
  }

  local_buffer->resize((std::max)(data_size, kMinMetricStreamerReadSize));
  data_size = local_buffer->size();
  status = zetMetricStreamerReadData(streamer, UINT32_MAX, &data_size, local_buffer->data());
  SPDLOG_DEBUG("{}: data read returned {} bytes with status=0x{:x}", __FUNCTION__, data_size,
               static_cast<uint32_t>(status));
  if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
    *samples_dropped = true;
  } else if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("{}: data read failed with status=0x{:x}", __FUNCTION__,
                static_cast<uint32_t>(status));
    return;
  }

  if (!raw_data_file->Append(local_buffer->data(), data_size)) {
    SPDLOG_WARN("{}: failed to spill {} bytes of raw metric data", __FUNCTION__, data_size);
    return;
  }

  SPDLOG_DEBUG("{}: accumulated raw data size is {} bytes", __FUNCTION__, raw_data_file->size());
}

/**
 * @brief Get the names and descriptions of all stall metrics in a metric group.
 *
 * Enumerates all metrics within the given metric group and retrieves
 * their name and description properties.
 * Does not populate an IP metric name as this is not a stall metric name.
 *
 * @param[in] group   Metric group handle to query metrics from
 *
 * @return Vector of pairs where each pair contains (metric_name, metric_description),
 *         empty if the group is invalid or on error
 */
inline std::vector<std::pair<std::string, std::string>> GetAllSupportedStallMetricNames(
    zet_metric_group_handle_t group) {
  std::vector<std::pair<std::string, std::string>> metric_names;
  uint32_t metric_count = 0;
  ze_result_t status = zetMetricGet(group, &metric_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || metric_count == 0) {
    SPDLOG_ERROR("{}: zetMetricGet failed to get metric count, status: {:#x}, metric_count: {}",
                 __FUNCTION__, static_cast<uint32_t>(status), metric_count);
    return metric_names;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  status = zetMetricGet(group, &metric_count, metric_list.data());
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zetMetricGet failed to get metrics, status: {:#x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return metric_names;
  }

  SPDLOG_DEBUG("{}: enumerating {} metrics from EuStallSampling group", __FUNCTION__, metric_count);

  for (auto metric : metric_list) {
    zet_metric_properties_t metric_props{};
    metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;

    status = zetMetricGetProperties(metric, &metric_props);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("{}: zetMetricGetProperties failed, status: {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
      continue;
    }

    SPDLOG_DEBUG("{}: metric name='{}', description='{}', units='{}', metric_type={}", __FUNCTION__,
                 metric_props.name, metric_props.description, metric_props.resultUnits,
                 static_cast<uint32_t>(metric_props.metricType));

    if (metric_props.metricType != ZET_METRIC_TYPE_EVENT) {
      SPDLOG_DEBUG("{}: skipping metric '{}' because metric_type={} is not EVENT", __FUNCTION__,
                   metric_props.name, static_cast<uint32_t>(metric_props.metricType));
      continue;
    }

    metric_names.emplace_back(metric_props.name, metric_props.description);
  }

  SPDLOG_DEBUG("{}: returning {} stall metrics after filtering", __FUNCTION__, metric_names.size());

  return metric_names;
}

}  // namespace pti::pc_sampling

#endif  // SRC_PTI_PC_SAMPLING_HELPER_H_
