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

#include <string>
#include <utility>
#include <vector>

#include "levelzero/ze_driver_init.h"
#include "ze_utils.h"

namespace pti::pc_sampling {

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

/** Name of the EU Stall Sampling metric group used for PC sampling */
constexpr const char* kPcSamplingMetricGroupName = "EuStallSampling";

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
  // Query metric group count
  uint32_t group_count = 0;
  ze_result_t status = zetMetricGroupGet(device, &group_count, nullptr);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zetMetricGroupGet failed, status: {:#x}, group_count: {}", __FUNCTION__,
                 static_cast<uint32_t>(status), group_count);
    return nullptr;
  }

  if (group_count == 0) {
    SPDLOG_WARN("{}: No metric groups found for device, cannot support PC sampling", __FUNCTION__);
    return nullptr;
  }

  // Retrieve all metric group handles
  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("{}: zetMetricGroupGet failed to get groups, status: {:#x}", __FUNCTION__,
                 static_cast<uint32_t>(status));
    return nullptr;
  }

  // Search for the EuStallSampling metric group with time-based sampling
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;

    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("{}: zetMetricGroupGetProperties failed, status: {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
      return nullptr;
    }

    // Check for EuStallSampling with time-based sampling support
    if (std::string(group_props.name) == kPcSamplingMetricGroupName &&
        (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED)) {
      return group_list[i];
    }
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
// Device Support Discovery
//-----------------------------------------------------------------------------

/**
 * @brief Discover all GPU devices that support EU stall sampling.
 *
 * Iterates through all drivers and devices to find GPUs with
 * the EuStallSampling metric group available.
 *
 * @return Vector of pairs containing device handle and corresponding
 *         metric group handle for each supported device
 */
inline std::vector<std::pair<ze_device_handle_t, zet_metric_group_handle_t>>
GetAllDevicesSupportEUStall() {
  std::vector<std::pair<ze_device_handle_t, zet_metric_group_handle_t>> supported_devices;

  std::vector<ze_device_handle_t> devices = ::utils::ze::GetDeviceList();
  for (auto device : devices) {
    // Get device properties to check type
    ze_device_properties_t props{};
    props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

    ze_result_t status = zeDeviceGetProperties(device, &props);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("{}: zeDeviceGetProperties failed, status: {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
      continue;
    }

    // Only check GPU devices for EU stall sampling support
    if (props.type == ZE_DEVICE_TYPE_GPU) {
      auto eu_stall_group = FindEUStallMetricGroupHandle(device);
      if (eu_stall_group != nullptr) {
        supported_devices.emplace_back(device, eu_stall_group);
        SPDLOG_INFO("{}: device {} supports {}", __FUNCTION__, props.name,
                    kPcSamplingMetricGroupName);
      }
    }
  }

  return supported_devices;
}

//-----------------------------------------------------------------------------
// Metric Enumeration
//-----------------------------------------------------------------------------

/**
 * @brief Get the names and descriptions of all metrics in a metric group.
 *
 * Enumerates all metrics within the given metric group and retrieves
 * their name and description properties.
 *
 * @param[in] group   Metric group handle to query metrics from
 *
 * @return Vector of pairs where each pair contains (metric_name, metric_description),
 *         empty if the group is invalid or on error
 */
inline std::vector<std::pair<std::string, std::string>> GetAllSupportedMetricNames(
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

  for (auto metric : metric_list) {
    zet_metric_properties_t metric_props{};
    metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;

    status = zetMetricGetProperties(metric, &metric_props);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("{}: zetMetricGetProperties failed, status: {:#x}", __FUNCTION__,
                   static_cast<uint32_t>(status));
      continue;
    }
    metric_names.emplace_back(metric_props.name, metric_props.description);
  }

  return metric_names;
}

}  // namespace pti::pc_sampling

#endif  // SRC_PTI_PC_SAMPLING_HELPER_H_
