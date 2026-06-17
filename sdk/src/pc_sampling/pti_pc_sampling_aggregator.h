//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_AGGREGATE_API_H_
#define PTI_PC_SAMPLING_AGGREGATE_API_H_

#include <level_zero/zet_api.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "pti/pti_pc_sampling.h"
#include "pti_pc_sampling_raw_data_file.h"

namespace pti::pc_sampling {

/**
 * @brief File-backed aggregate for a single observed kernel.
 */
struct KernelAggregate {
  uint64_t kernel_handle = 0;               /**< Unique kernel identifier (== base_address) */
  std::string kernel_name_;                 /**< Stable kernel name kept alive for API returns */
  size_t reason_count = 0;                  /**< Number of persisted stall reasons */
  size_t instruction_count = 0;             /**< Number of persisted instructions */
  std::vector<uint64_t> aggregated_samples; /**< In-memory aggregated samples per stall reason */
  TempRawDataFile samples_file;             /**< Instruction offsets + flattened sample rows */
};

/**
 * @brief Aggregated results for the single profiled device.
 */
struct DeviceAggregate {
  pti_pc_sampling_device_status_t status{};              /**< Device-level totals */
  std::unordered_map<uint64_t, KernelAggregate> kernels; /**< kernel_handle -> aggregate */

  void Clear() {
    status = pti_pc_sampling_device_status_t{};
    kernels.clear();
  }
};

/**
 * @brief Reads the spilled raw data and folds it into a compact
 * per-kernel/per-instruction aggregate for the single profiled device.
 *
 * - ip_metric_index: index of the "IP" metric within the metric report.
 * - reason_count: number of stall reasons. Total metric count is reason_count + 1.
 *
 * PCs that do not map to a tracked kernel are dropped (not stored); the dropped
 * sample/PC counts are logged. Device totals reflect mapped samples only.
 */
pti_result AggregateCollectedData(pti_device_handle_t device,
                                  zet_metric_group_handle_t metric_group, bool samples_dropped,
                                  const TempRawDataFile& raw_data, size_t ip_metric_index,
                                  size_t reason_count, DeviceAggregate* device_aggregate);

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_AGGREGATE_API_H_
