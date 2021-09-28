//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_CL_API_COLLECTOR_H_
#define PTI_TOOLS_CL_TRACER_CL_API_COLLECTOR_H_

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "correlator.h"
#include "trace_guard.h"

struct ClFunction {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ClFunction& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ClFunction& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using ClFunctionInfoMap = std::map<std::string, ClFunction>;

typedef void (*OnClFunctionFinishCallback)(
    void* data, uint64_t id, const std::string& name,
    uint64_t started, uint64_t ended);

class ClApiCollector;

void OnEnterFunction(
    cl_function_id function, cl_callback_data* data,
    uint64_t start, ClApiCollector* collector);
void OnExitFunction(
    cl_function_id function, cl_callback_data* data,
    uint64_t start, uint64_t end, ClApiCollector* collector);

class ClApiCollector {
 public: // User Interface
  static ClApiCollector* Create(
      cl_device_id device,
      Correlator* correlator,
      ApiCollectorOptions options = {false, false, false},
      OnClFunctionFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(correlator != nullptr);
    TraceGuard guard;

    ClApiCollector* collector = new ClApiCollector(
        device, correlator, options, callback, callback_data);
    PTI_ASSERT(collector != nullptr);

    ClApiTracer* tracer = new ClApiTracer(device, Callback, collector);
    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer " <<
        "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
        delete collector;
      }
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ClApiCollector() {
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    bool disabled = tracer_->Disable();
    PTI_ASSERT(disabled);
  }

  const ClFunctionInfoMap& GetFunctionInfoMap() const {
    return function_info_map_;
  }

  uint64_t GetKernelId() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetKernelId();
  }

  bool NeedTid() const {
    return options_.need_tid;
  }

  bool NeedPid() const {
    return options_.need_pid;
  }

  void Log(const std::string& text) {
    PTI_ASSERT(correlator_ != nullptr);
    correlator_->Log(text);
  }

  ClApiCollector(const ClApiCollector& copy) = delete;
  ClApiCollector& operator=(const ClApiCollector& copy) = delete;

