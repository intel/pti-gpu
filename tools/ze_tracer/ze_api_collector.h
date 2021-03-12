//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_COLLECTOR_H_
#define PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_COLLECTOR_H_

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>

#include <level_zero/layers/zel_tracing_api.h>

#include "utils.h"
#include "ze_correlator.h"
#include "ze_utils.h"

struct ZeFunction {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ZeFunction& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ZeFunction& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using ZeFunctionInfoMap = std::map<std::string, ZeFunction>;

typedef void (*OnZeFunctionFinishCallback)(
    void* data, const std::string& id, const std::string& name,
    uint64_t started, uint64_t ended);

class ZeApiCollector {
 public: // User Interface
  static ZeApiCollector* Create(
      ZeCorrelator* correlator,
      bool call_tracing = false,
      OnZeFunctionFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(correlator != nullptr);
    ZeApiCollector* collector = new ZeApiCollector(
        correlator, call_tracing, callback, callback_data);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {
      ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;

    status = zelTracerCreate(&tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS || tracer == nullptr) {
      std::cerr << "[WARNING] Unable to create L0 tracer" << std::endl;
      delete collector;
      return nullptr;
    }

    collector->tracer_ = tracer;
    SetTracingAPIs(tracer);

    status = zelTracerSetEnabled(tracer, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return collector;
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  const ZeFunctionInfoMap& GetFunctionInfoMap() const {
    return function_info_map_;
  }

  static void PrintFunctionsTable(const ZeFunctionInfoMap& function_info_map) {
    std::set< std::pair<std::string, ZeFunction>,
              utils::Comparator > sorted_list(
        function_info_map.begin(), function_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kFunctionLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::cerr << std::setw(max_name_length) << "Function" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kTimeLength) << min_duration << "," <<
        std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

  ~ZeApiCollector() {
    if (tracer_ != nullptr) {
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

 private: // Tracing Interface
  uint64_t GetTimestamp() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetTimestamp();
  }

  void AddFunctionTime(const std::string& name, uint64_t time) {
    const std::lock_guard<std::mutex> lock(lock_);
    if (function_info_map_.count(name) == 0) {
      function_info_map_[name] = {time, time, time, 1};
    } else {
      ZeFunction& function = function_info_map_[name];
      function.total_time += time;
      if (time < function.min_time) {
        function.min_time = time;
      }
      if (time > function.max_time) {
        function.max_time = time;
      }
      ++function.call_count;
    }
  }

 private: // Implementation Details
  ZeApiCollector(
      ZeCorrelator* correlator, bool call_tracing,
      OnZeFunctionFinishCallback callback, void* callback_data)
      : correlator_(correlator), call_tracing_(call_tracing),
        callback_(callback), callback_data_(callback_data) {
    PTI_ASSERT(correlator_ != nullptr);
  }

  #include <tracing.gen> // Auto-generated callbacks

 private: // Data
  zel_tracer_handle_t tracer_ = nullptr;

  ZeFunctionInfoMap function_info_map_;
  std::mutex lock_;

  ZeCorrelator* correlator_ = nullptr;
  bool call_tracing_ = false;

  OnZeFunctionFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  static const uint32_t kFunctionLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_COLLECTOR_H_