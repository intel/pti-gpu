//
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_TIMER_HELPER_H_
#define PTI_TOOLS_ZE_TIMER_HELPER_H_

#include <level_zero/ze_api.h>

#include "pti_assert.h"

struct CPUGPUTimeInterpolationHelper {
  constexpr static uint64_t kSyncDeltaDefault = 1;  // 1 ns
  ze_device_handle_t device_;
  uint32_t gpu_freq_;
  uint64_t gpu_timer_mask_;
  uint64_t cpu_timestamp_;
  uint64_t gpu_timestamp_;
  uint64_t delta_ = kSyncDeltaDefault;
  uint64_t coeff_;
  CPUGPUTimeInterpolationHelper(ze_device_handle_t device, uint32_t gpu_freq,
                                uint64_t gpu_timer_mask, uint64_t sync_delta)
      : device_(device),
        gpu_freq_(gpu_freq),
        gpu_timer_mask_(gpu_timer_mask),
        cpu_timestamp_(0),
        gpu_timestamp_(0) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(gpu_freq != 0ULL);
    PTI_ASSERT(gpu_timer_mask != 0ULL);
    if (sync_delta != 0ULL) {
      delta_ = sync_delta;
    }
    coeff_ = 1'000'000'000 / gpu_freq_;
  }
};

#endif  // PTI_TOOLS_ZE_TIMER_HELPER_H_
