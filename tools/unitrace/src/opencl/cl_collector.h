//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_

#include <atomic>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <string>
#include <vector>
#include <chrono>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "trace_guard.h"
#include "collector_options.h"
#include "logger.h"
#include "unikernel.h"
#include "unicontrol.h"

#include <level_zero/zes_api.h>

#include "common_header.gen"

#include "cl_intel_ext.h"

class ClCollector;

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
  uint64_t base_addr;
  uint32_t size;
  cl_device_id device_id;
};

struct ClKernelInstance {
  cl_event event = nullptr;
  ClKernelProps props;
  uint64_t kernel_id = 0;
  cl_ulong host_sync = 0;
  cl_ulong device_sync = 0;
  bool need_to_process = true;
  cl_device_id device;
  std::vector<int32_t> sub_device_list;
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

struct ClKernelMemInfo{
  std::string name;
  uint64_t base_addr;
  uint32_t size;
};

using ClKernelMemInfoMap = std::map<uint64_t, ClKernelMemInfo>;

using ClKernelInfoMap = std::map<std::string, ClKernelInfo>;
using ClKernelInstanceList = std::list<ClKernelInstance*>;

struct ClDevice {
  cl_device_id id_;
  bool isroot_;
  cl_device_id parent_;
  std::vector<cl_device_id> subdevs_;
};

typedef void (*OnClKernelFinishCallback)(
    cl_device_pci_bus_info_khr& pci,
    cl_device_id device,
    cl_command_queue& queue,
    int tile,
    bool implicit,
    const uint64_t id,
    uint64_t started,
    uint64_t ended);

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
static std::shared_mutex cl_kernel_command_properties_mutex_;
static std::map<uint64_t, ClKernelProps> cl_kernel_command_properties_;

// Keeps track of all active cl device name
std::map<cl_device_id, std::string>& GetDevicesNameMap() {
  static std::map<cl_device_id, std::string> devices_name;
  return devices_name;
}
inline std::string GetClDeviceName(cl_device_id device) {
  auto it = GetDevicesNameMap().find(device);
  if (it != GetDevicesNameMap().end()) {
    return it->second;
  }
  return "";
}

struct ClKernelProfileRecord {
  cl_device_id device_ = nullptr;
  uint64_t global_instance_id_ = 0;
  uint64_t device_started_;
  uint64_t device_ended_;
  std::string kernel_name_;
};

typedef void (*OnClFunctionFinishCallback)(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, API_TRACING_ID api_id, uint64_t started, uint64_t ended);

inline cl_device_pci_bus_info_khr GetDevicePciInfo(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  if (!utils::cl::CheckExtension(device, "cl_khr_pci_bus_info")) {
    return cl_device_pci_bus_info_khr{0, 0, 0, 0};
  }

  cl_device_pci_bus_info_khr pci_info{};
  cl_int status = clGetDeviceInfo(device, CL_DEVICE_PCI_BUS_INFO_KHR, sizeof(cl_device_pci_bus_info_khr), &pci_info, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return pci_info;
}

inline ze_device_handle_t GetZeDevice(cl_device_id device_id) {
  if (device_id == nullptr) {
    return nullptr;
  }

  cl_device_pci_bus_info_khr pci_info = GetDevicePciInfo(device_id);

  for (auto device : GetDeviceList()) {
    zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
    ze_result_t status = ZE_FUNC(zesDevicePciGetProperties)(device, &pci_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (pci_info.pci_domain == pci_props.address.domain &&
        pci_info.pci_bus == pci_props.address.bus &&
        pci_info.pci_device == pci_props.address.device &&
        pci_info.pci_function == pci_props.address.function) {
      return device;
    }
  }

  return nullptr;
}

void OnEnterFunction(cl_function_id function, cl_callback_data* data, uint64_t start, ClCollector* collector);
void OnExitFunction(cl_function_id function, cl_callback_data* data, uint64_t start, uint64_t end, ClCollector* collector);

template <cl_device_type device_type>
void *clHostMemAllocINTEL(cl_context context, const cl_mem_properties_intel *properties, size_t size, cl_uint alignment, cl_int *errcode_ret);
template <cl_device_type device_type>
void *clDeviceMemAllocINTEL(cl_context context, cl_device_id device, const cl_mem_properties_intel* properties, size_t size, cl_uint alignment, cl_int* errcode_ret);
template <cl_device_type device_type>
void *clSharedMemAllocINTEL(cl_context context, cl_device_id device, const cl_mem_properties_intel* properties, size_t size, cl_uint alignment, cl_int* errcode_ret);
template <cl_device_type device_type>
cl_int clMemFreeINTEL(cl_context context, void *ptr);
template <cl_device_type device_type>
cl_int clGetMemAllocInfoINTEL(cl_context context, const void *ptr, cl_mem_info_intel param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
template <cl_device_type device_type>
cl_int clSetKernelArgMemPointerINTEL(cl_kernel kernel, cl_uint arg_index, const void *arg_value);
template <cl_device_type device_type>
cl_int clEnqueueMemcpyINTEL(cl_command_queue command_queue, cl_bool blocking, void *dst_ptr, const void *src_ptr,
        size_t size, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
template <cl_device_type device_type>
cl_int clGetDeviceGlobalVariablePointerINTEL(cl_device_id device, cl_program program, const char *global_variable_name,
        size_t *global_variable_size_ret, void **global_variable_pointer_ret);
template <cl_device_type device_type>
cl_int clGetKernelSuggestedLocalWorkSizeINTEL(cl_command_queue command_queue, cl_kernel kernel, cl_uint workDim,
        const size_t *global_work_offset, const size_t *global_work_size, size_t *suggested_local_work_size);
template <cl_device_type device_type>
cl_mem clCreateBufferWithPropertiesINTEL( cl_context context, const cl_mem_properties_intel* properties, cl_mem_flags flags, size_t size, void *host_ptr, cl_int *errcode_ret);
template <cl_device_type device_type>
cl_int clEnqueueMemsetINTEL(cl_command_queue command_queue, void* dst_ptr, cl_int value, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
template <cl_device_type device_type>
cl_int clEnqueueMigrateMemINTEL(cl_command_queue command_queue, const void* ptr, size_t size, cl_mem_migration_flags flags,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
template <cl_device_type device_type>
cl_int clEnqueueMemAdviseINTEL(cl_command_queue command_queue, const void* ptr, size_t size, cl_mem_advice_intel advice,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
template <cl_device_type device_type>
cl_int clEnqueueMemFillINTEL(cl_command_queue command_queue, void* dst_ptr, const void* pattern, size_t pattern_size, size_t size,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
template <cl_device_type device_type>
cl_int clMemBlockingFreeINTEL( cl_context context, void* ptr);

inline std::string GetVerboseName(const ClKernelProps* props) {
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
    sstream << "[" <<
      std::to_string(props->bytes_transferred) << "]";
  }

  return sstream.str();
}

inline std::string GetClKernelCommandName(uint64_t id) {
  return GetVerboseName(&cl_kernel_command_properties_[id]);
}

class ClCollector {
 public: // Interface
  static ClCollector* Create(
      cl_device_id device,
      Logger *logger,
      CollectorOptions options,
      OnClKernelFinishCallback kcallback = nullptr,
      OnClFunctionFinishCallback fcallback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(logger != nullptr);

    std::string data_dir_name = utils::GetEnv("UNITRACE_DataDir");

    ClCollector* collector = new ClCollector(
        device, logger, options, kcallback, fcallback, callback_data, data_dir_name);
    PTI_ASSERT(collector != nullptr);

    collector->SetKernelTracingPoints();
 
    ClApiTracer* tracer;
    tracer = new ClApiTracer(device, TracingCallBack, collector);
    collector->EnableTracing(tracer);

    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer " <<
        "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
      }
      delete collector;
      return nullptr;
    }

    collector->tracer_ = tracer;

    if (collector->device_type_ == CL_DEVICE_TYPE_CPU) {
      cl_cpu_collector_ = collector;
    }
    else {
      cl_gpu_collector_ = collector;
    }

    return collector;
  }

  ~ClCollector() {
    ReleaseDeviceMap();
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);

    if (options_.stall_sampling) {
      std::map<uint32_t, std::map<uint64_t, const ClKernelProps *>> device_kprops;
      for (auto it = kprops_.begin(); it != kprops_.end(); it++) {
        auto dev_id = present_cl_devices_.find(it->second.device_id);
        if (dev_id == present_cl_devices_.end()) {
          std::cerr<<"Invalid device is found\n";
          return;
        }
        std::map<uint64_t, const ClKernelProps *> prop;
        prop.insert({it->second.base_addr, &(it->second)});
        device_kprops.insert({dev_id->second,std::move(prop)});
      }

      for(auto& props : device_kprops) {
        // kernel properties file path: data_dir/.kprops.<device_id>.<pid>.txt
        std::string fpath = data_dir_name_ + "/.kprops." + std::to_string(props.first) + "." + std::to_string(utils::GetPid()) + ".txt";
        std::ofstream kpfs = std::ofstream(fpath, std::ios::out | std::ios::trunc);
        uint64_t prev_base = 0;
        for (auto it = props.second.crbegin(); it != props.second.crend(); it++) {
          kpfs << "\"" << utils::Demangle(it->second->name.c_str()) << "\"" << std::endl;
          kpfs << std::to_string(it->second->base_addr) << std::endl;
          if (prev_base == 0) {
            kpfs << std::to_string(it->second->size) << std::endl;
          }
          else {
            size_t size = prev_base - it->second->base_addr;
            if (size > it->second->size) {
              size = it->second->size;
            }
            kpfs << std::to_string(size) << std::endl;
          }
          prev_base = it->second->base_addr;
        }
        kpfs.close();
      }
    }

    if (options_.metric_stream) {
      std::map<int32_t, std::vector<ClKernelProfileRecord>> device_kprofiles;
      for (const auto &record : profile_records_) {
        auto it = present_cl_devices_.find(record.device_);
        if (it == present_cl_devices_.end()) {
          std::cerr << "[ERROR] Invalid device is found\n";
          return;
        }
        auto device_id = it->second;
        if (device_kprofiles.find(device_id) != device_kprofiles.end()) {
          device_kprofiles[device_id].push_back(record);
        } else {
          std::vector<ClKernelProfileRecord> new_recs;
          new_recs.push_back(record);
          device_kprofiles[device_id] = std::move(new_recs);
        }
      }

      for (const auto &dev_kprofile : device_kprofiles) {
        std::ofstream ouf;
        // kernel instance time file path: <data_dir>/.ktime.<device_id>.<pid>.txt
        std::string fpath = data_dir_name_ + "/.ktime." + std::to_string(dev_kprofile.first) + "." + std::to_string(utils::GetPid()) + ".txt";
        ouf = std::ofstream(fpath, std::ios::out | std::ios::trunc);
        for(const auto &profile : dev_kprofile.second) {
          ouf << std::to_string((-1)) << std::endl;
          ouf << std::to_string(profile.global_instance_id_) << std::endl;
          ouf << std::to_string(profile.device_started_) << std::endl;
          ouf << std::to_string(profile.device_ended_) << std::endl;
          ouf << "\"" << profile.kernel_name_ << "\"" << std::endl;
        }
        ouf.close();
      }
    }
  }

  const ClKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  ClCollector(const ClCollector& copy) = delete;
  ClCollector& operator=(const ClCollector& copy) = delete;

  const ClKernelMemInfoMap& GetKernelMemInfo() const {
    return kernel_mem_info_map_;
  }

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

    logger_->Log(stream.str());
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

    logger_->Log(stream.str());
  }

  const ClFunctionInfoMap& GetFunctionInfoMap() const {
    return function_info_map_;
  }

  bool NeedTid() const {
    return options_.need_tid;
  }

  bool NeedPid() const {
    return options_.need_pid;
  }

  bool Demangle() const {
    return options_.demangle;
  }

  void Log(const std::string& text) {
    logger_->Log(text);
  }

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

    logger_->Log(stream.str());
  }

  static ClCollector *GetCollector(cl_device_type device_type) {
    if (device_type == CL_DEVICE_TYPE_GPU) {
      return cl_gpu_collector_;
    }
    else {
      return cl_cpu_collector_;
    }
  }

  uint64_t GetTimestamp() const {
    return utils::GetSystemTime();
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

  static bool IsTracingNow() {
    return trace_now_; 
  }

  static void TracingNowOn() {
    trace_now_ = true;
  }

  static void TracingNowOff() {
    trace_now_ = false;
  }

  static void PushTraceNesting() {
    trace_nesting_level_++; 
    if (trace_nesting_level_ == max_trace_nesting_level_) {
      std::cerr << "[ERROR] Nest tracing level " << trace_nesting_level_ << " is unsupported" << std::endl;
      exit(-1);
    }
  }

  static void PopTraceNesting() {
    trace_nesting_level_--; 
    if (trace_nesting_level_ < -1) {
      std::cerr << "[ERROR] Nest tracing level " << trace_nesting_level_ << " is unsupported" << std::endl;
      exit(-1);
    }
  }

  static uint64_t GetTraceStartTimeAndPopTraceNesting() {
    uint64_t t = trace_start_time_[trace_nesting_level_];
    PopTraceNesting();
    return t;
  }

  void PushTraceNestingAndCaptureTraceStartTime(){
    PushTraceNesting();
    trace_start_time_[trace_nesting_level_] = GetTimestamp();
  }

  bool IsHostTimingOn() {
    return options_.host_timing;
  }

  bool IsCallLoggingOn() {
    return options_.call_logging;
  }

  void *GetClExtFunction(int i) {
    void *ret;
    cl_ext_func_lock_.lock_shared();
    ret = cl_ext_func_[i];
    cl_ext_func_lock_.unlock();
    return ret;
  }

  void InvokeFunctionFinishCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, const cl_ext_api_id api_id, uint64_t started, uint64_t ended) {
    if (fcallback_ != nullptr) {
      fcallback_(kids, flow_dir, (API_TRACING_ID)api_id, started, ended);
    }
  }

 private: // Implementation Details
  ClCollector(
      cl_device_id device,
      Logger *logger,
      CollectorOptions options,
      OnClKernelFinishCallback kcallback,
      OnClFunctionFinishCallback fcallback,
      void* /* callback_data */,
      std::string& data_dir_name)
      : device_(device),
        logger_(logger),
        options_(options),
        kcallback_(kcallback),
        fcallback_(fcallback),
        data_dir_name_(data_dir_name) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(logger_ != nullptr);

    device_type_ = utils::cl::GetDeviceType(device);
    PTI_ASSERT(device_type_ == CL_DEVICE_TYPE_CPU || device_type_ == CL_DEVICE_TYPE_GPU);
    CreateDeviceMap();

    // initialize extension function replacements to nulls
    const std::lock_guard<std::shared_mutex> lock(cl_ext_func_lock_);
    for (int i = 0; i < (ClExtApiEnd - ClExtApiStart); i++) {
      cl_ext_func_[i] = nullptr;
    }
  }

  void CreateDeviceMap() {
    cl_device_type type = utils::cl::GetDeviceType(device_);

    cl_int status = CL_SUCCESS;
    cl_uint pcount = 0;
    status = clGetPlatformIDs(0, nullptr, &pcount);
    
    if ((status != CL_SUCCESS) || (pcount == 0)) {
      std::cerr << "[WARNING] Unable to get platform identifiers" << std::endl;
      return; 
    }

    std::vector<cl_platform_id> platforms(pcount, nullptr);
    status = clGetPlatformIDs(pcount, platforms.data(), nullptr);
    if (status != CL_SUCCESS) {
      std::cerr << "[WARNING] Unable to get platform identifiers" << std::endl;
      return;
    }

    for (auto plat : platforms) {
      cl_uint dcount = 0;

      status = clGetDeviceIDs(plat, type, 0, nullptr, &dcount);
      if ((status != CL_SUCCESS) || (dcount == 0)) {
        std::cerr << "[WARNING] Unable to get device identifiers" << std::endl;
        continue;
      }

      std::vector<cl_device_id> devs(dcount, nullptr);
      status = clGetDeviceIDs(plat, type, dcount, devs.data(), nullptr);
      if (status != CL_SUCCESS) {
        std::cerr << "[WARNING] Unable to get device identifiers" << std::endl;
        continue;
      }

      for (auto dev : devs) {
        ClDevice cd;

        cd.isroot_ = true;
        cd.id_ = dev;
        cd.parent_ = (cl_device_id)(-1);
        cl_device_partition_property props[] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE, 0};
        cl_uint subcount = 0;
        status = clCreateSubDevices(dev, props, 0, nullptr, &subcount);
        if ((status != CL_SUCCESS) || (subcount == 0)) {
          cd.subdevs_ = std::vector<cl_device_id>();
        }
        else {
          std::vector<cl_device_id> subdevs(subcount);
          status = clCreateSubDevices(dev, props, subcount, subdevs.data(), nullptr);
          if (status != CL_SUCCESS) {
            // TODO: check on Windows
            std::cerr << "[WARNING] Unable to create sub-devices" << std::endl;
            subdevs.clear();
          }
          else {
            for (auto subdev : subdevs) {
              ClDevice subcd;

              subcd.id_ = subdev;
              subcd.isroot_ = false;
              subcd.parent_ = dev;
              subcd.subdevs_ = std::vector<cl_device_id>();

              // get sub-device names
              size_t name_size = 0;
              status = clGetDeviceInfo(subdev, CL_DEVICE_NAME, 0, NULL, &name_size);
              if (status == CL_SUCCESS) {
                std::string str_dev_name(name_size+1, '\0');
                status = clGetDeviceInfo(subdev, CL_DEVICE_NAME, name_size, (void*)str_dev_name.c_str(), NULL);
                if (status == CL_SUCCESS) {
                  GetDevicesNameMap().insert({subdev, std::move(str_dev_name)});
                }
              }
              device_map_.insert({subdev, subcd});
              present_cl_devices_.insert({subdev,present_cl_devices_.size()});
            }
          }
          cd.subdevs_ = std::move(subdevs);
        }

        // get device names
        size_t name_size = 0;
        status = clGetDeviceInfo(dev, CL_DEVICE_NAME, 0, NULL, &name_size);
        if (status == CL_SUCCESS) {
          std::string str_dev_name(name_size+1, '\0');
          status = clGetDeviceInfo(dev, CL_DEVICE_NAME, name_size, (void*)str_dev_name.c_str(), NULL);
          if (status == CL_SUCCESS) {
            GetDevicesNameMap().insert({dev, std::move(str_dev_name)});
          }
        }

        if (status != CL_SUCCESS) {
          std::cerr << "[ERROR] clGetDeviceInfo failed with error "<<status<<std::endl;
        }

        device_map_.insert({dev, cd});
        present_cl_devices_.insert({dev,present_cl_devices_.size()});
      }
    }
  }

  void ReleaseDeviceMap() {
    for (auto dev : device_map_) {
      if (dev.second.isroot_) {
        for (auto subdev : dev.second.subdevs_) {
          clReleaseDevice(subdev);
        }
      }
    }
  }

  void SetKernelTracingPoints() {
    for (int i = 0; i < CL_FUNCTION_COUNT; i++) {
      kernel_tracing_points_enabled[i] = false;
    }
    kernel_tracing_points_enabled[CL_FUNCTION_clCreateCommandQueueWithProperties] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clCreateCommandQueue] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueNDRangeKernel] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueTask] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueReadBuffer] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueWriteBuffer] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueReadBufferRect] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueWriteBufferRect] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueCopyBuffer] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueCopyBufferRect] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueFillBuffer] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueReadImage] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueWriteImage] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueCopyImage] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueFillImage] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueCopyImageToBuffer] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueCopyBufferToImage] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clFinish] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clReleaseCommandQueue] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clReleaseEvent] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clWaitForEvents] = true;
  }

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);

    for (int id = 0; id < CL_FUNCTION_COUNT; ++id) {
      if (options_.api_tracing || (kernel_tracing_points_enabled[id] && options_.kernel_tracing)) {
        bool set = tracer->SetTracingFunction(static_cast<cl_function_id>(id));
        PTI_ASSERT(set);
      }
    }

    bool enabled = tracer->Enable();
    PTI_ASSERT(enabled);
  }

  void EnableKernelTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);

    for (int id = 0; id < CL_FUNCTION_COUNT; ++id) {
      if (kernel_tracing_points_enabled[id]) {
        bool set = tracer->SetTracingFunction(static_cast<cl_function_id>(id));
        PTI_ASSERT(set);
      }
    }

    bool enabled = tracer->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernelMemInfo(const std::string& name, uint64_t base_addr, uint32_t size) {
    if ((base_addr != 0) && (kernel_mem_info_map_.find(base_addr) == kernel_mem_info_map_.end())) {
      ClKernelMemInfo m;

      m.name = name;
      m.base_addr = base_addr;
      m.size = size;
      kernel_mem_info_map_.insert(std::pair<uint64_t, ClKernelMemInfo>(base_addr, m));
    }
  }

  void AddKernelInstance(ClKernelInstance* instance) {
    PTI_ASSERT(instance != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    cl_event event = instance->event;
    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);
    cl_device_id device = utils::cl::GetDevice(queue);
    PTI_ASSERT(device != nullptr);

    auto it = device_map_.find(device); 
    if (it != device_map_.end()) {
      if (it->second.isroot_) {
        int i = 0;
        if (it->second.subdevs_.size() > 0) {
          // implicit scaling in COMPOSITE mode
          for (auto subdev : it->second.subdevs_) {
            instance->device = subdev;
            instance->sub_device_list.push_back(i++);
          }
        }
        else {
          // FLAT mode
          instance->device = device;
        }
      }
      else {
        // explicit scaling in COMPOSITE mode
        instance->device = device;
      }
    }
    kernel_instance_list_.push_back(instance);
    AddKernelMemInfo(instance->props.name, instance->props.base_addr, instance->props.size);
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
    //PTI_ASSERT(queued > 0);
    cl_ulong submitted =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_SUBMIT);
    //PTI_ASSERT(submitted > 0);

    //PTI_ASSERT(instance->device_sync <= queued);
    // This is workaround due to driver bug. In some cases driver does not give right timestamp
    int64_t time_diff = 0 < queued ? (queued - instance->device_sync) : 0;
    uint64_t time_shift = ((time_diff > 0) ? time_diff : 0);
    host_queued = instance->host_sync + time_shift;
    //Win_Todo: Investigate why sometime "submitted > started" is happening
