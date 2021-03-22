//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_
#define PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <level_zero/layers/zel_tracing_api.h>

#include "correlator.h"
#include "i915_utils.h"
#include "utils.h"
#include "ze_utils.h"

struct ZeKernelInstance {
  std::string name;
  uint64_t kernel_id;
  size_t simd_width;
  size_t bytes_transferred;
  void* queue;
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  uint64_t append_time;
  uint64_t submit_time;
};

struct ZeKernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;
  size_t simd_width;
  size_t bytes_transferred;

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
  ze_context_handle_t context;
  std::vector<ZeKernelInstance*> kernel_list;
  bool immediate;
};

using ZeKernelInfoMap = std::map<std::string, ZeKernelInfo>;
using ZeKernelIntervalList = std::vector<ZeKernelInterval>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;
using ZeImageSizeMap = std::map<ze_image_handle_t, size_t>;

typedef void (*OnZeKernelFinishCallback)(
    void* data, void* queue,
    const std::string& id, const std::string& name,
    uint64_t appended, uint64_t submitted,
    uint64_t started, uint64_t ended);

class ZeKernelCollector {
 public: // Interface

  static ZeKernelCollector* Create(
      Correlator* correlator,
      OnZeKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(correlator != nullptr);
    ZeKernelCollector* collector = new ZeKernelCollector(
        correlator, callback, callback_data);
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

  const ZeKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  const ZeKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }

 private: // Implementation

  ZeKernelCollector(
      Correlator* correlator,
      OnZeKernelFinishCallback callback,
      void* callback_data)
      : correlator_(correlator),
        callback_(callback),
        callback_data_(callback_data),
        timer_frequency_(utils::i915::GetGpuTimerFrequency()),
        kernel_id_(1) {
    PTI_ASSERT(correlator_ != nullptr);
    PTI_ASSERT(timer_frequency_ > 0);
    if (callback_ != nullptr) {
      SetCpuGpuTimestamps();
    }
  }

