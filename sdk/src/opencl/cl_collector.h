//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_

#include <CL/cl.h>
#include <spdlog/spdlog.h>

// #include <CL/opencl.hpp>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "collector_options.h"
//#include "common_header.gen"
#include "trace_guard.h"
#include "unikernel.h"

// New queries for clGetKernelInfo:
#define CL_KERNEL_BINARY_PROGRAM_INTEL 0x407D
#define CL_KERNEL_BINARIES_INTEL 0x4102
#define CL_KERNEL_BINARY_SIZES_INTEL 0x4103
#define CL_KERNEL_BINARY_GPU_ADDRESS_INTEL 0x10010

class ClCollector;

struct ClInstanceApiData {
  uint64_t start_time;
  uint64_t end_time;
  uint32_t corr_id_ = 0;
};

static thread_local ClInstanceApiData cl_instance_api_data;
// TODO-OCL find home for kernel_id_
static thread_local uint64_t kernel_id_;

enum ClKernelType { KERNEL_TYPE_USER, KERNEL_TYPE_TRANSFER };

struct ClEnqueueData {
  cl_event event;
  cl_ulong host_sync;
  cl_ulong device_sync;
};

struct ClKernelProps {
  std::string name;
  UniMemoryCommandRoute route;
  KernelCommandType type = KernelCommandType::kInvalid;
  size_t simd_width;
  size_t bytes_transferred;
  size_t global_size[3];
  size_t local_size[3];
  uint64_t base_addr;
  uint32_t size;
  cl_device_id src_device = nullptr;  // Device for p2p memcpy, source of copy data
  cl_device_id dst_device = nullptr;  // Device for p2p memcpy, destination of copy data
  void* dst = nullptr;                // Addressess for MemorCopy or Fill
  void* src = nullptr;
};

struct ClKernelInstance {
  cl_event event = nullptr;
  ClKernelProps props;
  uint64_t kernel_id = 0;
  cl_ulong host_sync = 0;
  cl_ulong device_sync = 0;
  cl_device_id device;
  std::vector<int32_t> sub_device_list;
  uint32_t tid = 0;
  uint64_t sycl_node_id_ = 0;
  uint64_t sycl_queue_id_ =
      PTI_INVALID_QUEUE_ID;  // default to invalid till we determine otherwise.
  uint32_t sycl_invocation_id_ = 0;
  uint64_t sycl_task_begin_time_ = 0;
  uint64_t sycl_enqk_begin_time_ = 0;
  std::string source_file_name_;
  uint32_t source_line_number_ = 0;
  uint32_t corr_id_ = 0;
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

struct ClKernelMemInfo {
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

using OnClKernelFinishCallback =
    void (*)(void*, ZeKernelCommandExecutionRecord&);  // kernel records finish callback
using OnClApiCallsFinishCallback =
    void (*)(void*, ZeKernelCommandExecutionRecord&);  // Api calls finish callback

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

struct ClKernelProfileRecord {
  cl_device_id device_ = nullptr;
  uint64_t global_instance_id_ = 0;
  uint64_t device_started_;
  uint64_t device_ended_;
  std::string kernel_name_;
};

/*
typedef void (*OnClApiCallsFinishCallback)(
    std::vector<uint64_t>* kids, FLOW_DIR flow_dir,
    // API_TRACING_ID api_id, uint64_t started, uint64_t ended);
    uint64_t started, uint64_t ended);
*/

std::string stringify_uuid(uint8_t* uuid, std::string additional_string) {
  std::stringstream sstream;
  sstream << additional_string;
  sstream << std::hex << std::setfill('0');
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    sstream << std::setw(2);
    sstream << static_cast<uint16_t>(uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) sstream << "-";
  }
  sstream << std::setfill(' ') << std::dec;
  return sstream.str();
}

void print_uuid([[maybe_unused]] uint8_t* uuid, [[maybe_unused]] std::string additional_string) {
  // std::cout << stringify_uuid(uuid, std::move(additional_string)) << std::endl;
}

inline cl_device_pci_bus_info_khr GetDevicePciInfo(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  if (!utils::cl::CheckExtension(device, "cl_khr_pci_bus_info")) {
    return cl_device_pci_bus_info_khr{0, 0, 0, 0};
  }

  cl_device_pci_bus_info_khr pci_info{};
  cl_int status = clGetDeviceInfo(device, CL_DEVICE_PCI_BUS_INFO_KHR,
                                  sizeof(cl_device_pci_bus_info_khr), &pci_info, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return pci_info;
}

static bool SetDeviceUUIDinfo(cl_device_id device, uint8_t* ptr) {
  PTI_ASSERT(device != nullptr);

  if (!utils::cl::CheckExtension(device, "cl_khr_device_uuid")) {
    SPDLOG_DEBUG("CheckExtension failed for cl_khr_device_uuid in: {}", __FUNCTION__);
    return false;
  }

  cl_uchar uuid[16];
  cl_int status = clGetDeviceInfo(device, CL_DEVICE_UUID_KHR, 16, &uuid, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  std::copy_n(static_cast<uint8_t*>(uuid), ZE_MAX_DEVICE_UUID_SIZE, ptr);
  print_uuid(ptr, "Device UUID: ");
  return true;
}

void OnEnterFunction(ClFunctionId function, cl_callback_data* data, uint64_t start,
                     ClCollector* collector);
void OnExitFunction(ClFunctionId function, cl_callback_data* data, uint64_t start, uint64_t end,
                    ClCollector* collector);

class ClCollector {
 public:  // Interface
  static std::unique_ptr<ClCollector> Create(
      cl_device_id device, CollectorOptions options, OnClKernelFinishCallback kcallback = nullptr,
      OnClApiCallsFinishCallback fcallback = nullptr,
      // OnClExtFunctionFinishCallback extfcallback = nullptr,
      void* callback_data = nullptr) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(device != nullptr);
    TraceGuard guard;

    std::string data_dir_name = utils::GetEnv("UNITRACE_DataDir");

    auto collector = std::unique_ptr<ClCollector>(
        new ClCollector(device, options, kcallback, fcallback, callback_data, data_dir_name));
    PTI_ASSERT(collector != nullptr);

    collector->KernelTracingPointsQueuesOnlyOn();
    kernel_id_ = 0;

    ClApiTracer* tracer;
    tracer = new ClApiTracer(device, TracingCallBack, collector.get());

    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer "
                << "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
      }
      collector.reset();
      return nullptr;
    }

    collector->tracer_ = tracer;
    collector->EnableTracing();

    if (collector->options_.disabled_mode) {
      SPDLOG_DEBUG("\tRunning in disabled mode");
      collector->DisableTracing();
    } else {
      SPDLOG_DEBUG("\tRunning in enabled mode");
    }

    return collector;
  }

