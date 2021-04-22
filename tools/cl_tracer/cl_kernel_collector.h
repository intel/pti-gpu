//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_
#define PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "correlator.h"
#include "trace_guard.h"

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
  size_t simd_width;
  size_t bytes_transferred;
  size_t global_size[3];
  size_t local_size[3];
};

struct ClEventData {
  ClKernelCollector* collector;
  std::string kernel_name;
  uint64_t kernel_id;
  cl_ulong host_sync;
  cl_ulong device_sync;
  ClKernelProps props;
};

struct ClKernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

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

struct ClKernelInterval {
  std::string name;
  uint64_t start;
  uint64_t end;
};

using ClKernelInfoMap = std::map<std::string, ClKernelInfo>;
using ClKernelIntervalList = std::vector<ClKernelInterval>;

typedef void (*OnClKernelFinishCallback)(
    void* data, void* queue,
    uint64_t id, const std::string& name,
    uint64_t queued, uint64_t submitted,
    uint64_t started, uint64_t ended);

class ClKernelCollector {
 public: // Interface
  static ClKernelCollector* Create(
      cl_device_id device,
      Correlator* correlator,
      bool verbose,
      OnClKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(correlator != nullptr);
    TraceGuard guard;

    ClKernelCollector* collector = new ClKernelCollector(
        device, correlator, verbose, callback, callback_data);
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

  ~ClKernelCollector() {
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

  const ClKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }

  ClKernelCollector(const ClKernelCollector& copy) = delete;
  ClKernelCollector& operator=(const ClKernelCollector& copy) = delete;

  void PrintKernelsTable() const {
    std::set< std::pair<std::string, ClKernelInfo>,
              utils::Comparator > sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

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
    correlator_->Log(stream.str().c_str());
  }

 private: // Implementation Details
  ClKernelCollector(
      cl_device_id device,
      Correlator* correlator,
      bool verbose,
      OnClKernelFinishCallback callback,
      void* callback_data)
      : correlator_(correlator),
        verbose_(verbose),
        callback_(callback),
        callback_data_(callback_data),
        kernel_id_(1) {
    PTI_ASSERT(correlator_ != nullptr);
    device_type_ = utils::cl::GetDeviceType(device);
    PTI_ASSERT(
        device_type_ == CL_DEVICE_TYPE_CPU ||
        device_type_ == CL_DEVICE_TYPE_GPU);
  }

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    bool set = true;
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueueWithProperties);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueue);

    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueNDRangeKernel);
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clEnqueueTask);

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

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernelInfo(
      std::string name, uint64_t time, const ClKernelProps* props) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(props != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    if (verbose_) {
      if (props->simd_width > 0) {
        std::stringstream sstream;
        sstream << name << "[SIMD" << props->simd_width << ", {" <<
          props->global_size[0] << ", " <<
          props->global_size[1] << ", " <<
          props->global_size[2] << "}, {" <<
          props->local_size[0] << ", " <<
          props->local_size[1] << ", " <<
          props->local_size[2] << "}]";
        name = sstream.str();
      } else if (props->bytes_transferred > 0) {
        name = name + "[" +
          std::to_string(props->bytes_transferred) + " bytes]";
      }
    }

    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {
        time, time, time, 1};
    } else {
      ClKernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      if (time > kernel.max_time) {
        kernel.max_time = time;
      }
      if (time < kernel.min_time) {
        kernel.min_time = time;
      }
      kernel.call_count += 1;
    }
  }

  void AddKernelInterval(std::string name, uint64_t start, uint64_t end) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(start < end);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_interval_list_.push_back({name, start, end});
  }

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
  static void CL_CALLBACK EventNotify(
      cl_event event, cl_int event_status, void* user_data) {
    PTI_ASSERT(event_status == CL_COMPLETE);
    TraceGuard guard;

    PTI_ASSERT(user_data != nullptr);
    ClEventData* event_data = reinterpret_cast<ClEventData*>(user_data);

    ClKernelCollector* collector = event_data->collector;
    PTI_ASSERT(collector != nullptr);

    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);

    std::string name = event_data->kernel_name;
    PTI_ASSERT(!name.empty());

    cl_ulong started =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_START);
    cl_ulong ended =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_END);
    cl_ulong time = ended - started;
    PTI_ASSERT(time > 0);

    collector->AddKernelInfo(name, time, &event_data->props);

    if (event_data->props.simd_width > 0) {
      collector->AddKernelInterval(name, started, ended);
    }

    if (collector->callback_ != nullptr) {
      cl_ulong queued =
        utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_QUEUED);
      PTI_ASSERT(queued > 0);
      cl_ulong submitted =
        utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_SUBMIT);
      PTI_ASSERT(submitted > 0);

      PTI_ASSERT(event_data->device_sync <= queued);
      uint64_t time_shift = queued - event_data->device_sync;

      uint64_t host_queued = event_data->host_sync + time_shift;
      PTI_ASSERT(queued <= submitted);
      uint64_t host_submitted = host_queued + (submitted - queued);
      PTI_ASSERT(submitted <= started);
      uint64_t host_started = host_submitted + (started - submitted);
      PTI_ASSERT(started <= ended);
      uint64_t host_ended = host_started + (ended - started);

      collector->callback_(
          collector->callback_data_, queue,
          event_data->kernel_id, name,
          host_queued, host_submitted, host_started, host_ended);
    }

    cl_int status = clReleaseEvent(event);
    PTI_ASSERT(status == CL_SUCCESS);

    delete event_data;
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

  template <typename T>
  static void OnEnterEnqueueKernel(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    ClEnqueueData* enqueue_data = new ClEnqueueData;
    enqueue_data->event = nullptr;
    enqueue_data->device_sync =
      (collector->device_type_ == CL_DEVICE_TYPE_GPU) ?
      utils::cl::GetGpuTimestamp() :
      utils::cl::GetCpuTimestamp();
    enqueue_data->host_sync = collector->correlator_->GetTimestamp();

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

      ClEventData* event_data = new ClEventData;
      PTI_ASSERT(event_data != nullptr);
      cl_kernel kernel = *(params->kernel);
      event_data->collector = collector;
      event_data->kernel_name = utils::cl::GetKernelName(kernel);

      cl_command_queue queue = *(params->commandQueue);
      PTI_ASSERT(queue != nullptr);
      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);

      size_t simd_width = utils::cl::GetKernelSimdWidth(device, kernel);
      PTI_ASSERT(simd_width > 0);

      event_data->props.simd_width = simd_width;
      event_data->props.bytes_transferred = 0;

      collector->CalculateKernelGlobalSize(params, &event_data->props);
      collector->CalculateKernelLocalSize(params, &event_data->props);

      event_data->kernel_id =
        collector->kernel_id_.fetch_add(
            1, std::memory_order::memory_order_relaxed);
      PTI_ASSERT(collector->correlator_ != nullptr);
      collector->correlator_->SetKernelId(event_data->kernel_id);

      ClEnqueueData* enqueue_data =
        reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
      PTI_ASSERT(enqueue_data != nullptr);
      event_data->device_sync = enqueue_data->device_sync;
      event_data->host_sync = enqueue_data->host_sync;

      status = clSetEventCallback(
          **(params->event), CL_COMPLETE, EventNotify, event_data);
      PTI_ASSERT(status == CL_SUCCESS);

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

    ClEventData* event_data = new ClEventData;
    PTI_ASSERT(event_data != nullptr);
    event_data->collector = collector;
    event_data->kernel_name = name;

    event_data->props.simd_width = 0;
    event_data->props.bytes_transferred = bytes_transferred;

    event_data->kernel_id =
      collector->kernel_id_.fetch_add(
          1, std::memory_order::memory_order_relaxed);
    PTI_ASSERT(collector->correlator_ != nullptr);
    collector->correlator_->SetKernelId(event_data->kernel_id);

    ClEnqueueData* enqueue_data =
      reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
    PTI_ASSERT(enqueue_data != nullptr);
    event_data->device_sync = enqueue_data->device_sync;
    event_data->host_sync = enqueue_data->host_sync;

    cl_int status = clSetEventCallback(
        *event, CL_COMPLETE, EventNotify, event_data);
    PTI_ASSERT(status == CL_SUCCESS);
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
    }
  }

 private: // Data
  ClApiTracer* tracer_ = nullptr;
  Correlator* correlator_ = nullptr;

  bool verbose_ = false;

  std::atomic<uint64_t> kernel_id_;
  cl_device_type device_type_ = CL_DEVICE_TYPE_ALL;

  OnClKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  ClKernelInfoMap kernel_info_map_;
  ClKernelIntervalList kernel_interval_list_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_