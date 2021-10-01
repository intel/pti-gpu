//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_GPU_QUERY_CL_METRIC_COLLECTOR_H_
#define PTI_SAMPLES_CL_GPU_QUERY_CL_METRIC_COLLECTOR_H_

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "metric_device.h"
#include "trace_guard.h"

#define CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL 0x407F

cl_command_queue CL_API_CALL
clCreatePerfCountersCommandQueueINTEL(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet);

class ClMetricCollector;

struct ClEventData {
  ClMetricCollector* collector;
  cl_kernel kernel;
};

struct ClKernelInfo {
  uint64_t total_time;
  uint64_t call_count;
  float eu_active;
  float eu_stall;

  bool operator>(const ClKernelInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ClKernelInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using ClKernelInfoMap = std::map<std::string, ClKernelInfo>;

class ClMetricCollector {
 public: // Interface
  static ClMetricCollector* Create(cl_device_id device, const char* set_name) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(set_name != nullptr);

    std::string device_string = utils::GetEnv("PTI_DEVICE_ID");
    uint32_t device_id =
      device_string.empty() ? 0 : std::stoul(device_string);
    std::string sub_device_string = utils::GetEnv("PTI_SUB_DEVICE_ID");
    uint32_t sub_device_id =
      sub_device_string.empty() ? 0 : std::stoul(sub_device_string);

    MetricDevice* metric_device =
      MetricDevice::Create(device_id, sub_device_id);
    if (metric_device == nullptr) {
      std::cerr << "[WARNING] Unable to find MD library" << std::endl;
      return nullptr;
    }

    md::IConcurrentGroup_1_5* group =
      metric_device->FindMetricGroup(set_name);
    md::IMetricSet_1_5* set = metric_device->FindMetricSet(set_name);
    if (group == nullptr || set == nullptr) {
      std::cerr << "[WARNING] Metric set is not found: " <<
        set_name << std::endl;
      delete metric_device;
      return nullptr;
    }

    cl_platform_id platform = nullptr;
    cl_int status = clGetDeviceInfo(
        device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id),
        &platform, nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    decltype(clCreatePerfCountersCommandQueueINTEL)* ptr =
      reinterpret_cast<decltype(clCreatePerfCountersCommandQueueINTEL)*>(
        clGetExtensionFunctionAddressForPlatform(
          platform, "clCreatePerfCountersCommandQueueINTEL"));
    if (ptr == nullptr) {
      std::cerr << "[WARNING] Unable to find OpenCL metric query extension" <<
        std::endl;
      delete metric_device;
      return nullptr;
    }

    ClMetricCollector* collector =
      new ClMetricCollector(metric_device, group, set, ptr);
    PTI_ASSERT(collector != nullptr);

    ClApiTracer* tracer = new ClApiTracer(device, Callback, collector);
    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer " <<
        "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
      }
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ClMetricCollector() {
    if (tracer_ != nullptr) {
      delete tracer_;
    }

    md::TCompletionCode status = set_->Deactivate();
    PTI_ASSERT(status == md::CC_OK);

    PTI_ASSERT(device_ != nullptr);
    delete device_;
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    bool disabled = tracer_->Disable();
    PTI_ASSERT(disabled);
  }

  const ClKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  ClMetricCollector(const ClMetricCollector& copy) = delete;
  ClMetricCollector& operator=(const ClMetricCollector& copy) = delete;

  static void PrintKernelsTable(const ClKernelInfoMap& kernel_info_map) {
    std::set<
        std::pair<std::string, ClKernelInfo>,
        utils::Comparator> sorted_list(
            kernel_info_map.begin(), kernel_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::cerr << std::setw(max_name_length) << "Kernel" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kPercentLength) << "EU Active (%)" << "," <<
      std::setw(kPercentLength) << "EU Stall (%)" << "," <<
      std::setw(kPercentLength) << "EU Idle (%)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      float percent_duration = 100.0f * duration / total_duration;
      float eu_active = value.second.eu_active;
      float eu_stall = value.second.eu_stall;
      float eu_idle = (std::max)(0.0f, 100.0f - eu_stall - eu_active);
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << eu_active << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << eu_stall << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << eu_idle << std::endl;
    }
  }

 private: // Implementation Details
  ClMetricCollector(
      MetricDevice* device,
      md::IConcurrentGroup_1_5* group,
      md::IMetricSet_1_5* set,
      decltype(clCreatePerfCountersCommandQueueINTEL)* ptr)
          : device_(device), group_(group), set_(set),
            clCreatePerfCountersCommandQueue_(ptr) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(group_ != nullptr);
    PTI_ASSERT(set_ != nullptr);
    PTI_ASSERT(clCreatePerfCountersCommandQueue_ != nullptr);

