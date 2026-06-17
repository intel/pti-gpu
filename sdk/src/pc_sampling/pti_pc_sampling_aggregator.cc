//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pc_sampling/pti_pc_sampling_aggregator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pc_sampling/pti_pc_sampling_l0.h"

namespace {

using namespace pti::pc_sampling;

// EuStallSampling reports IP values in 8-byte units. On current Intel GPU
// drivers the reported address lives in the low 32-bit GPU address space,
// while zexKernelGetBaseAddress may expose a wider address. Normalize both to
// the sampled address representation before correlating PCs with kernels.
inline uint64_t NormalizeInstructionPointer(uint64_t instruction_pointer) {
  constexpr uint32_t kInstructionPointerAlignmentShift = 3;
  return (instruction_pointer << kInstructionPointerAlignmentShift) &
         kInstructionPointerAddressMask;
}

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

// Map a metric index to its stall-reason index. Reasons are all metrics except
// the IP metric, preserving metric-group order.
inline size_t ReasonIndexForMetric(size_t metric_index, size_t ip_metric_index) {
  return (metric_index < ip_metric_index) ? metric_index : metric_index - 1;
}

struct AggregationScratch {
  std::unordered_set<uint64_t> unique_pcs;
  uint64_t dropped_sample_count = 0;
  std::unordered_set<uint64_t> dropped_pcs;
  std::vector<uint32_t> reports;
  std::vector<zet_typed_value_t> values;
};

struct PendingKernelAggregate {
  uint64_t kernel_handle = 0;
  std::string kernel_name;
  std::vector<uint64_t> aggregated_samples;
  std::vector<uint64_t> instruction_offsets;
  std::vector<uint64_t> flattened_samples;
  std::unordered_map<uint64_t, size_t> instruction_rows;
};

using PendingKernelMap = std::unordered_map<uint64_t, PendingKernelAggregate>;

PendingKernelAggregate& GetOrCreateKernelAggregate(const KernelBinaryInfo& kernel_info,
                                                   size_t reason_count,
                                                   PendingKernelMap* pending_kernels) {
  const uint64_t kernel_handle = kernel_info.kernel_base_address_;
  auto kernel_it = pending_kernels->find(kernel_handle);
  if (kernel_it == pending_kernels->end()) {
    PendingKernelAggregate kernel_aggregate;
    kernel_aggregate.kernel_handle = kernel_handle;
    kernel_aggregate.kernel_name = kernel_info.kernel_name_;
    kernel_aggregate.aggregated_samples.assign(reason_count, 0);
    kernel_aggregate.flattened_samples.reserve(reason_count);
    kernel_it = pending_kernels->emplace(kernel_handle, std::move(kernel_aggregate)).first;
  }

  return kernel_it->second;
}

size_t GetOrCreateInstructionRow(uint64_t ip, size_t reason_count,
                                 PendingKernelAggregate* kernel_aggregate) {
  const uint64_t offset = ip - kernel_aggregate->kernel_handle;
  auto row_it = kernel_aggregate->instruction_rows.find(offset);
  if (row_it != kernel_aggregate->instruction_rows.end()) {
    return row_it->second;
  }

  const size_t row_index = kernel_aggregate->instruction_offsets.size();
  kernel_aggregate->instruction_offsets.push_back(offset);
  kernel_aggregate->instruction_rows.emplace(offset, row_index);
  kernel_aggregate->flattened_samples.insert(kernel_aggregate->flattened_samples.end(),
                                             reason_count, 0);

  return row_index;
}

uint64_t AccumulateUnmappedSampleCount(const zet_typed_value_t* sample_values, size_t metric_count,
                                       size_t ip_metric_index) {
  uint64_t sample_sum = 0;
  for (size_t metric_index = 0; metric_index < metric_count; ++metric_index) {
    if (metric_index == ip_metric_index) {
      continue;
    }
    sample_sum += GetMetricValueAsUint64(sample_values[metric_index]);
  }

  return sample_sum;
}

void AccumulateMappedSample(const zet_typed_value_t* sample_values, size_t metric_count,
                            size_t ip_metric_index, size_t instruction_row,
                            PendingKernelAggregate* kernel_aggregate,
                            pti_pc_sampling_device_status_t* device_status) {
  for (size_t metric_index = 0; metric_index < metric_count; ++metric_index) {
    if (metric_index == ip_metric_index) {
      continue;
    }

    const uint64_t metric_value = GetMetricValueAsUint64(sample_values[metric_index]);
    const size_t reason_index = ReasonIndexForMetric(metric_index, ip_metric_index);
    const size_t flat_index =
        instruction_row * kernel_aggregate->aggregated_samples.size() + reason_index;
    kernel_aggregate->flattened_samples[flat_index] += metric_value;
    kernel_aggregate->aggregated_samples[reason_index] += metric_value;
    device_status->_total_sample_count += metric_value;
  }
}

void AggregateSample(uint64_t ip, const zet_typed_value_t* sample_values, size_t metric_count,
                     size_t ip_metric_index, size_t reason_count,
                     pti_pc_sampling_device_status_t* device_status,
                     PendingKernelMap* pending_kernels, AggregationScratch* scratch) {
  const KernelBinaryInfo* kernel_info = KernelInfoStorage::Instance().FindByIpAddress(ip);
  if (kernel_info == nullptr) {
    // Drop PCs that do not map to a tracked kernel.
    scratch->dropped_sample_count +=
        AccumulateUnmappedSampleCount(sample_values, metric_count, ip_metric_index);
    scratch->dropped_pcs.insert(ip);
    return;
  }

  PendingKernelAggregate& kernel_aggregate =
      GetOrCreateKernelAggregate(*kernel_info, reason_count, pending_kernels);
  const size_t instruction_row = GetOrCreateInstructionRow(ip, reason_count, &kernel_aggregate);

  scratch->unique_pcs.insert(ip);
  AccumulateMappedSample(sample_values, metric_count, ip_metric_index, instruction_row,
                         &kernel_aggregate, device_status);
}

bool DecodeMetricBlock(zet_metric_group_handle_t metric_group, const std::vector<uint8_t>& chunk,
                       pti_pc_sampling_device_status_t* device_status,
                       AggregationScratch* scratch) {
  bool chunk_dropped = false;
  if (!CalculateMetricValues(metric_group, chunk, &scratch->reports, &scratch->values,
                             &chunk_dropped)) {
    return false;
  }
  if (chunk_dropped) {
    device_status->_samples_dropped = 1;
  }

  return true;
}

void AggregateDecodedReports(size_t ip_metric_index, size_t reason_count,
                             pti_pc_sampling_device_status_t* device_status,
                             PendingKernelMap* pending_kernels, AggregationScratch* scratch) {
  if (scratch->reports.empty() || scratch->values.empty()) {
    return;
  }

  // Assumes the decoded report stride is reason_count + 1: every metric in the
  // group is an EVENT stall reason except the single IP metric.
  const size_t metric_count = reason_count + 1;
  const zet_typed_value_t* value = scratch->values.data();
  for (size_t report_index = 0; report_index < scratch->reports.size(); ++report_index) {
    const size_t per_report_value_count = static_cast<size_t>(scratch->reports[report_index]);
    const size_t sample_count = metric_count == 0 ? 0 : per_report_value_count / metric_count;
    if (metric_count != 0 && per_report_value_count % metric_count != 0) {
      SPDLOG_WARN(
          "{}: report index {} has {} values which is not a multiple of the metric count {}",
          __FUNCTION__, report_index, per_report_value_count, metric_count);
    }

    for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
      const zet_typed_value_t* sample_values = value + sample_index * metric_count;
      const uint64_t ip =
          NormalizeInstructionPointer(GetMetricValueAsUint64(sample_values[ip_metric_index]));
      if (ip == 0) {
        continue;
      }

      AggregateSample(ip, sample_values, metric_count, ip_metric_index, reason_count, device_status,
                      pending_kernels, scratch);
    }
    value += per_report_value_count;
  }
}

