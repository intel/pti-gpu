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
  constexpr static uint64_t kSyncDeltaDefault = 300000;   // 300000 ns == 300 us == 0.3 ms
  constexpr static uint64_t kThresholdForMissync = 1500;  // device ticks
  ze_device_handle_t device_;

  // It is not GPU core or any other clock frequency,
  // this is the frequency of the GPU timer used for profiling needs,
  // expected that it is several MHz, e.g 12.5 MHz on PVC.
  // It is not expected to be higher than 500 MHz
  uint32_t gpu_timer_freq_hz_;
  uint64_t gpu_timer_mask_;
  uint64_t cpu_timestamp_;
  uint64_t gpu_timestamp_;
  uint64_t delta_ = kSyncDeltaDefault;
  uint64_t nsec_in_gpu_timer_tick_;
  CPUGPUTimeInterpolationHelper(ze_device_handle_t device, uint32_t gpu_timer_freq_hz,
                                uint64_t gpu_timer_mask, uint64_t sync_delta)
      : device_(device),
        gpu_timer_freq_hz_(gpu_timer_freq_hz),
        gpu_timer_mask_(gpu_timer_mask),
        cpu_timestamp_(0),
        gpu_timestamp_(0) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(gpu_timer_freq_hz != 0ULL);
    PTI_ASSERT(gpu_timer_mask != 0ULL);
    if (sync_delta != 0ULL) {
      delta_ = sync_delta;
    }
    nsec_in_gpu_timer_tick_ = 1'000'000'000 / gpu_timer_freq_hz_;
  }
  uint64_t GetNsecInGPUTimerTick() const { return nsec_in_gpu_timer_tick_; }
};

#endif  // PTI_TOOLS_ZE_TIMER_HELPER_H_
