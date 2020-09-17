//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "cl_tracer.h"
#include "cl_utils.h"

const char* kLine =
  "+-----------------------------------------------"
  "-----------------------------------------------+";
const char* kHeader =
  "| Function                                      "
  "  | Call Count | Avg Time, ms | Total Time, ms |";

class ToolContext;
static ToolContext* context = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_hot_functions[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {}

// Internal Tool Functionality ////////////////////////////////////////////////

struct FunctionInfo {
  float total_time;
  uint32_t call_count;

  bool operator>(const FunctionInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const FunctionInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using FunctionTimeMap = std::map<std::string, FunctionInfo>;

class ToolContext {
 public:
  ToolContext(ClTracer* gpu_tracer, ClTracer* cpu_tracer) :
      gpu_tracer_(gpu_tracer), cpu_tracer_(cpu_tracer) {
    PTI_ASSERT(gpu_tracer_ != nullptr || cpu_tracer_ != nullptr);
  }

  ClTracer* GetCpuTracer() const {
    return cpu_tracer_;
  }

  ClTracer* GetGpuTracer() const {
    return gpu_tracer_;
  }

  const FunctionTimeMap& GetFunctionTimeMap() const {
    return function_time_map_;
  }

  void AddFunctionTime(std::string name, float time) {
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(!name.empty());
    size_t function_count = function_time_map_.count(name);
    if (function_count == 0) {
      function_time_map_[name] = {time, 1};
    } else {
      FunctionInfo& info = function_time_map_[name];
      info.total_time += time;
      info.call_count += 1;
    }
  }

 private:
  ClTracer* gpu_tracer_ = nullptr;
  ClTracer* cpu_tracer_ = nullptr;
  std::mutex lock_;
  FunctionTimeMap function_time_map_;
};

static void OnFunctionEnter(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  std::chrono::time_point<std::chrono::steady_clock>* correlation_data =
    reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(
        data->correlationData);
  *correlation_data = std::chrono::steady_clock::now();
}

static void OnFunctionExit(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  std::chrono::time_point<std::chrono::steady_clock>* correlation_data =
    reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(
        data->correlationData);
  std::chrono::duration<float> time =
    std::chrono::steady_clock::now() - *correlation_data;

  PTI_ASSERT(context != nullptr);
  context->AddFunctionTime(data->functionName, time.count());
}

static void Callback(cl_function_id function,
                     cl_callback_data* callback_data,
                     void* user_data) {
  if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
    OnFunctionEnter(callback_data);
  } else {
    OnFunctionExit(callback_data);
  }
}

static void PrintResults() {
  PTI_ASSERT(context != nullptr);
  const FunctionTimeMap& function_time_map = context->GetFunctionTimeMap();
  if (function_time_map.size() == 0) {
    return;
  }

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  std::set<std::pair<std::string, FunctionInfo>, utils::Comparator> set(
      function_time_map.begin(), function_time_map.end());
  for (auto& pair : set) {
    PTI_ASSERT(pair.second.call_count > 0);
    std::cout << "| " << std::left << std::setw(47) << pair.first << " | " <<
      std::right << std::setw(10) << pair.second.call_count << " | " <<
      std::setw(12) << std::setprecision(2) << std::fixed <<
      MSEC_IN_SEC * pair.second.total_time / pair.second.call_count << " | " <<
      std::setw(14) << MSEC_IN_SEC * pair.second.total_time << " |" <<
      std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

static ClTracer* CreateTracer(cl_device_type type) {
  cl_device_id device = utils::cl::GetIntelDevice(type);
  if (device == nullptr) {
    std::cout << "[WARNING] Unable to find target " <<
      (type == CL_DEVICE_TYPE_GPU ? "GPU" : "CPU") <<
      " device for tracing" << std::endl;
    return nullptr;
  }

  ClTracer* tracer = new ClTracer(device, Callback, nullptr);
  if (tracer == nullptr || !tracer->IsValid()) {
    std::cout << "[WARNING] Unable to create OpenCL tracer for target " <<
      (type == CL_DEVICE_TYPE_GPU ? "GPU" : "CPU") <<
      " device" << std::endl;
    if (tracer != nullptr) {
      delete tracer;
      tracer = nullptr;
    }
    return nullptr;
  }

  return tracer;
}

static void EnableTracer(ClTracer* tracer) {
  if (tracer == nullptr) {
    return;
  }

  for (int i = 0; i < CL_FUNCTION_COUNT; ++i) {
    bool set = tracer->SetTracingFunction(static_cast<cl_function_id>(i));
    PTI_ASSERT(set);
  }

  bool enabled = tracer->Enable();
  PTI_ASSERT(enabled);
}

static void DisableTracer(ClTracer* tracer) {
  if (tracer == nullptr) {
    return;
  }

  bool disabled = tracer->Disable();
  PTI_ASSERT(disabled);
}

static void DestroyTracer(ClTracer* tracer) {
  if (tracer != nullptr) {
    delete tracer;
  }
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ClTracer* gpu_tracer = CreateTracer(CL_DEVICE_TYPE_GPU);
  ClTracer* cpu_tracer = CreateTracer(CL_DEVICE_TYPE_CPU);

  if (gpu_tracer != nullptr || cpu_tracer != nullptr) {
    PTI_ASSERT(context == nullptr);
    context = new ToolContext(gpu_tracer, cpu_tracer);
    PTI_ASSERT(context != nullptr);
  }

  EnableTracer(gpu_tracer);
  EnableTracer(cpu_tracer);
}

void DisableProfiling() {
  if (context != nullptr) {
    DisableTracer(context->GetGpuTracer());
    DisableTracer(context->GetCpuTracer());
    PrintResults();
    DestroyTracer(context->GetGpuTracer());
    DestroyTracer(context->GetCpuTracer());
    delete context;
  }
}