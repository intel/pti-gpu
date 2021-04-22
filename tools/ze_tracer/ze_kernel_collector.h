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

struct ZeSubmitData {
  uint64_t host_sync;
  uint64_t device_sync;
};

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

struct ZeKernelProps {
  size_t simd_width;
  size_t bytes_transferred;
  uint32_t group_count[3];
  uint32_t group_size[3];
};

struct ZeKernelInstance {
  std::string name;
  uint64_t kernel_id;
  void* queue;
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  uint64_t append_time;
  uint64_t submit_time;
  uint64_t device_submit_time;
  ZeKernelProps props;
};

struct ZeKernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

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

using ZeKernelGroupSizeMap = std::map<ze_kernel_handle_t, ZeKernelGroupSize>;
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
      bool verbose,
      OnZeKernelFinishCallback callback = nullptr,
      void* callback_data = nullptr) {
    PTI_ASSERT(correlator != nullptr);
    ZeKernelCollector* collector = new ZeKernelCollector(
        correlator, verbose, callback, callback_data);
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
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void PrintKernelsTable() const {
    std::set< std::pair<std::string, ZeKernelInfo>,
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
      bool verbose,
      OnZeKernelFinishCallback callback,
      void* callback_data)
      : correlator_(correlator),
        verbose_(verbose),
        callback_(callback),
        callback_data_(callback_data),
        timer_frequency_(utils::i915::GetGpuTimerFrequency()),
        kernel_id_(1) {
    PTI_ASSERT(correlator_ != nullptr);
    PTI_ASSERT(timer_frequency_ > 0);
  }

  uint64_t GetTimestamp() const {
    PTI_ASSERT(correlator_ != nullptr);
    return correlator_->GetTimestamp();
  }

  uint64_t GetDeviceTimestamp() const {
    return utils::i915::GetGpuTimestamp() & 0x0FFFFFFFF;
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
    PTI_ASSERT(kernel_instance->append_time > 0);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    command_list_info.kernel_list.push_back(kernel_instance);

    PTI_ASSERT(correlator_ != nullptr);
    correlator_->SetKernelId(kernel_instance->kernel_id);
    correlator_->AddKernelId(command_list, kernel_instance->kernel_id);
  }

  void ProcessInstance(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeEventQueryStatus(event);
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

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

    uint64_t duration = 0;
    if (start < end) {
      duration = (end - start) *
        static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    } else { // 32-bit timer overflow
      duration = ((1ull << 32) + end - start) *
        static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    }

    PTI_ASSERT(instance.submit_time > 0);
    PTI_ASSERT(instance.device_submit_time > 0);
    PTI_ASSERT(start > instance.device_submit_time);
    uint64_t time_shift = (start - instance.device_submit_time) *
      static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
    uint64_t host_start = instance.submit_time + time_shift;
    uint64_t host_end = host_start + duration;

    AddKernelInfo(instance.name, host_end - host_start, &instance.props);

    if (instance.props.simd_width > 0) { // User kernels only
      uint64_t start_ns = start *
        static_cast<uint64_t>(NSEC_IN_SEC) / timer_frequency_;
      uint64_t end_ns = start_ns + duration;
      AddKernelInterval(instance.name, start_ns, end_ns);
    }

    if (callback_ != nullptr) {
      PTI_ASSERT(instance.append_time > 0);
      PTI_ASSERT(instance.append_time < instance.submit_time);

      PTI_ASSERT(instance.queue != nullptr);
      PTI_ASSERT(!instance.name.empty());
      callback_(
          callback_data_, instance.queue,
          std::to_string(instance.kernel_id), instance.name,
          instance.append_time, instance.submit_time,
          host_start, host_end);
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
      std::string name, uint64_t time, const ZeKernelProps* props) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(props != nullptr);

    if (verbose_) {
      if (props->simd_width > 0) {
        std::stringstream sstream;
        sstream << name << "[SIMD" << props->simd_width << ", {" <<
          props->group_count[0] << ", " <<
          props->group_count[1] << ", " <<
          props->group_count[2] << "}, {" <<
          props->group_size[0] << ", " <<
          props->group_size[1] << ", " <<
          props->group_size[2] << "}]";
        name = sstream.str();
      } else if (props->bytes_transferred > 0) {
        name = name + "[" +
          std::to_string(props->bytes_transferred) + " bytes]";
      }
    }

    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {time, time, time, 1};
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
      ze_command_queue_handle_t queue, const ZeSubmitData* submit_data) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    if (!command_list_info.immediate) {
      std::vector<ZeKernelInstance*>& kernel_list =
        command_list_info.kernel_list;
      for (size_t i = 0; i < kernel_list.size(); ++i) {
        kernel_list[i]->queue = queue;
        kernel_list[i]->submit_time = submit_data->host_sync;
        kernel_list[i]->device_submit_time = submit_data->device_sync;
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

  static void OnEnterEventDestroy(
      ze_event_destroy_params_t *params,
      ze_result_t result, void *global_data, void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessInstance(*(params->phEvent));
    }
  }

  static void OnEnterEventHostReset(
      ze_event_host_reset_params_t *params, ze_result_t result,
      void *global_data, void **instance_data) {
    if (*(params->phEvent) != nullptr) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessInstance(*(params->phEvent));
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
      collector->ProcessInstance(*(params->phEvent));
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
      std::string name,
      const ZeKernelProps& props,
      ze_event_handle_t& signal_event,
      ze_command_list_handle_t command_list,
      void* global_data,
      void** instance_data) {
    PTI_ASSERT(!name.empty());

    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    if (command_list == nullptr) {
      return;
    }

    ZeKernelInstance* instance = new ZeKernelInstance;
    instance->name = name;
    instance->props = props;
    instance->append_time = collector->GetTimestamp();

    if (collector->IsCommandListImmediate(command_list)) {
      instance->submit_time = instance->append_time;
      instance->device_submit_time = collector->GetDeviceTimestamp();
      instance->queue = command_list;
    } else {
      instance->device_submit_time = 0;
      instance->submit_time = 0;
      instance->queue = nullptr;
    }

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

  static ZeKernelProps GetKernelProps(
      ze_kernel_handle_t kernel,
      const ze_group_count_t* group_count,
      void* global_data) {
    PTI_ASSERT(kernel != nullptr);

    ZeKernelProps props{0};

    props.simd_width =
      utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
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

  static ZeKernelProps GetTransferProps(size_t bytes_transferred) {
    ZeKernelProps props{0};
    props.bytes_transferred = bytes_transferred;
    return props;
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        GetKernelProps(
            *(params->phKernel),
            *(params->ppLaunchFuncArgs),
            global_data),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        GetKernelProps(
            *(params->phKernel),
            *(params->ppLaunchFuncArgs),
            global_data),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        utils::ze::GetKernelName(*(params->phKernel)),
        GetKernelProps(
            *(params->phKernel),
            *(params->ppLaunchArgumentsBuffer),
            global_data),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopy",
        GetTransferProps(*(params->psize)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryFill",
        GetTransferProps(*(params->psize)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopyFromContext",
        GetTransferProps(*(params->psize)),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendBarrier(
      ze_command_list_append_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendBarrier",
        GetTransferProps(0),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    OnEnterKernelAppend(
        "zeCommandListAppendMemoryRangesBarrier",
        GetTransferProps(0),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    size_t bytes_transferred = 0;
    const ze_copy_region_t* region = *(params->psrcRegion);

    if (region != nullptr) {
      size_t bytes_transferred =
        region->width * region->height * (*(params->psrcPitch));
      if (region->depth != 0) {
        bytes_transferred *= region->depth;
      }
    }

    OnEnterKernelAppend(
        "zeCommandListAppendMemoryCopyRegion",
        GetTransferProps(bytes_transferred),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
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
        "zeCommandListAppendImageCopy",
        GetTransferProps(bytes_transferred),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
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
        "zeCommandListAppendImageCopyRegion",
        GetTransferProps(bytes_transferred),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);
    size_t bytes_transferred = collector->GetImageSize(*(params->phSrcImage));

    OnEnterKernelAppend(
        "zeCommandListAppendImageCopyToMemory",
        GetTransferProps(bytes_transferred),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    size_t bytes_transferred = 0;
    const ze_image_region_t* region = *(params->ppDstRegion);

    if (region != nullptr) {
      bytes_transferred = region->width * region->height;
      if (region->depth != 0) {
        bytes_transferred *= region->depth;
      }
    }

    OnEnterKernelAppend(
        "zeCommandListAppendImageCopyFromMemory",
        GetTransferProps(bytes_transferred),
        *(params->phSignalEvent),
        *(params->phCommandList),
        global_data,
        instance_data);
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

    ZeSubmitData* submit_data = new ZeSubmitData;
    submit_data->host_sync = collector->GetTimestamp();
    submit_data->device_sync = collector->GetDeviceTimestamp();

    *reinterpret_cast<ZeSubmitData**>(instance_data) = submit_data;
  }

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    ZeSubmitData* submit_data = *reinterpret_cast<ZeSubmitData**>(instance_data);
    PTI_ASSERT(submit_data != nullptr);

    if (result == ZE_RESULT_SUCCESS) {
      ZeKernelCollector* collector =
        reinterpret_cast<ZeKernelCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      uint32_t command_list_count = *params->pnumCommandLists;
      ze_command_list_handle_t* command_lists = *params->pphCommandLists;
      for (uint32_t i = 0; i < command_list_count; ++i) {
        if (!collector->IsCommandListImmediate(command_lists[i])) {
          collector->UpdateKernelInstances(
              command_lists[i], *(params->phCommandQueue), submit_data);
        }
      }
    }

    delete submit_data;
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

 private: // Data
  zel_tracer_handle_t tracer_ = nullptr;

  bool verbose_ = false;

  uint64_t timer_frequency_ = 0;
  Correlator* correlator_ = nullptr;
  std::atomic<uint64_t> kernel_id_;

  OnZeKernelFinishCallback callback_ = nullptr;
  void* callback_data_ = nullptr;

  std::mutex lock_;
  ZeKernelInfoMap kernel_info_map_;
  ZeKernelIntervalList kernel_interval_list_;
  std::list<ZeKernelInstance> kernel_instance_list_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;
  ZeKernelGroupSizeMap kernel_group_size_map_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_ZE_HOT_KERNELS_ZE_KERNEL_COLLECTOR_H_