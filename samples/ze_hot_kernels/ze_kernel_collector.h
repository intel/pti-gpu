//==============================================================
// Copyright © 2020 Intel Corporation
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

#include "i915_utils.h"
#include "utils.h"
#include "ze_utils.h"

struct KernelInstance {
  std::string name;
  size_t simd_width;
  size_t bytes_transferred;
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  uint64_t append_time;
  uint64_t submit_time;
};

struct KernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;
  size_t simd_width;
  size_t bytes_transferred;

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

struct KernelInterval {
  std::string name;
  uint64_t start;
  uint64_t end;
};

struct CommandListInfo {
  ze_context_handle_t context;
  std::vector<KernelInstance*> kernel_list;
  bool immediate;
};

using KernelInfoMap = std::map<std::string, KernelInfo>;
using KernelIntervalList = std::vector<KernelInterval>;
using KernelNameMap = std::map<ze_kernel_handle_t, std::string>;
using KernelTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using CommandListMap = std::map<ze_command_list_handle_t, CommandListInfo>;

typedef void (*OnKernelFinishCallback)(
    void* data, const std::string& name,
    uint64_t appended, uint64_t submitted,
    uint64_t started, uint64_t ended);

class ZeKernelCollector {
 public: // Interface

  static ZeKernelCollector* Create(
      KernelTimePoint base_time = std::chrono::steady_clock::now(),
      OnKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    ZeKernelCollector* collector = new ZeKernelCollector(
        base_time, callback, callback_data);
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

  static void PrintKernelsTable(const KernelInfoMap& kernel_info_map) {
    std::set< std::pair<std::string, KernelInfo>,
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
      std::setw(kTransferredLength) <<
        "Transferred (bytes)" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      size_t simd_width = value.second.simd_width;
      size_t bytes_transferred = value.second.bytes_transferred;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kSimdLength) << simd_width << "," <<
        std::setw(kTransferredLength) <<
          bytes_transferred << "," <<
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

  const KernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  const KernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }

 private: // Implementation

  ZeKernelCollector(KernelTimePoint base_time,
                    OnKernelFinishCallback callback,
                    void* callback_data)
      : base_time_(base_time), callback_(callback),
        callback_data_(callback_data),
        timer_frequency_(utils::i915::GetGpuTimerFrequency()) {
    PTI_ASSERT(timer_frequency_ > 0);
    if (callback_ != nullptr) {
      SetCpuGpuTimestamps();
    }
  }

  uint64_t GetTimestamp() const {
    std::chrono::duration<uint64_t, std::nano> timestamp =
      std::chrono::steady_clock::now() - base_time_;
    return timestamp.count();
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

    prologue_callbacks.CommandList.pfnAppendMemoryCopyCb =
      OnEnterCommandListAppendMemoryCopy;
    epilogue_callbacks.CommandList.pfnAppendMemoryCopyCb =
      OnExitCommandListAppendMemoryCopy;

    prologue_callbacks.CommandQueue.pfnExecuteCommandListsCb =
      OnEnterCommandQueueExecuteCommandLists;

    epilogue_callbacks.CommandList.pfnCreateCb =
      OnExitCommandListCreate;
    epilogue_callbacks.CommandList.pfnCreateImmediateCb =
      OnExitCommandListImmediateCreate;
    epilogue_callbacks.CommandList.pfnDestroyCb =
      OnExitCommandListDestroy;
    epilogue_callbacks.CommandList.pfnResetCb =
      OnExitCommandListReset;

    epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;

    epilogue_callbacks.Kernel.pfnDestroyCb = OnExitKernelDestroy;

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

  void AddKernelName(ze_kernel_handle_t kernel, std::string name) {
    PTI_ASSERT(kernel != nullptr);
    PTI_ASSERT(!name.empty());

    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_name_map_.count(kernel) == 0);
    kernel_name_map_[kernel] = name;
  }

