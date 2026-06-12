//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pc_sampling/pti_pc_sampling_collector.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "pc_sampling/pti_pc_sampling_internal.h"

namespace {

constexpr uint32_t kDefaultNotifyEveryNReports = 32'768;
constexpr uint64_t kMetricStreamerEventWaitTimeoutNs = 50'000'000;
constexpr auto kMetricStreamerStartTimeout = std::chrono::seconds(5);

}  // namespace

namespace pti::pc_sampling {

PtiPcSamplingDataCollector::PtiPcSamplingDataCollector(pti_device_handle_t device,
                                                       ze_driver_handle_t driver,
                                                       zet_metric_group_handle_t metric_group,
                                                       TempRawDataFile& raw_data,
                                                       bool& samples_dropped)
    : device_(device),
      driver_(driver),
      metric_group_(metric_group),
      raw_data_(raw_data),
      samples_dropped_(samples_dropped) {}

pti_result PtiPcSamplingDataCollector::Start(uint32_t sampling_period_ns) {
  const auto cleanup_start_failure = [this] {
    raw_data_.Reset();
    handles_.Reset();
    state_.store(State::kIdle, std::memory_order_release);
  };

  State expected_state = State::kIdle;
  if (!state_.compare_exchange_strong(expected_state, State::kStarting, std::memory_order_acq_rel,
                                      std::memory_order_acquire)) {
    SPDLOG_ERROR("{}: collector is not idle", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  samples_dropped_ = false;
  handles_.Reset();
  if (!CreateMetricStreamerContext(driver_, &handles_.context)) {
    cleanup_start_failure();
    return PTI_ERROR_INTERNAL;
  }
  if (!raw_data_.OpenTemp()) {
    cleanup_start_failure();
    return PTI_ERROR_INTERNAL;
  }

  thread_ = std::thread(&PtiPcSamplingDataCollector::StreamerThread, this, sampling_period_ns);

  {
    std::unique_lock<std::mutex> cv_lock(start_cv_mutex_);
    const bool signaled = start_cv_.wait_for(cv_lock, kMetricStreamerStartTimeout, [this] {
      return state_.load(std::memory_order_acquire) != State::kStarting;
    });

    if (!signaled) {
      state_.store(State::kStopRequested, std::memory_order_release);
      thread_.join();
      cleanup_start_failure();
      return PTI_ERROR_INTERNAL;
    }
  }

  if (state_.load(std::memory_order_acquire) == State::kError) {
    thread_.join();
    cleanup_start_failure();
    return PTI_ERROR_INTERNAL;
  }

  return PTI_SUCCESS;
}

pti_result PtiPcSamplingDataCollector::Stop() {
  if (!thread_.joinable()) {
    return PTI_ERROR_INTERNAL;
  }

  state_.store(State::kStopRequested, std::memory_order_release);
  // Joining the worker is the synchronization point before the control thread
  // finalizes the handle-owned deferred raw-data spill file.
  thread_.join();

  if (handles_.streamer != nullptr && raw_data_.IsOpen()) {
    std::vector<uint8_t> local_buffer;
    ReadMetricStreamerData(handles_.streamer, &local_buffer, &raw_data_, &samples_dropped_);
  }

  if (!raw_data_.Finalize()) {
    SPDLOG_WARN("{}: failed to flush deferred raw data file", __FUNCTION__);
  }

  SPDLOG_DEBUG("{}: device={}, metric_group={}, raw_data_size={}, samples_dropped={}", __FUNCTION__,
               static_cast<const void*>(device_), static_cast<const void*>(metric_group_),
               raw_data_.size(), samples_dropped_);

  handles_.Reset();
  state_.store(State::kStopped, std::memory_order_release);
  return PTI_SUCCESS;
}

void PtiPcSamplingDataCollector::StreamerThread(uint32_t sampling_period_ns) {
  if (state_.load(std::memory_order_acquire) != State::kStarting || handles_.context == nullptr) {
    state_.store(State::kError, std::memory_order_release);
    start_cv_.notify_one();
    return;
  }

  if (!OpenMetricStreamer(handles_.context, reinterpret_cast<ze_device_handle_t>(device_),
                          metric_group_,
                          sampling_period_ns == 0 ? kDefaultSamplingPeriodNs : sampling_period_ns,
                          kDefaultNotifyEveryNReports, &handles_)) {
    state_.store(State::kError, std::memory_order_release);
    start_cv_.notify_one();
    return;
  }

  state_.store(State::kRunning, std::memory_order_release);
  start_cv_.notify_one();

  std::vector<uint8_t> local_buffer;
  while (state_.load(std::memory_order_acquire) != State::kStopRequested) {
    WaitForMetricStreamerReport(handles_.event, kMetricStreamerEventWaitTimeoutNs);
    ReadMetricStreamerData(handles_.streamer, &local_buffer, &raw_data_, &samples_dropped_);
  }
}

}  // namespace pti::pc_sampling