#ifdef _WIN32
    time_diff = queued <= submitted ? (submitted - queued) : 0;
    host_submitted = host_queued + time_diff;
    time_diff = submitted <= started ? (started - submitted) : 0;
    host_started = host_submitted + time_diff;
    time_diff = started <= ended ? (ended - started) : 0;
    host_ended = host_started + time_diff;
#else /* _WIN32 */
    PTI_ASSERT(queued <= submitted);
    host_submitted = host_queued + (submitted - queued);
    PTI_ASSERT(submitted <= started);
    host_started = host_submitted + (started - submitted);
    PTI_ASSERT(started <= ended);
    host_ended = host_started + (ended - started);
#endif /* _WIN32 */
  }

  void PrintOutOffloadedCommand(std::string& name, cl_device_id& device, uint64_t appended, uint64_t submitted, uint64_t kernel_start, uint64_t kernel_end) {
    std::stringstream stream;
    stream << "Thread " << utils::GetTid() << " Device " << device <<
      " : " << name << " [ns] " <<
      appended << " (append) " <<
      submitted << " (submit) " <<
      kernel_start << " (start) " <<
      kernel_end << " (end)" << std::endl;
    logger_->Log(stream.str());
  }

  inline cl_device_pci_bus_info_khr GetDevicePciInfo(cl_device_id device) {
    cl_device_pci_bus_info_khr pciInfo;
    cl_int status = clGetDeviceInfo(device, CL_DEVICE_PCI_BUS_INFO_KHR, sizeof(cl_device_pci_bus_info_khr), &pciInfo, nullptr);
    PTI_ASSERT(status == CL_SUCCESS);
    return pciInfo;
  }

  void ProcessKernelInstance(const ClKernelInstance* instance, int32_t tile) {
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

      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);
      cl_device_pci_bus_info_khr pciInfo = GetDevicePciInfo(device);

      std::string name = instance->props.name;
      PTI_ASSERT(!name.empty());

      if (options_.verbose) {
        name = GetVerboseName(&(instance->props));
      }

      uint64_t host_queued = 0, host_submitted = 0;
      uint64_t host_started = 0, host_ended = 0;
      ComputeHostTimestamps(instance, started, ended, host_queued, host_submitted, host_started, host_ended);
      AddKernelInfo(name, host_submitted - host_queued, host_started - host_submitted, host_ended - host_started);

      bool implicit = false;

      if (tile >= 0) {
        auto it = device_map_.find(device);
        if (it != device_map_.end()) {
          if (it->second.isroot_ && (it->second.subdevs_.size() > 0)) {
            // implicit scaling in COMPOSITE mode
            implicit = true;
          }
        }
      }

      if (options_.device_timeline) {
        PrintOutOffloadedCommand(name, device, host_queued, host_submitted, host_started, host_ended);
      }

      auto it = cl_kernel_command_properties_.find(instance->kernel_id);
      if (it == cl_kernel_command_properties_.end()) {
        cl_kernel_command_properties_.insert({instance->kernel_id, instance->props});
      }

      if (kcallback_ != nullptr) {
        kcallback_(pciInfo, device, queue, tile, implicit, instance->kernel_id, host_started, host_ended);
      }

      if (options_.metric_stream) {
        cl_ulong cl_host_timestamp = 0;
        cl_ulong cl_device_timestamp = 0;
        utils::cl::GetTimestamps(device, &cl_host_timestamp, &cl_device_timestamp);

        uint64_t ze_host_timestamp;
        uint64_t ze_device_timestamp;
        ze_device_handle_t ze_device;
        uint64_t mask;
        uint64_t freq;

        ze_device = GetZeDevice(device);
        PTI_ASSERT(ze_device != nullptr);
        mask = GetMetricTimestampMask(ze_device);
        freq = GetMetricTimerFrequency(ze_device);

        ZE_FUNC(zeDeviceGetGlobalTimestamps)(ze_device, &ze_host_timestamp, &ze_device_timestamp);
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
        ClKernelProfileRecord rec{device, instance->kernel_id, ze_started, ze_ended, std::move(name)};

        profile_records_.push_back(std::move(rec));
      }
    }
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
	if (instance->sub_device_list.size()) {
          for (size_t i = 0; i < instance->sub_device_list.size(); ++i) {
            ProcessKernelInstance(instance,instance->sub_device_list[i]);
	  };
	} else {
          ProcessKernelInstance(instance,-1);
	};
        cl_int status = clReleaseEvent(event);
        PTI_ASSERT(status == CL_SUCCESS);
	delete instance;

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
        if (instance->sub_device_list.size()) {
          for (size_t i = 0; i < instance->sub_device_list.size(); ++i) {
            ProcessKernelInstance(instance,instance->sub_device_list[i]);
          }
        } else {
          ProcessKernelInstance(instance,-1);
	}
        it = kernel_instance_list_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void AddKernelInfo(std::string name, uint64_t queued_time, uint64_t submit_time, uint64_t execute_time) {
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


  void CalculateKernelLocalSize(const cl_params_clEnqueueNDRangeKernel* params, ClKernelProps* props) {
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

  void CalculateKernelLocalSize(const cl_params_clEnqueueTask* /* params */, ClKernelProps* props) {
    PTI_ASSERT(props != nullptr);
    props->local_size[0] = 1;
    props->local_size[1] = 1;
    props->local_size[2] = 1;
  }

  void CalculateKernelGlobalSize(const cl_params_clEnqueueNDRangeKernel* params, ClKernelProps* props) {
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

  void CalculateKernelGlobalSize(const cl_params_clEnqueueTask* /* params */, ClKernelProps* props) {
    PTI_ASSERT(props != nullptr);
    props->global_size[0] = 1;
    props->global_size[1] = 1;
    props->global_size[2] = 1;
  }

  // Callbacks
  static void OnEnterCreateCommandQueueWithProperties(cl_callback_data* data, ClCollector* /* collector */) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clCreateCommandQueueWithProperties* params =
      reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    data->correlationData[0] = reinterpret_cast<cl_ulong>(nullptr);
    cl_queue_properties *props = (cl_queue_properties *)(*(params->properties));
    if (props == nullptr) {
      props = new cl_queue_properties[3];
      props[0] = CL_QUEUE_PROPERTIES;
      props[1] = CL_QUEUE_PROFILING_ENABLE;
      props[2] = 0;
      data->correlationData[0] = reinterpret_cast<cl_ulong>(props);
    } else {
      cl_queue_properties *p = props;
      int i;
      
      for (i = 0; p[i]; i += 2) {
        if (p[i] == CL_QUEUE_PROPERTIES) {
          p[i + 1] |= CL_QUEUE_PROFILING_ENABLE;
          break;
        }
      }
      if (p[i] == 0) {
        p = props;
        props = new cl_queue_properties[i + 3];
        for (int j = 0; j < i; j++) {
          props[j] = p[j];
        }
        props[i++] = CL_QUEUE_PROPERTIES;
        props[i++] = CL_QUEUE_PROFILING_ENABLE;
        props[i] = 0;
        data->correlationData[0] = reinterpret_cast<cl_ulong>(props);
      }
    }

    *(params->properties) = props;
  }

  static void OnExitCreateCommandQueueWithProperties(cl_callback_data* data, ClCollector* /* collector */) {
    PTI_ASSERT(data != nullptr);

    cl_queue_properties* props = reinterpret_cast<cl_queue_properties*>(data->correlationData[0]);
    if (props != nullptr) {
      delete[] props;
    }
  }

  static void OnEnterCreateCommandQueue(cl_callback_data* data, ClCollector* /* collector */) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clCreateCommandQueue* params =
      reinterpret_cast<const cl_params_clCreateCommandQueue*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);
    *(params->properties) |=
      static_cast<unsigned long>(CL_QUEUE_PROFILING_ENABLE);
  }

  template <typename T>
  static void OnEnterEnqueueKernel(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(collector->device_ != nullptr);

    ClEnqueueData* enqueue_data = new ClEnqueueData;
    enqueue_data->event = nullptr;

    utils::cl::GetTimestamps(collector->device_, &enqueue_data->host_sync, &enqueue_data->device_sync);

    const T* params = reinterpret_cast<const T*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) == nullptr) {
      *(params->event) = &(enqueue_data->event);
    }

    data->correlationData[0] = reinterpret_cast<cl_ulong>(enqueue_data);
  }

  template<typename T>
  static void OnExitEnqueueKernel(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    ClEnqueueData* enqueue_data = reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const T* params = reinterpret_cast<const T*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      PTI_ASSERT(*(params->event) != nullptr);
      cl_int status = CL_SUCCESS;

      if (*(params->event) != reinterpret_cast<cl_event*>(data->correlationData)) {
        status = clRetainEvent(**(params->event));
        PTI_ASSERT(status == CL_SUCCESS);
      }

      ClKernelInstance* instance = new ClKernelInstance;
      PTI_ASSERT(instance != nullptr);
      instance->event = **(params->event);

      cl_kernel kernel = *(params->kernel);
      instance->props.name = utils::cl::GetKernelName(kernel, collector->options_.demangle);

      cl_command_queue queue = *(params->commandQueue);
      PTI_ASSERT(queue != nullptr);
      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);
      instance->props.device_id = device;

      size_t simd_width = utils::cl::GetKernelSimdWidth(device, kernel);
      PTI_ASSERT(simd_width > 0);

      instance->props.simd_width = simd_width;
      instance->props.bytes_transferred = 0;

      collector->CalculateKernelGlobalSize(params, &instance->props);
      collector->CalculateKernelLocalSize(params, &instance->props);

      uint64_t base_addr = 0;
      uint64_t size = 0;

      status = clGetKernelInfo(kernel, CL_KERNEL_BINARY_GPU_ADDRESS_INTEL, 0, nullptr, &size);
      status = clGetKernelInfo(kernel, CL_KERNEL_BINARY_GPU_ADDRESS_INTEL, size, &base_addr, &size);

      PTI_ASSERT(status == CL_SUCCESS);
      instance->props.base_addr = (base_addr & 0xFFFFFFFF) - 65536;
      size = 0;
      status = clGetKernelInfo(kernel, CL_KERNEL_BINARY_PROGRAM_INTEL, 0, nullptr, &size);
      PTI_ASSERT(status == CL_SUCCESS);
      instance->props.size = size;

      auto it = collector->kprops_.find(instance->props.name);
      if (it == collector->kprops_.end()) {
        collector->kprops_.insert(std::pair<std::string, ClKernelProps>(instance->props.name, instance->props));
      }

      instance->kernel_id = UniKernelInstanceId::GetKernelInstanceId();
      if (kid) {
        *kid = instance->kernel_id;
      }
      instance->need_to_process = UniController::IsCollectionEnabled();

      PTI_ASSERT(enqueue_data != nullptr);
      instance->device_sync = enqueue_data->device_sync;
      instance->host_sync = enqueue_data->host_sync;

      collector->AddKernelInstance(instance);

      delete enqueue_data;
    }
  }

  static void OnExitEnqueueTransfer(std::string name, size_t bytes_transferred, cl_event* event, cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
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
    instance->props.name = std::move(name);

    instance->props.simd_width = 0;
    instance->props.bytes_transferred = bytes_transferred;

    instance->props.base_addr = 0;
    instance->props.size = 0;

    instance->kernel_id = UniKernelInstanceId::GetKernelInstanceId();
    if (kid) {
      *kid = instance->kernel_id;
    }
    instance->need_to_process = UniController::IsCollectionEnabled();

    ClEnqueueData* enqueue_data = reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
    PTI_ASSERT(enqueue_data != nullptr);
    instance->device_sync = enqueue_data->device_sync;
    instance->host_sync = enqueue_data->host_sync;

    collector->AddKernelInstance(instance);

    delete enqueue_data;
  }

  static void OnExitEnqueueReadBuffer(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(
        data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadBuffer* params =
        reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
            data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer( "clEnqueueReadBuffer", *(params->cb), *(params->event), data, collector, kid);

      if (*params->blockingRead) {
        collector->ProcessKernelInstances();
      }
    }
  }

  static void OnExitEnqueueWriteBuffer(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteBuffer* params = reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>( data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer( "clEnqueueWriteBuffer", *(params->cb), *(params->event), data, collector, kid);

      if (*params->blockingWrite) {
        collector->ProcessKernelInstances();
      }
    }
  }

  static void OnExitEnqueueCopyBuffer(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBuffer* params = reinterpret_cast<const cl_params_clEnqueueCopyBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer("clEnqueueCopyBuffer", *(params->cb), *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueFillBuffer(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillBuffer* params = reinterpret_cast<const cl_params_clEnqueueFillBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer("clEnqueueFillBuffer", *(params->size), *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueReadBufferRect(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadBufferRect* params = reinterpret_cast<const cl_params_clEnqueueReadBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueReadBufferRect", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueWriteBufferRect(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>( data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteBufferRect* params = reinterpret_cast<const cl_params_clEnqueueWriteBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueWriteBufferRect", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueCopyBufferRect(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferRect* params = reinterpret_cast<const cl_params_clEnqueueCopyBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueCopyBufferRect", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueReadImage(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadImage* params = reinterpret_cast<const cl_params_clEnqueueReadImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueReadImage", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueWriteImage(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteImage* params = reinterpret_cast<const cl_params_clEnqueueWriteImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueWriteImage", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueCopyImage(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImage* params = reinterpret_cast<const cl_params_clEnqueueCopyImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->srcImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyImage", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueFillImage(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillImage* params = reinterpret_cast<const cl_params_clEnqueueFillImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueFillImage", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueCopyImageToBuffer(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImageToBuffer* params = reinterpret_cast<const cl_params_clEnqueueCopyImageToBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->srcImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer( "clEnqueueCopyImageToBuffer", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitEnqueueCopyBufferToImage(cl_callback_data* data, ClCollector* collector, uint64_t *kid) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferToImage* params = reinterpret_cast<const cl_params_clEnqueueCopyBufferToImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->dstImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t), &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyBufferToImage", bytes_transferred, *(params->event), data, collector, kid);
    }
  }

  static void OnExitFinish(ClCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    collector->ProcessKernelInstances();
  }

  static void OnExitReleaseCommandQueue(ClCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    collector->ProcessKernelInstances();
  }

  static void OnEnterReleaseEvent(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    const cl_params_clReleaseEvent* params = reinterpret_cast<const cl_params_clReleaseEvent*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) != nullptr) {
      collector->ProcessKernelInstance(*(params->event));
    }
  }

  static void OnExitWaitForEvents(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clWaitForEvents* params = reinterpret_cast<const cl_params_clWaitForEvents*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const cl_event* event_list = *(params->eventList);
      if (event_list != nullptr) {
        for (cl_uint i = 0; i < *(params->numEvents); ++i) {
          collector->ProcessKernelInstance(event_list[i]);
        }
      }
    }
  }

  static void KernelTracingCallBackOnEnter(cl_function_id function, ClCollector *collector, cl_callback_data *callback_data, uint64_t * /* kid */) {
    switch (function) {
      case CL_FUNCTION_clCreateCommandQueueWithProperties:
        OnEnterCreateCommandQueueWithProperties(callback_data, collector);
        break;
      case CL_FUNCTION_clCreateCommandQueue:
        OnEnterCreateCommandQueue(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueNDRangeKernel:
        OnEnterEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueTask:
        OnEnterEnqueueKernel<cl_params_clEnqueueTask>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueReadBuffer:
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBuffer>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueWriteBuffer:
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBuffer>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueCopyBuffer:
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBuffer>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueFillBuffer:
        OnEnterEnqueueKernel<cl_params_clEnqueueFillBuffer>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueReadBufferRect:
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBufferRect>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueWriteBufferRect:
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBufferRect>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueReadImage:
        OnEnterEnqueueKernel<cl_params_clEnqueueReadImage>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueWriteImage:
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteImage>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueCopyImage:
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImage>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueFillImage:
        OnEnterEnqueueKernel<cl_params_clEnqueueFillImage>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueCopyImageToBuffer:
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImageToBuffer>(callback_data, collector);
        break;
      case CL_FUNCTION_clEnqueueCopyBufferToImage:
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBufferToImage>(callback_data, collector);
        break;
      case CL_FUNCTION_clFinish:
      case CL_FUNCTION_clReleaseCommandQueue:
        break;
      case CL_FUNCTION_clReleaseEvent:
        OnEnterReleaseEvent(callback_data, collector);
        break;
      case CL_FUNCTION_clWaitForEvents:
        break;
      default:
        break;
    }
  }

  static void KernelTracingCallBackOnExit(cl_function_id function, ClCollector *collector, cl_callback_data *callback_data, uint64_t *kid) {
    switch(function) {
      case CL_FUNCTION_clCreateCommandQueueWithProperties:
        OnExitCreateCommandQueueWithProperties(callback_data, collector);
        break;
      case CL_FUNCTION_clCreateCommandQueue:
        break;
      case CL_FUNCTION_clEnqueueNDRangeKernel:
        OnExitEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueTask:
        OnExitEnqueueKernel<cl_params_clEnqueueTask>(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueReadBuffer:
        OnExitEnqueueReadBuffer(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueWriteBuffer:
        OnExitEnqueueWriteBuffer(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueCopyBuffer:
        OnExitEnqueueCopyBuffer(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueFillBuffer:
        OnExitEnqueueFillBuffer(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueReadBufferRect:
        OnExitEnqueueReadBufferRect(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueWriteBufferRect:
        OnExitEnqueueWriteBufferRect(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueReadImage:
        OnExitEnqueueReadImage(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueWriteImage:
        OnExitEnqueueWriteImage(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueCopyImage:
        OnExitEnqueueCopyImage(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueFillImage:
        OnExitEnqueueFillImage(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueCopyImageToBuffer:
        OnExitEnqueueCopyImageToBuffer(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clEnqueueCopyBufferToImage:
        OnExitEnqueueCopyBufferToImage(callback_data, collector, kid);
        break;
      case CL_FUNCTION_clFinish:
        OnExitFinish(collector);
        break;
      case CL_FUNCTION_clReleaseCommandQueue:
        OnExitReleaseCommandQueue(collector);
        break;
      case CL_FUNCTION_clReleaseEvent:
        break;
      case CL_FUNCTION_clWaitForEvents:
        OnExitWaitForEvents(callback_data, collector);
        break;
      default:
        break;
    }
  }

  static void TracingCallBack(cl_function_id function, cl_callback_data* callback_data, void* user_data) {
    if (IsTracingNow()) {
      // no recursive tracing
      return;
    }
    ClCollector* collector = reinterpret_cast<ClCollector*>(user_data);
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      if (UniController::IsCollectionEnabled()) {
        collector->TracingNowOn();
        uint64_t kid = KERNEL_INSTANCE_ID_INVALID;
        if (collector->options_.kernel_tracing && collector->kernel_tracing_points_enabled[function]) {
          KernelTracingCallBackOnEnter(function, collector, callback_data, &kid);
        }

        if (collector->options_.call_logging) {
          OnEnterFunction(function, callback_data, collector->GetTimestamp(), collector);
        }

        collector->TracingNowOff();
      }
      collector->PushTraceNestingAndCaptureTraceStartTime();
    }
    else {
      // take end timestamp first to avoid tool overhead
      uint64_t end_time = collector->GetTimestamp();
      
      collector->TracingNowOn();
      uint64_t kid = KERNEL_INSTANCE_ID_INVALID;
      if (collector->options_.kernel_tracing && collector->kernel_tracing_points_enabled[function]) {
        KernelTracingCallBackOnExit(function, collector, callback_data, &kid);
      }

      uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

      if (collector->options_.host_timing) {
        collector->AddFunctionTime(callback_data->functionName, end_time - start_time);
      }

      if (collector->options_.call_logging) {
        OnExitFunction(function, callback_data, start_time, end_time, collector);
      }

      if (collector->fcallback_ != nullptr) {
        FLOW_DIR flow_dir = FLOW_NUL;
        std::vector<uint64_t> kids;
        if (function == CL_FUNCTION_clEnqueueNDRangeKernel ||
            function == CL_FUNCTION_clWaitForEvents ||
            function == CL_FUNCTION_clEnqueueReadBuffer ||
            function == CL_FUNCTION_clEnqueueWriteBuffer) {
          flow_dir = (function == CL_FUNCTION_clWaitForEvents ? FLOW_D2H : FLOW_H2D);
	  if (kid != KERNEL_INSTANCE_ID_INVALID) {
	    kids.push_back(kid);
	  }
        }

        API_TRACING_ID api_id = (API_TRACING_ID)(OCLStartTracingId + function);
        collector->fcallback_((kids.empty() ? nullptr: &kids), flow_dir, api_id, start_time, end_time);
      }

      #define REPLACE_INTEL_EXTENSION_FUNCTION(params, name, i, cb_data) \
        if (strcmp(#name, *(params->funcName)) == 0) { \
          if (collector->cl_ext_func_[i] == nullptr) { \
            const std::lock_guard<std::shared_mutex> lock(collector->cl_ext_func_lock_); \
            if (collector->cl_ext_func_[i] == nullptr) { \
              collector->cl_ext_func_[i] = *(void **)(cb_data->functionReturnValue); \
            } \
          } \
          if (collector->device_type_ == CL_DEVICE_TYPE_GPU) { \
            *reinterpret_cast<decltype(name<CL_DEVICE_TYPE_CPU>)**>(cb_data->functionReturnValue) = &name<CL_DEVICE_TYPE_GPU>; \
          } \
          else { \
            *reinterpret_cast<decltype(name<CL_DEVICE_TYPE_CPU>)**>(cb_data->functionReturnValue) = &name<CL_DEVICE_TYPE_CPU>; \
          } \
        }

      if (function == CL_FUNCTION_clGetExtensionFunctionAddress) {
        const cl_params_clGetExtensionFunctionAddress *params =
          reinterpret_cast<const cl_params_clGetExtensionFunctionAddress *>(callback_data->functionParams);
        if (params == nullptr) {
          std::cout<<"[ERROR] functionParams in callbackdata is null.";
          return;
        }
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clHostMemAllocINTEL, ClExtHostMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clDeviceMemAllocINTEL, ClExtDeviceMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clSharedMemAllocINTEL, ClExtSharedMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clMemFreeINTEL, ClExtMemFreeINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetMemAllocInfoINTEL, ClExtGetMemAllocInfoINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clSetKernelArgMemPointerINTEL, ClExtSetKernelArgMemPointerINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemcpyINTEL, ClExtEnqueueMemcpyINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetDeviceGlobalVariablePointerINTEL, ClExtGetDeviceGlobalVariablePointerINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetKernelSuggestedLocalWorkSizeINTEL, ClExtGetKernelSuggestedLocalWorkSizeINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clCreateBufferWithPropertiesINTEL, ClExtCreateBufferWithPropertiesINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemsetINTEL, ClExtEnqueueMemsetINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMigrateMemINTEL, ClExtEnqueueMigrateMemINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemAdviseINTEL, ClExtEnqueueMemAdviseINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemFillINTEL, ClExtEnqueueMemFillINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clMemBlockingFreeINTEL, ClExtMemBlockingFreeINTEL - ClExtApiStart, callback_data);
      } else if (function == CL_FUNCTION_clGetExtensionFunctionAddressForPlatform) {
        const cl_params_clGetExtensionFunctionAddressForPlatform *params =
          reinterpret_cast<const cl_params_clGetExtensionFunctionAddressForPlatform *>(callback_data->functionParams);
        if (params == nullptr) {
          std::cout<<"[ERROR] functionParams in callbackdata is null.";
          return;
        }
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clHostMemAllocINTEL, ClExtHostMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clDeviceMemAllocINTEL, ClExtDeviceMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clSharedMemAllocINTEL, ClExtSharedMemAllocINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clMemFreeINTEL, ClExtMemFreeINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetMemAllocInfoINTEL, ClExtGetMemAllocInfoINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clSetKernelArgMemPointerINTEL, ClExtSetKernelArgMemPointerINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemcpyINTEL, ClExtEnqueueMemcpyINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetDeviceGlobalVariablePointerINTEL, ClExtGetDeviceGlobalVariablePointerINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clGetKernelSuggestedLocalWorkSizeINTEL, ClExtGetKernelSuggestedLocalWorkSizeINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clCreateBufferWithPropertiesINTEL, ClExtCreateBufferWithPropertiesINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemsetINTEL, ClExtEnqueueMemsetINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMigrateMemINTEL, ClExtEnqueueMigrateMemINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemAdviseINTEL, ClExtEnqueueMemAdviseINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clEnqueueMemFillINTEL, ClExtEnqueueMemFillINTEL - ClExtApiStart, callback_data);
        REPLACE_INTEL_EXTENSION_FUNCTION(params, clMemBlockingFreeINTEL, ClExtMemBlockingFreeINTEL - ClExtApiStart, callback_data);
      }
      collector->TracingNowOff();
    }
  }


  constexpr static int max_trace_nesting_level_ = 2;
  inline static thread_local int trace_nesting_level_ = -1; // in case an extension is called within an OCL call
  inline static thread_local uint64_t trace_start_time_[max_trace_nesting_level_] = {0}; // start time of traced API 
  inline static thread_local bool trace_now_ = false; // prevent recursive tracing

  cl_device_id device_ = nullptr;
  Logger *logger_ = nullptr;
  CollectorOptions options_;
  ClApiTracer* tracer_ = nullptr;

  OnClKernelFinishCallback kcallback_ = nullptr;
  OnClFunctionFinishCallback fcallback_ = nullptr;

  std::mutex lock_;
  ClKernelInfoMap kernel_info_map_;
  ClKernelInstanceList kernel_instance_list_;

  bool kernel_tracing_points_enabled[CL_FUNCTION_COUNT];

  std::map<cl_device_id, ClDevice> device_map_;

  ClKernelMemInfoMap kernel_mem_info_map_;

  static const uint32_t kFunctionLength = 10;
  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 12;

  cl_device_type device_type_ = CL_DEVICE_TYPE_ALL;


  ClFunctionInfoMap function_info_map_;

  std::map<std::string, ClKernelProps> kprops_;
  std::string data_dir_name_;

  std::vector<ClKernelProfileRecord> profile_records_;
  std::map<cl_device_id, uint32_t> present_cl_devices_;

  std::shared_mutex cl_ext_func_lock_;	// lock for extension function table
  void *cl_ext_func_[ClExtApiEnd - ClExtApiStart] = {nullptr};

  inline static ClCollector *cl_gpu_collector_ = nullptr;
  inline static ClCollector *cl_cpu_collector_ = nullptr;
};

template <cl_device_type device_type>
void *clHostMemAllocINTEL(cl_context context, const cl_mem_properties_intel *properties, size_t size, cl_uint alignment, cl_int *errcode_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clHostMemAllocINTEL:";
  
        str += " context = " + std::to_string(uint64_t(context));
        str += " properties = " + std::to_string(uint64_t(properties));
        str += " size = " + std::to_string(size);
        str += " alignment = " + std::to_string(alignment);
        str += " errcode_ret = " + std::to_string(uint64_t(errcode_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (void *(*)(cl_context, const cl_mem_properties_intel *, size_t, cl_uint, cl_int *))(collector->GetClExtFunction(ClExtHostMemAllocINTEL - ClExtApiStart));
  auto result = f(context, properties, size, alignment, errcode_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clHostMemAllocINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clHostMemAllocINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " result = " + std::to_string(uint64_t(result));

      str += " -> " + std::string(utils::cl::GetErrorString(*errcode_ret));
      str += " (" + std::to_string(*errcode_ret) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtHostMemAllocINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
void *clDeviceMemAllocINTEL(cl_context context, cl_device_id device, const cl_mem_properties_intel* properties, size_t size, cl_uint alignment, cl_int* errcode_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clDeviceMemAllocINTEL:";
  
        str += " context = " + std::to_string(uint64_t(context));
        str += " device = " + std::to_string(uint64_t(device));
        str += " properties = " + std::to_string(uint64_t(properties));
        str += " size = " + std::to_string(size);
        str += " alignment = " + std::to_string(alignment);
        str += " errcode_ret = " + std::to_string(uint64_t(errcode_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (void *(*)(cl_context, cl_device_id, const cl_mem_properties_intel *, size_t, cl_uint, cl_int *))(collector->GetClExtFunction(ClExtDeviceMemAllocINTEL - ClExtApiStart));
  auto result = f(context, device, properties, size, alignment, errcode_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clDeviceMemAllocINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clDeviceMemAllocINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " result = " + std::to_string(uint64_t(result));

      str += " -> " + std::string(utils::cl::GetErrorString(*errcode_ret));
      str += " (" + std::to_string(*errcode_ret) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtDeviceMemAllocINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
void *clSharedMemAllocINTEL(cl_context context, cl_device_id device, const cl_mem_properties_intel* properties, size_t size, cl_uint alignment, cl_int* errcode_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clSharedMemAllocINTEL:";
  
        str += " context = " + std::to_string(uint64_t(context));
        str += " device = " + std::to_string(uint64_t(device));
        str += " properties = " + std::to_string(uint64_t(properties));
        str += " size = " + std::to_string(size);
        str += " alignment = " + std::to_string(alignment);
        str += " errcode_ret = " + std::to_string(uint64_t(errcode_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (void *(*)(cl_context, cl_device_id, const cl_mem_properties_intel *, size_t, cl_uint, cl_int *))(collector->GetClExtFunction(ClExtSharedMemAllocINTEL - ClExtApiStart));
  auto result = f(context, device, properties, size, alignment, errcode_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clSharedMemAllocINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str += "clSharedMemAllocINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " result = " + std::to_string(uint64_t(result));

      if (errcode_ret) {
        str += " -> " + std::string(utils::cl::GetErrorString(*errcode_ret));
        str += " (" + std::to_string(*errcode_ret) + ")";
      }
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtSharedMemAllocINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
cl_int clMemFreeINTEL(cl_context context, void *ptr) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clMemFreeINTEL:";
  
        str += " context = " + std::to_string(uint64_t(context));
        str += " ptr = " + std::to_string(uint64_t(ptr));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_context, void *))(collector->GetClExtFunction(ClExtMemFreeINTEL - ClExtApiStart));
  auto result = f(context, ptr);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clMemFreeINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clMemFreeINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";
      str += " result = " + std::to_string(result);
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtMemFreeINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
cl_int clGetMemAllocInfoINTEL(cl_context context, const void *ptr, cl_mem_info_intel param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clGetMemAllocInfoINTEL:";
  
        str += " context = " + std::to_string(uint64_t(context));
        str += " ptr = " + std::to_string(uint64_t(ptr));
        str += " param_name = " + std::to_string(param_name);
        str += " param_value_size = " + std::to_string(param_value_size);
        str += " param_value = " + std::to_string(uint64_t(param_value));
        str += " param_value_size_ret = " + std::to_string(uint64_t(param_value_size_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_context, const void *, cl_mem_info_intel, size_t, void *, size_t *))(collector->GetClExtFunction(ClExtGetMemAllocInfoINTEL - ClExtApiStart));
  auto result = f(context, ptr, param_name, param_value_size, param_value, param_value_size_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clGetMemAllocInfoINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clGetMemAllocInfoINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtGetMemAllocInfoINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
cl_int clSetKernelArgMemPointerINTEL(cl_kernel kernel, cl_uint arg_index, const void *arg_value) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clSetKernelArgMemPointerINTEL:";
  
        str += " kernel = " + std::to_string(uint64_t(kernel));
        str += " arg_index = " + std::to_string(arg_index);
        str += " arg_value = " + std::to_string(uint64_t(arg_value));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_kernel, cl_uint, const void *))(collector->GetClExtFunction(ClExtSetKernelArgMemPointerINTEL - ClExtApiStart));
  auto result = f(kernel, arg_index, arg_value);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clSetKernelArgMemPointerINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clSetKernelArgMemPointerINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtSetKernelArgMemPointerINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clEnqueueMemcpyINTEL(cl_command_queue command_queue, cl_bool blocking, void *dst_ptr, const void *src_ptr,
          size_t size, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clEnqueueMemcpyINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " blocking = " + std::to_string(blocking);
        str += " dst_ptr = " + std::to_string(uint64_t(dst_ptr));
        str += " src_ptr = " + std::to_string(uint64_t(src_ptr));
        str += " size = " + std::to_string(size);
        str += " num_events_in_wait_list = " + std::to_string(num_events_in_wait_list);
        str += " event_wait_list = " + std::to_string(uint64_t(event_wait_list));
        str += " event = " + std::to_string(uint64_t(event));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, cl_bool, void *, const void *, size_t, cl_uint, const cl_event *, cl_event *))(collector->GetClExtFunction(ClExtEnqueueMemcpyINTEL - ClExtApiStart));
  auto result = f(command_queue, blocking, dst_ptr, src_ptr, size, num_events_in_wait_list, event_wait_list, event);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clEnqueueMemcpyINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clEnqueueMemcpyINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtEnqueueMemcpyINTEL, start_time, end_time);
    collector->TracingNowOff();
  }


  return result;
}

template <cl_device_type device_type>
cl_int clGetDeviceGlobalVariablePointerINTEL(cl_device_id device, cl_program program, const char *global_variable_name,
          size_t *global_variable_size_ret, void **global_variable_pointer_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clGetDeviceGlobalVariablePointerINTEL:";
        str += " device = " + std::to_string(uint64_t(device));
        str += " program = " + std::to_string(uint64_t(program));
        str += " global_variable_name = " + std::string(global_variable_name);
        str += " global_variable_size_ret = " + std::to_string(uint64_t(global_variable_size_ret));
        str += " global_variable_pointer_ret = " + std::to_string(uint64_t(global_variable_pointer_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_device_id, cl_program, const char *, size_t *, void **))(collector->GetClExtFunction(ClExtGetDeviceGlobalVariablePointerINTEL- ClExtApiStart));
  auto result = f(device, program, global_variable_name, global_variable_size_ret, global_variable_pointer_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clGetDeviceGlobalVariablePointerINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clGetDeviceGlobalVariablePointerINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtGetDeviceGlobalVariablePointerINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clGetKernelSuggestedLocalWorkSizeINTEL(cl_command_queue command_queue, cl_kernel kernel, cl_uint workDim,
          const size_t *global_work_offset, const size_t *global_work_size, size_t *suggested_local_work_size) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clGetKernelSuggestedLocalWorkSizeINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " kernel = " + std::to_string(uint64_t(kernel));
        str += " workDim = " + std::to_string(workDim);
        str += " global_work_offset = " + std::to_string(uint64_t(global_work_offset));
        str += " global_work_size = " + std::to_string(uint64_t(global_work_size));
        str += " suggested_local_work_size = " + std::to_string(uint64_t(suggested_local_work_size));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, cl_kernel, cl_uint, const size_t *, const size_t *, size_t *))(collector->GetClExtFunction(ClExtGetKernelSuggestedLocalWorkSizeINTEL - ClExtApiStart));
  auto result = f(command_queue, kernel, workDim, global_work_offset, global_work_size, suggested_local_work_size);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clGetKernelSuggestedLocalWorkSizeINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clGetKernelSuggestedLocalWorkSizeINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";
      str += " suggested_local_work_size = " + std::to_string(uint64_t(suggested_local_work_size));
      if (suggested_local_work_size != nullptr) {
        str += " (" + std::to_string(*suggested_local_work_size) + ")";
      }
      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtGetKernelSuggestedLocalWorkSizeINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_mem clCreateBufferWithPropertiesINTEL(cl_context context, const cl_mem_properties_intel *properties, cl_mem_flags flags, size_t size, void *host_ptr, cl_int *errcode_ret) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clCreateBufferWithPropertiesINTEL:";
        str += " context = " + std::to_string(uint64_t(context));
        str += " properties = " + std::to_string(uint64_t(properties));
        str += " cl_mem_flags = " + std::to_string(flags);
        str += " size = " + std::to_string(size);
        str += " host_ptr = " + std::to_string(uint64_t(host_ptr));
        str += " errcode_ret = " + std::to_string(uint64_t(errcode_ret));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_mem (*)(cl_context, const cl_mem_properties_intel *, cl_mem_flags, size_t, void *, cl_int *))(collector->GetClExtFunction(ClExtCreateBufferWithPropertiesINTEL - ClExtApiStart));
  auto result = f(context, properties, flags, size, host_ptr, errcode_ret);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clCreateBufferWithPropertiesINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clCreateBufferWithPropertiesINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " result = " + std::to_string(uint64_t(result));

      if (errcode_ret) {
        str += " -> " + std::string(utils::cl::GetErrorString(*errcode_ret));
        str += " (" + std::to_string(*errcode_ret) + ")";
      }
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtCreateBufferWithPropertiesINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clEnqueueMemsetINTEL(cl_command_queue command_queue, void *dst_ptr, cl_int value, size_t size, cl_uint num_events_in_wait_list,
                const cl_event *event_wait_list, cl_event *event) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clEnqueueMemsetINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " dst_ptr = " + std::to_string(uint64_t(dst_ptr));
        str += " value = " + std::to_string(value);
        str += " size = " + std::to_string(size);
        str += " num_events_in_wait_list = " + std::to_string(num_events_in_wait_list);
        str += " event_wait_list = " + std::to_string(uint64_t(event_wait_list));
        str += " event = " + std::to_string(uint64_t(event));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, void *, cl_int, size_t, cl_uint, const cl_event *, cl_event *))(collector->GetClExtFunction(ClExtEnqueueMemsetINTEL - ClExtApiStart));
  auto result = f(command_queue, dst_ptr, value, size, num_events_in_wait_list, event_wait_list, event);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clEnqueueMemsetINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clEnqueueMemsetINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtEnqueueMemsetINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clEnqueueMigrateMemINTEL(cl_command_queue command_queue, const void *ptr, size_t size, cl_mem_migration_flags flags,
        cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clEnqueueMigrateMemINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " ptr = " + std::to_string(uint64_t(ptr));
        str += " size = " + std::to_string(size);
        str += " flags = " + std::to_string(uint64_t(flags));
        str += " num_events_in_wait_list = " + std::to_string(num_events_in_wait_list);
        str += " event_wait_list = " + std::to_string(uint64_t(event_wait_list));
        str += " event = " + std::to_string(uint64_t(event));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, const void *, size_t, cl_mem_migration_flags, cl_uint, const cl_event *, cl_event *))(collector->GetClExtFunction(ClExtEnqueueMigrateMemINTEL - ClExtApiStart));
  auto result = f(command_queue, ptr, size, flags, num_events_in_wait_list, event_wait_list, event);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clEnqueueMigrateMemINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clEnqueueMigrateMemINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtEnqueueMigrateMemINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clEnqueueMemAdviseINTEL(cl_command_queue command_queue, const void *ptr, size_t size, cl_mem_advice_intel advice,
        cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clEnqueueMemAdviseINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " ptr = " + std::to_string(uint64_t(ptr));
        str += " size = " + std::to_string(size);
        str += " advice = " + std::to_string(uint64_t(advice));
        str += " num_events_in_wait_list = " + std::to_string(num_events_in_wait_list);
        str += " event_wait_list = " + std::to_string(uint64_t(event_wait_list));
        str += " event = " + std::to_string(uint64_t(event));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, const void *, size_t, cl_mem_advice_intel, cl_uint, const cl_event *, cl_event *))(collector->GetClExtFunction(ClExtEnqueueMemAdviseINTEL - ClExtApiStart));
  auto result = f(command_queue, ptr, size, advice, num_events_in_wait_list, event_wait_list, event);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clEnqueueMemAdviseINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clEnqueueMemAdviseINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtEnqueueMemAdviseINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clEnqueueMemFillINTEL(cl_command_queue command_queue, void *dst_ptr, const void *pattern, size_t pattern_size, size_t size,
        cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clEnqueueMemFillINTEL:";
        str += " command_queue = " + std::to_string(uint64_t(command_queue));
        str += " dst_ptr = " + std::to_string(uint64_t(dst_ptr));
        str += " pattern = " + std::to_string(uint64_t(pattern));
        str += " pattern_size = " + std::to_string(pattern_size);
        str += " size = " + std::to_string(size);
        str += " num_events_in_wait_list = " + std::to_string(num_events_in_wait_list);
        str += " event_wait_list = " + std::to_string(uint64_t(event_wait_list));
        str += " event = " + std::to_string(uint64_t(event));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_command_queue, void *, const void *, size_t, size_t, cl_uint, const cl_event *, cl_event *))(collector->GetClExtFunction(ClExtEnqueueMemFillINTEL - ClExtApiStart));
  auto result = f(command_queue, dst_ptr, pattern, pattern_size, size, num_events_in_wait_list, event_wait_list, event);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clEnqueueMemFillINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clEnqueueMemFillINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtEnqueueMemFillINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}

template <cl_device_type device_type>
cl_int clMemBlockingFreeINTEL(cl_context context, void *ptr) {
  ClCollector *collector = ClCollector::GetCollector(device_type);

  if (collector->IsTracingNow() == false) {
    if (UniController::IsCollectionEnabled()) {
      if (collector->IsCallLoggingOn()) {
        collector->TracingNowOn();
        std::string str;
        str = ">>>> [" + std::to_string(collector->GetTimestamp()) + "] ";
        if (collector->NeedPid()) {
          str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
        }
        if (collector->NeedTid()) {
          str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
        }
        str += "clMemBlockingFreeINTEL:";
        str += " context = " + std::to_string(uint64_t(context));
        str += " ptr = " + std::to_string(uint64_t(ptr));
        str += "\n";
  
        collector->Log(str);
        collector->TracingNowOff();
      }
    }
    collector->PushTraceNestingAndCaptureTraceStartTime();
  }

  auto f = (cl_int (*)(cl_context, void *))(collector->GetClExtFunction(ClExtMemBlockingFreeINTEL - ClExtApiStart));
  auto result = f(context, ptr);

  if (collector->IsTracingNow() == false) {
    // take end timestamp first to avoid tool overhead
    uint64_t end_time = collector->GetTimestamp();
    collector->TracingNowOn();
    uint64_t start_time = collector->GetTraceStartTimeAndPopTraceNesting();

    if (collector->IsHostTimingOn()) {
      collector->AddFunctionTime("clMemBlockingFreeINTEL", end_time - start_time);
    }

    if (collector->IsCallLoggingOn()) {
      std::string str;
      str = "<<<< ["  + std::to_string(end_time) + "] ";
      if (collector->NeedPid()) {
        str += "<PID:" + std::to_string(utils::GetPid()) + "> ";
      }
      if (collector->NeedTid()) {
        str += "<TID:" + std::to_string((unsigned int)(utils::GetTid())) + "> ";
      }
      str +=  "clMemBlockingFreeINTEL";
      str += " [" + std::to_string(end_time - start_time) + " ns]";

      str += " -> " + std::string(utils::cl::GetErrorString(result));
      str += " (" + std::to_string(result) + ")";
      str += "\n";

      collector->Log(str);
    }

    collector->InvokeFunctionFinishCallback(nullptr, FLOW_NUL, ClExtMemBlockingFreeINTEL, start_time, end_time);
    collector->TracingNowOff();
  }

  return result;
}
#endif //PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_
