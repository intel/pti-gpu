//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
#define PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_

/*
 * Level zero collection methods:  hook into the level zero api to capture
 * kernel/memory movement, collect relevent timing and handles key structures
 * and issue callbacks to buffer,etc interfaces to capture this data in view
 * records.
 */

#include <dlfcn.h>
#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "collector_options.h"
#include "overhead_kinds.h"
#include "unikernel.h"
#include "utils.h"
#include "ze_event_cache.h"
#include "ze_utils.h"

struct CallbacksEnabled {
  std::atomic<bool> acallback = false;
};

struct ZeInstanceData {
  uint64_t start_time_host;
  uint64_t timestamp_host;    // in ns
  uint64_t timestamp_device;  // in ticks
  uint64_t end_time_host;
  uint64_t kid;  // passing kid from enter callback to exit callback
};

thread_local ZeInstanceData ze_instance_data;

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

enum class KernelCommandType { kInvalid = 0, kKernel = 1, kMemory = 2, kCommand = 3 };

struct ZeKernelCommandProps {
  std::string name;
  KernelCommandType type = KernelCommandType::kInvalid;
  size_t simd_width;
  size_t bytes_transferred;
  std::array<uint32_t, 3> group_count;
  std::array<uint32_t, 3> group_size;
  size_t value_size;
  std::byte* value_array;
  ze_device_handle_t src_device = nullptr;  // Device for p2p memcpy, source of copy data
  ze_device_handle_t dst_device = nullptr;  // Device for p2p memcpy, destination of copy data
};

struct ZeKernelCommand {
  ZeKernelCommandProps props;
  uint64_t device_timer_frequency_;
  uint64_t device_timer_mask_;
  ze_event_handle_t event = nullptr;
  ze_device_handle_t device =
      nullptr;  // Device where the operation is submitted, associated with command list
  uint64_t kernel_id = 0;
  uint64_t append_time = 0;
  ze_command_list_handle_t command_list = nullptr;
  ze_command_queue_handle_t queue = nullptr;
  ze_fence_handle_t fence;
  uint64_t submit_time = 0;          // in ns
  uint64_t submit_time_device_ = 0;  // in ticks
  uint64_t tid = 0;
  uint64_t sycl_node_id_ = 0;
  uint32_t sycl_invocation_id_ = 0;
  uint64_t sycl_task_begin_time_ = 0;
  uint64_t sycl_enqk_begin_time_ = 0;
  std::string source_file_name_;
  uint32_t source_line_number_ = 0;
  uint32_t corr_id_ = 0;
};

struct ZeCommandQueue {
  ze_command_queue_handle_t queue_;
  ze_context_handle_t context_;
  ze_device_handle_t device_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;
};

struct ZeCommandListInfo {
  std::vector<ZeKernelCommand*> kernel_commands;
  ze_context_handle_t context;
  ze_device_handle_t device;
  bool immediate;
  std::pair<uint32_t, uint32_t> oi_pair;
};

struct ZeDeviceDescriptor {
  uint64_t host_time_origin = 0;
  uint64_t device_time_origin = 0;
  uint64_t device_timer_frequency = 0;
  uint64_t device_timer_mask = 0;
  ze_driver_handle_t driver = nullptr;
  ze_context_handle_t context = nullptr;
  ze_pci_ext_properties_t pci_properties{};
};

using ZeKernelGroupSizeMap = std::map<ze_kernel_handle_t, ZeKernelGroupSize>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;
using ZeImageSizeMap = std::map<ze_image_handle_t, size_t>;
using ZeDeviceMap = std::map<ze_device_handle_t, std::vector<ze_device_handle_t>>;

using OnZeKernelFinishCallback = void (*)(void*, std::vector<ZeKernelCommandExecutionRecord>&);

// Work-around for ensuring ZE_ENABLE_TRACING_LAYER=1 is set before zeInit() is
// called. Not guarenteed to work if user calls zeInit() before main() in their
// program.
// Warning: Do not add a dependency on another static variable or there is a
// risk of undefined behavior.
// TODO: Fix when there's a better solution.
class GlobalZeInitializer {
 public:
  inline static ze_result_t Initialize() {
    utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
    return zeInit(ZE_INIT_FLAG_GPU_ONLY);
  }

  inline static ze_result_t result_ = Initialize();
};

class ZeCollector {
 public:  // Interface
  ZeCollector(const ZeCollector&) = delete;
  ZeCollector& operator=(const ZeCollector&) = delete;
  ZeCollector(ZeCollector&&) = delete;
  ZeCollector& operator=(ZeCollector&&) = delete;

