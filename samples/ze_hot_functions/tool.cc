//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>

#include "ze_tracer.h"
#include "ze_utils.h"
#include "utils.h"

const char* kLine =
  "+------------------------------------------------"
  "------------------------------------------------+";
const char* kHeader =
  "| Function                                       "
  "   | Call Count | Avg Time, ms | Total Time, ms |";

class ToolContext;
static ToolContext* context = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_hot_functions[.exe] <application> <args>" <<
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
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
}

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
  ToolContext(ZeTracer* gpu_tracer) : gpu_tracer_(gpu_tracer) {
    PTI_ASSERT(gpu_tracer_ != nullptr);
  }

  ZeTracer* GetGpuTracer() const {
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
  ZeTracer* gpu_tracer_ = nullptr;
  FunctionTimeMap function_time_map_;
  std::mutex lock_;
};

static void OnFunctionEnter(callback_data_t* data) {
  PTI_ASSERT(data != nullptr);

  std::chrono::time_point<std::chrono::steady_clock>* correlation_data =
    reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(
        data->correlation_data);
  *correlation_data = std::chrono::steady_clock::now();
}

static void OnFunctionExit(callback_data_t* data) {
  PTI_ASSERT(data != nullptr);

  std::chrono::time_point<std::chrono::steady_clock>* correlation_data =
    reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(
        data->correlation_data);
  std::chrono::duration<float> time = std::chrono::steady_clock::now() -
    *correlation_data;

  PTI_ASSERT(context != nullptr);
  context->AddFunctionTime(data->function_name, time.count());
}

static void Callback(function_id_t function, callback_data_t* callback_data,
                     void* user_data) {
  if (callback_data->site == ZE_CALLBACK_SITE_ENTER) {
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
    std::cout << "| " << std::left << std::setw(49) << pair.first << " | " <<
      std::right << std::setw(10) << pair.second.call_count << " | " <<
      std::setw(12) << std::setprecision(2) << std::fixed <<
      MSEC_IN_SEC * pair.second.total_time / pair.second.call_count << " | " <<
      std::setw(14) << MSEC_IN_SEC * pair.second.total_time << " |" <<
      std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

static ZeTracer* CreateTracer(ze_device_type_t type) {
  ze_device_handle_t device = nullptr;
  ze_driver_handle_t driver = nullptr;

  utils::ze::GetIntelDeviceAndDriver(type, device, driver);
  if (device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target" <<
      " device for tracing" << std::endl;
    return nullptr;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  ZeTracer* tracer = new ZeTracer(context, Callback, nullptr);
  if (tracer == nullptr || !tracer->IsValid()) {
    std::cout << "[WARNING] Unable to create Level Zero tracer for" <<
      " target driver" << std::endl;
    if (tracer != nullptr) {
      delete tracer;
      tracer = nullptr;
    }
    return nullptr;
  }

  for (int i = 0; i < ZE_FUNCTION_COUNT; ++i) {
    bool set = tracer->SetTracingFunction(static_cast<function_id_t>(i));
    PTI_ASSERT(set);
  }

  bool enabled = tracer->Enable();
  PTI_ASSERT(enabled);

  return tracer;
}

static void DisableTracer(ZeTracer* tracer) {
  PTI_ASSERT(tracer != nullptr);
  bool disabled = tracer->Disable();
  PTI_ASSERT(disabled);
}

static void DestroyTracer(ZeTracer* tracer) {
  PTI_ASSERT(tracer != nullptr);
  delete tracer;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ZeTracer* gpu_tracer = CreateTracer(ZE_DEVICE_TYPE_GPU);
  if (gpu_tracer == nullptr) {
    return;
  }

  PTI_ASSERT(context == nullptr);
  context = new ToolContext(gpu_tracer);
  PTI_ASSERT(context != nullptr);
}

void DisableProfiling() {
  if (context != nullptr) {
    DisableTracer(context->GetGpuTracer());
    PrintResults();
    DestroyTracer(context->GetGpuTracer());
    delete context;
  }
}