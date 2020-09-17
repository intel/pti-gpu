//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "cl_tracer.h"
#include "cl_utils.h"

const char* kLine =
  "+------------------------------------------------"
  "------------------------------------------------+";
const char* kHeader =
  "|                                           | Call "
  " | SIMD  | Total MBytes |   Avg    |  Total   |\n"
  "| Kernel                                    | Count"
  " | Width |  Transfered  | Time, ms | Time, ms |";

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

struct KernelInfo {
  uint64_t total_time;
  uint32_t call_count;
  size_t simd_width;
  size_t bytes_transfered;

  bool operator>(const KernelInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const KernelInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using KernelInfoMap = std::map<std::string, KernelInfo>;

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

  const KernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  void AddKernelInfo(std::string name, uint64_t time,
                     size_t simd_width, size_t bytes_transfered) {
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(!name.empty());
    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {time, 1, simd_width, bytes_transfered};
    } else {
      KernelInfo& info = kernel_info_map_[name];
      info.total_time += time;
      info.call_count += 1;
      info.bytes_transfered += bytes_transfered;
      PTI_ASSERT(info.simd_width == simd_width);
    }
  }

 private:
  ClTracer* gpu_tracer_ = nullptr;
  ClTracer* cpu_tracer_ = nullptr;
  std::mutex lock_;
  KernelInfoMap kernel_info_map_;
};

static void CL_CALLBACK KernelEventNotify(cl_event event, cl_int event_status,
                                          void* user_data) {
  PTI_ASSERT(event_status == CL_COMPLETE);

  PTI_ASSERT(user_data != nullptr);
  cl_kernel kernel = reinterpret_cast<cl_kernel>(user_data);
  std::string name = utils::cl::GetKernelName(kernel);

  cl_command_queue queue = utils::cl::GetCommandQueue(event);
  PTI_ASSERT(queue != nullptr);

  cl_device_id device = utils::cl::GetDevice(queue);
  PTI_ASSERT(device != nullptr);

  size_t simd_width = utils::cl::GetSimdWidth(device, kernel);
  PTI_ASSERT(simd_width > 0);

  cl_ulong time = utils::cl::GetEventTime(event);
  PTI_ASSERT(time > 0);

  cl_int status = clReleaseKernel(kernel);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clReleaseEvent(event);
  PTI_ASSERT(status == CL_SUCCESS);

  PTI_ASSERT(context != nullptr);
  context->AddKernelInfo(name, time, simd_width, 0);
}

static void CL_CALLBACK ReadEventNotify(cl_event event, cl_int event_status,
                                        void* user_data) {
  PTI_ASSERT(event_status == CL_COMPLETE);
  std::string name = "DtoH";

  size_t bytes_transfered = reinterpret_cast<size_t>(user_data);
  PTI_ASSERT(bytes_transfered > 0);

  cl_ulong time = utils::cl::GetEventTime(event);
  PTI_ASSERT(time > 0);

  cl_int status = clReleaseEvent(event);
  PTI_ASSERT(status == CL_SUCCESS);

  PTI_ASSERT(context != nullptr);
  context->AddKernelInfo(name, time, 0, bytes_transfered);
}

static void CL_CALLBACK WriteEventNotify(cl_event event, cl_int event_status,
                                         void* user_data) {
  PTI_ASSERT(event_status == CL_COMPLETE);
  std::string name = "HtoD";

  size_t bytes_transfered = reinterpret_cast<size_t>(user_data);
  PTI_ASSERT(bytes_transfered > 0);

  cl_ulong time = utils::cl::GetEventTime(event);
  PTI_ASSERT(time > 0);

  cl_int status = clReleaseEvent(event);
  PTI_ASSERT(status == CL_SUCCESS);

  PTI_ASSERT(context != nullptr);
  context->AddKernelInfo(name, time, 0, bytes_transfered);
}

static void OnEnterCreateCommandQueueWithProperties(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clCreateCommandQueueWithProperties* params =
    reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_queue_properties* props =
    utils::cl::EnableQueueProfiling(*(params->properties));
  *(params->properties) = props;
  data->correlationData[0] = reinterpret_cast<cl_ulong>(props);
}

static void OnExitCreateCommandQueueWithProperties(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  cl_queue_properties* props =
    reinterpret_cast<cl_queue_properties*>(data->correlationData[0]);
  PTI_ASSERT(props != nullptr);
  delete[] props;
}

static void OnEnterCreateCommandQueue(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clCreateCommandQueue* params =
    reinterpret_cast<const cl_params_clCreateCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);
  *(params->properties) |=
    static_cast<unsigned long>(CL_QUEUE_PROFILING_ENABLE);
}

static void OnEnterEnqueueNDRangeKernel(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueNDRangeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  if (*(params->event) == nullptr) {
    *(params->event) = reinterpret_cast<cl_event*>(data->correlationData);
  }
}

static void OnExitEnqueueNDRangeKernel(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueNDRangeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
  if (*return_value == CL_SUCCESS) {
    PTI_ASSERT(*(params->event) != nullptr);
    cl_int status = CL_SUCCESS;

    if (*(params->event) !=
        reinterpret_cast<cl_event*>(data->correlationData)) {
      status = clRetainEvent(**(params->event));
      PTI_ASSERT(status == CL_SUCCESS);
    }

    status = clRetainKernel(*(params->kernel));
    PTI_ASSERT(status == CL_SUCCESS);
    status = clSetEventCallback(**(params->event), CL_COMPLETE,
                                KernelEventNotify, *(params->kernel));
    PTI_ASSERT(status == CL_SUCCESS);
  }
}

