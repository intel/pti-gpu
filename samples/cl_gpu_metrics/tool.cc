//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

#include "cl_tracer.h"
#include "cl_utils.h"

#include "metric_collector.h"

struct TimeInterval {
  uint64_t start;
  uint64_t end;
};

using KernelTimeMap = std::map< std::string, std::vector<TimeInterval> >;

const char* kLine =
  "+---------------------------------------------------"
  "---------------------------------------------------+";
const char* kHeader =
  "| Kernel                       | Call Count | "
  "Total Time, ms | EU Active, % | EU Stall, % | EU Idle, % |";

class ToolContext;
static ToolContext* context = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_gpu_metrics[.exe] <application> <args>" <<
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

class ToolContext {
 public:
  ToolContext(MetricCollector* collector, ClTracer* tracer) :
      collector_(collector), tracer_(tracer) {
    PTI_ASSERT(collector != nullptr && tracer != nullptr);
  }

  MetricCollector* GetCollector() const {
    return collector_;
  }

  ClTracer* GetTracer() const {
    return tracer_;
  }

  const KernelTimeMap& GetKernelTimeMap() const {
    return kernel_time_map_;
  }

  void AddKernelTime(const std::string& name, uint64_t start, uint64_t end) {
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(!name.empty());
    std::vector<TimeInterval>& kernel_time_list = kernel_time_map_[name];
    kernel_time_list.push_back({start, end});
  }

  void EnableMetricCollector() {
    const std::lock_guard<std::mutex> lock(lock_);
    if (call_count_ == 0) {
      PTI_ASSERT(collector_ != nullptr);
      bool enabled = collector_->Enable();
      PTI_ASSERT(enabled);
    }
    ++call_count_;
  }

  void DisableMetricCollector() {
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(call_count_ > 0);
    --call_count_;
    if (call_count_ == 0) {
      PTI_ASSERT(collector_ != nullptr);
      bool disabled = collector_->Disable();
      PTI_ASSERT(disabled);
    }
  }

 private:
  MetricCollector* collector_ = nullptr;
  uint32_t call_count_ = 0;
  ClTracer* tracer_ = nullptr;
  std::mutex lock_;
  KernelTimeMap kernel_time_map_;
};

static void CL_CALLBACK KernelEventNotify(cl_event event, cl_int event_status,
                                          void* user_data) {
  PTI_ASSERT(event_status == CL_COMPLETE);

  PTI_ASSERT(user_data != nullptr);
  cl_kernel kernel = reinterpret_cast<cl_kernel>(user_data);
  std::string name = utils::cl::GetKernelName(kernel);
  PTI_ASSERT(!name.empty());

  cl_int status = CL_SUCCESS;
  cl_ulong start = 0, end = 0;
  status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START,
                                    sizeof(cl_ulong), &start, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,
                                    sizeof(cl_ulong), &end, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clReleaseEvent(event);
  PTI_ASSERT(status == CL_SUCCESS);

  PTI_ASSERT(context != nullptr);
  context->AddKernelTime(name, start, end);
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

static void OnExitCreateCommandQueue(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  cl_command_queue* queue = reinterpret_cast<cl_command_queue*>(
      data->functionReturnValue);
  if (*queue != nullptr) {
    PTI_ASSERT(context != nullptr);
    context->EnableMetricCollector();
  }
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

  cl_queue_properties* props = reinterpret_cast<cl_queue_properties*>(
      data->correlationData[0]);
  PTI_ASSERT(props != nullptr);
  delete[] props;

  OnExitCreateCommandQueue(data);
}

static void OnExitReleaseCommandQueue(cl_callback_data* data) {
  PTI_ASSERT(data != nullptr);

  cl_int* status = reinterpret_cast<cl_int*>(data->functionReturnValue);
  if (*status == CL_SUCCESS) {
    PTI_ASSERT(context != nullptr);
    context->DisableMetricCollector();
  }
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

    status = clSetEventCallback(**(params->event), CL_COMPLETE,
                                KernelEventNotify, *(params->kernel));
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
    } else {
      OnExitCreateCommandQueue(callback_data);
    }
  } else if (function == CL_FUNCTION_clReleaseCommandQueue) {
    if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
      OnExitReleaseCommandQueue(callback_data);
    }
  } else if (function == CL_FUNCTION_clEnqueueNDRangeKernel) {
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      OnEnterEnqueueNDRangeKernel(callback_data);
    } else {
      OnExitEnqueueNDRangeKernel(callback_data);
    }
  }
}

uint64_t CalculateSampleTime(uint64_t cpu_snap_point, uint64_t gpu_snap_point,
                             uint64_t gpu_timestamp) {
  uint64_t cpu_timestamp = cpu_snap_point - (gpu_snap_point - gpu_timestamp);
#if defined(__gnu_linux__)
  cpu_timestamp = utils::ConvertClockMonotonicToRaw(cpu_timestamp);
#endif
  return cpu_timestamp;
}

