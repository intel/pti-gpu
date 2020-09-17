//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_
#define PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "utils.h"

#define FUNCTION_LENGTH 10
#define CALLS_LENGTH 12
#define TIME_LENGTH 16
#define PERCENT_LENGTH 10

#define ZEI_CALL_LOGGING 0
#define ZEI_CALL_LOGGING_TIMESTAMPS 1
#define ZEI_HOST_TIMING  2
#define ZEI_DEVICE_TIMING 3

struct FunctionTime {
  uint64_t total = 0;
  uint64_t min = (std::numeric_limits<uint64_t>::max)();
  uint64_t max = (std::numeric_limits<uint64_t>::min)();
  uint64_t call_count = 0;

  bool operator!=(const FunctionTime& r) const {
    return total != r.total;
  }

  bool operator>(const FunctionTime& r) const {
    return total > r.total;
  }
};

using FunctionTimeMap = std::map<std::string, FunctionTime>;

class ZeIntercept {
 public:
  ZeIntercept(unsigned options) : options_(options) {
    start_time_ = std::chrono::steady_clock::now();
  }

  ~ZeIntercept() {
    Report();
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  uint64_t GetTimestamp() {
    std::chrono::duration<uint64_t, std::nano> timestamp =
      std::chrono::steady_clock::now() - start_time_;
    return timestamp.count();
  }

  void AddHostTime(const char* function_name, uint64_t duration) {
    const std::lock_guard<std::mutex> lock(lock_);
    FunctionTime& time = host_time_map_[function_name];
    time.call_count += 1;
    time.total += duration;
    if (duration < time.min) {
      time.min = duration;
    }
    if (duration > time.max) {
      time.max = duration;
    }
  }

  void AddDeviceTime(const char* function_name, uint64_t duration) {
    const std::lock_guard<std::mutex> lock(lock_);
    FunctionTime& time = device_time_map_[function_name];
    time.call_count += 1;
    time.total += duration;
    if (duration < time.min) {
      time.min = duration;
    }
    if (duration > time.max) {
      time.max = duration;
    }
  }

 private:
  void ReportTiming(const FunctionTimeMap& time_map) {
    std::set< std::pair<std::string, FunctionTime>,
              utils::Comparator > sorted_list(
        time_map.begin(), time_map.end());
    
    uint64_t total_duration = 0;
    size_t max_name_length = FUNCTION_LENGTH;
    for (auto& value : sorted_list) {
      total_duration += value.second.total;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    std::cerr << "Total time (ns): " << total_duration << std::endl;
    std::cerr << std::endl;
    if (total_duration == 0) {
      return;
    }

    std::cerr << std::setw(max_name_length) << "Function" << "," <<
      std::setw(CALLS_LENGTH) << "Calls" << "," <<
      std::setw(TIME_LENGTH) << "Time (ns)" << "," <<
      std::setw(PERCENT_LENGTH) << "Time (%)" << "," <<
      std::setw(TIME_LENGTH) << "Average (ns)" << "," <<
      std::setw(TIME_LENGTH) << "Min (ns)" << "," <<
      std::setw(TIME_LENGTH) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min;
      uint64_t max_duration = value.second.max;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(CALLS_LENGTH) << call_count << "," <<
        std::setw(TIME_LENGTH) << duration << "," <<
        std::setw(PERCENT_LENGTH) << std::setprecision(2) << std::fixed <<
            percent_duration << "," <<
        std::setw(TIME_LENGTH) << avg_duration << "," <<
        std::setw(TIME_LENGTH) << min_duration << "," <<
        std::setw(TIME_LENGTH) << max_duration << std::endl;
    }
  }

  void ReportHostTiming() {
    std::cerr << std::endl;
    std::cerr << "=== Host Timing Results: ===" << std::endl;
    std::cerr << std::endl;

    ReportTiming(host_time_map_);
  }

  void ReportDeviceTiming() {
    std::cerr << std::endl;
    std::cerr << "=== Device Timing Results: ===" << std::endl;
    std::cerr << std::endl;

    ReportTiming(device_time_map_);
  }
  
  void Report() {
    if (CheckOption(ZEI_HOST_TIMING)) {
      ReportHostTiming();
    }
    if (CheckOption(ZEI_DEVICE_TIMING)) {
      ReportDeviceTiming();
    }
  }

 private:
  unsigned options_;
  std::mutex lock_;
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  FunctionTimeMap host_time_map_;
  FunctionTimeMap device_time_map_;
};

#endif // PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_