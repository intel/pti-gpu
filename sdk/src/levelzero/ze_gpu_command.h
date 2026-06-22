//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_LEVELZERO_ZE_GPU_COMMAND_H_
#define PTI_LEVELZERO_ZE_GPU_COMMAND_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/layers/zel_tracing_register_cb.h>
#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>
#include <pti/pti_driver_levelzero_api_ids.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pti/pti_view.h"
#include "pti_memory_route.h"
#include "unikernel.h"
#include "utils.h"
#include "ze_driver_init.h"
#include "ze_event_managers.h"
#include "ze_timer_helper.h"
#include "ze_utils.h"

#if defined(PTI_TRACE_SYCL)
#include "sycl_collector.h"
#endif

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

struct ZeKernelCommandProps {
  std::string name;
  KernelCommandType type = KernelCommandType::kInvalid;
  PtiMemoryCommandRoute route;
  size_t simd_width;
  size_t bytes_transferred;
  std::array<uint32_t, 3> group_count;
  std::array<uint32_t, 3> group_size;
  size_t value_size;
  std::byte* value_array;
  ze_device_handle_t src_device = nullptr;  // Device for p2p memcpy, source of copy data
  ze_device_handle_t dst_device = nullptr;  // Device for p2p memcpy, destination of copy data
  void* dst = nullptr;                      // Addresses for MemoryCopy or Fill
  void* src = nullptr;
};

struct ZeMemoryInfo {
  ze_device_handle_t device = nullptr;
  pti_view_memory_type type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
  bool is_p2p_capable = false;
};

struct ZeKernelCommand {
  ZeKernelCommandProps props;
  uint64_t device_timer_frequency_;
  uint64_t device_timer_mask_;
  ze_event_handle_t event_self = nullptr;  // in Local mode this event goes to the Bridge kernel
  ZeEventView<ZeEventPool> event_swap;     // event created in Local collection mode
  ze_device_handle_t device =
      nullptr;  // Device where the operation is submitted, associated with command list
  uint64_t kernel_id = 0;
  uint64_t append_time = 0;
  ze_context_handle_t context = nullptr;
  ze_command_list_handle_t command_list = nullptr;
  ze_command_queue_handle_t queue = nullptr;
  ze_fence_handle_t fence;
  uint64_t submit_time = 0;          // in ns
  uint64_t submit_time_device_ = 0;  // in ticks
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
  uint32_t callback_id_ = 0;
  uint64_t api_start_time_ = 0;  // in ns
  uint64_t api_end_time_ = 0;    // in ns
  uint64_t num_wait_events = 0;  // tracks wait event count for synchronization activity commands
  ze_result_t result_ = ZE_RESULT_SUCCESS;
  utils::ze::TimestampBuffer timestamp = nullptr;
  ZeEventView<ZeEventPool> timestamp_query_event;
};

struct ZeCommandQueue {
  ze_command_queue_handle_t queue_;
  ze_context_handle_t context_;
  ze_device_handle_t device_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;
};

struct ZeCommandListInfo {
  std::vector<std::shared_ptr<ZeKernelCommand>> appended_commands;
  ze_context_handle_t context = nullptr;
  ze_device_handle_t device = nullptr;
  bool immediate = false;
  bool closed = false;
  std::pair<uint32_t, uint32_t> oi_pair;
  ze_command_list_flags_t flags = 0;
  ze_command_list_handle_t command_list = nullptr;
  ze_command_list_handle_t instrumented_command_list = nullptr;
};

struct ZeDeviceDescriptor {
  uint64_t host_time_origin = 0;
  uint64_t device_time_origin = 0;
  uint64_t device_timer_frequency = 0;
  uint64_t device_timer_mask = 0;
  uint64_t device_sync_delta = CPUGPUTimeInterpolationHelper::kSyncDeltaDefault;
  ze_driver_handle_t driver = nullptr;
  ze_context_handle_t context = nullptr;
  ze_pci_ext_properties_t pci_properties{};
  ze_device_uuid_t uuid{};
  uint32_t ip_version = 0;
  std::optional<ZeExts::Visit> visit = std::nullopt;
  std::optional<ZeExts::CmdListIntrospection> cmdlist_introspection = std::nullopt;
};