    md::TCompletionCode status = set_->SetApiFiltering(
        md::API_TYPE_OCL | md::API_TYPE_OGL4_X);
    PTI_ASSERT(status == md::CC_OK);
    status = set_->Activate();
    PTI_ASSERT(status == md::CC_OK);
  }

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    bool set = true;
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueueWithProperties);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clCreateCommandQueue);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueNDRangeKernel);
    PTI_ASSERT(set);

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernelInfo(
      std::string name, uint64_t time, float eu_active, float eu_stall) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(time > 0);

    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {time, 1, eu_active, eu_stall};
    } else {
      ClKernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      kernel.eu_active =
        (kernel.eu_active * kernel.call_count + eu_active) /
        (kernel.call_count + 1);
      kernel.eu_stall =
        (kernel.eu_stall * kernel.call_count + eu_stall) /
        (kernel.call_count + 1);
      kernel.call_count += 1;
    }
  }

  cl_command_queue CreateMetricCommandQueue(
      cl_context context,
      cl_device_id device) {
    PTI_ASSERT(context != nullptr);
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(set_ != nullptr);

    cl_int status = CL_SUCCESS;
    cl_command_queue queue = clCreatePerfCountersCommandQueue_(
        context, device, CL_QUEUE_PROFILING_ENABLE,
        set_->GetParams()->ApiSpecificId.OCL, &status);
    PTI_ASSERT(status == CL_SUCCESS);
    return queue;
  }

  size_t GetQueryReportSize() const {
    PTI_ASSERT(set_ != nullptr);
    return set_->GetParams()->QueryReportSize;
  }

  std::vector<md::TTypedValue_1_0> CalculateMetrics(
      const std::vector<uint8_t>& report) {
    PTI_ASSERT(!report.empty());
    PTI_ASSERT(set_ != nullptr);

    size_t metric_count = set_->GetParams()->MetricsCount +
      set_->GetParams()->InformationCount;
    PTI_ASSERT(metric_count > 0);

    std::vector<md::TTypedValue_1_0> metric_list(metric_count);
    uint32_t calculated_report_count = 0;

    PTI_ASSERT(
        report.size() < (std::numeric_limits<uint32_t>::max)());
    PTI_ASSERT(
        metric_list.size() * sizeof(md::TTypedValue_1_0) <
        (std::numeric_limits<uint32_t>::max)());
    md::TCompletionCode status = set_->CalculateMetrics(
        report.data(),
        static_cast<uint32_t>(report.size()),
        metric_list.data(),
        static_cast<uint32_t>(
            metric_list.size() * sizeof(md::TTypedValue_1_0)),
        &calculated_report_count, nullptr, 0);
    PTI_ASSERT(status == md::CC_OK);
    PTI_ASSERT(calculated_report_count == 1);

    return metric_list;
  }

  int GetMetricId(const char* name) const {
    PTI_ASSERT(name != nullptr);
    PTI_ASSERT(set_ != nullptr);

    for (uint32_t mid = 0; mid < set_->GetParams()->MetricsCount; ++mid) {
      md::IMetric_1_0* metric = set_->GetMetric(mid);
      PTI_ASSERT(metric != nullptr);
      if (strcmp(metric->GetParams()->SymbolName, name) == 0) {
        return mid;
      }
    }

    for (uint32_t iid = 0; iid < set_->GetParams()->InformationCount; ++iid) {
      md::IInformation_1_0* info = set_->GetInformation(iid);
      PTI_ASSERT(info != nullptr);
      if (strcmp(info->GetParams()->SymbolName, name) == 0) {
        return iid + set_->GetParams()->MetricsCount;
      }
    }

    return -1;
  }

 private: // Callbacks
  static void CL_CALLBACK EventNotify(
      cl_event event, cl_int event_status, void* user_data) {
    PTI_ASSERT(event_status == CL_COMPLETE);

    PTI_ASSERT(user_data != nullptr);
    ClEventData* event_data = reinterpret_cast<ClEventData*>(user_data);

    ClMetricCollector* collector = event_data->collector;
    PTI_ASSERT(collector != nullptr);

    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);

    std::string name = utils::cl::GetKernelName(event_data->kernel);
    PTI_ASSERT(!name.empty());

    size_t report_size = collector->GetQueryReportSize();
    PTI_ASSERT(report_size > 0);

    std::vector<uint8_t> report(report_size, 0);
    size_t output_size = 0;
    cl_int status = clGetEventProfilingInfo(
        event, CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL,
        report_size, report.data(), &output_size);
    PTI_ASSERT(status == CL_SUCCESS);
    report.resize(output_size);

    cl_ulong started =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_START);
    cl_ulong ended =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_END);
    cl_ulong time = ended - started;
    PTI_ASSERT(time > 0);

    auto metric_list = collector->CalculateMetrics(report);
    PTI_ASSERT(!metric_list.empty());

    int eu_active_id = collector->GetMetricId("EuActive");
    PTI_ASSERT(eu_active_id != -1);
    int eu_stall_id = collector->GetMetricId("EuStall");
    PTI_ASSERT(eu_stall_id != -1);

    PTI_ASSERT(metric_list[eu_active_id].ValueType == md::VALUE_TYPE_FLOAT);
    float eu_active = metric_list[eu_active_id].ValueFloat;
    PTI_ASSERT(metric_list[eu_stall_id].ValueType == md::VALUE_TYPE_FLOAT);
    float eu_stall = metric_list[eu_stall_id].ValueFloat;

    status = clReleaseKernel(event_data->kernel);
    PTI_ASSERT(status == CL_SUCCESS);

    collector->AddKernelInfo(name, time, eu_active, eu_stall);

    status = clReleaseEvent(event);
    PTI_ASSERT(status == CL_SUCCESS);

    delete event_data;
  }

  static void OnExitCreateCommandQueueWithProperties(
      cl_callback_data* data, ClMetricCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    const cl_params_clCreateCommandQueueWithProperties* params =
      reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (**(params->errcodeRet) == CL_SUCCESS) {
      cl_command_queue* queue =
        reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
      PTI_ASSERT(queue != nullptr);

      cl_int status = clReleaseCommandQueue(*queue);
      PTI_ASSERT(status == CL_SUCCESS);

      *queue = collector->CreateMetricCommandQueue(
          *(params->context), *(params->device));
      PTI_ASSERT(*queue != nullptr);
    }
  }

  static void OnExitCreateCommandQueue(
      cl_callback_data* data, ClMetricCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    const cl_params_clCreateCommandQueue* params =
      reinterpret_cast<const cl_params_clCreateCommandQueue*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (**(params->errcodeRet) == CL_SUCCESS) {
      cl_command_queue* queue =
        reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
      PTI_ASSERT(queue != nullptr);

      cl_int status = clReleaseCommandQueue(*queue);
      PTI_ASSERT(status == CL_SUCCESS);

      *queue = collector->CreateMetricCommandQueue(
          *(params->context), *(params->device));
      PTI_ASSERT(*queue != nullptr);
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

  static void OnExitEnqueueNDRangeKernel(
      cl_callback_data* data, ClMetricCollector* collector) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clEnqueueNDRangeKernel* params =
      reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    cl_int* return_value =
      reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      PTI_ASSERT(*(params->event) != nullptr);
      cl_int status = CL_SUCCESS;

      if (*(params->event) !=
          reinterpret_cast<cl_event*>(data->correlationData)) {
        status = clRetainEvent(**(params->event));
        PTI_ASSERT(status == CL_SUCCESS);
      }

      ClEventData* event_data = new ClEventData;
      PTI_ASSERT(event_data != nullptr);
      cl_kernel kernel = *(params->kernel);
      event_data->collector = collector;
      event_data->kernel = kernel;

      status = clRetainKernel(kernel);
      PTI_ASSERT(status == CL_SUCCESS);
      status = clSetEventCallback(
          **(params->event), CL_COMPLETE, EventNotify, event_data);
      PTI_ASSERT(status == CL_SUCCESS);
    }
  }

  static void Callback(
      cl_function_id function,
      cl_callback_data* callback_data,
      void* user_data) {
    if (TraceGuard::Inactive() > 0) return;
    TraceGuard guard;

    ClMetricCollector* collector =
      reinterpret_cast<ClMetricCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);

    if (function == CL_FUNCTION_clCreateCommandQueueWithProperties) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitCreateCommandQueueWithProperties(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clCreateCommandQueue) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitCreateCommandQueue(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueNDRangeKernel) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueNDRangeKernel(callback_data);
      } else {
        OnExitEnqueueNDRangeKernel(callback_data, collector);
      }
    }
  }

 private: // Data
  ClApiTracer* tracer_ = nullptr;

  std::mutex lock_;
  ClKernelInfoMap kernel_info_map_;

  MetricDevice* device_ = nullptr;
  md::IConcurrentGroup_1_5* group_ = nullptr;
  md::IMetricSet_1_5* set_ = nullptr;

  decltype(clCreatePerfCountersCommandQueueINTEL)*
    clCreatePerfCountersCommandQueue_ = nullptr;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 16;
};

#endif // PTI_SAMPLES_CL_GPU_QUERY_CL_METRIC_COLLECTOR_H_