//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_AGGREGATOR_H_
#define PTI_PC_SAMPLING_AGGREGATOR_H_

#include <vector>

#include "pc_sampling/pti_pc_sampling_internal.h"

namespace pti::pc_sampling {

pti_result AggregateCollectedData(
    pti_device_handle_t device, zet_metric_group_handle_t metric_group, bool samples_dropped,
    const TempRawDataFile& raw_data,
    std::vector<pti_pc_sampling_device_status_t>* aggregated_device_data);

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_AGGREGATOR_H_
