//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_
#define PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <level_zero/layers/zel_tracing_api.h>

#include "utils.h"
#include "ze_utils.h"

struct ZeKernelCommand {
  std::string name;
  size_t simd_width;
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  uint64_t timer_frequency;
};

struct ZeKernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;
  size_t simd_width;

  bool operator>(const ZeKernelInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ZeKernelInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

struct ZeKernelInterval {
  std::string name;
  uint64_t start;
  uint64_t end;
};

struct ZeCommandListInfo {
  std::vector<const ZeKernelCommand*> kernel_command_list;
  ze_context_handle_t context;
  ze_device_handle_t device;
  bool immediate;
};

using ZeKernelInfoMap = std::map<std::string, ZeKernelInfo>;
using ZeKernelIntervalList = std::vector<ZeKernelInterval>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;

class ZeKernelCollector {
 public: // Interface

  static ZeKernelCollector* Create() {
    PTI_ASSERT(utils::ze::GetVersion() != ZE_API_VERSION_1_0);

    ZeKernelCollector* collector = new ZeKernelCollector();
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

  static void PrintKernelsTable(const ZeKernelInfoMap& kernel_info_map) {
    std::set< std::pair<std::string, ZeKernelInfo>,
              utils::Comparator > sorted_list(
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
      std::setw(kSimdLength) << "SIMD" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      size_t simd_width = value.second.simd_width;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kSimdLength) << simd_width << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kTimeLength) << min_duration << "," <<
        std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

  ~ZeKernelCollector() {
    if (tracer_ != nullptr) {
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  const ZeKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  const ZeKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }

 private: // Implementation

  ZeKernelCollector() {}

  void EnableTracing(zel_tracer_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t prologue_callbacks{};
    zet_core_callbacks_t epilogue_callbacks{};

    prologue_callbacks.Event.pfnDestroyCb = OnEnterEventDestroy;

    prologue_callbacks.Event.pfnHostResetCb = OnEnterEventHostReset;

    epilogue_callbacks.Event.pfnHostSynchronizeCb = OnExitEventHostSynchronize;

    prologue_callbacks.EventPool.pfnCreateCb = OnEnterEventPoolCreate;
    epilogue_callbacks.EventPool.pfnCreateCb = OnExitEventPoolCreate;

    prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnEnterCommandListAppendLaunchKernel;
    epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnExitCommandListAppendLaunchKernel;

    epilogue_callbacks.CommandList.pfnCreateCb =
      OnExitCommandListCreate;
    epilogue_callbacks.CommandList.pfnCreateImmediateCb =
      OnExitCommandListCreateImmediate;
    epilogue_callbacks.CommandList.pfnDestroyCb =
      OnExitCommandListDestroy;
    epilogue_callbacks.CommandList.pfnResetCb =
      OnExitCommandListReset;

    epilogue_callbacks.CommandQueue.pfnExecuteCommandListsCb =
      OnExitCommandQueueExecuteCommandLists;
    epilogue_callbacks.CommandQueue.pfnSynchronizeCb =
      OnExitCommandQueueSynchronize;
    epilogue_callbacks.CommandQueue.pfnDestroyCb =
      OnExitCommandQueueDestroy;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetPrologues(tracer_, &prologue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddKernelCommand(
      ze_command_list_handle_t command_list,
      const ZeKernelCommand* command) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(command != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& info = command_list_map_[command_list];
    info.kernel_command_list.push_back(command);
  }

  void AddKernelCall(const ZeKernelCommand* call) {
    PTI_ASSERT(call != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_call_list_.push_back(call);
  }

  void AddKernelCalls(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& info = command_list_map_[command_list];
    PTI_ASSERT(!info.immediate);

    for (const ZeKernelCommand* command : info.kernel_command_list) {
      kernel_call_list_.push_back(command);
    }
  }

  void ProcessCall(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);

    for (auto it = kernel_call_list_.begin();
         it != kernel_call_list_.end(); ++it) {
      const ZeKernelCommand* call = *it;
      PTI_ASSERT(call != nullptr);
      if (call->event == event) {
        ProcessCall(call);
        kernel_call_list_.erase(it);
        break;
      }
    }
  }

  void ProcessCall(const ZeKernelCommand* call) {
    PTI_ASSERT(call != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeEventQueryStatus(call->event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_kernel_timestamp_result_t timestamp{};
    status = zeEventQueryKernelTimestamp(call->event, &timestamp);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    uint64_t start = timestamp.global.kernelStart;
    uint64_t end = timestamp.global.kernelEnd;
    uint64_t freq = call->timer_frequency;
    PTI_ASSERT(freq > 0);

    uint64_t time = 0, start_ns = 0, end_ns = 0;

    start_ns = start *
      static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    if (start < end) {
      end_ns = end *
        static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    } else { // 32-bit timer overflow
      PTI_ASSERT(start < (1ULL << 32));
      end_ns = ((1ULL << 32) + end) *
        static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    }
    time = end_ns - start_ns;

    AddKernelInfo(call->name, time, call->simd_width);
    AddKernelInterval(call->name, start_ns, end_ns);
  }

  void ProcessCalls() {
    ze_result_t status = ZE_RESULT_SUCCESS;

    const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_call_list_.begin();
    while (it != kernel_call_list_.end()) {
      const ZeKernelCommand* call = *it;
      PTI_ASSERT(call != nullptr);
      PTI_ASSERT(call->event != nullptr);
      status = zeEventQueryStatus(call->event);
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

  void AddKernelInfo(
      std::string name, uint64_t time, size_t simd_width) {
    PTI_ASSERT(!name.empty());
    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {time, time, time, 1, simd_width};
    } else {
      ZeKernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      if (time > kernel.max_time) {
        kernel.max_time = time;
      }
      if (time < kernel.min_time) {
        kernel.min_time = time;
      }
      kernel.call_count += 1;
      kernel.simd_width = std::max(kernel.simd_width, simd_width);
    }
  }

  void AddKernelInterval(std::string name, uint64_t start, uint64_t end) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(start < end);
    kernel_interval_list_.push_back({name, start, end});
  }

  void AddCommandList(
      ze_command_list_handle_t command_list,
      ze_context_handle_t context,
      ze_device_handle_t device,
      bool immediate) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(context != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 0);
    command_list_map_[command_list] = {
        std::vector<const ZeKernelCommand*>(), context, device, immediate};
  }

  void RemoveKernelCommands(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& info = command_list_map_[command_list];
    for (const ZeKernelCommand* command : info.kernel_command_list) {
      if (command->event_pool != nullptr) {
        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zeEventDestroy(command->event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(command->event_pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }

      for (const ZeKernelCommand* call : kernel_call_list_) {
        PTI_ASSERT(call != command);
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
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    RemoveKernelCommands(command_list);
  }

  ze_context_handle_t GetCommandListContext(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    return command_list_map_[command_list].context;
  }

  ze_device_handle_t GetCommandListDevice(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    return command_list_map_[command_list].device;
  }

  bool IsCommandListImmediate(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.immediate;
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

  static void OnEnterEventDestroy(ze_event_destroy_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCall(*(params->phEvent));
    }
  }

  static void OnEnterEventHostReset(ze_event_host_reset_params_t *params,
                                    ze_result_t result,
                                    void *global_data,
                                    void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessCall(*(params->phEvent));
    }
  }

  static void OnExitEventHostSynchronize(ze_event_host_synchronize_params_t *params,
                                    ze_result_t result,
                                    void *global_data,
                                    void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
    collector->ProcessCall(*(params->phEvent));
    collector->ProcessCalls();
    }
  }

  static void CreateEvent(ze_context_handle_t context,
                          ze_event_pool_handle_t& event_pool,
                          ze_event_handle_t& event) {
    PTI_ASSERT(context != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};
    status = zeEventPoolCreate(
        context, &event_pool_desc, 0, nullptr, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_desc_t event_desc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  static void OnEnterKernelAppend(
      std::string name, size_t simd_width,
      ze_event_handle_t& signal_event, ze_command_list_handle_t command_list,
      void* global_data, void** instance_data) {
    PTI_ASSERT(!name.empty());

    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (command_list == nullptr) {
      return;
    }

    ZeKernelCommand* command = new ZeKernelCommand;
    PTI_ASSERT(command != nullptr);

    command->name = name;
    command->simd_width = simd_width;

    ze_device_handle_t device = collector->GetCommandListDevice(command_list);
    PTI_ASSERT(device != nullptr);
    command->timer_frequency = utils::ze::GetDeviceTimerFrequency(device);
    PTI_ASSERT(command->timer_frequency > 0);

    if (signal_event == nullptr) {
      ze_context_handle_t context =
        collector->GetCommandListContext(command_list);
      CreateEvent(context, command->event_pool, command->event);
      signal_event = command->event;
    } else {
      command->event_pool = nullptr;
      command->event = signal_event;
    }

    *instance_data = static_cast<void*>(command);
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        utils::ze::GetKernelMaxSubgroupSize(*(params->phKernel)),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnExitKernelAppend(
      ze_command_list_handle_t command_list,
      void* global_data, void** instance_data,
      ze_result_t result) {
    PTI_ASSERT(command_list != nullptr);

    ZeKernelCommand* command =
      static_cast<ZeKernelCommand*>(*instance_data);
    if (command == nullptr) {
      return;
    }

    if (result != ZE_RESULT_SUCCESS) {
      if (command->event_pool != nullptr) {
        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zeEventDestroy(command->event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(command->event_pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }

      delete command;
    } else {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddKernelCommand(command_list, command);
      if (collector->IsCommandListImmediate(command_list)) {
        collector->AddKernelCall(command);
      }
    }
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
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

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      uint32_t command_list_count = *params->pnumCommandLists;
      ze_command_list_handle_t* command_lists = *params->pphCommandLists;
      for (uint32_t i = 0; i < command_list_count; ++i) {
        if (!collector->IsCommandListImmediate(command_lists[i])) {
          collector->AddKernelCalls(command_lists[i]);
        }
      }
    }
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

 private: // Data
  zel_tracer_handle_t tracer_ = nullptr;

  std::mutex lock_;
  ZeKernelInfoMap kernel_info_map_;
  ZeKernelIntervalList kernel_interval_list_;
  std::list<const ZeKernelCommand*> kernel_call_list_;
  ZeCommandListMap command_list_map_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kSimdLength = 5;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_