  static std::unique_ptr<ZeCollector> Create(CollectorOptions options,
                                             OnZeKernelFinishCallback acallback = nullptr,
                                             void* callback_data = nullptr) {
    if (GlobalZeInitializer::result_ != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN("Unable to initialize ZeCollector, error code {0:x}",
                  static_cast<std::size_t>(GlobalZeInitializer::result_));
      return nullptr;
    }

    ze_api_version_t version = utils::ze::GetVersion();
    PTI_ASSERT(ZE_MAJOR_VERSION(version) >= 1 && ZE_MINOR_VERSION(version) >= 2);

    auto collector =
        std::unique_ptr<ZeCollector>(new ZeCollector(options, acallback, callback_data));
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector.get()};
    zel_tracer_handle_t tracer = nullptr;
    overhead::Init();
    status = zelTracerCreate(&tracer_desc, &tracer);
    {
      std::string o_api_string = "zelTracerCreate";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }

    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN("Unable to create Level Zero tracer, error code {0:x}",
                  static_cast<std::size_t>(status));
      return nullptr;
    }

    collector->EnableTracing(tracer);

    collector->tracer_ = tracer;

    return collector;
  }

  ~ZeCollector() {
    ProcessCalls(nullptr, nullptr);
    if (tracer_ != nullptr) {
#if !defined(_WIN32)
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
    }
  }

  void EnableTracer() { cb_enabled_.acallback = true; }

  void DisableTracer() { cb_enabled_.acallback = false; }

  void DisableTracing() {
    // PTI_ASSERT(tracer_ != nullptr);
#if !defined(_WIN32)
    overhead::Init();
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    {
      std::string o_api_string = "zelTracerSetEnabled";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
  }

 private:  // Implementation
  ZeCollector(CollectorOptions options, OnZeKernelFinishCallback acallback, void* callback_data)
      : options_(options),
        acallback_(acallback),
        callback_data_(callback_data),
        event_cache_(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
    CreateDeviceMap();
  }

  void CreateDeviceMap() {
    const auto drivers = utils::ze::GetDriverList();
    for (auto* const driver : drivers) {
      const auto devices = utils::ze::GetDeviceList(driver);
      for (auto* const device : devices) {
        device_descriptors_[device] = GetZeDeviceDescriptor(device);

        const auto sub_devices = utils::ze::GetSubDeviceList(device);
        device_map_[device] = sub_devices;
        for (auto* const sub_device : sub_devices) {
          device_descriptors_[sub_device] = GetZeDeviceDescriptor(sub_device);
        }
      }
    }
  }

  static ZeDeviceDescriptor GetZeDeviceDescriptor(const ze_device_handle_t device) {
    ZeDeviceDescriptor desc = {};
    desc.device_timer_frequency = utils::ze::GetDeviceTimerFrequency(device);
    desc.device_timer_mask = utils::ze::GetDeviceTimestampMask(device);
    ze_pci_ext_properties_t pci_device_properties;

    overhead::Init();
    ze_result_t status = zeDevicePciGetPropertiesExt(device, &pci_device_properties);
    {
      std::string o_api_string = "zeDevicePciGetPropertiesExt";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    desc.pci_properties = pci_device_properties;
    uint64_t host_time = 0;
    uint64_t ticks = 0;
    uint64_t device_time = 0;

    overhead::Init();
    status = zeDeviceGetGlobalTimestamps(device, &host_time, &ticks);
    {
      std::string o_api_string = "zeDeviceGetGlobalTimestamps";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    device_time = ticks & desc.device_timer_mask;
    if (desc.device_timer_frequency) {
      device_time = device_time * NSEC_IN_SEC / desc.device_timer_frequency;
    }
    desc.host_time_origin = host_time;
    desc.device_time_origin = device_time;
    return desc;
  }

  const ZeCommandListInfo& GetCommandListInfoConst(ze_command_list_handle_t clist_handle) {
    std::shared_lock lock(command_list_map_mutex_);
    PTI_ASSERT(command_list_map_.count(clist_handle) == 1);
    return command_list_map_[clist_handle];
  }

  ZeCommandListInfo& GetCommandListInfo(ze_command_list_handle_t clist_handle) {
    std::shared_lock lock(command_list_map_mutex_);
    PTI_ASSERT(command_list_map_.count(clist_handle) == 1);
    return command_list_map_[clist_handle];
  }

  auto FindCommandListInfo(ze_command_list_handle_t clist_handle) {
    std::shared_lock lock(command_list_map_mutex_);
    return command_list_map_.find(clist_handle);
  }

  bool IsDeviceExist(ze_device_handle_t device_handle) {
    std::shared_lock lock(dev_uuid_map_mutex_);
    return dev_uuid_map_.find(device_handle) != dev_uuid_map_.end();
  }

  bool CopyDeviceUUIDTo(ze_device_handle_t device_handle, uint8_t* ptr) {
    std::shared_lock lock(dev_uuid_map_mutex_);
    if (dev_uuid_map_.find(device_handle) != dev_uuid_map_.end()) {
      std::copy_n(dev_uuid_map_[device_handle], ZE_MAX_DEVICE_UUID_SIZE, ptr);
      return true;
    }
    return false;
  }

  bool SaveDeviceUUID(ze_device_handle_t device_handle, uint8_t* ptr) {
    if (!IsDeviceExist(device_handle)) {
      std::unique_lock lock(dev_uuid_map_mutex_);
      std::copy_n(ptr, ZE_MAX_DEVICE_UUID_SIZE, dev_uuid_map_[device_handle]);
      return true;
    }
    return false;
  }

  void ProcessCall(ze_event_handle_t event, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeEventQueryStatus(event);
    {
      std::string o_api_string = "zeEventQueryStatus";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = *it;
      PTI_ASSERT(command != nullptr);
      if (command->event != nullptr) {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        }
        if (status == ZE_RESULT_SUCCESS) {
          if (command->event == event) {
            ProcessCall(command, kids, kcexecrec);
            done = true;
          } else {
            ProcessCall(command, nullptr, kcexecrec);
          }
        }
      }
      if (command->event == nullptr) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        it++;
      }
      if (done) {
        break;
      }
    }
  }

  void ProcessCall(ze_fence_handle_t fence, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeFenceQueryStatus(fence);
    {
      std::string o_api_string = "zeFenceQueryStatus";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = *it;
      PTI_ASSERT(command != nullptr);
      if ((command->fence != nullptr) && (command->fence == fence)) {
        ProcessCall(command, kids, kcexecrec);
        done = true;
      } else {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        }
        if ((command->event != nullptr) && (status == ZE_RESULT_SUCCESS)) {
          ProcessCall(command, nullptr, kcexecrec);
        }
      }
      if (command->event == nullptr) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        it++;
      }
      if (done) {
        break;
      }
    }
  }

  constexpr uint64_t ComputeDuration(uint64_t start, uint64_t end, uint64_t freq, uint64_t mask) {
    uint64_t duration = 0;
    if (start <= end) {
      duration = (end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    } else {  // Timer Overflow
      duration = ((mask + 1ULL) + end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    }
    return duration;
  }

  inline void GetHostTime(const ZeKernelCommand* command, const ze_kernel_timestamp_result_t& ts,
                          uint64_t& start, uint64_t& end) {
    uint64_t device_freq = command->device_timer_frequency_;
    uint64_t device_mask = command->device_timer_mask_;

    uint64_t device_start = ts.global.kernelStart & device_mask;
    uint64_t device_end = ts.global.kernelEnd & device_mask;

    // Why submit_time_device_ and time_shift ?
    //
    // - All times reported by PTI_VIEW in CPU (aka Host) timescale
    // - However GPU "commands" (kernel & memory transfers) start/end reported in GPU timescale
    // - There is significant time drift between CPU and GPU, so to cope wth it - need to
    // "sync" often calling zeDeviceGetGlobalTimestamps,
    //  where command->submit_time_device_ comes with GPU time
    //        command->submit_time         comes with CPU time
    //
    // "sync" points are made around "command(s)" submit to GPU:
    // - at Enter to CommandListAppendLaunch<...>  time for an Immediate Command List
    // - at Enter to CommandQueueExecuteCommandLists for not Immediate CommandLists

    //  GPU time mask applied to the GPU time to remove some spiritous bits (in case they made
    //  there)
    uint64_t device_submit_time = (command->submit_time_device_ & device_mask);

    // time_shift calculated in GPU scale between sync point and GPU command start,
    // then it recalculated to CPU timescale units
    uint64_t time_shift = 0;

    if (device_start > device_submit_time) {
      time_shift = (device_start - device_submit_time) * NSEC_IN_SEC / device_freq;
    } else {
      // overflow
      time_shift =
          (device_mask - device_submit_time + 1 + device_start) * NSEC_IN_SEC / device_freq;
    }

    // GPU command duration recalculated to CPU time scale units
    uint64_t duration = ComputeDuration(device_start, device_end, device_freq, device_mask);

    // here GPU command start and end (on GPU) are calculated in CPU timescale
    start = command->submit_time + time_shift;
    end = start + duration;
  }

  void ProcessCall(const ZeKernelCommand* command, const ze_kernel_timestamp_result_t& timestamp,
                   int tile, bool /*in_summary*/,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    uint64_t host_start = 0;
    uint64_t host_end = 0;
    GetHostTime(command, timestamp, host_start, host_end);
    PTI_ASSERT(host_start <= host_end);

    std::string name = command->props.name;

    PTI_ASSERT(!name.empty());

    if (kcexecrec && acallback_) {
      ZeKernelCommandExecutionRecord rec = {};

      rec.kid_ = command->kernel_id;
      rec.tid_ = command->tid;
      rec.cid_ = command->corr_id_;
      rec.append_time_ = command->append_time;
      rec.submit_time_ = command->submit_time;
      rec.start_time_ = host_start;
      rec.end_time_ = host_end;
      PTI_ASSERT(queue_ordinal_index_map_.count(command->queue) != 0);
      std::pair<uint32_t, uint32_t> oi;
      oi = queue_ordinal_index_map_[command->queue];
      rec.engine_ordinal_ = oi.first;
      rec.engine_index_ = oi.second;
      rec.tile_ = tile;
      auto it = device_descriptors_.find(command->device);
      PTI_ASSERT(it != device_descriptors_.end());
      rec.pci_prop_ = it->second.pci_properties;
      rec.name_ = std::move(name);
      rec.queue_ = command->queue;
      rec.device_ = command->device;
      CopyDeviceUUIDTo(command->props.src_device, static_cast<uint8_t*>(rec.src_device_uuid));
      CopyDeviceUUIDTo(command->props.dst_device, static_cast<uint8_t*>(rec.dst_device_uuid));

      if ((tile >= 0) && (device_map_.count(command->device) == 1) &&
          !device_map_[command->device].empty()) {  // Implicit Scaling
        rec.implicit_scaling_ = true;
      } else {
        rec.implicit_scaling_ = false;
      }

      if (command->props.type == KernelCommandType::kMemory) {
        rec.device_ = command->props.src_device;
        rec.dst_device_ = command->props.dst_device;
        if (command->props.src_device != nullptr) {
          auto it = device_descriptors_.find(command->props.src_device);
          PTI_ASSERT(it != device_descriptors_.end());
          rec.pci_prop_ = it->second.pci_properties;
        }
        if (command->props.dst_device != nullptr) {
          auto it = device_descriptors_.find(command->props.dst_device);
          PTI_ASSERT(it != device_descriptors_.end());
          rec.dst_pci_prop_ = it->second.pci_properties;
        }
        if (command->props.bytes_transferred > 0) {
          rec.bytes_xfered_ = command->props.bytes_transferred;
        } else if (command->props.value_size > 0) {
          rec.value_set_ = command->props.value_size;
        }
      }

      rec.context_ = command_list_map_[command->command_list].context;

      if (command->props.type == KernelCommandType::kKernel) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.sycl_enqk_begin_time_ = command->sycl_enqk_begin_time_;

        // rec.sycl_enqk_end_time_ = sycl_data_kview.sycl_enqk_end_time_;

        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
      }
      if (command->props.type == KernelCommandType::kMemory) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
      }

      kcexecrec->push_back(rec);
    }
  }

  void ProcessCall(ZeKernelCommand* command, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    if (kids) {
      kids->push_back(command->kernel_id);
    }

    ze_kernel_timestamp_result_t timestamp{};
    overhead::Init();
    ze_result_t status = zeEventQueryKernelTimestamp(command->event, &timestamp);
    {
      std::string o_api_string = "zeEventQueryKernelTimestamp";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ProcessCall(command, timestamp, -1, true, kcexecrec);

    event_cache_.ReleaseEvent(command->event);
    command->event = nullptr;
    // DO NOT RESET EVENT
    // event_cache_.ResetEvent(command->event);
  }

  void ProcessCalls(std::vector<uint64_t>* kids,
                    std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_command_list_.begin();
    while (it != kernel_command_list_.end()) {
      ZeKernelCommand* command = *it;

      if (command->event != nullptr) {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        }
        if (status == ZE_RESULT_SUCCESS) {
          ProcessCall(command, kids, kcexecrec);
        }
      }

      if (command->event == nullptr) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void CreateCommandList(ze_command_list_handle_t command_list, ze_context_handle_t context,
                         ze_device_handle_t device, std::pair<uint32_t, uint32_t>& oi_pair,
                         bool immediate) {
    const std::lock_guard<std::mutex> lock(lock_);

    // exclusive lock of command_list_map_   as we are changing it ("writing" to it)
    // all other accesses to it ("reading") would be protected by shared_lock
    command_list_map_mutex_.lock();
    if (command_list_map_.count(command_list)) {
      ZeCommandListInfo& command_list_info = command_list_map_[command_list];
      if (command_list_info.immediate) {
        queue_ordinal_index_map_.erase(reinterpret_cast<ze_command_queue_handle_t>(command_list));
      }
      command_list_map_.erase(command_list);
    }

    PTI_ASSERT(device_descriptors_.count(device) != 0);

    command_list_map_[command_list] = {std::vector<ZeKernelCommand*>(), context, device, immediate,
                                       oi_pair};
    command_list_map_mutex_.unlock();

    if (immediate) {
      if (queue_ordinal_index_map_.count(
              reinterpret_cast<ze_command_queue_handle_t>(command_list)) == 0) {
        queue_ordinal_index_map_[reinterpret_cast<ze_command_queue_handle_t>(command_list)] =
            oi_pair;
      }
    }
  }

  void PrepareToExecuteCommandLists(ze_command_list_handle_t* command_lists,
                                    uint32_t command_list_count, ze_command_queue_handle_t queue,
                                    ze_fence_handle_t fence) {
    const std::lock_guard<std::mutex> lock(lock_);
    uint64_t host_time_sync = 0;
    uint64_t device_time_sync = 0;
    auto it = command_queues_.find(queue);
    PTI_ASSERT(it != command_queues_.end());
    ze_device_handle_t device =
        it->second.device_;  // this should be only one device, as queue created on specific device
    PTI_ASSERT(nullptr != device);
    ze_result_t status = zeDeviceGetGlobalTimestamps(device, &host_time_sync, &device_time_sync);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_command_list_handle_t clist = command_lists[i];
      PTI_ASSERT(clist != nullptr);

      const ZeCommandListInfo& info = command_list_map_[clist];

      // as all command lists submitted to the execution into queue - they are not immediate
      PTI_ASSERT(!info.immediate);

      for (ZeKernelCommand* command : info.kernel_commands) {
        if (!command->tid) {
          command->tid = utils::GetTid();
        }
        command->queue = queue;
        // command->submit_time = host_sync;
        command->submit_time = host_time_sync;
        command->submit_time_device_ = device_time_sync;

        PTI_ASSERT(command->append_time <= command->submit_time);
        command->fence = fence;
      }
    }
  }

  void PostSubmitKernelCommands(ze_command_list_handle_t* command_lists,
                                uint32_t command_list_count, std::vector<uint64_t>* kids) {
    const std::lock_guard<std::mutex> lock(lock_);

    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_command_list_handle_t clist = command_lists[i];
      PTI_ASSERT(clist != nullptr);
      ZeCommandListInfo& info = GetCommandListInfo(clist);
      // as all command lists submitted to the execution into queue - they are not immediate
      PTI_ASSERT(!info.immediate);
      for (ZeKernelCommand* command : info.kernel_commands) {
        if (kids) {
          kids->push_back(command->kernel_id);
        }
        kernel_command_list_.push_back(command);
      }
      info.kernel_commands.clear();
    }
  }

  ze_context_handle_t GetCommandListContext(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.context;
  }

  ze_device_handle_t GetCommandListDevice(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.device;
  }

  bool IsCommandListImmediate(ze_command_list_handle_t command_list) {
    const ZeCommandListInfo& command_list_info = GetCommandListInfoConst(command_list);
    return command_list_info.immediate;
  }

  void AddImage(ze_image_handle_t image, size_t size) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 0);
    image_size_map_[image] = size;
  }

  void RemoveImage(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 1);
    image_size_map_.erase(image);
  }

  size_t GetImageSize(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (image_size_map_.count(image) == 1) {
      return image_size_map_[image];
    }
    return 0;
  }

  void AddKernelGroupSize(ze_kernel_handle_t kernel, const ZeKernelGroupSize& group_size) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_[kernel] = group_size;
  }

  void RemoveKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_.erase(kernel);
  }

  ZeKernelGroupSize GetKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_group_size_map_.count(kernel) == 0) {
      return {0, 0, 0};
    }
    return kernel_group_size_map_[kernel];
  }

  // Callbacks
  static void OnEnterEventPoolCreate(ze_event_pool_create_params_t* params, void* /*global_data*/,
                                     void** instance_data) {
    const ze_event_pool_desc_t* desc = *(params->pdesc);
    if (desc == nullptr) {
      return;
    }
    if (desc->flags & ZE_EVENT_POOL_FLAG_IPC) {
      return;
    }

    ze_event_pool_desc_t* profiling_desc = new ze_event_pool_desc_t;
    PTI_ASSERT(profiling_desc != nullptr);
    profiling_desc->stype = desc->stype;
    // PTI_ASSERT(profiling_desc->stype == ZE_STRUCTURE_TYPE_EVENT_POOL_DESC);
    profiling_desc->pNext = desc->pNext;
    profiling_desc->flags = desc->flags;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    profiling_desc->count = desc->count;

    *(params->pdesc) = profiling_desc;
    *instance_data = profiling_desc;
  }

  static void OnExitEventPoolCreate(ze_event_pool_create_params_t* /*params*/,
                                    ze_result_t /*result*/, void* /*global_data*/,
                                    void** instance_data) {
    ze_event_pool_desc_t* desc = static_cast<ze_event_pool_desc_t*>(*instance_data);
    delete desc;
  }

  static void OnEnterEventDestroy(ze_event_destroy_params_t* params, void* global_data,
                                  void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnEnterEventHostReset(ze_event_host_reset_params_t* params, void* global_data,
                                    void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitEventHostSynchronize(ze_event_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitEventQueryStatus(ze_event_query_status_params_t* params, ze_result_t result,
                                     void* global_data, void** /*instance_data*/,
                                     std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phEvent) != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitFenceHostSynchronize(ze_fence_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phFence) != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phFence), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitImageCreate(ze_image_create_params_t* params, ze_result_t result,
                                void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);

      ze_image_desc_t image_desc = **(params->pdesc);
      size_t image_size = image_desc.width;
      switch (image_desc.type) {
        case ZE_IMAGE_TYPE_2D:
        case ZE_IMAGE_TYPE_2DARRAY:
          image_size *= image_desc.height;
          break;
        case ZE_IMAGE_TYPE_3D:
          image_size *= image_desc.height * image_desc.depth;
          break;
        default:
          break;
      }

      switch (image_desc.format.type) {
        case ZE_IMAGE_FORMAT_TYPE_UINT:
        case ZE_IMAGE_FORMAT_TYPE_UNORM:
        case ZE_IMAGE_FORMAT_TYPE_FORCE_UINT32:
          image_size *= sizeof(unsigned int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_SINT:
        case ZE_IMAGE_FORMAT_TYPE_SNORM:
          image_size *= sizeof(int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_FLOAT:
          image_size *= sizeof(float);
          break;
        default:
          break;
      }

      collector->AddImage(**(params->pphImage), image_size);
    }
  }

  static void OnExitImageDestroy(ze_image_destroy_params_t* params, ze_result_t result,
                                 void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      collector->RemoveImage(*(params->phImage));
    }
  }

  static void PrepareToAppendKernelCommand(ZeCollector* collector, ze_event_handle_t& signal_event,
                                           ze_command_list_handle_t command_list,
                                           bool /*iskernel*/) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(collector->lock_);
    ze_context_handle_t context = collector->GetCommandListContext(command_list);
    ze_device_handle_t device = collector->GetCommandListDevice(command_list);

    if (signal_event == nullptr) {
      signal_event = collector->event_cache_.GetEvent(context);
      PTI_ASSERT(signal_event != nullptr);
    }

    uint64_t host_timestamp = 0;
    uint64_t device_timestamp = 0;  // in ticks

    ze_result_t status = zeDeviceGetGlobalTimestamps(device, &host_timestamp, &device_timestamp);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_instance_data.timestamp_host = host_timestamp;
    ze_instance_data.timestamp_device = device_timestamp;
  }

  void AppendKernelCommandCommon(ZeCollector* /*collector*/, ZeKernelCommandProps& props,
                                 ze_event_handle_t& signal_event,
                                 ze_command_list_handle_t command_list,
                                 ZeCommandListInfo& command_list_info, void** /*instance_data*/,
                                 std::vector<uint64_t>* kids) {
    ZeKernelCommand* command = new ZeKernelCommand;
    PTI_ASSERT(command != nullptr);
    command->props = props;
    command->command_list = command_list;

    ze_device_handle_t device = command_list_info.device;
    PTI_ASSERT(device != nullptr);
    command->device = device;

    PTI_ASSERT(signal_event != nullptr);
    command->event = signal_event;
    command->tid = utils::GetTid();
    uint64_t host_timestamp = ze_instance_data.start_time_host;
    command->append_time = host_timestamp;
    command->kernel_id = UniKernelId::GetKernelId();
    command->device_timer_frequency_ = device_descriptors_[command->device].device_timer_frequency;
    command->device_timer_mask_ = device_descriptors_[command->device].device_timer_mask;
    if (command->props.type == KernelCommandType::kKernel) {
      ze_device_properties_t dev_props;
      dev_props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      dev_props.pNext = nullptr;
      ze_result_t status = zeDeviceGetProperties(device, &dev_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      SaveDeviceUUID(command->props.src_device, static_cast<uint8_t*>(dev_props.uuid.id));

      command->sycl_node_id_ = sycl_data_kview.sycl_node_id_;
      command->sycl_invocation_id_ = sycl_data_kview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_kview.sycl_task_begin_time_;
      command->sycl_enqk_begin_time_ = sycl_data_kview.sycl_enqk_begin_time_;
      sycl_data_kview.kid_ = command->kernel_id;
      sycl_data_kview.tid_ = command->tid;
      command->source_file_name_ = sycl_data_kview.source_file_name_;
      command->source_line_number_ = sycl_data_kview.source_line_number_;
      if (sycl_data_kview.cid_) {
        command->corr_id_ = sycl_data_kview.cid_;
      } else {
        command->corr_id_ = UniCorrId::GetUniCorrId();
      }
    } else if (command->props.type == KernelCommandType::kMemory) {
      command->props.src_device = props.src_device;
      command->props.dst_device = props.dst_device;

      sycl_data_mview.kid_ = command->kernel_id;
      sycl_data_mview.tid_ = command->tid;
      if (sycl_data_mview.cid_) {
        command->corr_id_ = sycl_data_mview.cid_;
      } else {
        command->corr_id_ = UniCorrId::GetUniCorrId();
      }

      command->sycl_node_id_ = sycl_data_mview.sycl_node_id_;
      command->sycl_invocation_id_ = sycl_data_mview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_mview.sycl_task_begin_time_;
      command->source_file_name_ = sycl_data_mview.source_file_name_;
      command->source_line_number_ = sycl_data_mview.source_line_number_;
    } else {
      command->corr_id_ = UniCorrId::GetUniCorrId();
    }
    if (command_list_info.immediate) {
      // command->tid = utils::GetTid();
      command->submit_time = command->append_time;
      command->submit_time_device_ =
          ze_instance_data.timestamp_device;  // append time and submit time are the same
      command->queue = reinterpret_cast<ze_command_queue_handle_t>(command_list);
      kernel_command_list_.push_back(command);
      kids->push_back(command->kernel_id);
    } else {
      command_list_info.kernel_commands.push_back(command);
    }
  }

  void AppendKernel(ZeCollector* collector, ze_kernel_handle_t kernel,
                    const ze_group_count_t* group_count, ze_event_handle_t& signal_event,
                    ze_command_list_handle_t command_list, void** instance_data,
                    std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    ZeKernelCommandProps props{};

    props.name = utils::ze::GetKernelName(kernel, options_.demangle);
    props.type = KernelCommandType::kKernel;
    props.simd_width = utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelGroupSize group_size{};
    if (kernel_group_size_map_.count(kernel) == 0) {
      group_size = {0, 0, 0};
    } else {
      group_size = kernel_group_size_map_[kernel];
    }

    props.group_size[0] = group_size.x;
    props.group_size[1] = group_size.y;
    props.group_size[2] = group_size.z;

    if (group_count != nullptr) {
      props.group_count[0] = group_count->groupCountX;
      props.group_count[1] = group_count->groupCountY;
      props.group_count[2] = group_count->groupCountZ;
    }

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    AppendKernelCommandCommon(collector, props, signal_event, command_list, command_list_info,
                              instance_data, kids);
  }

  void AppendMemoryCommand(ZeCollector* collector, std::string command, size_t bytes_transferred,
                           const void* src, const void* dst, ze_event_handle_t& signal_event,
                           ze_command_list_handle_t command_list, void** instance_data,
                           std::vector<uint64_t>* kids, const void* pattern = nullptr,
                           size_t pattern_size = 0) {
    PTI_ASSERT(command_list != nullptr);
    ze_memory_allocation_properties_t mem_props;
    mem_props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    mem_props.pNext = nullptr;

    ze_device_properties_t dev_props;
    dev_props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    dev_props.pNext = nullptr;

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ze_device_handle_t src_device = nullptr;
    ze_device_handle_t dst_device = nullptr;

    // TODO(julia.fedorova@intel.com): zeMemGetAllocProperties called here and then
    //  in below GetTransferProperties => so twice for src and dst.
    //  this should be avoided
    if (dst != nullptr) {
      ze_result_t status = zeMemGetAllocProperties(context, dst, &mem_props, &dst_device);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (dst_device) {
        ze_result_t status = zeDeviceGetProperties(dst_device, &dev_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        SaveDeviceUUID(dst_device, static_cast<uint8_t*>(dev_props.uuid.id));
      }
    }
    if (src != nullptr) {
      ze_result_t status = zeMemGetAllocProperties(context, src, &mem_props, &src_device);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (src_device) {
        ze_result_t status = zeDeviceGetProperties(src_device, &dev_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        SaveDeviceUUID(src_device, static_cast<uint8_t*>(dev_props.uuid.id));
      }
    }

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, (src ? context : nullptr), src,
                         (dst ? context : nullptr), dst, pattern, pattern_size);

    AppendKernelCommandCommon(collector, props, signal_event, command_list, command_list_info,
                              instance_data, kids);
  }

  void AppendMemoryCommandContext(ZeCollector* collector, std::string command,
                                  size_t bytes_transferred, ze_context_handle_t src_context,
                                  const void* src, ze_context_handle_t dst_context, const void* dst,
                                  ze_event_handle_t& signal_event,
                                  ze_command_list_handle_t command_list, void** instance_data,
                                  std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, src_context, src,
                         (dst_context ? dst_context : context), dst);

    AppendKernelCommandCommon(collector, props, signal_event, command_list, command_list_info,
                              instance_data, kids);
  }

  void AppendImageMemoryCopyCommand(ZeCollector* collector, std::string command,
                                    ze_image_handle_t image, const void* src, const void* dst,
                                    ze_event_handle_t& signal_event,
                                    ze_command_list_handle_t command_list, void** instance_data,
                                    std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);
    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    size_t bytes_transferred = 0;
    if (image_size_map_.count(image) == 1) {
      bytes_transferred = image_size_map_[image];
    }

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, context, src, context, dst);

    AppendKernelCommandCommon(collector, props, signal_event, command_list, command_list_info,
                              instance_data, kids);
  }

  void AppendCommand(ZeCollector* collector, std::string command, ze_event_handle_t& signal_event,
                     ze_command_list_handle_t command_list, void** instance_data,
                     std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);

    ZeCommandListInfo& command_list_info = GetCommandListInfo(command_list);

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props{};
    props.name = std::move(command);
    props.type = KernelCommandType::kCommand;

    AppendKernelCommandCommon(collector, props, signal_event, command_list, command_list_info,
                              instance_data, kids);
  }

  static ZeKernelCommandProps GetTransferProps(std::string name, size_t bytes_transferred,
                                               ze_context_handle_t src_context, const void* src,
                                               ze_context_handle_t dst_context, const void* dst,
                                               [[maybe_unused]] const void* pattern = nullptr,
                                               size_t pattern_size = 0) {
    PTI_ASSERT(!name.empty());

    std::string direction;
    ze_device_handle_t hSrcDevice = nullptr;
    ze_device_handle_t hDstDevice = nullptr;
    bool p2p = false;

    if (src_context != nullptr && src != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(src_context, src, &props, &hSrcDevice);
      {
        std::string o_api_string = "zeMemGetAllocProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      }
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          p2p = true;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          p2p = true;
          break;
        default:
          p2p = false;
          break;
      }
    }

    if (dst_context != nullptr && dst != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(dst_context, dst, &props, &hDstDevice);
      {
        std::string o_api_string = "zeMemGetAllocProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      }
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      direction.push_back('2');
      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          break;
        default:
          p2p = false;
          break;
      }
    }

    //
    // TODO:  Redo the stringified -P2P propagation.
    //
    if (!direction.empty()) {
      ze_bool_t p2p_access = 0;
      if (p2p && hSrcDevice && hDstDevice && (hSrcDevice != hDstDevice)) {
        auto status = zeDeviceCanAccessPeer(hSrcDevice, hDstDevice, &p2p_access);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        if (p2p_access) {
          direction.append(" - P2P");
        }
      }
      name += "(" + direction + ")";
    }

    ZeKernelCommandProps props{};
    props.name = std::move(name);
    props.bytes_transferred = bytes_transferred;
    props.value_size = pattern_size;
    props.type = KernelCommandType::kMemory;
    props.src_device = hSrcDevice;
    props.dst_device = hDstDevice;
    return props;
  }

  static ZeKernelCommandProps GetCommandProps(std::string name) {
    PTI_ASSERT(!name.empty());

    ZeKernelCommandProps props{};
    props.name = name;
    props.type = KernelCommandType::kCommand;
    return props;
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 true);
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, ze_result_t result, void* global_data,
      void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                              *(params->phSignalEvent), *(params->phCommandList), instance_data,
                              kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 true);
  }

  static void OnExitCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                              *(params->phSignalEvent), *(params->phCommandList), instance_data,
                              kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 true);
  }

  static void OnExitCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchArgumentsBuffer),
                              *(params->phSignalEvent), *(params->phCommandList), instance_data,
                              kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendMemoryCopy(ze_command_list_append_memory_copy_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryCopy", *(params->psize),
                                     *(params->psrcptr), *(params->pdstptr),
                                     *(params->phSignalEvent), *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendMemoryFill(ze_command_list_append_memory_fill_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryFill", *(params->psize),
                                     *(params->pptr), nullptr, *(params->phSignalEvent),
                                     *(params->phCommandList), instance_data, kids,
                                     *(params->ppattern), *(params->ppattern_size));
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                              void* global_data, void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                             ze_result_t result, void* global_data,
                                             void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendCommand(collector, "zeCommandListAppendBarrier", *(params->phSignalEvent),
                               *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendCommand(collector, "zeCommandListAppendMemoryRangesBarrier",
                               *(params->phSignalEvent), *(params->phCommandList), instance_data,
                               kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      size_t bytes_transferred = 0;
      const ze_copy_region_t* region = *(params->psrcRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height * (*(params->psrcPitch));
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryCopyRegion",
                                     bytes_transferred, *(params->psrcptr), *(params->pdstptr),
                                     *(params->phSignalEvent), *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      ze_context_handle_t src_context = *(params->phContextSrc);
      // ze_context_handle_t dst_context = nullptr;
      collector->AppendMemoryCommandContext(collector, "zeCommandListAppendMemoryCopyFromContext",
                                            *(params->psize), src_context, *(params->psrcptr),
                                            nullptr, *(params->pdstptr), *(params->phSignalEvent),
                                            *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                                void* global_data, void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopy", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopyRegion", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), *(params->phCommandList), instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      collector->AppendImageMemoryCopyCommand(collector, "zeCommandListAppendImageCopyRegion",
                                              *(params->phSrcImage), nullptr, *(params->pdstptr),
                                              *(params->phSignalEvent), *(params->phCommandList),
                                              instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    PrepareToAppendKernelCommand(collector, *(params->phSignalEvent), *(params->phCommandList),
                                 false);
  }

  static void OnExitCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    if (result == ZE_RESULT_SUCCESS) {
      size_t bytes_transferred = 0;
      const ze_image_region_t* region = *(params->ppDstRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height;
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      collector->AppendMemoryCommand(collector, "zeCommandListAppendImageCopyFromMemory",
                                     bytes_transferred, *(params->psrcptr), nullptr,
                                     *(params->phSignalEvent), *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnExitCommandListCreate(ze_command_list_create_params_t* params, ze_result_t result,
                                      void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);

      // dummy pair
      std::pair<uint32_t, uint32_t> oi(-1, -1);
      collector->CreateCommandList(**(params->pphCommandList), *(params->phContext),
                                   *(params->phDevice), oi, false);
    }
  }

  static void OnExitCommandListCreateImmediate(ze_command_list_create_immediate_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      ze_device_handle_t* hDevice = params->phDevice;
      if (hDevice == nullptr) {
        return;
      }

      const ze_command_queue_desc_t* clq_desc = *params->paltdesc;
      if (clq_desc == nullptr) {
        return;
      }

      ze_command_list_handle_t* command_list = *params->pphCommandList;
      if (command_list == nullptr) {
        return;
      }

      std::pair<uint32_t, uint32_t> oi(clq_desc->ordinal, clq_desc->index);

      collector->CreateCommandList(**(params->pphCommandList), *(params->phContext),
                                   *(params->phDevice), oi, true);
    }
  }

  static void OnExitCommandListDestroy(ze_command_list_destroy_params_t* params, ze_result_t result,
                                       void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;

      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandListReset(ze_command_list_reset_params_t* params, ze_result_t result,
                                     void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      // collector->ResetCommandList(*params->phCommandList);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnEnterCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params, void* global_data,
      void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);

    uint32_t command_list_count = *params->pnumCommandLists;
    if (command_list_count == 0) {
      return;
    }

    ze_command_list_handle_t* command_lists = *params->pphCommandLists;
    if (command_lists == nullptr) {
      return;
    }

    collector->PrepareToExecuteCommandLists(command_lists, command_list_count,
                                            *(params->phCommandQueue), *(params->phFence));
  }
  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params, ze_result_t result,
      void* global_data, void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      uint32_t command_list_count = *params->pnumCommandLists;
      if (command_list_count == 0) {
        return;
      }

      ze_command_list_handle_t* command_lists = *params->pphCommandLists;
      if (command_lists == nullptr) {
        return;
      }

      collector->PostSubmitKernelCommands(command_lists, command_list_count, kids);
    }
  }

  static void OnExitCommandQueueSynchronize(ze_command_queue_synchronize_params_t* /*params*/,
                                            ze_result_t result, void* global_data,
                                            void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(kids, &kcexec);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandQueueCreate(ze_command_queue_create_params_t* params,
                                       ze_result_t /*result*/, void* global_data,
                                       void** /*instance_data*/) {
    ZeCollector* collector = static_cast<ZeCollector*>(global_data);
    ze_device_handle_t* device = params->phDevice;
    if (device == nullptr) {
      return;
    }
    const ze_command_queue_desc_t* queue_desc = *params->pdesc;
    if (queue_desc == nullptr) {
      return;
    }
    ze_command_queue_handle_t* command_queue = *params->pphCommandQueue;
    if (command_queue == nullptr) {
      return;
    }

    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (collector->queue_ordinal_index_map_.count(*command_queue) == 0) {
      collector->queue_ordinal_index_map_[*command_queue] =
          std::make_pair(queue_desc->ordinal, queue_desc->index);
    }

    ZeCommandQueue desc{};
    desc.queue_ = *command_queue;
    desc.context_ = *(params->phContext);
    desc.device_ = *device;
    desc.engine_ordinal_ = queue_desc->ordinal;
    desc.engine_index_ = queue_desc->index;

    collector->command_queues_.erase(*command_queue);
    collector->command_queues_.insert({*command_queue, std::move(desc)});
  }

  static void OnExitCommandQueueDestroy(ze_command_queue_destroy_params_t* params,
                                        ze_result_t result, void* global_data,
                                        void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      collector->queue_ordinal_index_map_.erase(*params->phCommandQueue);
      collector->command_queues_.erase(*params->phCommandQueue);
      collector->lock_.unlock();

      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitKernelSetGroupSize(ze_kernel_set_group_size_params_t* params,
                                       ze_result_t result, void* global_data,
                                       void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      ZeKernelGroupSize group_size{*(params->pgroupSizeX), *(params->pgroupSizeY),
                                   *(params->pgroupSizeZ)};
      collector->AddKernelGroupSize(*(params->phKernel), group_size);
    }
  }

  static void OnExitKernelDestroy(ze_kernel_destroy_params_t* params, ze_result_t result,
                                  void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      collector->RemoveKernelGroupSize(*(params->phKernel));
    }
  }

  static void OnExitContextDestroy(ze_context_destroy_params_t* params, ze_result_t result,
                                   void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = static_cast<ZeCollector*>(global_data);
      collector->ProcessCalls(nullptr, nullptr);
      collector->event_cache_.ReleaseContext(*(params->phContext));
    }
  }

#include <tracing.gen>  // Auto-generated callbacks

  zel_tracer_handle_t tracer_ = nullptr;
  CollectorOptions options_ = {};
  CallbacksEnabled cb_enabled_ = {};
  OnZeKernelFinishCallback acallback_ = nullptr;
  void* callback_data_ = nullptr;
  std::mutex lock_;

  std::list<ZeKernelCommand*> kernel_command_list_;

  mutable std::shared_mutex command_list_map_mutex_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;
  ZeKernelGroupSizeMap kernel_group_size_map_;
  ZeDeviceMap device_map_;
  std::map<ze_device_handle_t, ZeDeviceDescriptor> device_descriptors_;

  ZeEventCache event_cache_;

  std::map<ze_command_queue_handle_t, std::pair<uint32_t, uint32_t>> queue_ordinal_index_map_;

  mutable std::shared_mutex dev_uuid_map_mutex_;
  std::map<ze_device_handle_t, uint8_t[ZE_MAX_DEVICE_UUID_SIZE]> dev_uuid_map_;

  std::map<ze_command_queue_handle_t, ZeCommandQueue> command_queues_;
};

#endif  // PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
