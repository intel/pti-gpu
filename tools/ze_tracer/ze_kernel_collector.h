//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_TRACER_ZE_KERNEL_COLLECTOR_H_
#define PTI_TOOLS_ZE_TRACER_ZE_KERNEL_COLLECTOR_H_

#include <atomic>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <level_zero/layers/zel_tracing_api.h>

#include "correlator.h"
#include "utils.h"
#include "ze_event_cache.h"
#include "ze_utils.h"

struct ZeSyncPoint {
  uint64_t host_sync;
  uint64_t device_sync;
};

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

struct ZeKernelProps {
  std::string name;
  size_t simd_width;
  size_t bytes_transferred;
  uint32_t group_count[3];
  uint32_t group_size[3];
};

struct ZeKernelCommand {
  ZeKernelProps props;
  ze_event_handle_t event = nullptr;
  ze_device_handle_t device = nullptr;
  uint64_t kernel_id = 0;
  uint64_t append_time = 0;
  uint64_t call_count = 0;
  uint64_t timer_frequency = 0;
  uint64_t timer_mask = 0;
};

struct ZeKernelCall {
  ZeKernelCommand* command = nullptr;
  ze_command_queue_handle_t queue = nullptr;
  uint64_t submit_time = 0;
  uint64_t device_submit_time = 0;
  uint64_t call_id = 0;
  bool need_to_process = true;
};

