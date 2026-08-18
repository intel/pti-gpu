//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_METRICS_METRIC_STATE_MANAGER_H_
#define PTI_METRICS_METRIC_STATE_MANAGER_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "pti/pti_metrics.h"

namespace pti {
namespace metrics {

// Global metric enable/disable state manager with per-device reference counting
class MetricStateManager {
 public:
  static MetricStateManager& Instance();

  // If device is nullptr, enables/disables metrics for all devices
  pti_result EnableMetric(pti_device_handle_t device);
  pti_result DisableMetric(pti_device_handle_t device);

  // Check if metrics are enabled for a device (or any device if nullptr)
  // Returns true if metrics are enabled via environment variable OR
  // if the device has a positive reference count
  bool IsMetricEnabled(pti_device_handle_t device = nullptr);

 private:
  MetricStateManager() = default;
  ~MetricStateManager() = default;
  MetricStateManager(const MetricStateManager&) = delete;
  MetricStateManager& operator=(const MetricStateManager&) = delete;

  pti_result EnableSingleDevice(ze_device_handle_t device);
  pti_result DisableSingleDevice(ze_device_handle_t device);

  std::unordered_map<ze_device_handle_t, int32_t> device_ref_counts_;

  // Protects device_ref_counts_. A shared_mutex rather than a plain mutex because
  // IsMetricEnabled() only reads the map and is reached from every metrics entry point via
  // PtiMetricsCollectorHandler::MetricsUsable(); an exclusive lock there would serialize all of
  // them against each other. EnableMetric()/DisableMetric() mutate the map and still take it
  // exclusively.
  std::shared_mutex state_mutex_;
};

}  // namespace metrics
}  // namespace pti

#endif  // PTI_METRICS_METRIC_STATE_MANAGER_H_
