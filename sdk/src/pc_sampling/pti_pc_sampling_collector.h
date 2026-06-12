//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_COLLECTOR_H_
#define PTI_PC_SAMPLING_COLLECTOR_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "pc_sampling/pti_pc_sampling_helper.h"
#include "pc_sampling/pti_pc_sampling_raw_data_file.h"
#include "pti/pti_pc_sampling.h"

namespace pti::pc_sampling {

class PtiPcSamplingDataCollector {
 public:
  PtiPcSamplingDataCollector(pti_device_handle_t device, ze_driver_handle_t driver,
                             zet_metric_group_handle_t metric_group, TempRawDataFile& raw_data,
                             bool& samples_dropped);
  ~PtiPcSamplingDataCollector() {
    if (thread_.joinable()) {
      state_.store(State::kStopRequested, std::memory_order_release);
      thread_.join();
    }

    handles_.Reset();
    state_.store(State::kIdle, std::memory_order_release);
  }

  PtiPcSamplingDataCollector(const PtiPcSamplingDataCollector&) = delete;
  PtiPcSamplingDataCollector& operator=(const PtiPcSamplingDataCollector&) = delete;
  PtiPcSamplingDataCollector(PtiPcSamplingDataCollector&&) = delete;
  PtiPcSamplingDataCollector& operator=(PtiPcSamplingDataCollector&&) = delete;

  pti_result Start(uint32_t sampling_period_ns);
  pti_result Stop();

 private:
  enum class State : uint32_t {
    kIdle = 0,
    kStarting = 1,
    kRunning = 2,
    kStopRequested = 3,
    kStopped = 4,
    kError = 5,
    kForceUint32 = 0x7fffffff,
  };

  void StreamerThread(uint32_t sampling_period_ns);

  std::mutex start_cv_mutex_;
  std::condition_variable start_cv_;
  std::atomic<State> state_{State::kIdle};
  std::thread thread_;

  pti_device_handle_t device_ = nullptr;
  ze_driver_handle_t driver_ = nullptr;
  zet_metric_group_handle_t metric_group_ = nullptr;

  MetricStreamerHandles handles_;
  TempRawDataFile& raw_data_;
  bool& samples_dropped_;
};

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_COLLECTOR_H_