static void OnEnterEnqueueReadBuffer(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueReadBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  if (*(params->event) == nullptr) {
    *(params->event) = reinterpret_cast<cl_event*>(data->correlationData);
  }
}

static void OnExitEnqueueReadBuffer(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueReadBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_int* return_value = reinterpret_cast<cl_int*>(
      data->functionReturnValue);
  if (*return_value == CL_SUCCESS) {
    PTI_ASSERT(*(params->event) != nullptr);
    cl_int status = CL_SUCCESS;

    if (*(params->event) !=
        reinterpret_cast<cl_event*>(data->correlationData)) {
      status = clRetainEvent(**(params->event));
      PTI_ASSERT(status == CL_SUCCESS);
    }

    status = clSetEventCallback(**(params->event), CL_COMPLETE,
                                ReadEventNotify,
                                reinterpret_cast<void*>(*(params->cb)));
    PTI_ASSERT(status == CL_SUCCESS);
  }
}

static void OnEnterEnqueueWriteBuffer(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueWriteBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  if (*(params->event) == nullptr) {
    *(params->event) = reinterpret_cast<cl_event*>(data->correlationData);
  }
}

static void OnExitEnqueueWriteBuffer(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  const cl_params_clEnqueueWriteBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_int* return_value = reinterpret_cast<cl_int*>(
      data->functionReturnValue);
  if (*return_value == CL_SUCCESS) {
    PTI_ASSERT(*(params->event) != nullptr);
    cl_int status = CL_SUCCESS;

    if (*(params->event) !=
        reinterpret_cast<cl_event*>(data->correlationData)) {
      status = clRetainEvent(**(params->event));
      PTI_ASSERT(status == CL_SUCCESS);
    }

    status = clSetEventCallback(**(params->event), CL_COMPLETE,
                                WriteEventNotify,
                                reinterpret_cast<void*>(*(params->cb)));
    PTI_ASSERT(status == CL_SUCCESS);
  }
}

static void Callback(cl_function_id function,
                     cl_callback_data* callback_data,
                     void* user_data) {
  if (function == CL_FUNCTION_clCreateCommandQueueWithProperties) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterCreateCommandQueueWithProperties(callback_data);
    } else {
      OnExitCreateCommandQueueWithProperties(callback_data);
    }
  } else if (function == CL_FUNCTION_clCreateCommandQueue) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterCreateCommandQueue(callback_data);
    }
  } else if (function == CL_FUNCTION_clEnqueueNDRangeKernel) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterEnqueueNDRangeKernel(callback_data);
    } else {
      OnExitEnqueueNDRangeKernel(callback_data);
    }
  } else if (function == CL_FUNCTION_clEnqueueReadBuffer) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterEnqueueReadBuffer(callback_data);
    } else {
      OnExitEnqueueReadBuffer(callback_data);
    }
  } else if (function == CL_FUNCTION_clEnqueueWriteBuffer) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterEnqueueWriteBuffer(callback_data);
    } else {
      OnExitEnqueueWriteBuffer(callback_data);
    }
  }
}

static void PrintResults() {
  PTI_ASSERT(context != nullptr);
  const KernelInfoMap& kernel_info_map = context->GetKernelInfoMap();
  if (kernel_info_map.size() == 0) {
    return;
  }

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  std::set<std::pair<std::string, KernelInfo>, utils::Comparator> set(
    kernel_info_map.begin(), kernel_info_map.end());
  for (auto& pair : set) {
    PTI_ASSERT(pair.second.call_count > 0);
    float total_time = pair.second.total_time /
      static_cast<float>(NSEC_IN_MSEC);
    float avg_time = total_time / pair.second.call_count;
    float mbytes_transfered = pair.second.bytes_transfered /
      static_cast<float>(BYTES_IN_MBYTES);

    std::string kernel_name = pair.first;
    if (kernel_name.size() > 40) {
      kernel_name = kernel_name.substr(0, 32) + "<...>";
    }
    std::cout << "| " << std::left << std::setw(41) << kernel_name << " | ";
    std::cout << std::right << std::setw(5) << pair.second.call_count << " | ";
    std::cout << std::setw(5);
    if (pair.second.simd_width > 0) {
      std::cout << pair.second.simd_width << " | ";
    } else {
      std::cout << "-" << " | ";
    }
    std::cout << std::setw(12) << std::fixed << std::setprecision(2);
    if (mbytes_transfered > 0) {
      std::cout << mbytes_transfered << " | ";
    } else {
      std::cout << "-" << " | ";
    }
    std::cout << std::setw(8) << avg_time << " | ";
    std::cout << std::setw(8) << total_time << " |" << std::endl;
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

  bool set = true;
  set = set && tracer->SetTracingFunction(
      CL_FUNCTION_clCreateCommandQueueWithProperties);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clCreateCommandQueue);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueNDRangeKernel);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueReadBuffer);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueWriteBuffer);
  PTI_ASSERT(set);

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