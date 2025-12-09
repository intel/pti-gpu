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

// Workaround for supporting counter-based L0 events.
// https://github.com/intel/compute-runtime/blob/master/level_zero/doc/experimental_extensions/COUNTER_BASED_EVENTS.md
// These are now the default on BMG and newer platforms in 2025.3.
// preferred solution would be
// pseudo code:
//
// ```
//   if(zeCommandListInorder(command_list) || zeEventIsCounterEvent(signal_event))
//     Inject(CounterEvent() || SignalEvent()).
// ```
//
// However, these APIs do not exist yet.
// This code only works for in_order command lists.
bool A2AppendWaitAndSignalEvent(ze_command_list_handle_t command_list,
                                ze_event_handle_t signal_event, ze_event_handle_t wait_event) {
  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               static_cast<const void*>(command_list), static_cast<const void*>(signal_event),
               static_cast<const void*>(wait_event));
  overhead::Init();
  if (zeCommandListAppendWaitOnEvents(command_list, 1U, &wait_event) != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("In {}, zeCommandListAppendWaitOnEvents failed", __FUNCTION__);
    return false;
  }
  overhead_fini(zeCommandListAppendWaitOnEvents_id);

  overhead::Init();
  if (zeCommandListAppendSignalEvent(command_list, signal_event) != ZE_RESULT_SUCCESS) {
    SPDLOG_ERROR("In {}, zeCommandListAppendSignalEvent failed", __FUNCTION__);
    return false;
  }
  overhead_fini(zeCommandListAppendSignalEvent_id);

  return true;
}

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

  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               static_cast<const void*>(command_list), static_cast<const void*>(signal_event),
               static_cast<const void*>(wait_event));

  ze_group_count_t dim = {1, 1, 1};
  auto result = ZE_RESULT_SUCCESS;
  uint32_t count = 1;

  overhead::Init();
  result =
      zeCommandListAppendLaunchKernel(command_list, kernel, &dim, signal_event, count, &wait_event);
  overhead_fini(zeCommandListAppendLaunchKernel_id);

  return result == ZE_RESULT_SUCCESS;
}

/**
 * \internal
 * ze..MemoryFill still has smaller latency than ze..MemoryCopy. So we use it to lower the overhead
 */
bool A2AppendBridgeMemoryCopyOrFillEx(ze_command_list_handle_t command_list,
                                      ze_event_handle_t signal_event, ze_event_handle_t wait_event,
                                      void* dst, size_t size) {
  PTI_ASSERT(command_list != nullptr);
  PTI_ASSERT(wait_event != nullptr);

  SPDLOG_DEBUG(
      " --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}, dst: {}, \
                size: {}",
      __FUNCTION__, static_cast<void*>(command_list), static_cast<void*>(signal_event),
      static_cast<void*>(wait_event), dst, size);

  uint32_t count = 1;
  ze_result_t result = ZE_RESULT_SUCCESS;

  SPDLOG_TRACE("\tAppending Bridge MemoryFill dst: {}, size: {}", dst, size);
  uint32_t pattern = 0;
  overhead::Init();
  result = zeCommandListAppendMemoryFill(command_list, dst, &pattern, sizeof(pattern), size,
                                         signal_event, count, &wait_event);
  overhead_fini(zeCommandListAppendMemoryFill_id);
  SPDLOG_DEBUG("\t\tBridge MemOp Append MemoryFill result: {}", (uint32_t)result);
  return result == ZE_RESULT_SUCCESS;
}

bool A2AppendBridgeBarrier(ze_command_list_handle_t command_list, ze_event_handle_t signal_event,
                           ze_event_handle_t wait_event) {
  PTI_ASSERT(command_list != nullptr);
  PTI_ASSERT(wait_event != nullptr);

  SPDLOG_DEBUG(" --- In: {}, CmdList: {}, Signal event: {}, Wait event: {}", __FUNCTION__,
               static_cast<void*>(command_list), static_cast<void*>(signal_event),
               static_cast<void*>(wait_event));

  uint32_t count = 1;

  overhead::Init();
  ze_result_t result = zeCommandListAppendBarrier(command_list, signal_event, count, &wait_event);
  overhead_fini(zeCommandListAppendBarrier_id);
  return result == ZE_RESULT_SUCCESS;
}

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
                                      static_cast<uint32_t>(kKernelBinary.size() * 2),
                                      (uint8_t*)(kKernelBinary.data()),
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
  // Commands to get spv of the kernel at empty.cl
  // clang -cc1 -triple spir empty.cl -O2 -finclude-default-header -emit-llvm-bc -o empty.bc
  // llvm-spirv empty.bc -o empty.spv
  inline static constexpr std::array<uint16_t, 86> kKernelBinary{
      0x0203, 0x0723, 0x0000, 0x0001, 0x000e, 0x0006, 0x0006, 0x0000, 0x0000, 0x0000, 0x0011,
      0x0002, 0x0004, 0x0000, 0x0011, 0x0002, 0x0006, 0x0000, 0x000b, 0x0005, 0x0001, 0x0000,
      0x704f, 0x6e65, 0x4c43, 0x732e, 0x6474, 0x0000, 0x000e, 0x0003, 0x0001, 0x0000, 0x0002,
      0x0000, 0x000f, 0x0005, 0x0006, 0x0000, 0x0004, 0x0000, 0x6d65, 0x7470, 0x0079, 0x0000,
      0x0003, 0x0003, 0x0003, 0x0000, 0x8e70, 0x0001, 0x0005, 0x0004, 0x0005, 0x0000, 0x6e65,
      0x7274, 0x0079, 0x0000, 0x0013, 0x0002, 0x0002, 0x0000, 0x0021, 0x0003, 0x0003, 0x0000,
      0x0002, 0x0000, 0x0036, 0x0005, 0x0002, 0x0000, 0x0004, 0x0000, 0x0000, 0x0000, 0x0003,
      0x0000, 0x00f8, 0x0002, 0x0005, 0x0000, 0x00fd, 0x0001, 0x0038, 0x0001};

  std::mutex kernel_map_mutex_;
  std::map<std::pair<ze_context_handle_t, ze_device_handle_t>, ze_kernel_handle_t> kernel_map_;
};

class A2DeviceBufferPool {
 public:
  A2DeviceBufferPool() {}

  void* GetBuffers(ze_context_handle_t context, ze_device_handle_t device) {
    PTI_ASSERT(context != nullptr);
    PTI_ASSERT(device != nullptr);
    std::unique_lock lock(mutex_);
    if (buffer_map_.find(std::pair(context, device)) == buffer_map_.end()) {
      void* buff = nullptr;
      ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0,
                                               0};
      overhead::Init();
      ze_result_t status = zeMemAllocDevice(context, &alloc_desc, buffer_size_, 64, device, &buff);
      overhead_fini(zeMemAllocDevice_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      buffer_map_[std::pair(context, device)] = buff;
      SPDLOG_TRACE("Device buffers created in {} for context: {}, device: {}, buff {}, size: {} ",
                   __FUNCTION__, static_cast<void*>(context), static_cast<void*>(device),
                   static_cast<void*>(buff), buffer_size_);
    }
    return buffer_map_[std::pair(context, device)];
  }
  const size_t buffer_size_ = 64;

 private:
  std::mutex mutex_;
  std::map<std::pair<ze_context_handle_t, ze_device_handle_t>, void*> buffer_map_;
};

#endif  // PTI_TOOLS_ZE_LOCAL_COLLECTION_HELPERS_H_