  void RemoveKernelName(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_name_map_.count(kernel) == 1);
    kernel_name_map_.erase(kernel);
  }

  std::string GetKernelName(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);

    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_name_map_.count(kernel) == 1) {
      return kernel_name_map_[kernel];
    }

    return std::string();
  }

  void AddKernelInstance(ze_command_list_handle_t command_list,
                         const KernelInstance& instance) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_instance_list_.push_back(instance);
    KernelInstance* kernel_instance = &kernel_instance_list_.back();

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    CommandListInfo& command_list_info = command_list_map_[command_list];
    if (command_list_info.immediate) {
      kernel_instance->submit_time = kernel_instance->append_time;
    }
    command_list_info.kernel_list.push_back(kernel_instance);
  }

  void ProcessInstance(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    for (auto it = kernel_instance_list_.begin();
         it != kernel_instance_list_.end(); ++it) {
      if ((*it).event == event) {
        ProcessInstance(*it);
        kernel_instance_list_.erase(it);
        break;
      }
    }
  }

  void ProcessInstance(const KernelInstance& instance) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeEventQueryStatus(instance.event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_kernel_timestamp_result_t timestamp{};
    status = zeEventQueryKernelTimestamp(instance.event, &timestamp);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    uint64_t start = timestamp.global.kernelStart;
    uint64_t end = timestamp.global.kernelEnd;
    uint64_t time = 0, start_ns = 0, end_ns = 0;

    start_ns = start *
      static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    if (start < end) {
      end_ns = end *
        static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    } else { // 32-bit timer overflow
      PTI_ASSERT(start < (1ULL << 32));
      end_ns = ((1ULL << 32) + end) *
        static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    }
    time = end_ns - start_ns;

    AddKernelInfo(instance.name, time,
                  instance.simd_width,
                  instance.bytes_transferred);

    if (instance.simd_width > 0) { // User kernels only
      AddKernelInterval(instance.name, start_ns, end_ns);
    }

    if (callback_ != nullptr) {
      PTI_ASSERT(instance.append_time > 0);
      PTI_ASSERT(instance.submit_time > 0);

      uint64_t cpu_start = 0, cpu_end = 0;
      std::chrono::duration<uint64_t, std::nano> time_shift =
        cpu_timestamp_ - base_time_;

      if (gpu_timestamp_ < start) {
        cpu_start = (start - gpu_timestamp_) *
          static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_ +
          time_shift.count();
        cpu_end = cpu_start + time;
      } else { // 32-bit timer overflow
        SetCpuGpuTimestamps();
        time_shift = cpu_timestamp_ - base_time_;
        PTI_ASSERT(start < gpu_timestamp_);
        cpu_start = time_shift.count() - (gpu_timestamp_ - start) *
          static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
        cpu_end = cpu_start + time;
      }

      callback_(callback_data_, instance.name,
                instance.append_time, instance.submit_time,
                cpu_start, cpu_end);
    }

    if (instance.event_pool != nullptr) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zeEventDestroy(instance.event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      status = zeEventPoolDestroy(instance.event_pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void ProcessInstances() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_instance_list_.begin();
    while (it != kernel_instance_list_.end()) {
      PTI_ASSERT(it->event != nullptr);
      status = zeEventQueryStatus(it->event);
      if (status == ZE_RESULT_NOT_READY) {
        ++it;
      } else if (status == ZE_RESULT_SUCCESS) {
        ProcessInstance(*it);
        it = kernel_instance_list_.erase(it);
      } else {
        PTI_ASSERT(0);
      }
    }
  }

  void AddKernelInfo(
      std::string name, uint64_t time,
      size_t simd_width, size_t bytes_transferred) {
    PTI_ASSERT(!name.empty());
    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {
        time, time, time, 1, simd_width, bytes_transferred};
    } else {
      KernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      if (time > kernel.max_time) {
        kernel.max_time = time;
      }
      if (time < kernel.min_time) {
        kernel.min_time = time;
      }
      kernel.call_count += 1;
      kernel.bytes_transferred += bytes_transferred;
      PTI_ASSERT(kernel.simd_width == simd_width);
    }
  }

  void AddKernelInterval(std::string name, uint64_t start, uint64_t end) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(start < end);
    kernel_interval_list_.push_back({name, start, end});
  }

  void AddCommandList(ze_command_list_handle_t command_list,
                      ze_context_handle_t context, bool immediate) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(context != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 0);
    command_list_map_[command_list] = {context, std::vector<KernelInstance*>(), immediate};
  }

  void RemoveCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    command_list_map_.erase(command_list);
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    command_list_map_[command_list].kernel_list.clear();
  }

  void SetKernelInstanceSubmitTime(ze_command_list_handle_t command_list,
                                   uint64_t submit_time) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    CommandListInfo& command_list_info = command_list_map_[command_list];
    if (!command_list_info.immediate) {
      std::vector<KernelInstance*>& kernel_list =
        command_list_info.kernel_list;
      for (size_t i = 0; i < kernel_list.size(); ++i) {
        kernel_list[i]->submit_time = submit_time;
      }
    }
  }

  ze_context_handle_t GetCommandListContext(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    CommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.context;
  }

  void SetCpuGpuTimestamps() {
    gpu_timestamp_ = utils::i915::GetGpuTimestamp() & 0x0FFFFFFFF;
    cpu_timestamp_ = std::chrono::steady_clock::now();
  }

 private: // Callbacks

  static void OnExitKernelCreate(
      ze_kernel_create_params_t *params, ze_result_t result,
      void *global_data, void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddKernelName(
          **(params->pphKernel), (*(params->pdesc))->pKernelName);
    }
  }

  static void OnExitKernelDestroy(ze_kernel_destroy_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->RemoveKernelName(*(params->phKernel));
    }
  }

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
    profiling_desc->flags = (desc->flags | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
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
      collector->ProcessInstance(*(params->phEvent));
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
      collector->ProcessInstance(*(params->phEvent));
    }
  }

  static void CreateEvent(ze_context_handle_t context,
                          ze_event_pool_handle_t& event_pool,
                          ze_event_handle_t& event) {
    PTI_ASSERT(context != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1};
    status = zeEventPoolCreate(
        context, &event_pool_desc, 0, nullptr, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_desc_t event_desc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (*(params->phKernel) ==  nullptr ||
        *(params->phCommandList) == nullptr) {
      return;
    }

    KernelInstance* instance = new KernelInstance;
    PTI_ASSERT(instance != nullptr);
    instance->name = collector->GetKernelName(*(params->phKernel));
    PTI_ASSERT(!instance->name.empty());

    ze_kernel_properties_t props{};
    ze_result_t status = zeKernelGetProperties(*(params->phKernel), &props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    instance->simd_width = props.maxSubgroupSize;
    instance->bytes_transferred = 0;
    instance->append_time = collector->GetTimestamp();
    instance->submit_time = 0;

    if (*(params->phSignalEvent) == nullptr) {
      ze_context_handle_t context =
        collector->GetCommandListContext(*(params->phCommandList));
      CreateEvent(context, instance->event_pool, instance->event);
      *(params->phSignalEvent) = instance->event;
    } else {
      instance->event_pool = nullptr;
      instance->event = *(params->phSignalEvent);
    }

    *instance_data = static_cast<void*>(instance);
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (*(params->phCommandList) == nullptr) {
      return;
    }

    KernelInstance* instance = new KernelInstance;
    instance->name = "zeCommandListAppendMemoryCopy";
    instance->bytes_transferred = *(params->psize);
    instance->simd_width = 0;
    instance->append_time = collector->GetTimestamp();
    instance->submit_time = 0;

    if (*(params->phSignalEvent) == nullptr) {
      ze_context_handle_t context =
        collector->GetCommandListContext(*(params->phCommandList));
      CreateEvent(context, instance->event_pool, instance->event);
      *(params->phSignalEvent) = instance->event;
    } else {
      instance->event_pool = nullptr;
      instance->event = *(params->phSignalEvent);
    }

    *instance_data = static_cast<void*>(instance);
  }

  static void OnExitKernelAppend(ze_command_list_handle_t command_list,
                                 void* global_data, void** instance_data,
                                 ze_result_t result) {
    PTI_ASSERT(command_list != nullptr);

    KernelInstance* instance = static_cast<KernelInstance*>(*instance_data);
    if (instance == nullptr) {
      return;
    }

    if (result != ZE_RESULT_SUCCESS) {
      if (instance->event_pool != nullptr) {
        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zeEventDestroy(instance->event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(instance->event_pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }
    } else {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddKernelInstance(command_list, *instance);
    }

    delete instance;
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
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

  static void OnExitCommandListCreate(
      ze_command_list_create_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddCommandList(
          **(params->pphCommandList), *(params->phContext), false);
    }
  }

  static void OnExitCommandListImmediateCreate(
      ze_command_list_create_immediate_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddCommandList(
          **(params->pphCommandList), *(params->phContext), true);
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
      collector->ProcessInstances();
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
      collector->ProcessInstances();
      collector->ResetCommandList(*params->phCommandList);
    }
  }

  static void OnEnterCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    uint64_t submit_time = collector->GetTimestamp();
    uint32_t command_list_count = *params->pnumCommandLists;
    ze_command_list_handle_t* command_lists = *params->pphCommandLists;
    for (uint32_t i = 0; i < command_list_count; ++i) {
      collector->SetKernelInstanceSubmitTime(command_lists[i], submit_time);
    }
  }

  static void OnExitCommandQueueSynchronize(
      ze_command_queue_synchronize_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessInstances();
    }
  }

  static void OnExitCommandQueueDestroy(
      ze_command_queue_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessInstances();
    }
  }

 private: // Data
  zel_tracer_handle_t tracer_ = nullptr;

  uint64_t timer_frequency_ = 0;
  KernelTimePoint base_time_;

  OnKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  KernelTimePoint cpu_timestamp_;
  uint64_t gpu_timestamp_ = 0;

  std::mutex lock_;
  KernelInfoMap kernel_info_map_;
  KernelIntervalList kernel_interval_list_;
  KernelNameMap kernel_name_map_;
  std::list<KernelInstance> kernel_instance_list_;
  CommandListMap command_list_map_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kSimdLength = 5;
  static const uint32_t kTransferredLength = 20;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_