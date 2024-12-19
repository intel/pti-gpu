//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_
#define PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include "overhead_kinds.h"
#include "pti_assert.h"

/*
 * Misc classes and functions enabling so called "local" collection of GPU device kernels
 * Local collection is the collection that collects (traces) _only_
 * between ptiViewEnable and ptiViewDisable
 * It referred as collect Anytime Anywhere - so we use "A2" acronym for classes names
 * As of April 11'24 - it is the first implementation - so issues are possible
 */

bool A2AppendBridgeKernel(ze_kernel_handle_t kernel, ze_command_list_handle_t command_list,
                          ze_event_handle_t signal_event, ze_event_handle_t wait_event) {
  PTI_ASSERT(command_list != nullptr);
  PTI_ASSERT(wait_event != nullptr);
  PTI_ASSERT(kernel != nullptr);

  ze_group_count_t dim = {1, 1, 1};
  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               (void*)command_list, (void*)signal_event, (void*)wait_event);

  uint32_t count = 1;
  overhead::Init();
  ze_result_t result =
      zeCommandListAppendLaunchKernel(command_list, kernel, &dim, signal_event, count, &wait_event);
  overhead_fini(zeCommandListAppendLaunchKernel_id);
  return result == ZE_RESULT_SUCCESS;
}

bool A2AppendBridgeMemoryCopyOrFill(ze_command_list_handle_t command_list,
                                    ze_event_handle_t signal_event, ze_event_handle_t wait_event,
                                    void* dst, void* src, size_t size1, size_t size2,
                                    bool is_two_devices = false) {
  // TODO make a small host to device mem transfer - kind of "brige" transfer
  // this would require allocatiing memory on host and device in the specific context that
  // the command list belongs to
  PTI_ASSERT(command_list != nullptr);
  PTI_ASSERT(wait_event != nullptr);

  SPDLOG_DEBUG(
      " --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}, dst: {}, src: {}, \
                size1: {}, size2: {}, is_two_devices: {}",
      __FUNCTION__, (void*)command_list, (void*)signal_event, (void*)wait_event, dst, src, size1,
      size2, is_two_devices);

  uint32_t count = 1;
  ze_result_t result = ZE_RESULT_SUCCESS;
  if (0 == size2) {  // MemoryCopy
    const size_t size_64 = (size1 > 64) ? 64 : size1;
    size_t size_here = ((dst == src) || (is_two_devices)) ? size_64 : 0;
    SPDLOG_TRACE("\tAppending Bridge MemoryCopy dst: {}, src: {}, size_here: {}", dst, src,
                 size_here);
    overhead::Init();
    result = zeCommandListAppendMemoryCopy(command_list, dst, src, size_here, signal_event, count,
                                           &wait_event);
    overhead_fini(zeCommandListAppendMemoryCopy_id);
  } else {  // MemoryFill
    PTI_ASSERT(size1 >= size2);
    SPDLOG_TRACE("\tAppending Bridge MemoryFill dst: {}, src: {}, size1: {}, size2: {}", dst, src,
                 size1, size2);
    overhead::Init();
    result = zeCommandListAppendMemoryFill(command_list, dst, src, size2, 0, signal_event, count,
                                           &wait_event);
    overhead_fini(zeCommandListAppendMemoryFill_id);
  }
  SPDLOG_DEBUG("\t\tBridge MemOp Append result: {}", (uint32_t)result);
  return result == ZE_RESULT_SUCCESS;
}

bool A2AppendBridgeBarrier(ze_command_list_handle_t command_list, ze_event_handle_t signal_event,
                           ze_event_handle_t wait_event) {
  PTI_ASSERT(command_list != nullptr);
  PTI_ASSERT(wait_event != nullptr);

  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               (void*)command_list, (void*)signal_event, (void*)wait_event);

  uint32_t count = 1;
  overhead::Init();
  ze_result_t result = zeCommandListAppendBarrier(command_list, signal_event, count, &wait_event);
  overhead_fini(zeCommandListAppendBarrier_id);
  return result == ZE_RESULT_SUCCESS;
}

class A2EventPool {
  // EventPool per context
  // TODO - current implementation is not optimal and covering very basic case,
  // - ZeEventCache and this class should be merged - better to have one class
  //   then all below items need to be revisited
  //   Not doing it as it would be rather big change and might compromise stability
  //
  // - create events in groups more than by 2
  // - simplify treating of ready and busy events
  // - make a stress test for this case for creating many pools for the context
 public:
  A2EventPool(const A2EventPool&) = delete;
  A2EventPool& operator=(const A2EventPool&) = delete;
  A2EventPool(A2EventPool&&) = delete;
  A2EventPool& operator=(A2EventPool&&) = delete;
  A2EventPool() = delete;
  A2EventPool(uint32_t events_per_pool) : events_per_pool_count_(events_per_pool) {}
  ~A2EventPool() {}