struct ZeKernelInfo {
  uint64_t append_time;
  uint64_t submit_time;
  uint64_t execute_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ZeKernelInfo& r) const {
    if (execute_time != r.execute_time) {
      return execute_time > r.execute_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ZeKernelInfo& r) const {
    if (execute_time == r.execute_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

struct ZeCommandListInfo {
  std::vector<ZeKernelCommand*> kernel_command_list;
  ze_context_handle_t context;
  ze_device_handle_t device;
  bool immediate;
};

#ifdef PTI_KERNEL_INTERVALS

struct ZeDeviceInterval {
  uint64_t start;
  uint64_t end;
  uint32_t sub_device_id;
};

struct ZeKernelInterval {
  std::string kernel_name;
  ze_device_handle_t device;
  std::vector<ZeDeviceInterval> device_interval_list;
};

using ZeKernelIntervalList = std::vector<ZeKernelInterval>;

#endif // PTI_KERNEL_INTERVALS

using ZeKernelGroupSizeMap = std::map<ze_kernel_handle_t, ZeKernelGroupSize>;
using ZeKernelInfoMap = std::map<std::string, ZeKernelInfo>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;
using ZeImageSizeMap = std::map<ze_image_handle_t, size_t>;
using ZeDeviceMap = std::map<
    ze_device_handle_t, std::vector<ze_device_handle_t> >;

typedef void (*OnZeKernelFinishCallback)(
    void* data,
    const std::string& queue,
    const std::string& id,
    const std::string& name,
    uint64_t appended,
    uint64_t submitted,
    uint64_t started,
    uint64_t ended);

class ZeKernelCollector {
 public: // Interface

  static ZeKernelCollector* Create(
      Correlator* correlator,
      KernelCollectorOptions options,
      OnZeKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    ze_api_version_t version = utils::ze::GetVersion();
    PTI_ASSERT(
        ZE_MAJOR_VERSION(version) >= 1 &&
        ZE_MINOR_VERSION(version) >= 2);

    PTI_ASSERT(correlator != nullptr);
    ZeKernelCollector* collector = new ZeKernelCollector(
        correlator, options, callback, callback_data);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {
        ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;
    status = zelTracerCreate(&tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to create Level Zero tracer" << std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ZeKernelCollector() {
    if (tracer_ != nullptr) {
#if !defined(_WIN32)
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
    }
  }

  void PrintKernelsTable() const {
    std::set< std::pair<std::string, ZeKernelInfo>,
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
    std::set< std::pair<std::string, ZeKernelInfo>,
              utils::Comparator > sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

    uint64_t total_append_duration = 0;
    uint64_t total_submit_duration = 0;
    uint64_t total_execute_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_append_duration += value.second.append_time;
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
      std::setw(kTimeLength) << "Append (ns)" << "," <<
      std::setw(kPercentLength) << "Append (%)" << "," <<
      std::setw(kTimeLength) << "Submit (ns)" << "," <<
      std::setw(kPercentLength) << "Submit (%)" << "," <<
      std::setw(kTimeLength) << "Execute (ns)" << "," <<
      std::setw(kPercentLength) << "Execute (%)" << "," << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t append_duration = value.second.append_time;
      float append_percent =
        100.0f * append_duration / total_append_duration;
      uint64_t submit_duration = value.second.submit_time;
      float submit_percent =
        100.0f * submit_duration / total_submit_duration;
      uint64_t execute_duration = value.second.execute_time;
      float execute_percent =
        100.0f * execute_duration / total_execute_duration;
      stream << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTimeLength) << append_duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << append_percent << "," <<
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

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
#if !defined(_WIN32)
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
  }

  const ZeKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

#ifdef PTI_KERNEL_INTERVALS
  const ZeKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }
#endif // PTI_KERNEL_INTERVALS

 private: // Implementation

  ZeKernelCollector(
      Correlator* correlator,
      KernelCollectorOptions options,
      OnZeKernelFinishCallback callback,
      void* callback_data)
      : correlator_(correlator),
        options_(options),
        callback_(callback),
        callback_data_(callback_data),
        kernel_id_(1) {
    PTI_ASSERT(correlator_ != nullptr);
    CreateDeviceMap();
#ifdef PTI_KERNEL_INTERVALS
    SetSyncPoints();
#endif
  }

#ifdef PTI_KERNEL_INTERVALS
  void SetSyncPoints() {
    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList();
    for (auto device : device_list) {
      std::vector<ze_device_handle_t> sub_device_list =
        utils::ze::GetSubDeviceList(device);
      if (sub_device_list.empty()) {
        ZeSyncPoint sync_point{0, 0};
        GetSyncTimestamps(
            device, sync_point.host_sync, sync_point.device_sync);
        PTI_ASSERT(sync_point_map_.count(device) == 0);
        sync_point_map_[device] = sync_point;
      } else {
        for (auto sub_device : sub_device_list) {
          ZeSyncPoint sync_point{0, 0};
          GetSyncTimestamps(
              sub_device, sync_point.host_sync, sync_point.device_sync);
          PTI_ASSERT(sync_point_map_.count(sub_device) == 0);
          sync_point_map_[sub_device] = sync_point;
        }
      }
    }
  }
#endif

  void CreateDeviceMap() {
    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList();
    for (auto device : device_list) {
      std::vector<ze_device_handle_t> sub_device_list =
        utils::ze::GetSubDeviceList(device);
      PTI_ASSERT(device_map_.count(device) == 0);
      device_map_[device] = sub_device_list;
    }
  }

  int GetSubDeviceId(ze_device_handle_t sub_device) const {
    for (auto it : device_map_) {
      std::vector<ze_device_handle_t> sub_device_list = it.second;
      for (size_t i = 0; i < sub_device_list.size(); ++i) {
        if (sub_device_list[i] == sub_device) {
          return static_cast<int>(i);
        }
      }
    }
    return -1;
  }

  ze_device_handle_t GetDeviceForSubDevice(
      ze_device_handle_t sub_device) const {
    for (auto it : device_map_) {
      std::vector<ze_device_handle_t> sub_device_list = it.second;
      for (size_t i = 0; i < sub_device_list.size(); ++i) {
        if (sub_device_list[i] == sub_device) {
          return it.first;
        }
      }
    }
    return nullptr;
  }

  uint64_t GetHostTimestamp() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetTimestamp();
  }

  void GetSyncTimestamps(
      ze_device_handle_t device,
      uint64_t& host_timestamp,
      uint64_t& device_timestamp) const {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(correlator_ != nullptr);
#ifdef PTI_KERNEL_INTERVALS
    utils::ze::GetMetricTimestamps(device, &host_timestamp, &device_timestamp);
    host_timestamp = correlator_->GetTimestamp(host_timestamp);
    device_timestamp &= utils::ze::GetMetricTimestampMask(device);
#else
    utils::ze::GetDeviceTimestamps(device, &host_timestamp, &device_timestamp);
    host_timestamp = correlator_->GetTimestamp(host_timestamp);
    device_timestamp &= utils::ze::GetDeviceTimestampMask(device);
#endif
  }

  void EnableTracing(zel_tracer_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t prologue_callbacks{};
    zet_core_callbacks_t epilogue_callbacks{};

    prologue_callbacks.Event.pfnDestroyCb = OnEnterEventDestroy;

    prologue_callbacks.Event.pfnHostResetCb = OnEnterEventHostReset;

    prologue_callbacks.EventPool.pfnCreateCb = OnEnterEventPoolCreate;
    epilogue_callbacks.EventPool.pfnCreateCb = OnExitEventPoolCreate;

    prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnEnterCommandListAppendLaunchKernel;
    epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnExitCommandListAppendLaunchKernel;

    prologue_callbacks.CommandList.pfnAppendLaunchCooperativeKernelCb =
      OnEnterCommandListAppendLaunchCooperativeKernel;
    epilogue_callbacks.CommandList.pfnAppendLaunchCooperativeKernelCb =
      OnExitCommandListAppendLaunchCooperativeKernel;

    prologue_callbacks.CommandList.pfnAppendLaunchKernelIndirectCb =
      OnEnterCommandListAppendLaunchKernelIndirect;
    epilogue_callbacks.CommandList.pfnAppendLaunchKernelIndirectCb =
      OnExitCommandListAppendLaunchKernelIndirect;

    prologue_callbacks.CommandList.pfnAppendMemoryCopyCb =
      OnEnterCommandListAppendMemoryCopy;
    epilogue_callbacks.CommandList.pfnAppendMemoryCopyCb =
      OnExitCommandListAppendMemoryCopy;

    prologue_callbacks.CommandList.pfnAppendMemoryFillCb =
      OnEnterCommandListAppendMemoryFill;
    epilogue_callbacks.CommandList.pfnAppendMemoryFillCb =
      OnExitCommandListAppendMemoryFill;

    prologue_callbacks.CommandList.pfnAppendBarrierCb =
      OnEnterCommandListAppendBarrier;
    epilogue_callbacks.CommandList.pfnAppendBarrierCb =
      OnExitCommandListAppendBarrier;

    prologue_callbacks.CommandList.pfnAppendMemoryRangesBarrierCb =
      OnEnterCommandListAppendMemoryRangesBarrier;
    epilogue_callbacks.CommandList.pfnAppendMemoryRangesBarrierCb =
      OnExitCommandListAppendMemoryRangesBarrier;

    prologue_callbacks.CommandList.pfnAppendMemoryCopyRegionCb =
      OnEnterCommandListAppendMemoryCopyRegion;
    epilogue_callbacks.CommandList.pfnAppendMemoryCopyRegionCb =
      OnExitCommandListAppendMemoryCopyRegion;

    prologue_callbacks.CommandList.pfnAppendMemoryCopyFromContextCb =
      OnEnterCommandListAppendMemoryCopyFromContext;
    epilogue_callbacks.CommandList.pfnAppendMemoryCopyFromContextCb =
      OnExitCommandListAppendMemoryCopyFromContext;

    prologue_callbacks.CommandList.pfnAppendImageCopyCb =
      OnEnterCommandListAppendImageCopy;
    epilogue_callbacks.CommandList.pfnAppendImageCopyCb =
      OnExitCommandListAppendImageCopy;

    prologue_callbacks.CommandList.pfnAppendImageCopyRegionCb =
      OnEnterCommandListAppendImageCopyRegion;
    epilogue_callbacks.CommandList.pfnAppendImageCopyRegionCb =
      OnExitCommandListAppendImageCopyRegion;

    prologue_callbacks.CommandList.pfnAppendImageCopyToMemoryCb =
      OnEnterCommandListAppendImageCopyToMemory;
    epilogue_callbacks.CommandList.pfnAppendImageCopyToMemoryCb =
      OnExitCommandListAppendImageCopyToMemory;

    prologue_callbacks.CommandList.pfnAppendImageCopyFromMemoryCb =
      OnEnterCommandListAppendImageCopyFromMemory;
    epilogue_callbacks.CommandList.pfnAppendImageCopyFromMemoryCb =
      OnExitCommandListAppendImageCopyFromMemory;

    prologue_callbacks.CommandQueue.pfnExecuteCommandListsCb =
      OnEnterCommandQueueExecuteCommandLists;
    epilogue_callbacks.CommandQueue.pfnExecuteCommandListsCb =
      OnExitCommandQueueExecuteCommandLists;

    epilogue_callbacks.CommandList.pfnCreateCb =
      OnExitCommandListCreate;
    epilogue_callbacks.CommandList.pfnCreateImmediateCb =
      OnExitCommandListCreateImmediate;
    epilogue_callbacks.CommandList.pfnDestroyCb =
      OnExitCommandListDestroy;
    epilogue_callbacks.CommandList.pfnResetCb =
      OnExitCommandListReset;

    epilogue_callbacks.CommandQueue.pfnSynchronizeCb =
      OnExitCommandQueueSynchronize;
    epilogue_callbacks.CommandQueue.pfnDestroyCb =
      OnExitCommandQueueDestroy;

    epilogue_callbacks.Image.pfnCreateCb =
      OnExitImageCreate;
    epilogue_callbacks.Image.pfnDestroyCb =
      OnExitImageDestroy;

    epilogue_callbacks.Kernel.pfnSetGroupSizeCb =
      OnExitKernelSetGroupSize;
    epilogue_callbacks.Kernel.pfnDestroyCb =
      OnExitKernelDestroy;

    epilogue_callbacks.Event.pfnHostSynchronizeCb =
      OnExitEventHostSynchronize;

    epilogue_callbacks.Context.pfnDestroyCb =
      OnExitContextDestroy;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetPrologues(tracer_, &prologue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddKernelCommand(
      ze_command_list_handle_t command_list, ZeKernelCommand* command) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(command != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    command->kernel_id =
      kernel_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
    PTI_ASSERT(correlator_ != nullptr);
    correlator_->SetKernelId(command->kernel_id);
    correlator_->AddKernelId(command_list, command->kernel_id);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    command_list_info.kernel_command_list.push_back(command);
  }

  void AddKernelCall(
      ze_command_list_handle_t command_list, ZeKernelCall* call) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(call != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);
    ++(command->call_count);
    call->call_id = command->call_count;

    kernel_call_list_.push_back(call);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->AddCallId(command_list, call->call_id);
  }

  void ProcessCall(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeEventQueryStatus(event);
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

    for (auto it = kernel_call_list_.begin();
         it != kernel_call_list_.end(); ++it) {
      ZeKernelCall* call = *it;
      PTI_ASSERT(call != nullptr);
      ZeKernelCommand* command = call->command;
      PTI_ASSERT(command != nullptr);

      if (command->event == event) {
        ProcessCall(call);
        kernel_call_list_.erase(it);
        break;
      }
    }
  }

  uint64_t ComputeDuration(
      uint64_t start, uint64_t end, uint64_t freq, uint64_t mask) {
    uint64_t duration = 0;
    if (start < end) {
      duration = (end - start) *
        static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    } else { // Timer Overflow
      duration = ((mask + 1ull) + end - start) *
        static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    }
    return duration;
  }

#ifdef PTI_KERNEL_INTERVALS
  uint64_t ConvertToMetricTimestamp(
      uint64_t kernel_timestamp,
      uint64_t mask) {
    return (kernel_timestamp & mask);
  }

  uint64_t ProcessTimerOverflow(
      const ZeSyncPoint& base_sync,
      const ZeSyncPoint& target_sync,
      uint64_t mask,
      uint64_t freq) {
    PTI_ASSERT(base_sync.host_sync < target_sync.host_sync);
    uint64_t duration = target_sync.host_sync - base_sync.host_sync;

    uint64_t base_time = base_sync.device_sync *
      static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    uint64_t target_time = base_time + duration;

    uint64_t max_time = (mask + 1ull) *
      static_cast<uint64_t>(NSEC_IN_SEC) / freq;

    uint64_t shift = 0;
    uint64_t metric_time = target_sync.device_sync *
      static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    while (metric_time + shift + max_time < target_time) {
      shift += max_time;
    }

    return shift;
  }

  void GetMetricTime(
      const ZeKernelCall* call,
      ze_device_handle_t device,
      const ze_kernel_timestamp_result_t& timestamp,
      uint64_t& metric_start, uint64_t& metric_end) {
    PTI_ASSERT(call != nullptr);

    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    uint64_t freq = command->timer_frequency;
    uint64_t mask = command->timer_mask;
    PTI_ASSERT(freq > 0);
    PTI_ASSERT(mask > 0);

    uint64_t start = ConvertToMetricTimestamp(
        timestamp.global.kernelStart, mask);
    uint64_t end = ConvertToMetricTimestamp(
        timestamp.global.kernelEnd, mask);

    PTI_ASSERT(!sync_point_map_.empty());
    PTI_ASSERT(sync_point_map_.count(device) == 1);
    const ZeSyncPoint& base_sync = sync_point_map_[device];

    PTI_ASSERT(call->submit_time > 0);
    ZeSyncPoint submit_sync{call->submit_time, call->device_submit_time};

    uint64_t time_shift = ComputeDuration(
        submit_sync.device_sync, start, freq, mask);
    uint64_t duration = ComputeDuration(start, end, freq, mask);

    uint64_t metric_sync = submit_sync.device_sync *
      static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    metric_sync += ProcessTimerOverflow(base_sync, submit_sync, mask, freq);
    metric_start = metric_sync + time_shift;
    metric_end = metric_start + duration;
  }

#else // PTI_KERNEL_INTERVALS
  void GetHostTime(
      const ZeKernelCall* call,
      const ze_kernel_timestamp_result_t& timestamp,
      uint64_t& host_start, uint64_t& host_end) {
    PTI_ASSERT(call != nullptr);

    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    uint64_t start = timestamp.global.kernelStart;
    uint64_t end = timestamp.global.kernelEnd;

    uint64_t freq = command->timer_frequency;
    uint64_t mask = command->timer_mask;
    PTI_ASSERT(freq > 0);
    PTI_ASSERT(mask > 0);

    PTI_ASSERT(call->submit_time > 0);
    uint64_t time_shift =
      ComputeDuration(call->device_submit_time, start, freq, mask);
    uint64_t duration = ComputeDuration(start, end, freq, mask);

    host_start = call->submit_time + time_shift;
    host_end = host_start + duration;
  }

  void ProcessCall(
      const ZeKernelCall* call,
      const ze_kernel_timestamp_result_t& timestamp,
      int tile, bool in_summary) {
    PTI_ASSERT(call != nullptr);

    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    uint64_t host_start = 0, host_end = 0;
    GetHostTime(call, timestamp, host_start, host_end);
    PTI_ASSERT(host_start <= host_end);

    std::string name = command->props.name;
    PTI_ASSERT(!name.empty());

    if (options_.verbose) {
      name = GetVerboseName(&command->props);
    }

    if (tile >= 0) {
      name += "(" + std::to_string(tile) + "T)";
    }

    if (in_summary) {
      PTI_ASSERT(command->append_time > 0);
      PTI_ASSERT(command->append_time <= call->submit_time);
      uint64_t append_time = call->submit_time - command->append_time;
      PTI_ASSERT(call->submit_time <= host_start);
      uint64_t submit_time = host_start - call->submit_time;
      PTI_ASSERT(host_start <= host_end);
      uint64_t execute_time = host_end - host_start;
      AddKernelInfo(append_time, submit_time, execute_time, name);
    }

    if (callback_ != nullptr) {
      PTI_ASSERT(command->append_time > 0);
      PTI_ASSERT(command->append_time <= call->submit_time);

      PTI_ASSERT(call->queue != nullptr);
      PTI_ASSERT(!command->props.name.empty());
      std::string id = std::to_string(command->kernel_id) + "." +
        std::to_string(call->call_id);

      std::stringstream stream;
      stream << std::hex << call->queue;
      if (tile >= 0) {
        stream << "." << std::dec << tile;
      }

      callback_(
          callback_data_, stream.str(), id, name,
          command->append_time, call->submit_time,
          host_start, host_end);
    }
  }
#endif // PTI_KERNEL_INTERVALS

  void ProcessCall(const ZeKernelCall* call) {
    PTI_ASSERT(call != nullptr);
    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    if (call->need_to_process) {
#ifdef PTI_KERNEL_INTERVALS
      AddKernelInterval(call);
#else // PTI_KERNEL_INTERVALS
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zeEventQueryStatus(command->event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      ze_kernel_timestamp_result_t timestamp{};
      status = zeEventQueryKernelTimestamp(command->event, &timestamp);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (options_.kernels_per_tile && command->props.simd_width > 0) {
        if (device_map_.count(command->device) == 1 &&
            !device_map_[command->device].empty()) { // Implicit Scaling
          uint32_t count = 0;
          status = zeEventQueryTimestampsExp(
              command->event, command->device, &count, nullptr);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          PTI_ASSERT(count > 0);

          std::vector<ze_kernel_timestamp_result_t> timestamps(count);
          status = zeEventQueryTimestampsExp(
              command->event, command->device, &count, timestamps.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          if (count == 1) { // First tile is used only
            ProcessCall(call, timestamp, 0, true);
          } else {
            ProcessCall(call, timestamp, -1, false);
            for (uint32_t i = 0; i < count; ++i) {
              ProcessCall(call, timestamps[i], static_cast<int>(i), true);
            }
          }
        } else { // Explicit Scaling
          if (device_map_.count(command->device) == 0) { // Subdevice
            int sub_device_id = GetSubDeviceId(command->device);
            PTI_ASSERT(sub_device_id >= 0);
            ProcessCall(call, timestamp, sub_device_id, true);
          } else { // Device with no subdevices
            ProcessCall(call, timestamp, 0, true);
          }
        }
      } else {
        ProcessCall(call, timestamp, -1, true);
      }
#endif // PTI_KERNEL_INTERVALS
    }

    event_cache_.ResetEvent(command->event);
    delete call;
  }

  void ProcessCalls() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_call_list_.begin();
    while (it != kernel_call_list_.end()) {
      ZeKernelCall* call = *it;
      PTI_ASSERT(call != nullptr);
      ZeKernelCommand* command = call->command;
      PTI_ASSERT(command != nullptr);

      PTI_ASSERT(command->event != nullptr);
      status = zeEventQueryStatus(command->event);
      if (status == ZE_RESULT_NOT_READY) {
        ++it;
      } else if (status == ZE_RESULT_SUCCESS) {
        ProcessCall(call);
        it = kernel_call_list_.erase(it);
      } else {
        PTI_ASSERT(0);
      }
    }
  }

  static std::string GetVerboseName(const ZeKernelProps* props) {
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
      sstream << ", {" <<
        props->group_count[0] << "; " <<
        props->group_count[1] << "; " <<
        props->group_count[2] << "} {" <<
        props->group_size[0] << "; " <<
        props->group_size[1] << "; " <<
        props->group_size[2] << "}]";
    } else if (props->bytes_transferred > 0) {
      sstream << "[" << props->bytes_transferred << " bytes]";
    }

    return sstream.str();
  }

  void AddKernelInfo(
      uint64_t append_time, uint64_t submit_time,
      uint64_t execute_time, const std::string& name) {
    PTI_ASSERT(!name.empty());

    if (kernel_info_map_.count(name) == 0) {
      ZeKernelInfo info;
      info.append_time = append_time;
      info.submit_time = submit_time;
      info.execute_time = execute_time;
      info.min_time = execute_time;
      info.max_time = execute_time;
      info.call_count = 1;
      kernel_info_map_[name] = info;
    } else {
      ZeKernelInfo& kernel = kernel_info_map_[name];
      kernel.append_time += append_time;
      kernel.submit_time +=  submit_time;
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
  void AddKernelInterval(const ZeKernelCall* call) {
    PTI_ASSERT(call != nullptr);

    const ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    if (command->props.simd_width == 0) {
      return; // Process user kernels only
    }

    std::string name = command->props.name;
    PTI_ASSERT(!name.empty());

    if (options_.verbose) {
      name = GetVerboseName(&command->props);
    }

    ze_result_t status = zeEventQueryStatus(command->event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (device_map_.count(command->device) == 1 &&
        !device_map_[command->device].empty()) { // Implicit Scaling
      uint32_t count = 0;
      status = zeEventQueryTimestampsExp(
          command->event, command->device, &count, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      PTI_ASSERT(count > 0);

      std::vector<ze_kernel_timestamp_result_t> timestamps(count);
      status = zeEventQueryTimestampsExp(
          command->event, command->device, &count, timestamps.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      PTI_ASSERT(count <= device_map_[command->device].size());

      ZeKernelInterval kernel_interval{
          name, command->device, std::vector<ZeDeviceInterval>()};
      for (uint32_t i = 0; i < count; ++i) {
        ze_device_handle_t sub_device = device_map_[command->device][i];

        uint64_t host_start = 0, host_end = 0;
        GetMetricTime(call, sub_device, timestamps[i], host_start, host_end);
        PTI_ASSERT(host_start <= host_end);

        kernel_interval.device_interval_list.push_back(
            {host_start, host_end, i});
      }
      kernel_interval_list_.push_back(kernel_interval);
    } else { // Explicit scaling
      ze_kernel_timestamp_result_t timestamp{};
      status = zeEventQueryKernelTimestamp(command->event, &timestamp);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      uint64_t host_start = 0, host_end = 0;
      GetMetricTime(call, command->device, timestamp, host_start, host_end);
      PTI_ASSERT(host_start <= host_end);

      if (device_map_.count(command->device) == 0) { // Subdevice
        ze_device_handle_t device = GetDeviceForSubDevice(command->device);
        PTI_ASSERT(device != nullptr);
        int sub_device_id = GetSubDeviceId(command->device);
        PTI_ASSERT(sub_device_id >= 0);

        ZeKernelInterval kernel_interval{
            name, device, std::vector<ZeDeviceInterval>()};
        kernel_interval.device_interval_list.push_back(
            {host_start, host_end, static_cast<uint32_t>(sub_device_id)});
        kernel_interval_list_.push_back(kernel_interval);
      } else { // Device with no subdevices
        PTI_ASSERT(device_map_[command->device].empty());
        ZeKernelInterval kernel_interval{
            name, command->device, std::vector<ZeDeviceInterval>()};
        kernel_interval.device_interval_list.push_back(
            {host_start, host_end, 0});
        kernel_interval_list_.push_back(kernel_interval);
      }
    }
  }
#endif // PTI_KERNEL_INTERVALS

  void AddCommandList(
      ze_command_list_handle_t command_list,
      ze_context_handle_t context,
      ze_device_handle_t device,
      bool immediate) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(context != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 0);
    command_list_map_[command_list] =
      {std::vector<ZeKernelCommand*>(), context, device, immediate};

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->CreateKernelIdList(command_list);
    correlator_->CreateCallIdList(command_list);
  }

  void RemoveKernelCommands(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& info = command_list_map_[command_list];
    for (ZeKernelCommand* command : info.kernel_command_list) {
      event_cache_.ReleaseEvent(command->event);
      for (ZeKernelCall* call : kernel_call_list_) {
        PTI_ASSERT(call->command != command);
      }

      delete command;
    }
    info.kernel_command_list.clear();
  }

  void RemoveCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    RemoveKernelCommands(command_list);
    command_list_map_.erase(command_list);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->RemoveKernelIdList(command_list);
    correlator_->RemoveCallIdList(command_list);
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    RemoveKernelCommands(command_list);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->ResetKernelIdList(command_list);
    correlator_->ResetCallIdList(command_list);
  }

  void AddKernelCalls(
      ze_command_list_handle_t command_list,
      ze_command_queue_handle_t queue, const ZeSyncPoint* submit_data) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& info = command_list_map_[command_list];
    PTI_ASSERT(!info.immediate);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->ResetCallIdList(command_list);

    for (ZeKernelCommand* command : info.kernel_command_list) {
      ZeKernelCall* call = new ZeKernelCall;
      PTI_ASSERT(call != nullptr);

      call->command = command;
      call->queue = queue;
      call->submit_time = submit_data->host_sync;
      call->device_submit_time = submit_data->device_sync;
      PTI_ASSERT(command->append_time <= call->submit_time);
      ++(command->call_count);
      call->call_id = command->call_count;
      call->need_to_process = correlator_->IsCollectionEnabled();

      kernel_call_list_.push_back(call);
      correlator_->AddCallId(command_list, call->call_id);
    }
  }

  ze_context_handle_t GetCommandListContext(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.context;
  }

  ze_device_handle_t GetCommandListDevice(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.device;
  }

  bool IsCommandListImmediate(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.immediate;
  }

  void AddImage(ze_image_handle_t image, size_t size) {
    PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 0);
    image_size_map_[image] = size;
  }

  void RemoveImage(ze_image_handle_t image) {
    PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 1);
    image_size_map_.erase(image);
  }

  size_t GetImageSize(ze_image_handle_t image) {
    PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (image_size_map_.count(image) == 1) {
      return image_size_map_[image];
    }
    return 0;
  }

  void AddKernelGroupSize(
      ze_kernel_handle_t kernel, const ZeKernelGroupSize& group_size) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_[kernel] = group_size;
  }

  void RemoveKernelGroupSize(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_.erase(kernel);
  }

  ZeKernelGroupSize GetKernelGroupSize(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_group_size_map_.count(kernel) == 0) {
      return {0, 0, 0};
    }
    return kernel_group_size_map_[kernel];
  }

 private: // Callbacks

  static void OnEnterEventPoolCreate(ze_event_pool_create_params_t *params,
                                     ze_result_t result,
                                     void *global_data,
                                     void **instance_data) {
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

  static void OnExitEventPoolCreate(ze_event_pool_create_params_t *params,
                                    ze_result_t result,
                                    void *global_data,
                                    void **instance_data) {
    ze_event_pool_desc_t* desc =
      static_cast<ze_event_pool_desc_t*>(*instance_data);
    if (desc != nullptr) {
      delete desc;
    }
  }

  static void OnEnterEventDestroy(
      ze_event_destroy_params_t *params,
      ze_result_t result, void *global_data, void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCall(*(params->phEvent));
    }
  }

  static void OnEnterEventHostReset(
      ze_event_host_reset_params_t *params, ze_result_t result,
      void *global_data, void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCall(*(params->phEvent));
    }
  }

  static void OnExitEventHostSynchronize(
      ze_event_host_synchronize_params_t *params,
      ze_result_t result, void *global_data, void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phEvent) != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCall(*(params->phEvent));
    }
  }

  static void OnExitImageCreate(
      ze_image_create_params_t *params, ze_result_t result,
      void *global_data, void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
          reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      ze_image_desc_t image_desc = **(params->pdesc);
      size_t image_size = image_desc.width;
      switch(image_desc.type) {
        case ZE_IMAGE_TYPE_2D:
        case ZE_IMAGE_TYPE_2DARRAY:
          image_size *= image_desc.height;
          break;
        case ZE_IMAGE_TYPE_3D:
          image_size *= image_desc.height * image_desc.depth;
          break;
      }

      switch(image_desc.format.type) {
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
      }

      collector->AddImage(**(params->pphImage), image_size);
    }
  }

  static void OnExitImageDestroy(
      ze_image_destroy_params_t *params, ze_result_t result,
      void *global_data, void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->RemoveImage(*(params->phImage));
    }
  }

  static void OnEnterKernelAppend(
      ZeKernelCollector* collector,
      const ZeKernelProps& props,
      ze_event_handle_t& signal_event,
      ze_command_list_handle_t command_list,
      void** instance_data) {
    PTI_ASSERT(collector != nullptr);

    if (command_list == nullptr) {
      return;
    }

    ZeKernelCommand* command = new ZeKernelCommand;
    PTI_ASSERT(command != nullptr);
    command->props = props;
    command->append_time = collector->GetHostTimestamp();

    ze_device_handle_t device = collector->GetCommandListDevice(command_list);
    PTI_ASSERT(device != nullptr);
    command->device = device;
#ifdef PTI_KERNEL_INTERVALS
    command->timer_frequency = utils::ze::GetMetricTimerFrequency(device);
#else
    command->timer_frequency = utils::ze::GetDeviceTimerFrequency(device);
#endif
    PTI_ASSERT(command->timer_frequency > 0);
#ifdef PTI_KERNEL_INTERVALS
    command->timer_mask = utils::ze::GetMetricTimestampMask(device);
#else
    command->timer_mask = utils::ze::GetDeviceTimestampMask(device);
#endif
    PTI_ASSERT(command->timer_mask > 0);

    if (signal_event == nullptr) {
      ze_context_handle_t context =
        collector->GetCommandListContext(command_list);
      command->event = collector->event_cache_.GetEvent(context);
      PTI_ASSERT(command->event != nullptr);
      signal_event = command->event;
    } else {
      command->event = signal_event;
    }

    ZeKernelCall* call = new ZeKernelCall{};
    PTI_ASSERT(call != nullptr);
    call->command = command;

    if (collector->IsCommandListImmediate(command_list)) {
      uint64_t host_timestamp = 0, device_timestamp = 0;
      collector->GetSyncTimestamps(device, host_timestamp, device_timestamp);
      command->append_time = host_timestamp;
      call->submit_time = command->append_time;
      call->device_submit_time = device_timestamp;
      call->queue = reinterpret_cast<ze_command_queue_handle_t>(command_list);
      PTI_ASSERT(collector->correlator_ != nullptr);
      call->need_to_process = collector->correlator_->IsCollectionEnabled();
    }

    *instance_data = static_cast<void*>(call);
  }

  static void OnExitKernelAppend(
      ze_command_list_handle_t command_list,
      void* global_data, void** instance_data,
      ze_result_t result) {
    PTI_ASSERT(command_list != nullptr);

    ZeKernelCall* call = static_cast<ZeKernelCall*>(*instance_data);
    if (call == nullptr) {
      return;
    }

    ZeKernelCommand* command = call->command;
    PTI_ASSERT(command != nullptr);

    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (result != ZE_RESULT_SUCCESS) {
      collector->event_cache_.ReleaseEvent(command->event);
      delete call;
      delete command;
    } else {
      collector->AddKernelCommand(command_list, command);
      if (call->queue != nullptr) {
        collector->AddKernelCall(command_list, call);
      } else {
        delete call;
      }
    }
  }

  static ZeKernelProps GetKernelProps(
      ZeKernelCollector* collector,
      ze_kernel_handle_t kernel,
      const ze_group_count_t* group_count) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(kernel != nullptr);

    ZeKernelProps props{};

    props.name = utils::ze::GetKernelName(
        kernel, collector->options_.demangle);
    props.simd_width =
      utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelGroupSize group_size = collector->GetKernelGroupSize(kernel);

    props.group_size[0] = group_size.x;
    props.group_size[1] = group_size.y;
    props.group_size[2] = group_size.z;

    if (group_count != nullptr) {
      props.group_count[0] = group_count->groupCountX;
      props.group_count[1] = group_count->groupCountY;
      props.group_count[2] = group_count->groupCountZ;
    }

    return props;
  }

  static ZeKernelProps GetTransferProps(
      ZeKernelCollector* collector,
      std::string name,
      size_t bytes_transferred,
      ze_context_handle_t src_context,
      const void* src,
      ze_context_handle_t dst_context,
      const void* dst) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(!name.empty());

    std::string direction;

    if (src_context != nullptr && src != nullptr) {
      ze_memory_allocation_properties_t props{
          ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES,};
      ze_result_t status =
        zeMemGetAllocProperties(src_context, src, &props, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          break;
        default:
          break;
      }
    }

    if (dst_context != nullptr && dst != nullptr) {
      ze_memory_allocation_properties_t props{
          ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES,};
      ze_result_t status = zeMemGetAllocProperties(dst_context, dst, &props, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      direction.push_back('2');
      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          break;
        default:
          break;
      }
    }

    if (!direction.empty()) {
      name += "(" + direction + ")";
    }

    ZeKernelProps props{};
    props.name = name;
    props.bytes_transferred = bytes_transferred;
    return props;
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    OnEnterKernelAppend(
        collector,
        GetKernelProps(
            collector,
            *(params->phKernel),
            *(params->ppLaunchFuncArgs)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    OnEnterKernelAppend(
        collector,
        GetKernelProps(
            collector,
            *(params->phKernel),
            *(params->ppLaunchFuncArgs)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    OnEnterKernelAppend(
        collector,
        GetKernelProps(
            collector,
            *(params->phKernel),
            *(params->ppLaunchArgumentsBuffer)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    ze_context_handle_t context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendMemoryCopy", *(params->psize),
            context, *(params->psrcptr), context, *(params->pdstptr)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    ze_context_handle_t context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendMemoryFill", *(params->psize),
            context, *(params->pptr), nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    ze_context_handle_t src_context = *(params->phContextSrc);
    ze_context_handle_t dst_context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      dst_context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(dst_context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendMemoryCopyFromContext",
            *(params->psize), src_context, *(params->psrcptr),
            dst_context, *(params->pdstptr)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendBarrier(
      ze_command_list_append_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendBarrier", 0,
            nullptr, nullptr, nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendMemoryRangesBarrier", 0,
            nullptr, nullptr, nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    size_t bytes_transferred = 0;
    const ze_copy_region_t* region = *(params->psrcRegion);

    if (region != nullptr) {
      size_t bytes_transferred =
        region->width * region->height * (*(params->psrcPitch));
      if (region->depth != 0) {
        bytes_transferred *= region->depth;
      }
    }

    ze_context_handle_t context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendMemoryCopyRegion",
            bytes_transferred, context, *(params->psrcptr),
            context, *(params->pdstptr)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopy(
      ze_command_list_append_image_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    size_t bytes_transferred = collector->GetImageSize(*(params->phSrcImage));

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendImageCopy", bytes_transferred,
            nullptr, nullptr, nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    size_t bytes_transferred = collector->GetImageSize(*(params->phSrcImage));

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendImageCopyRegion",
            bytes_transferred, nullptr, nullptr, nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    size_t bytes_transferred = collector->GetImageSize(*(params->phSrcImage));

    ze_context_handle_t context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendImageCopyToMemory",
            bytes_transferred, nullptr, nullptr,
            context, *(params->pdstptr)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    size_t bytes_transferred = 0;
    const ze_image_region_t* region = *(params->ppDstRegion);

    if (region != nullptr) {
      bytes_transferred = region->width * region->height;
      if (region->depth != 0) {
        bytes_transferred *= region->depth;
      }
    }

    ze_context_handle_t context = nullptr;
    if (*(params->phCommandList) != nullptr) {
      context = collector->GetCommandListContext(*(params->phCommandList));
      PTI_ASSERT(context != nullptr);
    }

    OnEnterKernelAppend(
        collector,
        GetTransferProps(
            collector, "zeCommandListAppendImageCopyFromMemory",
            bytes_transferred, context, *(params->psrcptr),
            nullptr, nullptr),
        *(params->phSignalEvent),
        *(params->phCommandList),
        instance_data);
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendBarrier(
      ze_command_list_append_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendImageCopy(
      ze_command_list_append_image_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(*(params->phSignalEvent) != nullptr);
    OnExitKernelAppend(*params->phCommandList, global_data,
                       instance_data, result);
  }

  static void OnExitCommandListCreate(
      ze_command_list_create_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddCommandList(
          **(params->pphCommandList),
          *(params->phContext),
          *(params->phDevice),
          false);
    }
  }

  static void OnExitCommandListCreateImmediate(
      ze_command_list_create_immediate_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddCommandList(
          **(params->pphCommandList),
          *(params->phContext),
          *(params->phDevice),
          true);
    }
  }

  static void OnExitCommandListDestroy(
      ze_command_list_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCalls();
      collector->RemoveCommandList(*params->phCommandList);
    }
  }

  static void OnExitCommandListReset(
      ze_command_list_reset_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCalls();
      collector->ResetCommandList(*params->phCommandList);
    }
  }

  static void OnEnterCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    uint32_t command_list_count = *params->pnumCommandLists;
    if (command_list_count == 0) {
      return;
    }

    ze_command_list_handle_t* command_lists = *params->pphCommandLists;
    if (command_lists == nullptr) {
      return;
    }

    std::vector<ZeSyncPoint>* submit_data_list =
      new std::vector<ZeSyncPoint>();
    PTI_ASSERT(submit_data_list != nullptr);

    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_device_handle_t device =
        collector->GetCommandListDevice(command_lists[i]);
      PTI_ASSERT(device != nullptr);

      uint64_t host_timestamp = 0, device_timestamp = 0;
      collector->GetSyncTimestamps(device, host_timestamp, device_timestamp);
      submit_data_list->push_back({host_timestamp, device_timestamp});
    }

    *reinterpret_cast<std::vector<ZeSyncPoint>**>(instance_data) =
      submit_data_list;
  }

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    std::vector<ZeSyncPoint>* submit_data_list =
      *reinterpret_cast<std::vector<ZeSyncPoint>**>(instance_data);
    PTI_ASSERT(submit_data_list != nullptr);

    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      uint32_t command_list_count = *params->pnumCommandLists;
      ze_command_list_handle_t* command_lists = *params->pphCommandLists;
      for (uint32_t i = 0; i < command_list_count; ++i) {
        if (!collector->IsCommandListImmediate(command_lists[i])) {
          collector->AddKernelCalls(
              command_lists[i],
              *(params->phCommandQueue),
              &submit_data_list->at(i));
        }
      }
    }

    delete submit_data_list;
  }

  static void OnExitCommandQueueSynchronize(
      ze_command_queue_synchronize_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCalls();
    }
  }

  static void OnExitCommandQueueDestroy(
      ze_command_queue_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCalls();
    }
  }

  static void OnExitKernelSetGroupSize(
      ze_kernel_set_group_size_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      ZeKernelGroupSize group_size{
          *(params->pgroupSizeX),
          *(params->pgroupSizeY),
          *(params->pgroupSizeZ)};
      collector->AddKernelGroupSize(*(params->phKernel), group_size);
    }
  }

  static void OnExitKernelDestroy(
      ze_kernel_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->RemoveKernelGroupSize(*(params->phKernel));
    }
  }

  static void OnExitContextDestroy(
      ze_context_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->event_cache_.ReleaseContext(*(params->phContext));
    }
  }

 private: // Data
  zel_tracer_handle_t tracer_ = nullptr;

  KernelCollectorOptions options_;

  Correlator* correlator_ = nullptr;
  std::atomic<uint64_t> kernel_id_;

  OnZeKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  ZeKernelInfoMap kernel_info_map_;
  std::list<ZeKernelCall*> kernel_call_list_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;
  ZeKernelGroupSizeMap kernel_group_size_map_;
  ZeDeviceMap device_map_;

  ZeEventCache event_cache_;

#ifdef PTI_KERNEL_INTERVALS
  ZeKernelIntervalList kernel_interval_list_;
  std::map<ze_device_handle_t, ZeSyncPoint> sync_point_map_;
#endif // PTI_KERNEL_INTERVALS

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 12;
};

#endif // PTI_TOOLS_ZE_TRACER_ZE_KERNEL_COLLECTOR_H_