bool PersistKernelSamplesFile(const PendingKernelAggregate& pending_kernel, size_t reason_count,
                              KernelAggregate* kernel_aggregate) {
  if (kernel_aggregate == nullptr || !kernel_aggregate->samples_file.OpenTemp()) {
    return false;
  }

  if (reason_count != 0 && pending_kernel.instruction_offsets.size() >
                               ((std::numeric_limits<size_t>::max)() / reason_count)) {
    kernel_aggregate->samples_file.Reset();
    return false;
  }

  const size_t offsets_bytes = pending_kernel.instruction_offsets.size() * sizeof(uint64_t);
  if (offsets_bytes != 0 &&
      !kernel_aggregate->samples_file.Append(
          reinterpret_cast<const uint8_t*>(pending_kernel.instruction_offsets.data()),
          offsets_bytes)) {
    kernel_aggregate->samples_file.Reset();
    return false;
  }

  const size_t samples_bytes = pending_kernel.flattened_samples.size() * sizeof(uint64_t);
  if (samples_bytes != 0 &&
      !kernel_aggregate->samples_file.Append(
          reinterpret_cast<const uint8_t*>(pending_kernel.flattened_samples.data()),
          samples_bytes)) {
    kernel_aggregate->samples_file.Reset();
    return false;
  }

  if (!kernel_aggregate->samples_file.Finalize()) {
    kernel_aggregate->samples_file.Reset();
    return false;
  }

  return true;
}