  ~ClCollector() {
    ReleaseDeviceMap();
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  const ClKernelInfoMap& GetKernelInfoMap() const { return kernel_info_map_; }

  ClCollector(const ClCollector& copy) = delete;
  ClCollector& operator=(const ClCollector& copy) = delete;

  const ClKernelMemInfoMap& GetKernelMemInfo() const { return kernel_mem_info_map_; }

  const ClFunctionInfoMap& GetFunctionInfoMap() const { return function_info_map_; }

  uint64_t GetKernelId() const { return kernel_id_; }

  void SetKernelId(uint64_t kernel_id) { kernel_id_ = kernel_id; }

  void EnableTracing() {
    const std::lock_guard<std::mutex> lock(enable_lock_);
    SPDLOG_DEBUG("In {}: current state {} Tid: {}", __FUNCTION__, tracer_->GetTracingState(),
                 utils::GetTid());
    ClApiTracer* tracer = tracer_;
    bool enabled_already = tracer->GetTracingState();
    if (enabled_already && !lw_profiling_on) return;
    PTI_ASSERT(tracer != nullptr);

    KernelTracingPointsOn();
    for (int id = 0; id < CL_FUNCTION_COUNT; ++id) {
      if (options_.api_tracing || (kernel_tracing_points_enabled[id] && options_.kernel_tracing)) {
        SPDLOG_DEBUG("Setting functionid {} - Tid: {}", id, utils::GetTid());
        bool set = tracer->SetTracingFunction(static_cast<ClFunctionId>(id));
        PTI_ASSERT(set);
      }
    }

    lw_profiling_on = false;
    if (!enabled_already) {
      bool enabled = tracer->Enable();
      PTI_ASSERT(enabled);
    }
  }

  void DisableTracing() {
    SPDLOG_DEBUG("In {}: current state {} Tid: {}", __FUNCTION__, tracer_->GetTracingState(),
                 utils::GetTid());
    PTI_ASSERT(tracer_ != nullptr);
    // tracer_->Disable();
    // lw_profiling_on = false;
    EnableKernelTracingQueuesOnly();
  }

  pti_result EnableKernelTracingQueuesOnly() {
    ClApiTracer* tracer = tracer_;
    PTI_ASSERT(tracer != nullptr);

    bool enabled_already = tracer->GetTracingState();
    SPDLOG_DEBUG("In {}: current state {} Tid: {}", __FUNCTION__, enabled_already, utils::GetTid());
    KernelTracingPointsQueuesOnlyOn();
    for (int id = 0; id < CL_FUNCTION_COUNT; ++id) {
      if (kernel_tracing_points_enabled[id]) {
        SPDLOG_DEBUG("Setting functionid {} - Tid: {}", id, utils::GetTid());
        bool set = tracer->SetTracingFunction(static_cast<ClFunctionId>(id));
        PTI_ASSERT(set);
      }
    }

    if (!enabled_already) {
      bool enabled = tracer->Enable();
      PTI_ASSERT(enabled);
    }
    lw_profiling_on = true;
    return pti_result::PTI_SUCCESS;
  }

 private:  // Implementation Details
  ClCollector(cl_device_id device, CollectorOptions options, OnClKernelFinishCallback kcallback,
              OnClApiCallsFinishCallback fcallback,
              // OnClExtFunctionFinishCallback extfcallback,
              void* callback_data, std::string& data_dir_name)
      : options_(options),
        device_(device),
        kcallback_(kcallback),
        fcallback_(fcallback),
        // extfcallback_(extfcallback),
        callback_data_(callback_data),
        data_dir_name_(data_dir_name) {
    PTI_ASSERT(device_ != nullptr);

    device_type_ = utils::cl::GetDeviceType(device);
    PTI_ASSERT(device_type_ == CL_DEVICE_TYPE_CPU || device_type_ == CL_DEVICE_TYPE_GPU);
    CreateDeviceMap();
  }

  void CreateDeviceMap() {
    cl_device_type type = utils::cl::GetDeviceType(device_);

    cl_int status = CL_SUCCESS;
    cl_uint pcount = 0;
    status = clGetPlatformIDs(0, nullptr, &pcount);

    if ((status != CL_SUCCESS) || (pcount == 0)) {
      return;
    }

    std::vector<cl_platform_id> platforms(pcount, nullptr);
    status = clGetPlatformIDs(pcount, platforms.data(), nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    for (auto plat : platforms) {
      cl_uint dcount = 0;

      status = clGetDeviceIDs(plat, type, 0, nullptr, &dcount);
      if ((status != CL_SUCCESS) || (dcount == 0)) continue;

      std::vector<cl_device_id> devs(dcount, nullptr);
      status = clGetDeviceIDs(plat, type, dcount, devs.data(), nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      for (auto dev : devs) {
        ClDevice cd;

        cd.isroot_ = true;
        cd.id_ = dev;
        cd.parent_ = (cl_device_id)(-1);
        cl_device_partition_property props[] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
                                                CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE, 0};
        cl_uint subcount = 0;
        status = clCreateSubDevices(dev, props, 0, nullptr, &subcount);
        if ((status != CL_SUCCESS) || (subcount == 0)) {
          cd.subdevs_ = std::vector<cl_device_id>();
        } else {
          std::vector<cl_device_id> subdevs(subcount);
          status = clCreateSubDevices(dev, props, subcount, subdevs.data(), nullptr);
          PTI_ASSERT(status == CL_SUCCESS);
          for (auto subdev : subdevs) {
            ClDevice subcd;

            subcd.id_ = subdev;
            subcd.isroot_ = false;
            subcd.parent_ = dev;
            subcd.subdevs_ = std::vector<cl_device_id>();
            device_map_.insert({subdev, subcd});
          }
          cd.subdevs_ = std::move(subdevs);
        }
        device_map_.insert({dev, cd});
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
  void KernelTracingPointsOff() {
    for (int i = 0; i < CL_FUNCTION_COUNT; i++) {
      kernel_tracing_points_enabled[i] = false;
    }
  }

  void KernelTracingPointsQueuesOnlyOn() {
    KernelTracingPointsOff();
    kernel_tracing_points_enabled[CL_FUNCTION_clCreateCommandQueueWithProperties] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clCreateCommandQueue] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clReleaseCommandQueue] = true;
  }

  void KernelTracingPointsOn() {
    KernelTracingPointsOff();
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
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueSVMMemFill] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueSVMMemcpy] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueMemFillINTEL] = true;
    kernel_tracing_points_enabled[CL_FUNCTION_clEnqueueMemcpyINTEL] = true;
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
    // cl_int event_status = utils::cl::GetEventStatus(event);
    // PTI_ASSERT(event_status == CL_COMPLETE);
    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);
    cl_device_id device = utils::cl::GetDevice(queue);
    PTI_ASSERT(device != nullptr);

