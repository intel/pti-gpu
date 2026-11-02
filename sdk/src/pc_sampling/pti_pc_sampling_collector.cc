//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pc_sampling/pti_pc_sampling_collector.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "pc_sampling/pti_pc_sampling_internal.h"

namespace pti::pc_sampling {

PtiPcSamplingDataCollector::PtiPcSamplingDataCollector(
    pti_device_handle_t device, ze_driver_handle_t driver, zet_metric_group_handle_t metric_group,
    TempRawDataFile& raw_data, bool& samples_dropped, size_t collection_buffer_size)
    : device_(device),
      driver_(driver),
      metric_group_(metric_group),
      raw_data_(raw_data),
      samples_dropped_(samples_dropped),
      collection_buffer_size_(collection_buffer_size) {}

pti_result PtiPcSamplingDataCollector::Start(uint32_t sampling_period_ns) {
  constexpr auto kStartTimeout = std::chrono::seconds(5);

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
  SPDLOG_INFO("{}: starting metric streamer with collection buffer size {} bytes", __FUNCTION__,
              collection_buffer_size_);
  try {
    read_buffer_.resize(collection_buffer_size_);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("{}: failed to allocate {} byte collection buffer: {}", __FUNCTION__,
                 collection_buffer_size_, e.what());
    cleanup_start_failure();
    return PTI_ERROR_INTERNAL;
  }
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
    const bool signaled = start_cv_.wait_for(cv_lock, kStartTimeout, [this] {
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

  const bool drain_succeeded =
      handles_.streamer == nullptr || !raw_data_.IsOpen() ||
      ReadMetricStreamerData(handles_.streamer, read_buffer_.data(), read_buffer_.size(),
                             &raw_data_, &samples_dropped_, true);

  const bool finalize_succeeded = raw_data_.Finalize();
  if (!finalize_succeeded) {
    SPDLOG_WARN("{}: failed to flush deferred raw data file", __FUNCTION__);
  }

  SPDLOG_DEBUG("{}: device={}, metric_group={}, raw_data_size={}, samples_dropped={}", __FUNCTION__,
               static_cast<const void*>(device_), static_cast<const void*>(metric_group_),
               raw_data_.size(), samples_dropped_);

  handles_.Reset();
  state_.store(State::kStopped, std::memory_order_release);

  if (!drain_succeeded || !finalize_succeeded) {
    return PTI_ERROR_INTERNAL;
  }

  if (raw_data_.size() == 0) {
    SPDLOG_WARN(
        "{}: no EU stall data received from device {}; likely the profiling window was too short. "
        "Please try profiling longer or decrease the sampling period.",
        __FUNCTION__, static_cast<const void*>(device_));
    return PTI_WARN_PC_SAMPLING_NO_DATA_COLLECTED;
  }
  return PTI_SUCCESS;
}

bool PtiPcSamplingDataCollector::IsStopped() const {
  return state_.load(std::memory_order_acquire) == State::kStopped;
}

void PtiPcSamplingDataCollector::StreamerThread(uint32_t sampling_period_ns) {
  constexpr uint64_t kEventWaitTimeoutNs = 50'000'000;

  if (state_.load(std::memory_order_acquire) != State::kStarting || handles_.context == nullptr) {
    state_.store(State::kError, std::memory_order_release);
    start_cv_.notify_one();
    return;
  }

  if (!OpenMetricStreamer(
          handles_.context, reinterpret_cast<ze_device_handle_t>(device_), metric_group_,
          sampling_period_ns == 0 ? kDefaultSamplingPeriodNs : sampling_period_ns, &handles_)) {
    state_.store(State::kError, std::memory_order_release);
    start_cv_.notify_one();
    return;
  }

  state_.store(State::kRunning, std::memory_order_release);
  start_cv_.notify_one();

  while (state_.load(std::memory_order_acquire) != State::kStopRequested) {
    WaitForMetricStreamerReport(handles_.event, kEventWaitTimeoutNs);
    ReadMetricStreamerData(handles_.streamer, read_buffer_.data(), read_buffer_.size(), &raw_data_,
                           &samples_dropped_, false);
  }
}

}  // namespace pti::pc_sampling