pti_result PersistAggregates(const PendingKernelMap& pending_kernels, size_t reason_count,
                             DeviceAggregate* device_aggregate) {
  for (const auto& [kernel_handle, pending_kernel] : pending_kernels) {
    KernelAggregate kernel_aggregate;
    kernel_aggregate.kernel_handle = pending_kernel.kernel_handle;
    kernel_aggregate.kernel_name_ = pending_kernel.kernel_name;
    kernel_aggregate.reason_count = reason_count;
    kernel_aggregate.instruction_count = pending_kernel.instruction_offsets.size();
    kernel_aggregate.aggregated_samples = pending_kernel.aggregated_samples;

    if (!PersistKernelSamplesFile(pending_kernel, reason_count, &kernel_aggregate)) {
      SPDLOG_ERROR("{}: failed to persist aggregate for kernel handle {:#x}", __FUNCTION__,
                   kernel_handle);
      return PTI_ERROR_INTERNAL;
    }

    device_aggregate->kernels.emplace(kernel_handle, std::move(kernel_aggregate));
  }

  return PTI_SUCCESS;
}

}  // namespace

namespace pti::pc_sampling {

pti_result AggregateCollectedData(pti_device_handle_t device,
                                  zet_metric_group_handle_t metric_group, bool samples_dropped,
                                  const TempRawDataFile& raw_data_file, size_t ip_metric_index,
                                  size_t reason_count, DeviceAggregate* device_aggregate) {
  if (device_aggregate == nullptr) {
    SPDLOG_ERROR("{}: device_aggregate is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  device_aggregate->Clear();
  if (device == nullptr) {
    SPDLOG_ERROR("{}: device is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  pti_pc_sampling_device_status_t& device_status = device_aggregate->status;
  device_status._struct_size = sizeof(pti_pc_sampling_device_status_t);
  device_status._device = device;
  device_status._samples_dropped = static_cast<uint32_t>(samples_dropped);

  const size_t raw_data_size = raw_data_file.size();
  SPDLOG_DEBUG("{}: device={}, raw_data_size={}, metric_group={}, metric_count={}, reason_count={}",
               __FUNCTION__, static_cast<const void*>(device), raw_data_size,
               static_cast<const void*>(metric_group), reason_count + 1, reason_count);

  if (metric_group == nullptr || raw_data_size == 0) {
    return PTI_SUCCESS;
  }

  if (ip_metric_index > reason_count) {
    SPDLOG_ERROR("{}: invalid IP metric index {}", __FUNCTION__, ip_metric_index);
    return PTI_ERROR_INTERNAL;
  }

  AggregationScratch scratch;
  PendingKernelMap pending_kernels;
  std::vector<uint8_t> raw_data;
  if (!raw_data_file.ReadAll(&raw_data)) {
    SPDLOG_ERROR("{}: failed to read raw data", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  if (!raw_data.empty()) {
    if (!DecodeMetricBlock(metric_group, raw_data, &device_status, &scratch)) {
      return PTI_ERROR_INTERNAL;
    }
    AggregateDecodedReports(ip_metric_index, reason_count, &device_status, &pending_kernels,
                            &scratch);
  }

  device_status._total_pc_count = scratch.unique_pcs.size();

  const pti_result persist_status =
      PersistAggregates(pending_kernels, reason_count, device_aggregate);
  if (persist_status != PTI_SUCCESS) {
    return persist_status;
  }

  if (!scratch.dropped_pcs.empty() || scratch.dropped_sample_count != 0) {
    SPDLOG_INFO("{}: dropped {} samples across {} unmapped PC(s)", __FUNCTION__,
                scratch.dropped_sample_count, scratch.dropped_pcs.size());
  }

  SPDLOG_DEBUG("{}: kernels={}, total_sample_count={}, total_pc_count={}", __FUNCTION__,
               device_aggregate->kernels.size(), device_status._total_sample_count,
               device_status._total_pc_count);
  return PTI_SUCCESS;
}

}  // namespace pti::pc_sampling