static void PrintResults(const MetricCollector* collector,
                         const KernelTimeMap& kernel_time_map) {
  PTI_ASSERT(collector != nullptr);
  if (kernel_time_map.size() == 0) {
    return;
  }

  uint32_t calculated_report_size = collector->GetCalculatedReportSize();
  std::vector<md::TTypedValue_1_0> calculated_reports = collector->Calculate();
  if (calculated_reports.size() == 0) {
    return;
  }

  int eu_avtive_id = collector->GetMetricInfoId("EuActive");
  PTI_ASSERT(eu_avtive_id != -1);
  int eu_stall_id = collector->GetMetricInfoId("EuStall");
  PTI_ASSERT(eu_stall_id != -1);
  int timestamp_id = collector->GetMetricInfoId("QueryBeginTime");
  PTI_ASSERT(timestamp_id != -1);

  uint64_t cpu_snap_point = 0, gpu_snap_point = 0;
  bool success = collector->GetGpuCpuTimestamps(&gpu_snap_point, &cpu_snap_point);
  PTI_ASSERT(success);

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  for (auto kernel : kernel_time_map) {
    float active_total = 0.0f, stall_total = 0.0f;
    int sample_count = 0;

    uint64_t time_total = 0;
    int call_count = 0;
    
    std::cout << "| " << std::left << std::setw(28) << kernel.first << " | ";

    std::sort(kernel.second.begin(), kernel.second.end(),
              [](const TimeInterval& l,
                 const TimeInterval& r) {
                return l.start < r.start;
              });

    const md::TTypedValue_1_0* report = calculated_reports.data();
    for (auto time : kernel.second) {
      while (report < calculated_reports.data() + calculated_reports.size()) {
        PTI_ASSERT(report[timestamp_id].ValueType == md::VALUE_TYPE_UINT64);
        uint64_t gpu_timestamp = report[timestamp_id].ValueUInt64;
        uint64_t cpu_timestamp = CalculateSampleTime(cpu_snap_point,
                                                     gpu_snap_point,
                                                     gpu_timestamp);

        if (cpu_timestamp >= time.start && cpu_timestamp <= time.end) {
          PTI_ASSERT(report[eu_avtive_id].ValueType == md::VALUE_TYPE_FLOAT);
          active_total += report[eu_avtive_id].ValueFloat;
          PTI_ASSERT(report[eu_stall_id].ValueType == md::VALUE_TYPE_FLOAT);
          stall_total += report[eu_stall_id].ValueFloat;
          ++sample_count;
        }

        report += calculated_report_size;
      }

      time_total += (time.end - time.start);
      ++call_count;
    }

    float active = 0.0f, stall = 0.0f, idle = 100.0f;
    if (sample_count > 0) {
      active = active_total / sample_count;
      stall = stall_total / sample_count;
      idle = idle - active - stall;
      if (idle < 0.0f) idle = 0.0f;
    }

    std::cout << std::right << std::setw(10) << call_count << " | ";
    std::cout << std::right << std::setw(14) << std::setprecision(2) <<
      std::fixed << time_total / static_cast<float>(NSEC_IN_MSEC) << " | ";
    std::cout << std::right << std::setw(12) << std::setprecision(2) <<
      std::fixed << active << " | ";
    std::cout << std::right << std::setw(11) << std::setprecision(2) <<
      std::fixed << stall << " | ";
    std::cout << std::right << std::setw(10) << std::setprecision(2) <<
      std::fixed << idle << " | " << std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  PTI_ASSERT(context == nullptr);

  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cout << "[WARNING] Unable to find target GPU device for tracing" <<
      std::endl;
    return;
  }

  MetricCollector* collector = new MetricCollector("ComputeBasic");
  PTI_ASSERT(collector != nullptr);
  if (!collector->IsValid()) {
    delete collector;
    return;
  }

  ClTracer* tracer = new ClTracer(device, Callback, device);
  if (tracer == nullptr || !tracer->IsValid()) {
    std::cout << "[WARNING] Unable to create OpenCL tracer for " <<
      "target GPU device" << std::endl;
    if (tracer != nullptr) {
      delete tracer;
    }
    delete collector;
    return;
  }

  bool set = true;
  set = set && tracer->SetTracingFunction(
      CL_FUNCTION_clCreateCommandQueueWithProperties);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clCreateCommandQueue);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clReleaseCommandQueue);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueNDRangeKernel);
  PTI_ASSERT(set);

  bool enabled = tracer->Enable();
  PTI_ASSERT(enabled);

  context = new ToolContext(collector, tracer);
  PTI_ASSERT(context != nullptr);
}

void DisableProfiling() {
  if (context != nullptr) {
    MetricCollector* collector = context->GetCollector();
    PTI_ASSERT(collector != nullptr);
    ClTracer* tracer = context->GetTracer();
    PTI_ASSERT(tracer != nullptr);

    bool disabled = tracer->Disable();
    PTI_ASSERT(disabled);
    PrintResults(collector, context->GetKernelTimeMap());

    delete collector;
    delete tracer;
    delete context;
  }
}