//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_CL_KERNEL_COLLECTOR_H_
#define PTI_TOOLS_CL_TRACER_CL_KERNEL_COLLECTOR_H_

#include <atomic>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "correlator.h"
#include "trace_guard.h"

#ifdef PTI_KERNEL_INTERVALS
#include "prof_utils.h"
#endif /* PTI_KERNEL_INTERVALS */

class ClKernelCollector;

enum ClKernelType {
  KERNEL_TYPE_USER,
  KERNEL_TYPE_TRANSFER
};

struct ClEnqueueData {
  cl_event event;
  cl_ulong host_sync;
  cl_ulong device_sync;
};

struct ClKernelProps {
  std::string name;
  size_t simd_width;
  size_t bytes_transferred;
  size_t global_size[3];
  size_t local_size[3];
};

struct ClKernelInstance {
  cl_event event = nullptr;
  ClKernelProps props;
  uint64_t kernel_id = 0;
  cl_ulong host_sync = 0;
  cl_ulong device_sync = 0;
  bool need_to_process = true;
};

struct ClKernelInfo {
  uint64_t queued_time;
  uint64_t submit_time;
  uint64_t execute_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ClKernelInfo& r) const {
    if (execute_time != r.execute_time) {
      return execute_time > r.execute_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ClKernelInfo& r) const {
    if (execute_time == r.execute_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using ClKernelInfoMap = std::map<std::string, ClKernelInfo>;
using ClKernelInstanceList = std::list<ClKernelInstance*>;

#ifdef PTI_KERNEL_INTERVALS

struct ClDeviceInterval {
  uint64_t start;
  uint64_t end;
  uint32_t sub_device_id;
};

struct ClKernelInterval {
  std::string kernel_name;
  cl_device_id device;
  std::vector<ClDeviceInterval> device_interval_list;
};

using ClKernelIntervalList = std::vector<ClKernelInterval>;
using ClDeviceMap = std::map<
    cl_device_id, std::vector<cl_device_id> >;

#endif // PTI_KERNEL_INTERVALS

typedef void (*OnClKernelFinishCallback)(
    void* data,
    const std::string& queue,
    const std::string& id,
    const std::string& name,
    uint64_t queued,
    uint64_t submitted,
    uint64_t started,
    uint64_t ended);

class ClKernelCollector {
 public: // Interface
  static ClKernelCollector* Create(
      cl_device_id device,
      Correlator* correlator,
      KernelCollectorOptions options,
      OnClKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(correlator != nullptr);
    TraceGuard guard;

    ClKernelCollector* collector = new ClKernelCollector(
        device, correlator, options, callback, callback_data);
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

  ~ClKernelCollector() {
#ifdef PTI_KERNEL_INTERVALS
    ReleaseDeviceMap();
#endif // PTI_KERNEL_INTERVALS
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    bool disabled = tracer_->Disable();
    PTI_ASSERT(disabled);
  }

  const ClKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

#ifdef PTI_KERNEL_INTERVALS
  const ClKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }
#endif // PTI_KERNEL_INTERVALS

  ClKernelCollector(const ClKernelCollector& copy) = delete;
  ClKernelCollector& operator=(const ClKernelCollector& copy) = delete;

  void PrintKernelsTable() const {
    std::set< std::pair<std::string, ClKernelInfo>,
              utils::Comparator > sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.execute_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Kernel" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.execute_time;
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

  void PrintSubmissionTable() const {
    std::set< std::pair<std::string, ClKernelInfo>,
              utils::Comparator > sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

    uint64_t total_queued_duration = 0;
    uint64_t total_submit_duration = 0;
    uint64_t total_execute_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_queued_duration += value.second.queued_time;
      total_submit_duration += value.second.submit_time;
      total_execute_duration += value.second.execute_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_execute_duration == 0) {
      return;
    }

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Kernel" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kTimeLength) << "Queued (ns)" << "," <<
      std::setw(kPercentLength) << "Queued (%)" << "," <<
      std::setw(kTimeLength) << "Submit (ns)" << "," <<
      std::setw(kPercentLength) << "Submit (%)" << "," <<
      std::setw(kTimeLength) << "Execute (ns)" << "," <<
      std::setw(kPercentLength) << "Execute (%)" << "," << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t queued_duration = value.second.queued_time;
      float queued_percent =
        100.0f * queued_duration / total_queued_duration;
      uint64_t submit_duration = value.second.submit_time;
      float submit_percent =
        100.0f * submit_duration / total_submit_duration;
      uint64_t execute_duration = value.second.execute_time;
      float execute_percent =
        100.0f * execute_duration / total_execute_duration;
      stream << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTimeLength) << queued_duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << queued_percent << "," <<
        std::setw(kTimeLength) << submit_duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << submit_percent << "," <<
        std::setw(kTimeLength) << execute_duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << execute_percent << "," << std::endl;
    }

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->Log(stream.str());
  }

 private: // Implementation Details
  ClKernelCollector(
      cl_device_id device,
      Correlator* correlator,
      KernelCollectorOptions options,
      OnClKernelFinishCallback callback,
      void* callback_data)
      : device_(device),
        correlator_(correlator),
        options_(options),
        callback_(callback),
        callback_data_(callback_data),
        kernel_id_(1) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(correlator_ != nullptr);
#ifdef PTI_KERNEL_INTERVALS
    ze_device_ = GetZeDevice(device_);
    PTI_ASSERT(ze_device_ != nullptr);
    timer_mask_ = utils::ze::GetMetricTimestampMask(ze_device_);
    timer_freq_ = utils::ze::GetMetricTimerFrequency(ze_device_);
    CreateDeviceMap();
#endif // PTI_KERNEL_INTERVALS
  }

#ifdef PTI_KERNEL_INTERVALS
  void CreateDeviceMap() {
    cl_device_type type = utils::cl::GetDeviceType(device_);
    PTI_ASSERT(type == CL_DEVICE_TYPE_GPU);

    std::vector<cl_device_id> device_list = utils::cl::GetDeviceList(type);
    for (auto device : device_list) {
      std::vector<cl_device_id> sub_device_list =
        utils::cl::CreateSubDeviceList(device);
      PTI_ASSERT(device_map_.count(device) == 0);
      device_map_[device] = sub_device_list;
    }
  }

  void ReleaseDeviceMap() {
    for (auto it : device_map_) {
      if (!it.second.empty()) {
        utils::cl::ReleaseSubDeviceList(it.second);
      }
    }
  }
#endif // PTI_KERNEL_INTERVALS

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    bool set = true;
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueueWithProperties);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueue);
    PTI_ASSERT(set);

    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueNDRangeKernel);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueTask);
    PTI_ASSERT(set);

    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueReadBuffer);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueWriteBuffer);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueReadBufferRect);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueWriteBufferRect);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueCopyBuffer);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueCopyBufferRect);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueFillBuffer);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueReadImage);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueWriteImage);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueCopyImage);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueFillImage);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueCopyImageToBuffer);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueCopyBufferToImage);
    PTI_ASSERT(set);

    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clFinish);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clReleaseCommandQueue);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clReleaseEvent);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clWaitForEvents);
    PTI_ASSERT(set);

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernelInstance(ClKernelInstance* instance) {
    PTI_ASSERT(instance != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_instance_list_.push_back(instance);
  }

  static void ComputeHostTimestamps(
      const ClKernelInstance* instance,
      cl_ulong started,
      cl_ulong ended,
      uint64_t& host_queued,
      uint64_t& host_submitted,
      uint64_t& host_started,
      uint64_t& host_ended) {
    PTI_ASSERT(instance != nullptr);
    PTI_ASSERT(started < ended);

    PTI_ASSERT(instance->event != nullptr);
    cl_event event = instance->event;

    cl_ulong queued =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_QUEUED);
    PTI_ASSERT(queued > 0);
    cl_ulong submitted =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_SUBMIT);
    PTI_ASSERT(submitted > 0);

    PTI_ASSERT(instance->device_sync <= queued);
    uint64_t time_shift = queued - instance->device_sync;

    host_queued = instance->host_sync + time_shift;
    PTI_ASSERT(queued <= submitted);
    host_submitted = host_queued + (submitted - queued);
    PTI_ASSERT(submitted <= started);
    host_started = host_submitted + (started - submitted);
    PTI_ASSERT(started <= ended);
    host_ended = host_started + (ended - started);
  }

  void ProcessKernelInstance(const ClKernelInstance* instance) {
    PTI_ASSERT(instance != nullptr);

    PTI_ASSERT(instance->event != nullptr);
    cl_event event = instance->event;

    cl_int event_status = utils::cl::GetEventStatus(event);
    PTI_ASSERT(event_status == CL_COMPLETE);

    if (instance->need_to_process) {
      cl_command_queue queue = utils::cl::GetCommandQueue(event);
      PTI_ASSERT(queue != nullptr);

      cl_ulong started =
        utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_START);
      cl_ulong ended =
        utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_END);
      cl_ulong time = ended - started;
      PTI_ASSERT(time > 0);

