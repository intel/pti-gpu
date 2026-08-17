//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "metric_state_manager.h"

#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "pti/pti_metrics.h"
#include "spdlog/spdlog.h"
#include "utils/utils.h"
#include "utils/ze_utils.h"
#include "ze_driver_init.h"

namespace pti {
namespace metrics {

MetricStateManager& MetricStateManager::Instance() {
  static MetricStateManager instance;
  return instance;
}

pti_result MetricStateManager::EnableSingleDevice(ze_device_handle_t device) {
  // Check if device is already enabled
  auto it = device_ref_counts_.find(device);
  if (it != device_ref_counts_.end()) {
    // Device already enabled, increment reference count
    it->second++;
    return PTI_SUCCESS;
  }

  // Device not yet enabled, call L0 API
  ze_result_t status = zetDeviceEnableMetricsExp(device);
  if (status != ZE_RESULT_SUCCESS) {
    if (status == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
      SPDLOG_ERROR(
          "zetDeviceEnableMetricsExp is not supported by the Level Zero driver. "
          "WORKAROUND: Set the environment variable ZET_ENABLE_METRICS=1 before running your "
          "application.");
      return PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED;
    } else {
      SPDLOG_ERROR("zetDeviceEnableMetricsExp failed with status: 0x{:x}",
                   static_cast<uint32_t>(status));
      return PTI_ERROR_DRIVER;
    }
  }

  // Add device with reference count = 1
  device_ref_counts_[device] = 1;
  return PTI_SUCCESS;
}

pti_result MetricStateManager::DisableSingleDevice(ze_device_handle_t device) {
  auto it = device_ref_counts_.find(device);
  if (it == device_ref_counts_.end()) {
    // Device not in our map, nothing to disable
    return PTI_SUCCESS;
  }

  if (it->second > 1) {
    // Other users still need metrics enabled on this device, only drop this reference
    it->second--;
    return PTI_SUCCESS;
  }

  // This was the last reference, disable metrics on this device. The reference is kept on
  // failure so that a later call can retry the disable.
  ze_result_t status = zetDeviceDisableMetricsExp(device);
  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("zetDeviceDisableMetricsExp failed with status: 0x{:x}",
                static_cast<uint32_t>(status));
    return PTI_ERROR_DRIVER;
  }

  device_ref_counts_.erase(it);
  return PTI_SUCCESS;
}

pti_result MetricStateManager::EnableMetric(pti_device_handle_t device) {
  // Check if metrics are already enabled via environment variable
  bool metrics_enabled_env = (::utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  if (metrics_enabled_env) {
    // Metrics are already enabled via environment variable, no need to enable again
    return PTI_SUCCESS;
  }

  // Driver initialization is done before taking state_mutex_: it calls into zeInit/zesInit and
  // enumerates drivers and extensions, which is far too much work to hold a global lock across.
  // ZeDriverInit is idempotent, so constructing it outside the lock is safe.
  ZeDriverInit init_drivers{};
  if (!init_drivers.Success()) {
    SPDLOG_ERROR("Level Zero driver initialization failed");
    return PTI_ERROR_DRIVER;
  }

  std::lock_guard<std::shared_mutex> lock(state_mutex_);

  if (device == nullptr) {
    // Enable for all devices.
    //
    // Enumerate through init_drivers.Drivers() rather than the argument-less overload.
    // ZeDriverInit unions the drivers reported by zeDriverGet with those from zeInitDrivers, so
    // the argument-less overload can miss devices that PtiMetricsCollectorHandler still discovers
    // and validates against. Any such device would never be reference counted here, and the
    // handler would then permanently report metrics as not enabled on all devices.
    // The unique overload is required, not just preferred: one physical device may be reported by
    // several driver handles, and a duplicate would take an extra reference that no matching
    // ptiMetricsDisable call would ever release.
    std::vector<ze_device_handle_t> devices_to_enable =
        ::utils::ze::GetUniqueDeviceList(init_drivers.Drivers());
    if (devices_to_enable.empty()) {
      SPDLOG_ERROR("No devices found to enable metrics");
      return PTI_ERROR_DRIVER;
    }

    // Track all successfully enabled devices for rollback on failure
    std::vector<ze_device_handle_t> enabled_devs;
    pti_result overall_status = PTI_SUCCESS;

    for (auto dev : devices_to_enable) {
      pti_result status = EnableSingleDevice(dev);
      if (status != PTI_SUCCESS) {
        overall_status = status;
        // Rollback: release every reference this call acquired, including devices that were
        // already enabled and only had their reference count incremented.
        for (auto enabled_dev : enabled_devs) {
          DisableSingleDevice(enabled_dev);
        }
        SPDLOG_ERROR("Failed to enable metrics on device {}: {}", static_cast<void*>(dev),
                     ptiResultTypeToString(status));
        return overall_status;
      }
      enabled_devs.push_back(dev);
    }
    return PTI_SUCCESS;
  } else {
    // Enable for specific device
    return EnableSingleDevice(reinterpret_cast<ze_device_handle_t>(device));
  }
}

pti_result MetricStateManager::DisableMetric(pti_device_handle_t device) {
  // Check if metrics are already enabled via environment variable
  bool metrics_enabled_env = (::utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  if (metrics_enabled_env) {
    // Metrics are already enabled via environment variable, no needs to enable again
    return PTI_SUCCESS;
  }

  std::lock_guard<std::shared_mutex> lock(state_mutex_);

  if (device == nullptr) {
    // Disable for all currently enabled devices
    // Make a copy of keys since we may modify the map during iteration
    std::vector<ze_device_handle_t> devices_to_disable;
    devices_to_disable.reserve(device_ref_counts_.size());
    for (const auto& pair : device_ref_counts_) {
      devices_to_disable.push_back(pair.first);
    }

    pti_result overall_status = PTI_SUCCESS;
    for (auto dev : devices_to_disable) {
      pti_result status = DisableSingleDevice(dev);
      if (status != PTI_SUCCESS) {
        overall_status = status;
        // Continue trying to disable remaining devices
      }
    }
    return overall_status;
  } else {
    // Disable for specific device
    return DisableSingleDevice(reinterpret_cast<ze_device_handle_t>(device));
  }
}

bool MetricStateManager::IsMetricEnabled(pti_device_handle_t device) {
  // Check if metrics are enabled via environment variable
  bool metrics_enabled_env = (::utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  if (metrics_enabled_env) {
    return true;
  }

  // Shared lock: this path only reads device_ref_counts_ and is on the hot path of every
  // metrics API call via MetricsUsable().
  std::shared_lock<std::shared_mutex> lock(state_mutex_);

  if (device == nullptr) {
    // Check if any device has metrics enabled
    return !device_ref_counts_.empty();
  } else {
    // Check if specific device has metrics enabled
    ze_device_handle_t ze_device = reinterpret_cast<ze_device_handle_t>(device);
    auto it = device_ref_counts_.find(ze_device);
    return (it != device_ref_counts_.end() && it->second > 0);
  }
}

}  // namespace metrics
}  // namespace pti
