//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pc_sampling/pti_pc_sampling_aggregator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "pc_sampling/pti_pc_sampling_helper.h"

namespace {

constexpr uint32_t kInstructionPointerAlignmentShift = 3;

uint64_t GetMetricValueAsUint64(const zet_typed_value_t& typed_value) {
  switch (typed_value.type) {
    case ZET_VALUE_TYPE_UINT32:
      return typed_value.value.ui32;
    case ZET_VALUE_TYPE_UINT64:
      return typed_value.value.ui64;
    case ZET_VALUE_TYPE_BOOL8:
      return typed_value.value.b8;
    default:
      SPDLOG_ERROR("{}: unsupported metric value type {}", __FUNCTION__,
                   static_cast<int64_t>(typed_value.type));
      return 0;
  }
}

size_t GetMetricId(const std::vector<std::string>& metric_names, std::string_view metric_name) {
  const auto metric_it = std::find(metric_names.begin(), metric_names.end(), metric_name);
  if (metric_it == metric_names.end()) {
    return metric_names.size();
  }

  return static_cast<size_t>(std::distance(metric_names.begin(), metric_it));
}

}  // namespace

namespace pti::pc_sampling {

pti_result AggregateCollectedData(
    pti_device_handle_t device, zet_metric_group_handle_t metric_group, bool samples_dropped,
    const TempRawDataFile& raw_data_file,
    std::vector<pti_pc_sampling_device_status_t>* aggregated_device_data) {
  if (aggregated_device_data == nullptr) {
    SPDLOG_ERROR("{}: aggregated_device_data is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  aggregated_device_data->clear();
  if (device == nullptr) {
    SPDLOG_ERROR("{}: device is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  pti_pc_sampling_device_status_t device_data{};
  device_data._struct_size = sizeof(pti_pc_sampling_device_status_t);
  device_data._device = device;

  std::vector<uint8_t> raw_data;
  if (!raw_data_file.ReadAll(&raw_data)) {
    return PTI_ERROR_INTERNAL;
  }

  SPDLOG_DEBUG("{}: device={}, raw_data_size={}, metric_group={}, samples_dropped={}", __FUNCTION__,
               static_cast<const void*>(device), raw_data.size(),
               static_cast<const void*>(metric_group), samples_dropped);

  if (metric_group == nullptr || raw_data.empty()) {
    device_data._samples_dropped = static_cast<uint32_t>(samples_dropped);
    aggregated_device_data->push_back(device_data);
    return PTI_SUCCESS;
  }

  std::vector<std::string> metric_names = ::utils::ze::GetMetricList(metric_group, false);
  if (metric_names.empty()) {
    SPDLOG_ERROR("{}: failed to collect metric names", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  SPDLOG_DEBUG("{}: metric_count={} first_metric={}", __FUNCTION__, metric_names.size(),
               metric_names.front());

  const size_t ip_metric_index = GetMetricId(metric_names, "IP");
  if (ip_metric_index >= metric_names.size()) {
    SPDLOG_ERROR("{}: failed to locate IP metric in EU stall sampling group", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  std::vector<uint32_t> reports;
  std::vector<zet_typed_value_t> values;
  if (!CalculateMetricValues(metric_group, raw_data, &reports, &values, &samples_dropped)) {
    return PTI_ERROR_INTERNAL;
  }
  SPDLOG_DEBUG("{}: report_sets={}, decoded_values={}, samples_dropped={}", __FUNCTION__,
               reports.size(), values.size(), samples_dropped);
  device_data._samples_dropped = static_cast<uint32_t>(samples_dropped);
  if (reports.empty() || values.empty()) {
    aggregated_device_data->push_back(device_data);
    return PTI_SUCCESS;
  }

  std::unordered_set<uint64_t> unique_pcs;
  const zet_typed_value_t* value = values.data();
  const size_t metric_count = metric_names.size();
  for (size_t report_index = 0; report_index < reports.size(); ++report_index) {
    const size_t per_report_value_count = static_cast<size_t>(reports[report_index]);
    const size_t sample_count = metric_count == 0 ? 0 : per_report_value_count / metric_count;
    if (per_report_value_count % metric_count != 0) {
      SPDLOG_WARN(
          "{}: report index {} has {} values which is not a multiple of the metric count {}",
          __FUNCTION__, report_index, per_report_value_count, metric_count);
    }
    for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
      const zet_typed_value_t* sample_values = value + sample_index * metric_count;
      // EuStallSampling reports instruction pointers in 8-byte units.
      const uint64_t ip = GetMetricValueAsUint64(sample_values[ip_metric_index])
                          << kInstructionPointerAlignmentShift;
      if (ip == 0) {
        continue;
      }

      unique_pcs.insert(ip);
      for (size_t metric_index = 0; metric_index < metric_count; ++metric_index) {
        if (metric_index == ip_metric_index) {
          continue;
        }

        const uint64_t metric_value = GetMetricValueAsUint64(sample_values[metric_index]);
        device_data._total_sample_count += metric_value;
      }
    }
    value += per_report_value_count;
  }

  device_data._total_pc_count = unique_pcs.size();
  SPDLOG_DEBUG("{}: total_sample_count={}, total_pc_count={}", __FUNCTION__,
               device_data._total_sample_count, device_data._total_pc_count);
  aggregated_device_data->push_back(device_data);
  return PTI_SUCCESS;
}

}  // namespace pti::pc_sampling