#ifdef PTI_KERNEL_INTERVALS
      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);
      AddKernelInterval(instance, device, started, ended);
#else // PTI_KERNEL_INTERVALS
      std::string name = instance->props.name;
      PTI_ASSERT(!name.empty());

      if (options_.verbose) {
        name = GetVerboseName(&(instance->props));
      }

      uint64_t host_queued = 0, host_submitted = 0;
      uint64_t host_started = 0, host_ended = 0;
      ComputeHostTimestamps(
          instance,
          started, ended,
          host_queued, host_submitted,
          host_started, host_ended);

      AddKernelInfo(
        name,
        host_submitted - host_queued,
        host_started - host_submitted,
        host_ended - host_started);

      if (callback_ != nullptr) {
        std::stringstream stream;
        stream << std::hex << queue;

        callback_(
            callback_data_, stream.str(),
            std::to_string(instance->kernel_id), name,
            host_queued, host_submitted,
            host_started, host_ended);
      }
#endif // PTI_KERNEL_INTERVALS
    }

    cl_int status = clReleaseEvent(event);
    PTI_ASSERT(status == CL_SUCCESS);

    delete instance;
  }

  void ProcessKernelInstance(cl_event event) {
    PTI_ASSERT(event != nullptr);
    cl_int event_status = utils::cl::GetEventStatus(event);
    if (event_status != CL_COMPLETE) {
      return;
    }

    const std::lock_guard<std::mutex> lock(lock_);
    for (auto it = kernel_instance_list_.begin();
         it != kernel_instance_list_.end(); ++it) {
      ClKernelInstance* instance = *it;
      PTI_ASSERT(instance != nullptr);
      PTI_ASSERT(instance->event != nullptr);
      if (instance->event == event) {
        ProcessKernelInstance(instance);
        it = kernel_instance_list_.erase(it);
        break;
      }
    }
  }

  void ProcessKernelInstances() {
    const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_instance_list_.begin();
    while (it != kernel_instance_list_.end()) {
      ClKernelInstance* instance = *it;
      PTI_ASSERT(instance != nullptr);

      PTI_ASSERT(instance->event != nullptr);
      cl_int event_status = utils::cl::GetEventStatus(instance->event);
      if (event_status == CL_COMPLETE) {
        ProcessKernelInstance(instance);
        it = kernel_instance_list_.erase(it);
      } else {
        ++it;
      }
    }
  }

  std::string GetVerboseName(const ClKernelProps* props) {
    PTI_ASSERT(props != nullptr);
    PTI_ASSERT(!props->name.empty());

    std::stringstream sstream;
    sstream << props->name;
    if (props->simd_width > 0) {
      sstream << "[SIMD";
      if (props->simd_width == 1) {
        sstream << "_ANY";
      } else {
        sstream << props->simd_width;
      }
      sstream << " {" <<
        props->global_size[0] << "; " <<
        props->global_size[1] << "; " <<
        props->global_size[2] << "} {" <<
        props->local_size[0] << "; " <<
        props->local_size[1] << "; " <<
        props->local_size[2] << "}]";
    } else if (props->bytes_transferred > 0) {
      sstream << props->name << "[" <<
        std::to_string(props->bytes_transferred) << " bytes]";
    }

    return sstream.str();
  }

  void AddKernelInfo(
      std::string name, uint64_t queued_time,
      uint64_t submit_time, uint64_t execute_time) {
    PTI_ASSERT(!name.empty());

    if (kernel_info_map_.count(name) == 0) {
      ClKernelInfo info;
      info.queued_time = queued_time;
      info.submit_time = submit_time;
      info.execute_time = execute_time;
      info.min_time = execute_time;
      info.max_time = execute_time;
      info.call_count = 1;
      kernel_info_map_[name] = info;
    } else {
      ClKernelInfo& kernel = kernel_info_map_[name];
      kernel.queued_time += queued_time;
      kernel.submit_time += submit_time;
      kernel.execute_time += execute_time;
      if (execute_time > kernel.max_time) {
        kernel.max_time = execute_time;
      }
      if (execute_time < kernel.min_time) {
        kernel.min_time = execute_time;
      }
      kernel.call_count += 1;
    }
  }