  ze_event_handle_t GetEvent(const ze_context_handle_t context) {
    std::unique_lock lock(mutex_);
    ze_event_handle_t our_event = nullptr;

    ze_event_pool_handle_t event_pool = IsEventPoolExistAndSufficient(context);
    if (nullptr != event_pool) {
      //  get event from that Pool:

      if (ready_event_map_[context].size() >= 1) {
        //    if (there any ready events) -  take ready event and mark it as busy
        SPDLOG_TRACE("In: {}, ready events size: {}", __FUNCTION__, ready_event_map_.size());
        our_event = *(ready_event_map_[context].begin());
        ready_event_map_[context].erase(our_event);
        busy_event_map_[context].insert(our_event);
      } else {
        // no ready events
        // increment number of events taken from pool
        //  create new and mark it as busy
        SPDLOG_TRACE("In: {}, No ready events, creating 2 new, returning 1", __FUNCTION__);
        uint32_t index = ++used_pool_index_map_[event_pool];
        PTI_ASSERT(index < events_per_pool_count_ - 1);
        our_event = CreateEvent(event_pool, index);
        PTI_ASSERT(nullptr != our_event);
        event_context_map_[our_event] = context;
        busy_event_map_[context].insert(our_event);

        index = ++used_pool_index_map_[event_pool];
        PTI_ASSERT(index < events_per_pool_count_ - 1);
        ze_event_handle_t event = CreateEvent(event_pool, index);  // creating in advance
        PTI_ASSERT(nullptr != event);
        event_context_map_[event] = context;
        ready_event_map_[context].insert(event);
      }
    } else {
      if (event_pool_map_.find(context) == event_pool_map_.end()) {
        event_pool_map_[context] = std::vector<ze_event_pool_handle_t>{};
      }
      SPDLOG_TRACE("In: {}, Creating Events Pool", __FUNCTION__);
      ze_event_pool_handle_t new_pool = CreateEventPool(context);
      event_pool_map_[context].push_back(new_pool);

      used_pool_index_map_[new_pool] = 2;
      our_event = CreateEvent(new_pool, 0);
      ze_event_handle_t event = CreateEvent(new_pool, 1);  // creating in advance
      event_context_map_[our_event] = context;
      event_context_map_[event] = context;
      busy_event_map_[context] = std::unordered_set<ze_event_handle_t>{our_event};
      ready_event_map_[context] = std::unordered_set<ze_event_handle_t>{event};
    }
    return our_event;
  }

  bool ReturnSwapEvent(ze_event_handle_t our_event) {
    SPDLOG_TRACE("In: {} with swap event: {}", __FUNCTION__, (void*)our_event);
    // move this event to be ready to use
    if (our_event == nullptr) {
      return false;
    }
    bool res = false;

    std::unique_lock lock(mutex_);
    if (event_context_map_.find(our_event) != event_context_map_.end()) {
      ze_context_handle_t context = event_context_map_[our_event];
      if (busy_event_map_.find(context) != busy_event_map_.end() &&
          ready_event_map_.find(context) != ready_event_map_.end()) {
        auto busy_map = busy_event_map_[context];
        auto ready_map = ready_event_map_[context];
        if (busy_map.find(our_event) != busy_map.end()) {
          busy_map.erase(our_event);
          ready_map.insert(our_event);
          ze_result_t status = zeEventHostReset(our_event);
          if (status != ZE_RESULT_SUCCESS) {
            SPDLOG_DEBUG("\tIn {} zeEventHostReset for event: {} returned {}", __FUNCTION__,
                         (void*)our_event, (uint32_t)status);
          }
          res = true;
        }
      }
    }
    return res;
  }

  bool StoreEventsToShadowCache(ze_event_handle_t key_for_event,
                                ze_event_handle_t value_our_event) {
    // map of "foreign" events -> "our" events
    // so naming: ..for_event and ..our_event
    std::unique_lock lock(shadow_map_mutex_);
    bool res = true;
    if (shadow_map_.find(key_for_event) != shadow_map_.end()) {
      res = false;
    }
    shadow_map_[key_for_event] = value_our_event;
    return res;
  }

