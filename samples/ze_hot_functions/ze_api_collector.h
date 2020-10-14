//==============================================================
// Copyright © 2020 Intel Corporation
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

#include <level_zero/zet_api.h>

#include "utils.h"
#include "ze_utils.h"

namespace ze_api_collector {
  const uint32_t kFunctionLength = 10;
  const uint32_t kCallsLength = 12;
  const uint32_t kTimeLength = 20;
  const uint32_t kPercentLength = 10;
} // namespace ze_api_collector

struct Function {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const Function& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const Function& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using FunctionInfoMap = std::map<std::string, Function>;
using FunctionTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

typedef void (*OnFunctionFinishCallback)(
    void* data, const std::string& name,
    uint64_t started, uint64_t ended);

class ZeApiCollector {
 public: // User Interface
  static ZeApiCollector* Create(
      ze_driver_handle_t driver,
      FunctionTimePoint base_time = std::chrono::steady_clock::now(),
      bool call_tracing = false,
      OnFunctionFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(driver != nullptr);

    ze_context_handle_t context = utils::ze::GetContext(driver);
    PTI_ASSERT(context != nullptr);

    ZeApiCollector* collector =
      new ZeApiCollector(context, base_time, call_tracing,
                         callback, callback_data);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zet_tracer_exp_handle_t tracer = nullptr;

    status = zetTracerExpCreate(context, &tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS || tracer == nullptr) {
      std::cerr <<
        "[WARNING] Unable to create Level Zero tracer for target context" <<
        std::endl;
      delete collector;
      return nullptr;
    }

    collector->tracer_ = tracer;
    SetTracingAPIs(tracer);

    status = zetTracerExpSetEnabled(tracer, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return collector;
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  const FunctionInfoMap& GetFunctionInfoMap() const {
    return function_info_map_;
  }

  static void PrintFunctionsTable(const FunctionInfoMap& function_info_map) {
    std::set< std::pair<std::string, Function>,
              utils::Comparator > sorted_list(
        function_info_map.begin(), function_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = ze_api_collector::kFunctionLength;
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
      std::setw(ze_api_collector::kCallsLength) << "Calls" << "," <<
      std::setw(ze_api_collector::kTimeLength) << "Time (ns)" << "," <<
      std::setw(ze_api_collector::kPercentLength) << "Time (%)" << "," <<
      std::setw(ze_api_collector::kTimeLength) << "Average (ns)" << "," <<
      std::setw(ze_api_collector::kTimeLength) << "Min (ns)" << "," <<
      std::setw(ze_api_collector::kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(ze_api_collector::kCallsLength) << call_count << "," <<
        std::setw(ze_api_collector::kTimeLength) << duration << "," <<
        std::setw(ze_api_collector::kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(ze_api_collector::kTimeLength) << avg_duration << "," <<
        std::setw(ze_api_collector::kTimeLength) << min_duration << "," <<
        std::setw(ze_api_collector::kTimeLength) << max_duration << std::endl;
    }
  }

  ~ZeApiCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;

    if (tracer_ != nullptr) {
      status = zetTracerExpDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    PTI_ASSERT(context_ != nullptr);
    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

 private: // Tracing Interface
  uint64_t GetTimestamp() const {
    std::chrono::duration<uint64_t, std::nano> timestamp =
      std::chrono::steady_clock::now() - base_time_;
    return timestamp.count();
  }

  void AddFunctionTime(const std::string& name, uint64_t time) {
    const std::lock_guard<std::mutex> lock(lock_);
    if (function_info_map_.count(name) == 0) {
      function_info_map_[name] = {time, time, time, 1};
    } else {
      Function& function = function_info_map_[name];
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
  ZeApiCollector(ze_context_handle_t context,
                 FunctionTimePoint base_time,
                 bool call_tracing,
                 OnFunctionFinishCallback callback,
                 void* callback_data)
      : context_(context),
        base_time_(base_time),
        call_tracing_(call_tracing),
        callback_(callback),
        callback_data_(callback_data) {
    PTI_ASSERT(context_ != nullptr);
  }

  #include <tracing.gen> // Auto-generated callbacks

 private:
  ze_context_handle_t context_ = nullptr;
  zet_tracer_exp_handle_t tracer_ = nullptr;

  FunctionInfoMap function_info_map_;
  std::mutex lock_;

  FunctionTimePoint base_time_;
  bool call_tracing_ = false;

  OnFunctionFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;
};

#endif // PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_COLLECTOR_H_