#ifdef PTI_KERNEL_INTERVALS
  void AddKernelInterval(
      const ClKernelInstance* instance,
      cl_device_id device,
      uint64_t started, uint64_t ended) {
    PTI_ASSERT(instance != nullptr);
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(started < ended);

    cl_ulong cl_host_timestamp = 0;
    cl_ulong cl_device_timestamp = 0;
    utils::cl::GetTimestamps(device, &cl_host_timestamp, &cl_device_timestamp);

    uint64_t ze_host_timestamp;
    uint64_t ze_device_timestamp;
    ze_device_handle_t ze_device;

    uint64_t mask;
    uint64_t freq;
    if (device = device_) {
      ze_device = ze_device_;
      mask = timer_mask_;
      freq = timer_freq_;
    }
    else {
      ze_device = GetZeDevice(device);
      PTI_ASSERT(ze_device != nullptr);
      mask = utils::ze::GetMetricTimestampMask(ze_device);
      freq = utils::ze::GetMetricTimerFrequency(ze_device);
    }

    zeDeviceGetGlobalTimestamps(ze_device, &ze_host_timestamp, &ze_device_timestamp);
    ze_device_timestamp = ze_device_timestamp & mask;

    cl_ulong elapsed;

    elapsed = cl_device_timestamp - started;
    elapsed += (ze_host_timestamp - cl_host_timestamp);

    uint64_t ze_started;
    uint64_t ze_ended;

    uint64_t ns_per_cycle;
    ns_per_cycle = static_cast<uint64_t>(NSEC_IN_SEC) / freq;

    ze_started = (ze_device_timestamp - (elapsed / ns_per_cycle)) & mask;
    ze_ended = (ze_started + ((ended - started) / ns_per_cycle)) & mask;;

    ze_started = ze_started * ns_per_cycle;
    ze_ended = ze_ended * ns_per_cycle;

    if (ze_ended < ze_started) {
      ze_ended += ((mask + 1)* ns_per_cycle);
    }

#if 0
    uint64_t host_queued = 0, host_submitted = 0;
    uint64_t host_started = 0, host_ended = 0;
    ComputeHostTimestamps(
        instance,
        started, ended,
        host_queued, host_submitted,
        host_started, host_ended);
#endif /* 0 */

    std::string name = instance->props.name;
    PTI_ASSERT(!name.empty());

    if (options_.verbose) {
      name = GetVerboseName(&instance->props);
    }

    if (device_map_.count(device) == 1 &&
        !device_map_[device].empty()) { // Implicit Scaling
      ClKernelInterval kernel_interval{
          name, device, std::vector<ClDeviceInterval>()};
      std::vector<cl_device_id> sub_device_list = device_map_[device];
      for (size_t i = 0; i < sub_device_list.size(); ++i) {
        kernel_interval.device_interval_list.push_back(
            {ze_started, ze_ended, static_cast<uint32_t>(i)});
      }
      kernel_interval_list_.push_back(kernel_interval);
    } else { // Explicit Scaling
      if (device_map_.count(device) == 0) { // Subdevice
        cl_device_id parent = utils::cl::GetDeviceParent(device);
        PTI_ASSERT(parent != nullptr);

        PTI_ASSERT(device_map_.count(parent) == 1);
        std::vector<cl_device_id> sub_device_list = device_map_[parent];
        PTI_ASSERT(!sub_device_list.empty());

        for (size_t i = 0; i < sub_device_list.size(); ++i) {
          if (sub_device_list[i] == device) {
            ClKernelInterval kernel_interval{
                name, parent, std::vector<ClDeviceInterval>()};
            kernel_interval.device_interval_list.push_back(
                {ze_started, ze_ended, static_cast<uint32_t>(i)});
            kernel_interval_list_.push_back(kernel_interval);
            return;
          }

          PTI_ASSERT(0);
        }
      } else { // Device with no subdevices
        PTI_ASSERT(device_map_[device].empty());
        ClKernelInterval kernel_interval{
            name, device, std::vector<ClDeviceInterval>()};
        kernel_interval.device_interval_list.push_back(
            {ze_started, ze_ended, 0});
        kernel_interval_list_.push_back(kernel_interval);
      }
    }
  }