  ze_event_handle_t GetSwapEventFromShadowCache(ze_event_handle_t key_event) {
    std::shared_lock lock(shadow_map_mutex_);
    SPDLOG_TRACE(" --- In: {}, event: {}", __FUNCTION__, (void*)key_event);
    return (shadow_map_.find(key_event) == shadow_map_.end()) ? nullptr : shadow_map_[key_event];
  }

  ze_event_handle_t RemoveKeyEventFromShadowCache(ze_event_handle_t key_event) {
    ze_event_handle_t our_event = nullptr;
    if (key_event == nullptr) {
      return nullptr;
    }
    std::unique_lock lock(shadow_map_mutex_);
    if (shadow_map_.find(key_event) == shadow_map_.end()) {
    } else {
      our_event = shadow_map_[key_event];
      shadow_map_.erase(key_event);
    }
    return our_event;
  }

  void Clean(ze_context_handle_t context = nullptr) {
    std::unique_lock lock(shadow_map_mutex_);

    if (context) {
      CleanBusyEvents(context);
      CleanReadyEvents(context);
      CleanPools(context);
    } else {
      for (const auto& context_handle : event_pool_map_) {
        CleanBusyEvents(context_handle.first);
        CleanReadyEvents(context_handle.first);
        CleanPools(context_handle.first);
      }
    }
  }

 private:
  const uint32_t events_per_pool_count_;
  ze_event_pool_handle_t IsEventPoolExistAndSufficient(ze_context_handle_t context) {
    ze_event_pool_handle_t event_pool = nullptr;
    if (event_pool_map_.find(context) != event_pool_map_.end()) {
      for (auto pool : event_pool_map_[context]) {
        if (used_pool_index_map_[pool] < events_per_pool_count_ - 2) {
          // still at least two event could be created in this pool
          event_pool = pool;
          break;
        }
      }
    }
    return event_pool;
  }

  ze_event_pool_handle_t CreateEventPool(ze_context_handle_t context) {
    SPDLOG_TRACE("In: {}", __FUNCTION__);
    PTI_ASSERT(context != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_event_pool_handle_t event_pool = nullptr;
    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                            ZE_EVENT_POOL_FLAG_IPC |
                                                ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP |
                                                ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                            events_per_pool_count_};
    // number of devices: 0 and nullptr as devices -> events visible on all devices
    overhead::Init();
    status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
    overhead_fini(zeEventPoolCreate_id);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    return event_pool;
  }

  ze_event_handle_t CreateEvent(ze_event_pool_handle_t event_pool, uint32_t index) {
    SPDLOG_TRACE("In: {}", __FUNCTION__);
    ze_result_t status = ZE_RESULT_SUCCESS;
    ze_event_handle_t event = nullptr;

    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, index,
                                  //        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
                                  ZE_EVENT_SCOPE_FLAG_HOST, 0};
    overhead::Init();
    zeEventCreate(event_pool, &event_desc, &event);
    overhead_fini(zeEventCreate_id);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(event != nullptr);
    return event;
  }

  ze_event_handle_t FindKeyEvent(std::unordered_map<ze_event_handle_t, ze_event_handle_t>& the_map,
                                 ze_event_handle_t eventValue) {
    for (auto const key : the_map) {
      if (key.second == eventValue) {
        return key.first;
      }
    }
    return nullptr;
  }

  void CleanBusyEvents(ze_context_handle_t context) {
    for (auto event : busy_event_map_[context]) {
      ze_result_t status = zeEventDestroy(event);
      if (status != ZE_RESULT_SUCCESS) {
        SPDLOG_DEBUG("\tIn {} zeEventDestroy for event: {} returned {}", __FUNCTION__,
                     static_cast<const void*>(event), (uint32_t)status);
      }
      ze_event_handle_t ev = FindKeyEvent(shadow_map_, event);
      if (ev) {
        shadow_map_.erase(ev);
      }
      event_context_map_.erase(event);
    }
    busy_event_map_.erase(context);
  }

  void CleanReadyEvents(ze_context_handle_t context) {
    for (auto event : ready_event_map_[context]) {
      overhead::Init();
      ze_result_t status = zeEventHostReset(event);
      overhead_fini(zeEventHostReset_id);
      if (status != ZE_RESULT_SUCCESS) {
        SPDLOG_DEBUG("\tIn {} zeEventHostReset for event: {} returned {}", __FUNCTION__,
                     (void*)event, (uint32_t)status);
      }
      event_context_map_.erase(event);
    }
    ready_event_map_.erase(context);
  }

  void CleanPools(ze_context_handle_t context) {
    for (auto pool : event_pool_map_[context]) {
      overhead::Init();
      ze_result_t status = zeEventPoolDestroy(pool);
      overhead_fini(zeEventPoolDestroy_id);
      if (status != ZE_RESULT_SUCCESS) {
        SPDLOG_DEBUG("\tIn {} zeEventPoolDestroy for pool: {} returned {}", __FUNCTION__,
                     static_cast<const void*>(pool), (uint32_t)status);
      }
      used_pool_index_map_.erase(pool);
    }
    event_pool_map_.erase(context);
  }

  std::shared_mutex mutex_;
  std::shared_mutex shadow_map_mutex_;
  std::unordered_map<ze_context_handle_t, std::vector<ze_event_pool_handle_t> > event_pool_map_;
  std::unordered_map<ze_event_handle_t, ze_context_handle_t> event_context_map_;
  std::unordered_map<ze_context_handle_t, std::unordered_set<ze_event_handle_t> > busy_event_map_;
  std::unordered_map<ze_context_handle_t, std::unordered_set<ze_event_handle_t> > ready_event_map_;
  std::unordered_map<ze_event_pool_handle_t, uint32_t> used_pool_index_map_;

  std::unordered_map<ze_event_handle_t, ze_event_handle_t> shadow_map_;
};