  void PrintFunctionsTable() const {
    std::set< std::pair<std::string, ClFunction>,
              utils::Comparator > sorted_list(
        function_info_map_.begin(), function_info_map_.end());

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

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Function" << "," <<
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
      stream << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kTimeLength) << min_duration << "," <<
        std::setw(kTimeLength) << max_duration << std::endl;
    }

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->Log(stream.str());
  }

 private: // Implementation Details
  ClApiCollector(
      cl_device_id device,
      Correlator* correlator,
      ApiCollectorOptions options,
      OnClFunctionFinishCallback callback,
      void* callback_data)
      : correlator_(correlator),
        options_(options),
        callback_(callback),
        callback_data_(callback_data) {
    PTI_ASSERT(correlator_ != nullptr);
    device_type_ = utils::cl::GetDeviceType(device);
    PTI_ASSERT(
        device_type_ == CL_DEVICE_TYPE_CPU ||
        device_type_ == CL_DEVICE_TYPE_GPU);
  }

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    for (int id = 0; id < CL_FUNCTION_COUNT; ++id) {
      bool set = tracer_->SetTracingFunction(static_cast<cl_function_id>(id));
      PTI_ASSERT(set);
    }

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  uint64_t GetTimestamp() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetTimestamp();
  }

  void AddFunctionTime(const std::string& name, uint64_t time) {
    const std::lock_guard<std::mutex> lock(lock_);
    if (function_info_map_.count(name) == 0) {
      function_info_map_[name] = {time, time, time, 1};
    } else {
      ClFunction& function = function_info_map_[name];
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

 private: // Callbacks
  static void Callback(
      cl_function_id function,
      cl_callback_data* callback_data,
      void* user_data) {
    if (TraceGuard::Inactive()) return;

    ClApiCollector* collector = reinterpret_cast<ClApiCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(callback_data != nullptr);
    PTI_ASSERT(callback_data->correlationData != nullptr);

    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      uint64_t& start_time = *reinterpret_cast<uint64_t*>(
          callback_data->correlationData);
      start_time = collector->GetTimestamp();

      if (collector->options_.call_tracing) {
        OnEnterFunction(function, callback_data, start_time, collector);
      }
    } else {
      uint64_t end_time = collector->GetTimestamp();

      uint64_t& start_time = *reinterpret_cast<uint64_t*>(
          callback_data->correlationData);
      collector->AddFunctionTime(
        callback_data->functionName, end_time - start_time);

      if (collector->options_.call_tracing) {
        OnExitFunction(
            function, callback_data, start_time, end_time, collector);
      }

      if (collector->callback_ != nullptr) {
        uint64_t kernel_id = 0;
        if (function == CL_FUNCTION_clEnqueueNDRangeKernel ||
            function == CL_FUNCTION_clEnqueueReadBuffer ||
            function == CL_FUNCTION_clEnqueueWriteBuffer) {
          PTI_ASSERT(collector->correlator_ != nullptr);
          kernel_id = collector->correlator_->GetKernelId();
        }

        collector->callback_(
            collector->callback_data_, kernel_id,
            callback_data->functionName, start_time, end_time);
      }
    }

    #define SET_EXTENSION_FUNCTION(name) \
      if (std::string(#name) == *params->funcName) { \
        if (collector->device_type_ == CL_DEVICE_TYPE_GPU) { \
          *reinterpret_cast<decltype(name<CL_DEVICE_TYPE_GPU>)**>( \
              callback_data->functionReturnValue) = &name<CL_DEVICE_TYPE_GPU>; \
        } else { \
          PTI_ASSERT(collector->device_type_ == CL_DEVICE_TYPE_CPU); \
          *reinterpret_cast<decltype(name<CL_DEVICE_TYPE_CPU>)**>( \
              callback_data->functionReturnValue) = &name<CL_DEVICE_TYPE_CPU>; \
        } \
      }

    if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
      if (function == CL_FUNCTION_clGetExtensionFunctionAddress) {
        const cl_params_clGetExtensionFunctionAddress* params =
          reinterpret_cast<const cl_params_clGetExtensionFunctionAddress*>(
              callback_data->functionParams);
        SET_EXTENSION_FUNCTION(clHostMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clDeviceMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clSharedMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clMemFreeINTEL);
        SET_EXTENSION_FUNCTION(clGetMemAllocInfoINTEL);
        SET_EXTENSION_FUNCTION(clSetKernelArgMemPointerINTEL);
        SET_EXTENSION_FUNCTION(clEnqueueMemcpyINTEL);
        SET_EXTENSION_FUNCTION(clGetDeviceGlobalVariablePointerINTEL);
        SET_EXTENSION_FUNCTION(clGetKernelSuggestedLocalWorkSizeINTEL);
      } else if (function ==
                 CL_FUNCTION_clGetExtensionFunctionAddressForPlatform) {
        const cl_params_clGetExtensionFunctionAddressForPlatform* params =
          reinterpret_cast<
              const cl_params_clGetExtensionFunctionAddressForPlatform*>(
                  callback_data->functionParams);
        SET_EXTENSION_FUNCTION(clHostMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clDeviceMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clSharedMemAllocINTEL);
        SET_EXTENSION_FUNCTION(clMemFreeINTEL);
        SET_EXTENSION_FUNCTION(clGetMemAllocInfoINTEL);
        SET_EXTENSION_FUNCTION(clSetKernelArgMemPointerINTEL);
        SET_EXTENSION_FUNCTION(clEnqueueMemcpyINTEL);
        SET_EXTENSION_FUNCTION(clGetDeviceGlobalVariablePointerINTEL);
        SET_EXTENSION_FUNCTION(clGetKernelSuggestedLocalWorkSizeINTEL);
      }
    }
  }

 private: // Data
  ClApiTracer* tracer_ = nullptr;

  Correlator* correlator_ = nullptr;
  ApiCollectorOptions options_ = {false, false, false};
  cl_device_type device_type_ = CL_DEVICE_TYPE_ALL;

  OnClFunctionFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  ClFunctionInfoMap function_info_map_;

  static const uint32_t kFunctionLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;

  friend class ClExtCollector;
};

#endif // PTI_TOOLS_CL_TRACER_CL_API_COLLECTOR_H_