#endif // PTI_KERNEL_INTERVALS

  void CalculateKernelLocalSize(
      const cl_params_clEnqueueNDRangeKernel* params,
      ClKernelProps* props) {
    PTI_ASSERT(params != nullptr);
    PTI_ASSERT(props != nullptr);

    props->local_size[0] = 1;
    props->local_size[1] = 1;
    props->local_size[2] = 1;
    if (*(params->localWorkSize) == nullptr) {
      props->local_size[0] = 0;
      props->local_size[1] = 0;
      props->local_size[2] = 0;
    } else {
      PTI_ASSERT(*(params->workDim) <= 3);
      for (cl_uint i = 0; i < *(params->workDim); ++i) {
        props->local_size[i] = (*(params->localWorkSize))[i];
      }
    }
  }

  void CalculateKernelLocalSize(
      const cl_params_clEnqueueTask* params,
      ClKernelProps* props) {
    PTI_ASSERT(props != nullptr);
    props->local_size[0] = 1;
    props->local_size[1] = 1;
    props->local_size[2] = 1;
  }

  void CalculateKernelGlobalSize(
      const cl_params_clEnqueueNDRangeKernel* params,
      ClKernelProps* props) {
    PTI_ASSERT(params != nullptr);
    PTI_ASSERT(props != nullptr);
    props->global_size[0] = 1;
    props->global_size[1] = 1;
    props->global_size[2] = 1;

    PTI_ASSERT(*(params->workDim) <= 3);
    for (cl_uint i = 0; i < *(params->workDim); ++i) {
      props->global_size[i] = (*(params->globalWorkSize))[i];
    }
  }

  void CalculateKernelGlobalSize(
      const cl_params_clEnqueueTask* params,
      ClKernelProps* props) {
    PTI_ASSERT(props != nullptr);
    props->global_size[0] = 1;
    props->global_size[1] = 1;
    props->global_size[2] = 1;
  }

 private: // Callbacks
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

  template <typename T>
  static void OnEnterEnqueueKernel(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(collector->device_ != nullptr);

    ClEnqueueData* enqueue_data = new ClEnqueueData;
    enqueue_data->event = nullptr;

    cl_ulong host_timestamp = 0;
    utils::cl::GetTimestamps(
        collector->device_, &enqueue_data->host_sync, &enqueue_data->device_sync);
    PTI_ASSERT(collector->correlator_ != nullptr);
    enqueue_data->host_sync =
      collector->correlator_->GetTimestamp(enqueue_data->host_sync);

    const T* params = reinterpret_cast<const T*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) == nullptr) {
      *(params->event) = &(enqueue_data->event);
    }

    data->correlationData[0] = reinterpret_cast<cl_ulong>(enqueue_data);
  }

  template<typename T>
  static void OnExitEnqueueKernel(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value =
      reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const T* params = reinterpret_cast<const T*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      PTI_ASSERT(*(params->event) != nullptr);
      cl_int status = CL_SUCCESS;

      if (*(params->event) !=
          reinterpret_cast<cl_event*>(data->correlationData)) {
        status = clRetainEvent(**(params->event));
        PTI_ASSERT(status == CL_SUCCESS);
      }

      ClKernelInstance* instance = new ClKernelInstance;
      PTI_ASSERT(instance != nullptr);
      instance->event = **(params->event);

      cl_kernel kernel = *(params->kernel);
      instance->props.name = utils::cl::GetKernelName(
          kernel, collector->options_.demangle);

      cl_command_queue queue = *(params->commandQueue);
      PTI_ASSERT(queue != nullptr);
      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);

      size_t simd_width = utils::cl::GetKernelSimdWidth(device, kernel);
      PTI_ASSERT(simd_width > 0);

      instance->props.simd_width = simd_width;
      instance->props.bytes_transferred = 0;

      collector->CalculateKernelGlobalSize(params, &instance->props);
      collector->CalculateKernelLocalSize(params, &instance->props);

      instance->kernel_id =
        collector->kernel_id_.fetch_add(
            1, std::memory_order::memory_order_relaxed);
      PTI_ASSERT(collector->correlator_ != nullptr);
      collector->correlator_->SetKernelId(instance->kernel_id);
      instance->need_to_process =
        collector->correlator_->IsCollectionEnabled();

      ClEnqueueData* enqueue_data =
        reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
      PTI_ASSERT(enqueue_data != nullptr);
      instance->device_sync = enqueue_data->device_sync;
      instance->host_sync = enqueue_data->host_sync;

      collector->AddKernelInstance(instance);

      delete enqueue_data;
    }
  }

  static void OnExitEnqueueTransfer(
      std::string name, size_t bytes_transferred, cl_event* event,
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(event != nullptr);
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    if (event != reinterpret_cast<cl_event*>(data->correlationData)) {
      cl_int status = clRetainEvent(*event);
      PTI_ASSERT(status == CL_SUCCESS);
    }

    ClKernelInstance* instance = new ClKernelInstance;
    PTI_ASSERT(instance != nullptr);
    instance->event = *event;
    instance->props.name = name;

    instance->props.simd_width = 0;
    instance->props.bytes_transferred = bytes_transferred;

    instance->kernel_id =
      collector->kernel_id_.fetch_add(
          1, std::memory_order::memory_order_relaxed);
    PTI_ASSERT(collector->correlator_ != nullptr);
    collector->correlator_->SetKernelId(instance->kernel_id);
    instance->need_to_process =
        collector->correlator_->IsCollectionEnabled();

    ClEnqueueData* enqueue_data =
      reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
    PTI_ASSERT(enqueue_data != nullptr);
    instance->device_sync = enqueue_data->device_sync;
    instance->host_sync = enqueue_data->host_sync;

    collector->AddKernelInstance(instance);

    delete enqueue_data;
  }

  static void OnExitEnqueueReadBuffer(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer(
          "clEnqueueReadBuffer", *(params->cb),
          *(params->event), data, collector);

      if (*params->blockingRead) {
        collector->ProcessKernelInstances();
      }
    }
  }

  static void OnExitEnqueueWriteBuffer(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer(
          "clEnqueueWriteBuffer", *(params->cb),
          *(params->event), data, collector);

      if (*params->blockingWrite) {
        collector->ProcessKernelInstances();
      }
    }
  }

  static void OnExitEnqueueCopyBuffer(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueCopyBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer(
          "clEnqueueCopyBuffer", *(params->cb),
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueFillBuffer(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueFillBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer(
          "clEnqueueFillBuffer", *(params->size),
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueReadBufferRect(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadBufferRect* params =
        reinterpret_cast<const cl_params_clEnqueueReadBufferRect*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer(
          "clEnqueueReadBufferRect", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueWriteBufferRect(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteBufferRect* params =
        reinterpret_cast<const cl_params_clEnqueueWriteBufferRect*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer(
          "clEnqueueWriteBufferRect", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueCopyBufferRect(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferRect* params =
        reinterpret_cast<const cl_params_clEnqueueCopyBufferRect*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer(
          "clEnqueueCopyBufferRect", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueReadImage(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadImage* params =
        reinterpret_cast<const cl_params_clEnqueueReadImage*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->image), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer(
          "clEnqueueReadImage", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueWriteImage(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteImage* params =
        reinterpret_cast<const cl_params_clEnqueueWriteImage*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->image), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer(
          "clEnqueueWriteImage", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueCopyImage(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImage* params =
        reinterpret_cast<const cl_params_clEnqueueCopyImage*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->srcImage), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyImage", bytes_transferred,
                            *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueFillImage(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillImage* params =
        reinterpret_cast<const cl_params_clEnqueueFillImage*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->image), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer(
          "clEnqueueFillImage", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueCopyImageToBuffer(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImageToBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueCopyImageToBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->srcImage), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer(
          "clEnqueueCopyImageToBuffer", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitEnqueueCopyBufferToImage(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferToImage* params =
        reinterpret_cast<const cl_params_clEnqueueCopyBufferToImage*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(
          *(params->dstImage), CL_IMAGE_ELEMENT_SIZE,
          sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred =
        region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer(
          "clEnqueueCopyBufferToImage", bytes_transferred,
          *(params->event), data, collector);
    }
  }

  static void OnExitFinish(ClKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    collector->ProcessKernelInstances();
  }

  static void OnExitReleaseCommandQueue(ClKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    collector->ProcessKernelInstances();
  }

  static void OnEnterReleaseEvent(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    const cl_params_clReleaseEvent* params =
      reinterpret_cast<const cl_params_clReleaseEvent*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) != nullptr) {
      collector->ProcessKernelInstance(*(params->event));
    }
  }

  static void OnExitWaitForEvents(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clWaitForEvents* params =
        reinterpret_cast<const cl_params_clWaitForEvents*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      const cl_event* event_list = *(params->eventList);
      if (event_list != nullptr) {
        for (cl_uint i = 0; i < *(params->numEvents); ++i) {
          collector->ProcessKernelInstance(event_list[i]);
        }
      }
    }
  }

  static void Callback(cl_function_id function,
                      cl_callback_data* callback_data,
                      void* user_data) {
    if (TraceGuard::Inactive()) return;
    TraceGuard guard;

    ClKernelCollector* collector =
      reinterpret_cast<ClKernelCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);

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
        OnEnterEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(
            callback_data, collector);
      } else {
        OnExitEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(
            callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueTask) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueTask>(
            callback_data, collector);
      } else {
        OnExitEnqueueKernel<cl_params_clEnqueueTask>(
            callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueReadBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueWriteBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueCopyBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueFillBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueFillBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueFillBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadBufferRect) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBufferRect>(
            callback_data, collector);
      } else {
        OnExitEnqueueReadBufferRect(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteBufferRect) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBufferRect>(
            callback_data, collector);
      } else {
        OnExitEnqueueWriteBufferRect(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueCopyBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadImage>(
            callback_data, collector);
      } else {
        OnExitEnqueueReadImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteImage>(
            callback_data, collector);
      } else {
        OnExitEnqueueWriteImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImage>(
            callback_data, collector);
      } else {
        OnExitEnqueueCopyImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueFillImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueFillImage>(
            callback_data, collector);
      } else {
        OnExitEnqueueFillImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyImageToBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImageToBuffer>(
            callback_data, collector);
      } else {
        OnExitEnqueueCopyImageToBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBufferToImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBufferToImage>(
            callback_data, collector);
      } else {
        OnExitEnqueueCopyBufferToImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clFinish) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitFinish(collector);
      }
    } else if (function == CL_FUNCTION_clReleaseCommandQueue) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitReleaseCommandQueue(collector);
      }
    } else if (function == CL_FUNCTION_clReleaseEvent) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterReleaseEvent(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clWaitForEvents) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitWaitForEvents(callback_data, collector);
      }
    }
  }

 private: // Data
  ClApiTracer* tracer_ = nullptr;
  Correlator* correlator_ = nullptr;

  KernelCollectorOptions options_;

  std::atomic<uint64_t> kernel_id_;
  cl_device_id device_ = nullptr;

  OnClKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  ClKernelInfoMap kernel_info_map_;
  ClKernelInstanceList kernel_instance_list_;

#ifdef PTI_KERNEL_INTERVALS
  ze_device_handle_t ze_device_;
  uint64_t timer_mask_;
  uint64_t timer_freq_;
  ClDeviceMap device_map_;
  ClKernelIntervalList kernel_interval_list_;
#endif // PTI_KERNEL_INTERVALS

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 12;
};

#endif // PTI_TOOLS_CL_TRACER_CL_KERNEL_COLLECTOR_H_