class A2BridgeKernelPool {
 public:
  A2BridgeKernelPool() {}
  ~A2BridgeKernelPool() {}

  ze_kernel_handle_t GetMarkKernel(ze_context_handle_t context, ze_device_handle_t device) {
    PTI_ASSERT(context != nullptr);
    PTI_ASSERT(device != nullptr);

    std::unique_lock lock(kernel_map_mutex_);

    // if kernel not exists - create it and store in the map
    if (kernel_map_.find(std::pair(context, device)) == kernel_map_.end()) {
      ze_kernel_handle_t kernel = nullptr;

      ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                      nullptr,
                                      ZE_MODULE_FORMAT_IL_SPIRV,
                                      static_cast<uint32_t>(kernel_binary_.size() * 2),
                                      (uint8_t*)(kernel_binary_.data()),
                                      nullptr,
                                      nullptr};
      ze_module_handle_t module = nullptr;
      overhead::Init();
      ze_result_t status = zeModuleCreate(context, device, &module_desc, &module, nullptr);
      overhead_fini(zeModuleCreate_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS && module != nullptr);

      ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "empty"};
      overhead::Init();
      status = zeKernelCreate(module, &kernel_desc, &kernel);
      overhead_fini(zeKernelCreate_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS && kernel != nullptr);
      kernel_map_.insert({std::pair(context, device), kernel});
      SPDLOG_TRACE("Probe Kernel Created in {} for context: {}, device: {}", __FUNCTION__,
                   (void*)context, (void*)device);
    }
    return kernel_map_[std::pair(context, device)];
  }

 private:
  static std::vector<uint16_t> kernel_binary_;
  std::mutex kernel_map_mutex_;
  std::map<std::pair<ze_context_handle_t, ze_device_handle_t>, ze_kernel_handle_t> kernel_map_;
};

// Commands to get spv of the kernel at empty.cl
// clang -cc1 -triple spir empty.cl -O2 -finclude-default-header -emit-llvm-bc -o empty.bc
// llvm-spirv empty.bc -o empty.spv

std::vector<uint16_t> A2BridgeKernelPool::kernel_binary_{
    0x0203, 0x0723, 0x0000, 0x0001, 0x000e, 0x0006, 0x0006, 0x0000, 0x0000, 0x0000, 0x0011,
    0x0002, 0x0004, 0x0000, 0x0011, 0x0002, 0x0006, 0x0000, 0x000b, 0x0005, 0x0001, 0x0000,
    0x704f, 0x6e65, 0x4c43, 0x732e, 0x6474, 0x0000, 0x000e, 0x0003, 0x0001, 0x0000, 0x0002,
    0x0000, 0x000f, 0x0005, 0x0006, 0x0000, 0x0004, 0x0000, 0x6d65, 0x7470, 0x0079, 0x0000,
    0x0003, 0x0003, 0x0003, 0x0000, 0x8e70, 0x0001, 0x0005, 0x0004, 0x0005, 0x0000, 0x6e65,
    0x7274, 0x0079, 0x0000, 0x0013, 0x0002, 0x0002, 0x0000, 0x0021, 0x0003, 0x0003, 0x0000,
    0x0002, 0x0000, 0x0036, 0x0005, 0x0002, 0x0000, 0x0004, 0x0000, 0x0000, 0x0000, 0x0003,
    0x0000, 0x00f8, 0x0002, 0x0005, 0x0000, 0x00fd, 0x0001, 0x0038, 0x0001};
#endif  // PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_