[[nodiscard]] inline ZeMemoryInfo GetMemoryInfo(ze_context_handle_t ctx, const void* ptr) {
  ZeMemoryInfo mem_info{};
  if (ctx == nullptr || ptr == nullptr) {
    return mem_info;
  }

  ze_memory_allocation_properties_t props{};
  props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
  auto status = ZE_RESULT_SUCCESS;
  ze_device_handle_t device = nullptr;
  {
    overhead::ScopedOverheadCollector overhead_collector(zeMemGetAllocProperties_id);
    status = zeMemGetAllocProperties(ctx, ptr, &props, &device);
  }

  if (status != ZE_RESULT_SUCCESS) {
    SPDLOG_DEBUG("Failed to get memory properties for pointer: {} in context: {}, error: {:x}.",
                 ptr, static_cast<const void*>(ctx), static_cast<uint32_t>(status));
    return mem_info;
  }
  mem_info.device = device;

  // Conversion from L0 -> PTI view memory types
  switch (props.type) {
    case ZE_MEMORY_TYPE_UNKNOWN:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
      mem_info.is_p2p_capable = false;
      break;
    case ZE_MEMORY_TYPE_HOST:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
      mem_info.is_p2p_capable = false;
      break;
    case ZE_MEMORY_TYPE_DEVICE:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
      mem_info.is_p2p_capable = true;
      break;
    case ZE_MEMORY_TYPE_SHARED:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
      mem_info.is_p2p_capable = true;
      break;
    case ZE_MEMORY_TYPE_HOST_IMPORTED:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
      mem_info.is_p2p_capable = false;
      break;
    default:
      mem_info.type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
      mem_info.is_p2p_capable = false;
      break;
  }

  return mem_info;
}

[[nodiscard]] inline ZeKernelCommandProps GetTransferProps(
    std::string_view name, size_t bytes_transferred, ze_context_handle_t src_context,
    const void* src, ze_context_handle_t dst_context, const void* dst, size_t pattern_size = 0) {
  SPDLOG_TRACE("In {}", __FUNCTION__);
  PTI_ASSERT(!name.empty());

  ZeKernelCommandProps transfer_properties{};
  transfer_properties.name = std::string(name);
  transfer_properties.bytes_transferred = bytes_transferred;
  transfer_properties.value_size = pattern_size;
  transfer_properties.type = KernelCommandType::kMemory;

  PtiMemoryCommandRoute memory_route{};

  const auto src_info = GetMemoryInfo(src_context, src);
  const auto dst_info = GetMemoryInfo(dst_context, dst);

  memory_route.is_peer_2_peer = (src_info.is_p2p_capable && dst_info.is_p2p_capable) &&
                                (src_info.device != nullptr) && (dst_info.device != nullptr) &&
                                (src_info.device != dst_info.device);

  memory_route.src_type = src_info.type;
  memory_route.dst_type = dst_info.type;

  transfer_properties.name += "(" + memory_route.GetCompactStringForTypes();

  ze_bool_t can_access_peer = 0;
  ze_result_t status = ZE_RESULT_SUCCESS;
  if (memory_route.is_peer_2_peer) {
    overhead::ScopedOverheadCollector overhead_collector(zeDeviceCanAccessPeer_id);
    status = zeDeviceCanAccessPeer(src_info.device, dst_info.device, &can_access_peer);
  }

  if (status == ZE_RESULT_SUCCESS && can_access_peer) {
    transfer_properties.name += memory_route.GetCompactStringForP2P();
  }

  transfer_properties.name += ")";
  SPDLOG_TRACE("\t\tIn {}, ops name: {}, p2p: {}", __FUNCTION__, transfer_properties.name,
               can_access_peer ? "true" : "false");

  transfer_properties.route = memory_route;
  transfer_properties.src_device = src_info.device;
  transfer_properties.dst_device = dst_info.device;
  transfer_properties.dst = const_cast<void*>(dst);
  transfer_properties.src = const_cast<void*>(src);
  return transfer_properties;
}

#endif  // PTI_LEVELZERO_ZE_GPU_COMMAND_H_