    auto it = device_map_.find(device);
    if (it != device_map_.end()) {
      if (it->second.isroot_) {
        // ClKernelInterval kernel_interval{ name, device, std::vector<ClDeviceInterval>()};
        int i = 0;
        if (it->second.subdevs_.size() > 0) {
          // implicit scaling in COMPOSITE mode
          for (auto subdev : it->second.subdevs_) {
            instance->device = subdev;
            instance->sub_device_list.push_back(i++);
          }
        } else {
          // FLAT mode
          instance->device = device;
        }
      } else {
        // explicit scaling in COMPOSITE mode
        instance->device = device;
      }
    }
    kernel_instance_list_.push_back(instance);
    AddKernelMemInfo(instance->props.name, instance->props.base_addr, instance->props.size);
  }

  static void ComputeHostTimestamps(const ClKernelInstance* instance, cl_ulong started,
                                    cl_ulong ended, uint64_t& host_queued, uint64_t& host_submitted,
                                    uint64_t& host_started, uint64_t& host_ended) {
    PTI_ASSERT(instance != nullptr);
    PTI_ASSERT(started < ended);

    PTI_ASSERT(instance->event != nullptr);
    cl_event event = instance->event;

    cl_ulong queued = utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_QUEUED);
    PTI_ASSERT(queued >= 0);
    cl_ulong submitted = utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_SUBMIT);
    PTI_ASSERT(submitted >= 0);

    // PTI_ASSERT(instance->device_sync <= queued);
    //  This is workaround due to driver bug. In some cases driver does not give right timestamp
    int64_t time_diff = queued - instance->device_sync;
    uint64_t time_shift = ((time_diff > 0) ? time_diff : 0);

    host_queued = instance->host_sync + time_shift;
    PTI_ASSERT(queued <= submitted);
    host_submitted = host_queued + (submitted - queued);
    PTI_ASSERT(submitted <= started);
    host_started = host_submitted + (started - submitted);
    PTI_ASSERT(started <= ended);
    host_ended = host_started + (ended - started);
  }

  void PrintOutOffloadedCommand(std::string& name, cl_device_id& device, uint64_t appended,
                                uint64_t submitted, uint64_t kernel_start, uint64_t kernel_end) {
    std::stringstream stream;
    stream << "Thread " << utils::GetTid() << " Device " << device << " : " << name << " [ns] "
           << appended << " (append) " << submitted << " (submit) " << kernel_start << " (start) "
           << kernel_end << " (end)" << std::endl;
  }

  inline cl_device_pci_bus_info_khr GetDevicePciInfo(cl_device_id device) {
    cl_device_pci_bus_info_khr pciInfo;
    cl_int status = clGetDeviceInfo(device, CL_DEVICE_PCI_BUS_INFO_KHR,
                                    sizeof(cl_device_pci_bus_info_khr), &pciInfo, nullptr);
    PTI_ASSERT(status == CL_SUCCESS);
    SPDLOG_DEBUG("In {} -- info: {}-{}-{}-{}", __FUNCTION__, pciInfo.pci_domain, pciInfo.pci_bus,
                 pciInfo.pci_device, pciInfo.pci_function);
    return pciInfo;
  }

  void ProcessKernelInstance(const ClKernelInstance* instance, int32_t tile) {
    SPDLOG_DEBUG("In {} -- 2arg", __FUNCTION__);
    PTI_ASSERT(instance != nullptr);

    PTI_ASSERT(instance->event != nullptr);
    cl_event event = instance->event;

    cl_int event_status = utils::cl::GetEventStatus(event);
    PTI_ASSERT(event_status == CL_COMPLETE);

    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);

    cl_ulong started = utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_START);
    cl_ulong ended = utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_END);
    cl_ulong time = ended - started;
    PTI_ASSERT(time > 0);

    cl_device_id device = utils::cl::GetDevice(queue);
    PTI_ASSERT(device != nullptr);
    cl_device_pci_bus_info_khr pciInfo = GetDevicePciInfo(device);

    std::string name = instance->props.name;
    PTI_ASSERT(!name.empty());

    /*
          if (options_.verbose) {
            name = GetVerboseName(&(instance->props));
          }
    */

    uint64_t host_queued = 0, host_submitted = 0;
    uint64_t host_started = 0, host_ended = 0;
    ComputeHostTimestamps(instance, started, ended, host_queued, host_submitted, host_started,
                          host_ended);

    AddKernelInfo(name, host_submitted - host_queued, host_started - host_submitted,
                  host_ended - host_started);

    [[maybe_unused]] bool implicit = false;

    if (tile >= 0) {
      auto it = device_map_.find(device);
      if (it != device_map_.end()) {
        if (it->second.isroot_ && (it->second.subdevs_.size() > 0)) {
          // implicit scaling in COMPOSITE mode
          implicit = true;
        }
      }
    }

    auto it = cl_kernel_command_properties_.find(instance->kernel_id);
    if (it == cl_kernel_command_properties_.end()) {
      cl_kernel_command_properties_.insert({instance->kernel_id, instance->props});
    }

    if (kcallback_ != nullptr) {
      // SPDLOG_DEBUG("callback kernel name {}", name);
      ZeKernelCommandExecutionRecord rec = {};
      rec.name_ = std::move(name);
      rec.queue_ = queue;
      rec.tile_ = tile;
      rec.device_ = device;
      // rec.pci_info_ = pciInfo;
      rec.pci_prop_.domain = pciInfo.pci_domain;
      rec.pci_prop_.bus = pciInfo.pci_bus;
      rec.pci_prop_.device = pciInfo.pci_device;
      rec.pci_prop_.function = pciInfo.pci_function;
      rec.start_time_ = host_started;
      rec.end_time_ = host_ended;
      rec.append_time_ = host_queued;
      rec.submit_time_ = host_submitted;

      rec.bytes_xfered_ = instance->props.bytes_transferred;

      rec.kid_ = instance->kernel_id;
      rec.tid_ = instance->tid;
      rec.cid_ = instance->corr_id_;
      rec.sycl_node_id_ = instance->sycl_node_id_;
      rec.sycl_queue_id_ = instance->sycl_queue_id_;
      rec.sycl_invocation_id_ = instance->sycl_invocation_id_;
      rec.sycl_task_begin_time_ = instance->sycl_task_begin_time_;
      rec.sycl_enqk_begin_time_ = instance->sycl_enqk_begin_time_;
      rec.source_file_name_ = instance->source_file_name_;
      rec.source_line_number_ = instance->source_line_number_;
      rec.route_ = instance->props.route;
      SPDLOG_DEBUG("callback kernel name {} - file: {}", rec.name_, rec.source_file_name_);
      // SPDLOG_DEBUG("src_device {} -- kname: {}", rec.route_.src_device_id, rec.name_);
      if (rec.route_.src_device_id != nullptr) {
        SetDeviceUUIDinfo(static_cast<cl_device_id>(rec.route_.src_device_id),
                          static_cast<uint8_t*>(rec.src_device_uuid));
        print_uuid(static_cast<uint8_t*>(rec.src_device_uuid), "Src Device UUID just set: ");
      }
      // SPDLOG_DEBUG("dst_device {} -- kname: {}", rec.route_.dst_device_id, rec.name_);
      if (rec.route_.dst_device_id != nullptr) {
        SetDeviceUUIDinfo(static_cast<cl_device_id>(rec.route_.dst_device_id),
                          static_cast<uint8_t*>(rec.dst_device_uuid));
        print_uuid(static_cast<uint8_t*>(rec.dst_device_uuid), "Dst Device UUID just set: ");
        rec.dst_pci_prop_.domain = pciInfo.pci_domain;
        rec.dst_pci_prop_.bus = pciInfo.pci_bus;
        rec.dst_pci_prop_.device = pciInfo.pci_device;
        rec.dst_pci_prop_.function = pciInfo.pci_function;
      }

      if (instance->props.type == KernelCommandType::kKernel) {
        SetDeviceUUIDinfo(static_cast<cl_device_id>(rec.device_),
                          static_cast<uint8_t*>(rec.src_device_uuid));
        print_uuid(static_cast<uint8_t*>(rec.src_device_uuid),
                   "kKernel Src Device UUID just set for kernel: ");
      }
      if (instance->props.type == KernelCommandType::kMemory) {
        rec.device_ = rec.route_.src_device_id;
        rec.dst_device_ = rec.route_.dst_device_id;
        print_uuid(static_cast<uint8_t*>(rec.src_device_uuid),
                   "kMemory Dst Device UUID just set for kernel: ");
      }
      kcallback_(callback_data_, rec);
    }
  }

  void ProcessKernelInstance(cl_event event) {
    SPDLOG_DEBUG("In {} - 1arg", __FUNCTION__);
    PTI_ASSERT(event != nullptr);
    cl_int event_status = utils::cl::GetEventStatus(event);
    if (event_status != CL_COMPLETE) {
      return;
    }

    const std::lock_guard<std::mutex> lock(lock_);
    for (auto it = kernel_instance_list_.begin(); it != kernel_instance_list_.end(); ++it) {
      ClKernelInstance* instance = *it;
      PTI_ASSERT(instance != nullptr);
      PTI_ASSERT(instance->event != nullptr);
      if (instance->event == event) {
        if (instance->sub_device_list.size()) {
          for (size_t i = 0; i < instance->sub_device_list.size(); ++i) {
            ProcessKernelInstance(instance, instance->sub_device_list[i]);
          };
        } else {
          ProcessKernelInstance(instance, -1);
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
    SPDLOG_DEBUG("In {} - 0arg", __FUNCTION__);
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
            ProcessKernelInstance(instance, instance->sub_device_list[i]);
          };
        } else {
          ProcessKernelInstance(instance, -1);
        };
        it = kernel_instance_list_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void AddKernelInfo(std::string name, uint64_t queued_time, uint64_t submit_time,
                     uint64_t execute_time) {
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

  // Callbacks
  static void OnEnterCreateCommandQueueWithProperties(cl_callback_data* data,
                                                      [[maybe_unused]] ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(data != nullptr);

    const cl_params_clCreateCommandQueueWithProperties* params =
        reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    data->correlationData[0] = reinterpret_cast<cl_ulong>(nullptr);
    cl_queue_properties* props = (cl_queue_properties*)(*(params->properties));
    if (props == nullptr) {
      props = new cl_queue_properties[3];
      props[0] = CL_QUEUE_PROPERTIES;
      props[1] = CL_QUEUE_PROFILING_ENABLE;
      props[2] = 0;
      data->correlationData[0] = reinterpret_cast<cl_ulong>(props);
    } else {
      cl_queue_properties* p = props;
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

  static void OnExitCreateCommandQueueWithProperties(cl_callback_data* data,
                                                     [[maybe_unused]] ClCollector* collector) {
    PTI_ASSERT(data != nullptr);

    cl_queue_properties* props = reinterpret_cast<cl_queue_properties*>(data->correlationData[0]);
    if (props != nullptr) {
      delete[] props;
    }
  }

  static void OnEnterCreateCommandQueue(cl_callback_data* data,
                                        [[maybe_unused]] ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    SPDLOG_DEBUG("In {}", __FUNCTION__);

    const cl_params_clCreateCommandQueue* params =
        reinterpret_cast<const cl_params_clCreateCommandQueue*>(data->functionParams);
    PTI_ASSERT(params != nullptr);
    *(params->properties) |= static_cast<unsigned long>(CL_QUEUE_PROFILING_ENABLE);
  }

  template <typename T>
  static void OnEnterEnqueueKernel(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(collector->device_ != nullptr);

    ClEnqueueData* enqueue_data = new ClEnqueueData;
    enqueue_data->event = nullptr;

    utils::cl::GetTimestamps(collector->device_, &enqueue_data->host_sync,
                             &enqueue_data->device_sync);

    const T* params = reinterpret_cast<const T*>(data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) == nullptr) {
      *(params->event) = &(enqueue_data->event);
    }

    data->correlationData[0] = reinterpret_cast<cl_ulong>(enqueue_data);
  }

  template <typename T>
  static void OnExitEnqueueKernel(cl_callback_data* data, ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
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
      instance->props.type = KernelCommandType::kKernel;
      instance->sycl_queue_id_ = sycl_data_kview.sycl_queue_id_;
      instance->source_file_name_ = sycl_data_kview.source_file_name_;
      instance->source_line_number_ = sycl_data_kview.source_line_number_;
      instance->sycl_task_begin_time_ = sycl_data_kview.sycl_task_begin_time_;
      instance->sycl_enqk_begin_time_ = sycl_data_kview.sycl_enqk_begin_time_;
      sycl_data_kview.tid_ = instance->tid = utils::GetTid();
      instance->event = **(params->event);

      cl_kernel kernel = *(params->kernel);
      instance->props.name = utils::cl::GetKernelName(kernel, collector->options_.demangle);

      cl_command_queue queue = *(params->commandQueue);
      PTI_ASSERT(queue != nullptr);
      cl_device_id device = utils::cl::GetDevice(queue);
      PTI_ASSERT(device != nullptr);

      // SetDeviceUUIDinfo(static_cast<cl_device_id>(rec.route_.src_device_id),
      //                      static_cast<uint8_t*>(rec.src_device_uuid));
      // print_uuid(static_cast<uint8_t*>(rec.src_device_uuid), "Src Device UUID just set: ");
      size_t simd_width = utils::cl::GetKernelSimdWidth(device, kernel);
      PTI_ASSERT(simd_width > 0);

      instance->props.simd_width = simd_width;
      instance->props.bytes_transferred = 0;

      // collector->CalculateKernelGlobalSize(params, &instance->props);
      // collector->CalculateKernelLocalSize(params, &instance->props);

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
        collector->kprops_.insert(
            std::pair<std::string, ClKernelProps>(instance->props.name, instance->props));
      }

      // instance->kernel_id = UniKernelInstanceId::GetKernelInstanceId();
      instance->kernel_id = UniKernelId::GetKernelId();
      collector->SetKernelId(instance->kernel_id);

      if (sycl_data_kview.cid_) {
        instance->corr_id_ = sycl_data_kview.cid_;
      } else {
        instance->corr_id_ = UniCorrId::GetUniCorrId();
      }
      SPDLOG_DEBUG("In {} - corrId {}", __FUNCTION__, instance->corr_id_);

      PTI_ASSERT(enqueue_data != nullptr);
      instance->device_sync = enqueue_data->device_sync;
      instance->host_sync = enqueue_data->host_sync;

      collector->AddKernelInstance(instance);

      delete enqueue_data;
    }
  }

  static void GetHostRouteInfo(cl_command_queue queue, UniMemoryCommandRoute& route, bool isRead) {
    cl_device_id aDevice = utils::cl::GetDevice(queue);
    if (isRead) {
      route.src_device_id = aDevice;
      route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
      route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    } else {
      route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
      route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
      route.dst_device_id = aDevice;
    }
  }

  // Memory related apis -- setsup data for view handler related buffer insertions.
  static void OnExitEnqueueTransfer(std::string name, size_t bytes_transferred, cl_event* event,
                                    cl_callback_data* data, ClCollector* collector,
                                    UniMemoryCommandRoute* route = nullptr) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
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

    // instance->kernel_id = UniKernelInstanceId::GetKernelInstanceId();
    instance->kernel_id = UniKernelId::GetKernelId();
    instance->props.type = KernelCommandType::kMemory;
    collector->SetKernelId(instance->kernel_id);
    sycl_data_mview.kid_ = instance->kernel_id;
    sycl_data_mview.tid_ = instance->tid = utils::GetTid();
    if (sycl_data_mview.cid_) {
      instance->corr_id_ = sycl_data_mview.cid_;
    } else {
      instance->corr_id_ = UniCorrId::GetUniCorrId();
      SPDLOG_DEBUG("In {} - corrId new {}", __FUNCTION__, instance->corr_id_);
    }

    if (route) {
      // instance->props.type = KernelCommandType::kMemory;
      instance->props.route = *route;
      instance->props.name += "(" + instance->props.route.StringifyTypesCompact() + ")";
    }

    instance->sycl_node_id_ = sycl_data_mview.sycl_node_id_;
    instance->sycl_queue_id_ = sycl_data_mview.sycl_queue_id_;
    instance->sycl_invocation_id_ = sycl_data_mview.sycl_invocation_id_;
    instance->sycl_task_begin_time_ = sycl_data_mview.sycl_task_begin_time_;
    instance->source_file_name_ = sycl_data_mview.source_file_name_;
    instance->source_line_number_ = sycl_data_mview.source_line_number_;

    ClEnqueueData* enqueue_data = reinterpret_cast<ClEnqueueData*>(data->correlationData[0]);
    PTI_ASSERT(enqueue_data != nullptr);
    instance->device_sync = enqueue_data->device_sync;
    instance->host_sync = enqueue_data->host_sync;

    collector->AddKernelInstance(instance);

    delete enqueue_data;
  }

  static void OnExitEnqueueReadBuffer(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      UniMemoryCommandRoute route;
      const cl_params_clEnqueueReadBuffer* params =
          reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);
      GetHostRouteInfo(*params->commandQueue, route, true);

      OnExitEnqueueTransfer("clEnqueueReadBuffer", *(params->cb), *(params->event), data, collector,
                            &route);

      if (*params->blockingRead) {
        collector->ProcessKernelInstances();
      }
    }
  }

  // TODO-OCL Route info needed uuid,src,dst
  static void OnExitEnqueueWriteBuffer(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      UniMemoryCommandRoute route;
      const cl_params_clEnqueueWriteBuffer* params =
          reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);
      GetHostRouteInfo(*params->commandQueue, route, false);

      OnExitEnqueueTransfer("clEnqueueWriteBuffer", *(params->cb), *(params->event), data,
                            collector, &route);

      if (*params->blockingWrite) {
        collector->ProcessKernelInstances();
      }
    }
  }

  static void OnExitEnqueueCopyBuffer(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBuffer* params =
          reinterpret_cast<const cl_params_clEnqueueCopyBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer("clEnqueueCopyBuffer", *(params->cb), *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueFillBuffer(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillBuffer* params =
          reinterpret_cast<const cl_params_clEnqueueFillBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      OnExitEnqueueTransfer("clEnqueueFillBuffer", *(params->size), *(params->event), data,
                            collector);
    }
  }

  static bool IsMemcpyP2P(cl_context aContext, const void* srcPtr, const void* dstPtr,
                          UniMemoryCommandRoute& route, ClCollector* collector) {
    constexpr auto kMaxDevices = 10;  // assume 10 max number of devices returned.
    cl_device_id aSrcDevice;
    cl_device_id aDstDevice;

    cl_int status = collector->tracer_->clGetMemAllocInfo_(
        aContext, srcPtr, CL_MEM_ALLOC_DEVICE_INTEL, sizeof(cl_device_id), &aSrcDevice, NULL);
    PTI_ASSERT(status == CL_SUCCESS);
    route.src_device_id = aSrcDevice;
    status = collector->tracer_->clGetMemAllocInfo_(aContext, dstPtr, CL_MEM_ALLOC_DEVICE_INTEL,
                                                    sizeof(cl_device_id), &aDstDevice, NULL);
    PTI_ASSERT(status == CL_SUCCESS);
    route.dst_device_id = aDstDevice;

    cl_device_id listDevices[kMaxDevices];
    cl_uint numDevices;
    status = clGetContextInfo(aContext, CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint), &numDevices, NULL);
    PTI_ASSERT(status == CL_SUCCESS);
    // std::cout << "Number of Devices in Context(" << aContext << "): " << numDevices << "\n";

    status = clGetContextInfo(aContext, CL_CONTEXT_DEVICES, sizeof(listDevices), listDevices, NULL);
    PTI_ASSERT(status == CL_SUCCESS);
    bool inSameContext = false;
    bool foundSrcDevInContext = false;
    bool foundDstDevInContext = false;
    for (cl_uint i = 0; i < numDevices; i++) {
      foundSrcDevInContext = foundSrcDevInContext || (listDevices[i] == aSrcDevice);
      foundDstDevInContext = foundDstDevInContext || (listDevices[i] == aDstDevice);
      if (foundSrcDevInContext && foundDstDevInContext) {
        // std::cout << "In Same Context Last Device Id: " << listDevices[i] << "\n";
        inSameContext = true;
        break;
      }
    }
    if ((aSrcDevice != aDstDevice) && inSameContext) return true;
    // if (inSameContext) std::cout << "Same Devices: \n";
    return false;
  }

  static pti_view_memory_type GetMemType(cl_unified_shared_memory_type_intel& aMemType) {
    switch (aMemType) {
      case CL_MEM_TYPE_HOST_INTEL:
        // std::cout << "Host Memory\n";
        return pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
        break;
      case CL_MEM_TYPE_SHARED_INTEL:
        // std::cout << "Shared Memory\n";
        return pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
        break;
      case CL_MEM_TYPE_DEVICE_INTEL:
        return pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
        // std::cout << "Device Memory\n";
        break;
      default:
        // std::cout << "Unknown Memory\n";
        return pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
        break;
    };
  }

  // TODO-OCL add USM Intel extension method for Set
  static void OnExitEnqueueSVMMemFill(cl_callback_data* data, ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    UniMemoryCommandRoute route;

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueSVMMemFill* params =
          reinterpret_cast<const cl_params_clEnqueueSVMMemFill*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      cl_context aContext;
      cl_int status = clGetCommandQueueInfo(*params->commandQueue, CL_QUEUE_CONTEXT,
                                            sizeof(cl_context), &aContext, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      cl_unified_shared_memory_type_intel aMemType;
      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->svmPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);

      route.dst_type = GetMemType(aMemType);
      route.dst_device_id = utils::cl::GetDevice(*params->commandQueue);
      OnExitEnqueueTransfer("clEnqueueSVMMemFill", *(params->size), *(params->event), data,
                            collector, &route);
    }
  }

  // TODO-OCL add USM Intel extension method for Set
  static void OnExitEnqueueMemFillINTEL(cl_callback_data* data, ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    UniMemoryCommandRoute route;

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueMemFillINTEL* params =
          reinterpret_cast<const cl_params_clEnqueueMemFillINTEL*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      cl_context aContext;
      cl_int status = clGetCommandQueueInfo(*params->commandQueue, CL_QUEUE_CONTEXT,
                                            sizeof(cl_context), &aContext, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      cl_unified_shared_memory_type_intel aMemType;
      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->dstPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      route.dst_type = GetMemType(aMemType);
      route.dst_device_id = utils::cl::GetDevice(*params->commandQueue);
      OnExitEnqueueTransfer("clEnqueueMemFillINTEL", *(params->size), *(params->event), data,
                            collector, &route);
    }
  }

  static void OnExitEnqueueSVMMemcpy(cl_callback_data* data, ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    const cl_params_clGetExtensionFunctionAddressForPlatform* params =
        reinterpret_cast<const cl_params_clGetExtensionFunctionAddressForPlatform*>(
            data->functionParams);
    PTI_ASSERT(params != nullptr);
    UniMemoryCommandRoute route;

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueSVMMemcpy* params =
          reinterpret_cast<const cl_params_clEnqueueSVMMemcpy*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      cl_context aContext;
      cl_int status = clGetCommandQueueInfo(*params->commandQueue, CL_QUEUE_CONTEXT,
                                            sizeof(cl_context), &aContext, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      cl_unified_shared_memory_type_intel aSrcMemType;
      cl_unified_shared_memory_type_intel aDstMemType;

      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->srcPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aSrcMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->dstPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aDstMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);

      route.src_type = GetMemType(aSrcMemType);
      route.dst_type = GetMemType(aDstMemType);
      route.peer_2_peer = IsMemcpyP2P(aContext, *params->srcPtr, *params->dstPtr, route, collector);
      OnExitEnqueueTransfer("clEnqueueSVMMemcpy", *(params->size), *(params->event), data,
                            collector, &route);
    }
  }

  static void OnExitEnqueueMemcpyINTEL(cl_callback_data* data, ClCollector* collector) {
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);
    const cl_params_clGetExtensionFunctionAddressForPlatform* params =
        reinterpret_cast<const cl_params_clGetExtensionFunctionAddressForPlatform*>(
            data->functionParams);
    PTI_ASSERT(params != nullptr);
    UniMemoryCommandRoute route;

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueMemcpyINTEL* params =
          reinterpret_cast<const cl_params_clEnqueueMemcpyINTEL*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      cl_context aContext;
      cl_int status = clGetCommandQueueInfo(*params->commandQueue, CL_QUEUE_CONTEXT,
                                            sizeof(cl_context), &aContext, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      cl_unified_shared_memory_type_intel aSrcMemType;
      cl_unified_shared_memory_type_intel aDstMemType;

      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->srcPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aSrcMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);
      status = collector->tracer_->clGetMemAllocInfo_(
          aContext, *params->dstPtr, CL_MEM_ALLOC_TYPE_INTEL,
          sizeof(cl_unified_shared_memory_type_intel), &aDstMemType, NULL);
      PTI_ASSERT(status == CL_SUCCESS);

      route.src_type = GetMemType(aSrcMemType);
      route.dst_type = GetMemType(aDstMemType);

      route.peer_2_peer = IsMemcpyP2P(aContext, *params->srcPtr, *params->dstPtr, route, collector);

      OnExitEnqueueTransfer("clEnqueueMemcpyINTEL", *(params->size), *(params->event), data,
                            collector, &route);
    }
  }

  static void OnExitEnqueueReadBufferRect(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      UniMemoryCommandRoute route;
      const cl_params_clEnqueueReadBufferRect* params =
          reinterpret_cast<const cl_params_clEnqueueReadBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);
      GetHostRouteInfo(*params->commandQueue, route, true);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueReadBufferRect", bytes_transferred, *(params->event), data,
                            collector, &route);
    }
  }

  static void OnExitEnqueueWriteBufferRect(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteBufferRect* params =
          reinterpret_cast<const cl_params_clEnqueueWriteBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueWriteBufferRect", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueCopyBufferRect(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferRect* params =
          reinterpret_cast<const cl_params_clEnqueueCopyBufferRect*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *params->region;
      PTI_ASSERT(region != nullptr);
      size_t bytes_transferred = region[0] * region[1] * region[2];

      OnExitEnqueueTransfer("clEnqueueCopyBufferRect", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueReadImage(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueReadImage* params =
          reinterpret_cast<const cl_params_clEnqueueReadImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueReadImage", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueWriteImage(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueWriteImage* params =
          reinterpret_cast<const cl_params_clEnqueueWriteImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueWriteImage", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueCopyImage(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImage* params =
          reinterpret_cast<const cl_params_clEnqueueCopyImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->srcImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyImage", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueFillImage(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueFillImage* params =
          reinterpret_cast<const cl_params_clEnqueueFillImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->image), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueFillImage", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueCopyImageToBuffer(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyImageToBuffer* params =
          reinterpret_cast<const cl_params_clEnqueueCopyImageToBuffer*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->srcImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyImageToBuffer", bytes_transferred, *(params->event), data,
                            collector);
    }
  }

  static void OnExitEnqueueCopyBufferToImage(cl_callback_data* data, ClCollector* collector) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(collector != nullptr);

    cl_int* return_value = reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      const cl_params_clEnqueueCopyBufferToImage* params =
          reinterpret_cast<const cl_params_clEnqueueCopyBufferToImage*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const size_t* region = *(params->region);
      PTI_ASSERT(region != nullptr);

      size_t element_size = 0;
      cl_int status = clGetImageInfo(*(params->dstImage), CL_IMAGE_ELEMENT_SIZE, sizeof(size_t),
                                     &element_size, nullptr);
      PTI_ASSERT(status == CL_SUCCESS);

      size_t bytes_transferred = region[0] * region[1] * region[2] * element_size;
      OnExitEnqueueTransfer("clEnqueueCopyBufferToImage", bytes_transferred, *(params->event), data,
                            collector);
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

    const cl_params_clReleaseEvent* params =
        reinterpret_cast<const cl_params_clReleaseEvent*>(data->functionParams);
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
      const cl_params_clWaitForEvents* params =
          reinterpret_cast<const cl_params_clWaitForEvents*>(data->functionParams);
      PTI_ASSERT(params != nullptr);

      const cl_event* event_list = *(params->eventList);
      if (event_list != nullptr) {
        for (cl_uint i = 0; i < *(params->numEvents); ++i) {
          collector->ProcessKernelInstance(event_list[i]);
        }
      }
    }
  }

  static void KernelTracingCallBack(ClFunctionId function, cl_callback_data* callback_data,
                                    void* user_data) {
    if (TraceGuard::Inactive()) return;

    TraceGuard guard;

    ClCollector* collector = reinterpret_cast<ClCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);

    if (function == CL_FUNCTION_clCreateCommandQueueWithProperties) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterCreateCommandQueueWithProperties(callback_data, collector);
      } else {
        OnExitCreateCommandQueueWithProperties(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clCreateCommandQueue) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterCreateCommandQueue(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueNDRangeKernel) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(callback_data, collector);
      } else {
        OnExitEnqueueKernel<cl_params_clEnqueueNDRangeKernel>(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueTask) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueTask>(callback_data, collector);
      } else {
        OnExitEnqueueKernel<cl_params_clEnqueueTask>(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueReadBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueWriteBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueCopyBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueFillBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueFillBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueFillBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueSVMMemFill) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueSVMMemFill>(callback_data, collector);
      } else {
        OnExitEnqueueSVMMemFill(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueSVMMemcpy) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueSVMMemcpy>(callback_data, collector);
      } else {
        OnExitEnqueueSVMMemcpy(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueMemFillINTEL) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueMemFillINTEL>(callback_data, collector);
      } else {
        OnExitEnqueueMemFillINTEL(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueMemcpyINTEL) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueMemcpyINTEL>(callback_data, collector);
      } else {
        OnExitEnqueueMemcpyINTEL(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadBufferRect) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadBufferRect>(callback_data, collector);
      } else {
        OnExitEnqueueReadBufferRect(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteBufferRect) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteBufferRect>(callback_data, collector);
      } else {
        OnExitEnqueueWriteBufferRect(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueCopyBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueReadImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueReadImage>(callback_data, collector);
      } else {
        OnExitEnqueueReadImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueWriteImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueWriteImage>(callback_data, collector);
      } else {
        OnExitEnqueueWriteImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImage>(callback_data, collector);
      } else {
        OnExitEnqueueCopyImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueFillImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueFillImage>(callback_data, collector);
      } else {
        OnExitEnqueueFillImage(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyImageToBuffer) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyImageToBuffer>(callback_data, collector);
      } else {
        OnExitEnqueueCopyImageToBuffer(callback_data, collector);
      }
    } else if (function == CL_FUNCTION_clEnqueueCopyBufferToImage) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueKernel<cl_params_clEnqueueCopyBufferToImage>(callback_data, collector);
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

  static void TracingCallBack(ClFunctionId function, cl_callback_data* callback_data,
                              void* user_data) {
    if (TraceGuard::Inactive()) return;
    SPDLOG_DEBUG("In {}", __FUNCTION__);

    ClCollector* collector = reinterpret_cast<ClCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(callback_data != nullptr);
    PTI_ASSERT(callback_data->correlationData != nullptr);

    uint64_t end_time;
    uint32_t cid_ = 0;
    if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
      // take end timestamp first to avoid tool overhead
      end_time = collector->GetTimestamp();
    }

    if (collector->options_.kernel_tracing && collector->kernel_tracing_points_enabled[function]) {
      KernelTracingCallBack(function, callback_data, user_data);
    }

    TraceGuard guard;
    if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
      cl_instance_api_data.start_time = collector->GetTimestamp();
    } else {
      if (collector->options_.kernel_tracing &&
          collector->kernel_tracing_points_enabled[function]) {
        if (sycl_data_kview.cid_) {
          cid_ = sycl_data_kview.cid_;
        } else if (sycl_data_mview.cid_) {
          cid_ = sycl_data_mview.cid_;
        }
      }
      cl_instance_api_data.end_time = end_time;
      uint64_t start_time = cl_instance_api_data.start_time;
      collector->AddFunctionTime(callback_data->functionName, end_time - start_time);

      if (collector->fcallback_ != nullptr) {
        ZeKernelCommandExecutionRecord rec = {};
        // FLOW_DIR flow_dir = FLOW_NUL;
        std::vector<uint64_t> kernel_id;
        if (function == CL_FUNCTION_clEnqueueNDRangeKernel ||
            function == CL_FUNCTION_clWaitForEvents ||
            function == CL_FUNCTION_clEnqueueReadBuffer ||
            function == CL_FUNCTION_clEnqueueWriteBuffer) {
          // flow_dir = (function == CL_FUNCTION_clWaitForEvents ? FLOW_D2H : FLOW_H2D);
          kernel_id.push_back(collector->GetKernelId());
        }

        rec.start_time_ = start_time;
        rec.end_time_ = end_time;
        rec.callback_id_ = function + 1;
        rec.pid_ = utils::GetPid();
        rec.tid_ = utils::GetTid();

        SPDLOG_DEBUG("In {} - function {} - callbackId {}", __FUNCTION__,
                     callback_data->functionName, rec.callback_id_);
        if (cid_) {
          rec.cid_ = cid_;  // if we are a kernel function.
        } else {
          rec.cid_ = UniCorrId::GetUniCorrId();
        }

        collector->fcallback_(collector->callback_data_, rec);
        // collector->fcallback_((kernel_id.empty() ? nullptr : &kernel_id), flow_dir, start_time,
        //                       end_time);
      }
    }
  }

  uint64_t GetTimestamp() const { return utils::GetSystemTime(); }

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

  // Data
  ClApiTracer* tracer_ = nullptr;

  CollectorOptions options_;

  cl_device_id device_ = nullptr;

  OnClKernelFinishCallback kcallback_ = nullptr;
  OnClApiCallsFinishCallback fcallback_ = nullptr;
  // OnClExtFunctionFinishCallback extfcallback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  std::mutex enable_lock_;
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

  // OnClApiCallsFinishCallback callback_ = nullptr;

  ClFunctionInfoMap function_info_map_;

  std::map<std::string, ClKernelProps> kprops_;
  std::string data_dir_name_;

  std::vector<ClKernelProfileRecord> profile_records_;
  bool lw_profiling_on = false;

  friend class ClExtCollector;
};

#endif  // PTI_TOOLS_UNITRACE_CL_COLLECTOR_H_