  uint64_t GetTimestamp() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetTimestamp();
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

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetPrologues(tracer_, &prologue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddKernelInstance(ze_command_list_handle_t command_list,
                         const ZeKernelInstance& instance) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_instance_list_.push_back(instance);

    ZeKernelInstance* kernel_instance = &kernel_instance_list_.back();
    kernel_instance->kernel_id = kernel_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
    kernel_instance->append_time = (kernel_instance->append_time + GetTimestamp()) / 2;

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    if (command_list_info.immediate) {
      kernel_instance->submit_time = kernel_instance->append_time;
      kernel_instance->queue = command_list;
    }
    command_list_info.kernel_list.push_back(kernel_instance);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->SetKernelId(kernel_instance->kernel_id);
    correlator_->AddKernelId(command_list, kernel_instance->kernel_id);
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

  void ProcessInstance(const ZeKernelInstance& instance) {
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
      PTI_ASSERT(instance.append_time < instance.submit_time);

      uint64_t cpu_start = 0, cpu_end = 0;
      uint64_t time_shift = correlator_->GetTimeDiff(cpu_timestamp_);

      if (gpu_timestamp_ < start) {
        cpu_start = (start - gpu_timestamp_) *
          static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_ + time_shift;
        cpu_end = cpu_start + time;
      } else { // 32-bit timer overflow
        SetCpuGpuTimestamps();
        time_shift = correlator_->GetTimeDiff(cpu_timestamp_);
        PTI_ASSERT(start < gpu_timestamp_);
        cpu_start = time_shift - (gpu_timestamp_ - start) *
          static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
        cpu_end = cpu_start + time;
      }

      PTI_ASSERT(instance.queue != nullptr);
      PTI_ASSERT(!instance.name.empty());
      PTI_ASSERT(cpu_start > instance.append_time);
      callback_(
          callback_data_, instance.queue,
          std::to_string(instance.kernel_id), instance.name,
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
      ZeKernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      if (time > kernel.max_time) {
        kernel.max_time = time;
      }
      if (time < kernel.min_time) {
        kernel.min_time = time;
      }
      kernel.call_count += 1;
      kernel.bytes_transferred += bytes_transferred;
      kernel.simd_width = std::max(kernel.simd_width, simd_width);
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
    command_list_map_[command_list] = {context, std::vector<ZeKernelInstance*>(), immediate};

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->CreateKernelIdList(command_list);
  }

  void RemoveCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    command_list_map_.erase(command_list);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->RemoveKernelIdList(command_list);
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    command_list_map_[command_list].kernel_list.clear();

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->ResetKernelIdList(command_list);
  }

  void UpdateKernelInstances(
      ze_command_list_handle_t command_list,
      ze_command_queue_handle_t queue, uint64_t submit_time) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    if (!command_list_info.immediate) {
      std::vector<ZeKernelInstance*>& kernel_list =
        command_list_info.kernel_list;
      for (size_t i = 0; i < kernel_list.size(); ++i) {
        kernel_list[i]->queue = queue;
        kernel_list[i]->submit_time = submit_time;
        PTI_ASSERT(kernel_list[i]->append_time < kernel_list[i]->submit_time);
      }
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

  void SetCpuGpuTimestamps() {
    gpu_timestamp_ = utils::i915::GetGpuTimestamp() & 0x0FFFFFFFF;
    cpu_timestamp_ = std::chrono::steady_clock::now();
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
    profiling_desc->flags = (desc->flags | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
    profiling_desc->flags = (desc->flags | ZE_EVENT_POOL_FLAG_HOST_VISIBLE);
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
      std::string name, size_t simd_width, size_t bytes_transferred,
      ze_event_handle_t& signal_event, ze_command_list_handle_t command_list,
      void* global_data, void** instance_data) {
    PTI_ASSERT(!name.empty());

    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (command_list == nullptr) {
      return;
    }

    ZeKernelInstance* instance = new ZeKernelInstance;
    instance->name = name;
    instance->bytes_transferred = bytes_transferred;
    instance->simd_width = simd_width;
    instance->append_time = collector->GetTimestamp();
    instance->submit_time = 0;
    instance->queue = nullptr;

    if (signal_event == nullptr) {
      ze_context_handle_t context =
        collector->GetCommandListContext(command_list);
      CreateEvent(context, instance->event_pool, instance->event);
      signal_event = instance->event;
    } else {
      instance->event_pool = nullptr;
      instance->event = signal_event;
    }

    *instance_data = static_cast<void*>(instance);
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

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        utils::ze::GetKernelMaxSubgroupSize(*(params->phKernel)),
        0, *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        utils::ze::GetKernelMaxSubgroupSize(*(params->phKernel)),
        0, *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        utils::ze::GetKernelMaxSubgroupSize(*(params->phKernel)),
        0, *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopy", 0, *(params->psize),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryFill", 0, *(params->psize),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendBarrier(
      ze_command_list_append_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendBarrier", 0, 0,
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryRangesBarrier", 0, 0,
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ze_copy_region_t region = **(params->psrcRegion);
    size_t bytes_transferred =
      region.width * region.height * (*(params->psrcPitch));
    if (region.depth != 0) {
      bytes_transferred *= region.depth;
    }
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopyRegion", 0, bytes_transferred,
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopyFromContext", 0, *(params->psize),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendImageCopy(
      ze_command_list_append_image_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    OnEnterKernelAppend(
        "zeCommandListAppendImageCopy",
        0, collector->GetImageSize(*(params->phSrcImage)),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    OnEnterKernelAppend(
        "zeCommandListAppendImageCopyRegion",
        0, collector->GetImageSize(*(params->phSrcImage)),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    OnEnterKernelAppend(
        "zeCommandListAppendImageCopyToMemory",
        0, collector->GetImageSize(*(params->phSrcImage)),
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ze_image_region_t region_desc = **(params->ppDstRegion);
    size_t bytes_transferred =
      region_desc.width * region_desc.height;
    if (region_desc.depth != 0) {
      bytes_transferred *= region_desc.depth;
    }
    OnEnterKernelAppend(
        "zeCommandListAppendImageCopyFromMemory", 0, bytes_transferred,
        *(params->phSignalEvent), *(params->phCommandList),
        global_data, instance_data);
  }

  static void OnExitKernelAppend(
      ze_command_list_handle_t command_list,
      void* global_data, void** instance_data,
      ze_result_t result) {
    PTI_ASSERT(command_list != nullptr);

    ZeKernelInstance* instance =
      static_cast<ZeKernelInstance*>(*instance_data);
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
          **(params->pphCommandList), *(params->phContext), false);
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

    uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_data);
    start_time = collector->GetTimestamp();
  }

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result != ZE_RESULT_SUCCESS) {
      return;
    }

    ZeKernelCollector* collector =
      reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    uint64_t end_time = collector->GetTimestamp();
    uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_data);
    PTI_ASSERT(start_time > 0);
    PTI_ASSERT(start_time < end_time);

    uint64_t submit_time = (start_time + end_time) / 2;
    uint32_t command_list_count = *params->pnumCommandLists;
    ze_command_list_handle_t* command_lists = *params->pphCommandLists;
    for (uint32_t i = 0; i < command_list_count; ++i) {
      collector->UpdateKernelInstances(
          command_lists[i], *(params->phCommandQueue), submit_time);
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
  Correlator* correlator_ = nullptr;
  std::atomic<uint64_t> kernel_id_;

  OnZeKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  TimePoint cpu_timestamp_;
  uint64_t gpu_timestamp_ = 0;

  std::mutex lock_;
  ZeKernelInfoMap kernel_info_map_;
  ZeKernelIntervalList kernel_interval_list_;
  std::list<ZeKernelInstance> kernel_instance_list_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kSimdLength = 5;
  static const uint32_t kTransferredLength = 20;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_