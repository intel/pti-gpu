//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "ze_command_visitor.h"

#include <level_zero/driver_experimental/zex_visit.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lz_api_tracing_api_loader.h"
#include "unikernel.h"
#include "utils.h"
#include "ze_driver_init.h"
#include "ze_event_managers.h"
#include "ze_gpu_command.h"
#include "ze_local_collection_helpers.h"
#include "ze_utils.h"

#if defined(PTI_TRACE_SYCL)
#include "sycl_collector.h"
#endif

namespace {
#define ZE_COMMAND(visitor, fn, ...)                 \
  do {                                               \
    (visitor)->ZeCommand<fn##_id>(&fn, __VA_ARGS__); \
  } while (0)

// If the symbol is not found, record internal_error_ and skip re-appending this command; we
// cannot re-append it to the target command list without the symbol. This makes the visit no longer
// valid.
#define ZE_VISITOR_COMMAND(visitor, fn, ...)                 \
  do {                                                       \
    auto* symbol = pti::PtiLzTracerLoader::Instance().fn##_; \
    if (!symbol) {                                           \
      visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;    \
      return;                                                \
    }                                                        \
    (visitor)->ZeCommand<fn##_id>(symbol, __VA_ARGS__);      \
  } while (0)

#define APPEND_COMMAND(append_name)                                 \
  std::make_pair(                                                   \
      std::string_view{TOSTRING(zeCommandListAppend##append_name)}, \
      reinterpret_cast<void*>(&ZeCommandVisitor::VisitCommandListAppend##append_name)),  // NOLINT

/* clang-format off */
auto& GetVisitorTable() {
  static std::array visitor_table = {
#include "ze_visitor_commands.inc"
  };  // NOLINT
  return visitor_table;
}
/* clang-format on */
#undef APPEND_COMMAND

// Static table of visitors. My thinking is that this should be generated at compile time and treat
// it like a configuration. Eventually, we can support new configurations with different static
// tables of visitors. This cannot be compile-time generated yet due to reinterpret_casts (fn ptr ->
// void*).
auto* MakeVisitors() {
  using VisitorTableType = std::decay_t<decltype(GetVisitorTable())>;
  static auto visitors = []() {
    std::array<ze_concrete_visitor_ext_desc_t, std::tuple_size_v<VisitorTableType>> visitors_temp =
        {};
    std::transform(GetVisitorTable().begin(), GetVisitorTable().end(), visitors_temp.begin(),
                   [](const auto& str_func) {
                     return ze_concrete_visitor_ext_desc_t{
                         ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC, nullptr,
                         str_func.first.data(), str_func.second};
                   });
    return visitors_temp;
  }();

  // repair pNext pointers.
  [[maybe_unused]] static const bool linked = []() {
    for (size_t i = 0; i < visitors.size() - 1; ++i) {
      visitors[i].pNext = visitors.data() + (i + 1);  // NOLINT
    }
    return true;
  }();

  return visitors.data();
}

template <KernelCommandType CommandType,
          typename = std::enable_if_t<CommandType != KernelCommandType::kInvalid>>
inline void GetSyclCollectorData(ZeKernelCommand* command) {
  if constexpr (CommandType == KernelCommandType::kKernel) {
    if (sycl_data_kview.cid_) {
      command->corr_id_ = sycl_data_kview.cid_;
    } else {
      command->corr_id_ = UniCorrId::GetUniCorrId();
#if defined(PTI_TRACE_SYCL)
      if (SyclCollector::Instance().Enabled()) {
        sycl_data_kview.cid_ = command->corr_id_;
      }
#endif
    }
    sycl_data_kview.kid_ = command->kernel_id;
    sycl_data_kview.tid_ = command->tid;
    command->sycl_node_id_ = sycl_data_kview.sycl_node_id_;
    command->sycl_queue_id_ = sycl_data_kview.sycl_queue_id_;
    command->sycl_invocation_id_ = sycl_data_kview.sycl_invocation_id_;
    command->sycl_task_begin_time_ = sycl_data_kview.sycl_task_begin_time_;
    command->sycl_enqk_begin_time_ = sycl_data_kview.sycl_enqk_begin_time_;
    command->source_file_name_ = sycl_data_kview.source_file_name_;
    command->source_line_number_ = sycl_data_kview.source_line_number_;
  } else if constexpr (CommandType == KernelCommandType::kMemory) {
    if (sycl_data_mview.cid_) {
      command->corr_id_ = sycl_data_mview.cid_;
    } else {
      command->corr_id_ = UniCorrId::GetUniCorrId();
#if defined(PTI_TRACE_SYCL)
      if (SyclCollector::Instance().Enabled()) {
        sycl_data_mview.cid_ = command->corr_id_;
      }
#endif
    }

    sycl_data_mview.kid_ = command->kernel_id;
    sycl_data_mview.tid_ = command->tid;
    command->sycl_node_id_ = sycl_data_mview.sycl_node_id_;
    command->sycl_invocation_id_ = sycl_data_mview.sycl_invocation_id_;
    command->sycl_task_begin_time_ = sycl_data_mview.sycl_task_begin_time_;
    command->sycl_queue_id_ = sycl_data_mview.sycl_queue_id_;

    // Some memory operations come in as kernel ops from XPTI.
    // TODO: Work with XPTI team and investigate
    if (command->sycl_queue_id_ == PTI_INVALID_QUEUE_ID) {
      SPDLOG_TRACE("Missing SYCL queue id. Taking from kernel view.");
      command->sycl_queue_id_ = std::exchange(sycl_data_kview.sycl_queue_id_, PTI_INVALID_QUEUE_ID);
    }
    command->source_file_name_ = sycl_data_mview.source_file_name_;
    command->source_line_number_ = sycl_data_mview.source_line_number_;
  } else {
    if (!command
             ->corr_id_) {  // for synchronization activity commands the corrid is the api corrid.
      command->corr_id_ =
          UniCorrId::GetUniCorrId();  // setting here for non-synchronization activity corr_id.
    }
  }
}

template <KernelCommandType CommandType,
          typename = std::enable_if_t<CommandType != KernelCommandType::kInvalid>>
[[nodiscard]] auto MakeCommand(ze_command_list_handle_t user_command_list,
                               const ZeCommandListInfo& command_list_info,
                               const ZeDeviceDescriptor& device_desc) {
  auto command = std::make_shared<ZeKernelCommand>();
  command->props.type = CommandType;
  command->tid = PidTidInfo::Get().tid;
  command->kernel_id = UniKernelId::GetKernelId();
  command->result_ = ZE_RESULT_SUCCESS;
  command->append_time = ::utils::GetTime();
  GetSyclCollectorData<CommandType>(command.get());
  if constexpr (CommandType == KernelCommandType::kKernel) {
    command->callback_id_ = zeCommandListAppendLaunchKernel_id;
  } else if constexpr (CommandType == KernelCommandType::kMemory) {
    command->callback_id_ = zeCommandListAppendMemoryCopy_id;
  } else if constexpr (CommandType == KernelCommandType::kCommand) {
    command->callback_id_ = zeCommandListAppendBarrier_id;
  }
  command->command_list = user_command_list;
  command->device = command_list_info.device;
  command->context = command_list_info.context;
  command->device_timer_frequency_ = device_desc.device_timer_frequency;
  command->device_timer_mask_ = device_desc.device_timer_mask;
  return command;
}

}  // namespace

ZeCommandVisitor::ZeCommandVisitor(ZeExts::Visit visitor_extension,
                                   ZeEventPoolManager* event_pool_manager)
    : visitor_extension_(visitor_extension),
      event_pool_manager_(event_pool_manager),
      visit_desc_({ZEX_STRUCTURE_TYPE_COMMAND_VISIT_EXT_DESC, MakeVisitors(), this, nullptr,
                   nullptr, ZE_VISIT_EXT_DEFAULT_OP_REAPPEND, nullptr}),
      internal_error_(ZE_RESULT_SUCCESS) {
  if (!event_pool_manager_) {
    throw std::invalid_argument("Event pool manager cannot be null");
  }
  if (!visitor_extension_.ze_command_list_visit || !visitor_extension_.ze_graph_visit) {
    throw std::invalid_argument("Visitor extension function pointers cannot be null");
  }
}

ZeCommandVisitor::Result ZeCommandVisitor::CollectResultAndReset() {
  visit_desc_.hReappendTargetCmdList = nullptr;
  current_device_desc_ = {};
  current_command_list_info_ = {};
  Result result;
  result.first = std::exchange(commands_, {});
  result.second = std::exchange(internal_error_, ZE_RESULT_SUCCESS);
  if (internal_exception_) {
    auto internal_exception = std::exchange(internal_exception_, nullptr);
    std::rethrow_exception(internal_exception);
  }
  return result;
}

std::pair<ZeEventView<ZeEventPool>, utils::ze::TimestampBuffer>
ZeCommandVisitor::AppendQueryTimestamp(ze_command_list_handle_t command_list,
                                       ze_event_handle_t event_to_query) {
  auto buf = utils::ze::MakeTimestampBuffer(current_command_list_info_.context, 1);
  if (!buf) {
    SPDLOG_INFO("Failed to create timestamp buffer");
    internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
    return {};
  }

  auto timestamp_event = event_pool_manager_->AcquireEvent(current_command_list_info_.context);
  ZE_COMMAND(this, zeCommandListAppendQueryKernelTimestamps, command_list, 1, &event_to_query,
             buf.get(), nullptr, timestamp_event.Get(), 1, &event_to_query);
  return std::make_pair(std::move(timestamp_event), std::move(buf));
}

ZeCommandVisitor::Result ZeCommandVisitor::Visit(
    const ZeDeviceDescriptor& device_desc, const ZeCommandListInfo& info,
    ze_command_list_handle_t command_list, ze_command_list_handle_t instrumented_command_list) {
  current_device_desc_ = device_desc;
  current_command_list_info_ = info;
  visit_desc_.hReappendTargetCmdList = instrumented_command_list;
  auto result = visitor_extension_.ze_command_list_visit(command_list, &visit_desc_);
  SPDLOG_TRACE("Visit result: {:x}", static_cast<std::uint32_t>(result));
  if (HasError() || result != ZE_RESULT_SUCCESS) {
    SPDLOG_INFO("Failed to visit command list: {:x}",
                static_cast<std::uint32_t>(result == ZE_RESULT_SUCCESS ? internal_error_ : result));
    if (internal_error_ == ZE_RESULT_SUCCESS && result != ZE_RESULT_SUCCESS) {
      internal_error_ = result;
    }
    // clear commands to avoid returning partial/corrupted data. However, we still want the result
    // propagated to the user, so refrain from returning.
    commands_.clear();
  }
  return CollectResultAndReset();
}

ZeCommandVisitor::Result ZeCommandVisitor::GraphVisit(const ZeDeviceDescriptor& device_desc,
                                                      const ZeGraphInfo& graph_info,
                                                      ze_graph_handle_t graph) {
  current_device_desc_ = device_desc;
  // Convert graph_info to command list info. For now, its easier to use the graph information for
  // graphs. This is the relevant information between the two.
  current_command_list_info_.context = graph_info.context;
  current_command_list_info_.device = graph_info.device;
  auto result = visitor_extension_.ze_graph_visit(graph, &visit_desc_);
  SPDLOG_TRACE("GraphVisit result: {:x}", static_cast<std::uint32_t>(result));
  if (HasError() || result != ZE_RESULT_SUCCESS) {
    SPDLOG_INFO("Failed to visit graph: {:x}",
                static_cast<std::uint32_t>(result == ZE_RESULT_SUCCESS ? internal_error_ : result));
    if (internal_error_ == ZE_RESULT_SUCCESS && result != ZE_RESULT_SUCCESS) {
      internal_error_ = result;
    }
    // clear commands to avoid returning partial/corrupted data. However, we still want the result
    // propagated to the user, so refrain from returning.
    commands_.clear();
  }
  return CollectResultAndReset();
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t* pLaunchFuncArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendLaunchKernel, target_command_list, hKernel,
               pLaunchFuncArgs, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    auto [timestamp_event, buf] = visitor->AppendQueryTimestamp(target_command_list, event.Get());
    if (!buf) {
      return;
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kKernel>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->timestamp_query_event = std::move(timestamp_event);
    visitor->commands_.back()->timestamp = std::move(buf);

    visitor->commands_.back()->props.name = utils::ze::GetKernelName(hKernel, true);
    visitor->commands_.back()->props.simd_width = utils::ze::GetKernelMaxSubgroupSize(hKernel);
    if (pLaunchFuncArgs != nullptr) {
      visitor->commands_.back()->props.group_count[0] = pLaunchFuncArgs->groupCountX;
      visitor->commands_.back()->props.group_count[1] = pLaunchFuncArgs->groupCountY;
      visitor->commands_.back()->props.group_count[2] = pLaunchFuncArgs->groupCountZ;
    }
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts, const ze_group_size_t groupSizes, void** pArguments,
    const void* pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_VISITOR_COMMAND(visitor, zeCommandListAppendLaunchKernelWithArguments, target_command_list,
                       hKernel, groupCounts, groupSizes, pArguments, pNext, event.Get(),
                       numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    auto [timestamp_event, buf] = visitor->AppendQueryTimestamp(target_command_list, event.Get());
    if (!buf) {
      return;
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kKernel>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->timestamp_query_event = std::move(timestamp_event);
    visitor->commands_.back()->timestamp = std::move(buf);
    visitor->commands_.back()->props.name = utils::ze::GetKernelName(hKernel, true);
    visitor->commands_.back()->props.simd_width = utils::ze::GetKernelMaxSubgroupSize(hKernel);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t* pGroupCounts, const void* pNext, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_VISITOR_COMMAND(visitor, zeCommandListAppendLaunchKernelWithParameters, target_command_list,
                       hKernel, pGroupCounts, pNext, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    auto [timestamp_event, buf] = visitor->AppendQueryTimestamp(target_command_list, event.Get());
    if (!buf) {
      return;
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kKernel>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->timestamp_query_event = std::move(timestamp_event);
    visitor->commands_.back()->timestamp = std::move(buf);
    visitor->commands_.back()->props.name = utils::ze::GetKernelName(hKernel, true);
    visitor->commands_.back()->props.simd_width = utils::ze::GetKernelMaxSubgroupSize(hKernel);
    if (pGroupCounts != nullptr) {
      visitor->commands_.back()->props.group_count[0] = pGroupCounts->groupCountX;
      visitor->commands_.back()->props.group_count[1] = pGroupCounts->groupCountY;
      visitor->commands_.back()->props.group_count[2] = pGroupCounts->groupCountZ;
    }
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t* pLaunchFuncArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendLaunchCooperativeKernel, target_command_list, hKernel,
               pLaunchFuncArgs, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    auto [timestamp_event, buf] = visitor->AppendQueryTimestamp(target_command_list, event.Get());
    if (!buf) {
      return;
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kKernel>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->timestamp_query_event = std::move(timestamp_event);
    visitor->commands_.back()->timestamp = std::move(buf);
    visitor->commands_.back()->props.name = utils::ze::GetKernelName(hKernel, true);
    visitor->commands_.back()->props.simd_width = utils::ze::GetKernelMaxSubgroupSize(hKernel);
    if (pLaunchFuncArgs != nullptr) {
      visitor->commands_.back()->props.group_count[0] = pLaunchFuncArgs->groupCountX;
      visitor->commands_.back()->props.group_count[1] = pLaunchFuncArgs->groupCountY;
      visitor->commands_.back()->props.group_count[2] = pLaunchFuncArgs->groupCountZ;
    }
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
    const ze_group_count_t* pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendLaunchKernelIndirect, target_command_list, hKernel,
               pLaunchArgumentsBuffer, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    auto [timestamp_event, buf] = visitor->AppendQueryTimestamp(target_command_list, event.Get());
    if (!buf) {
      return;
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kKernel>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->timestamp_query_event = std::move(timestamp_event);
    visitor->commands_.back()->timestamp = std::move(buf);
    visitor->commands_.back()->props.name = utils::ze::GetKernelName(hKernel, true);
    visitor->commands_.back()->props.simd_width = utils::ze::GetKernelMaxSubgroupSize(hKernel);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList, void* dstptr, const void* srcptr, size_t size,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendMemoryCopy, target_command_list, dstptr, srcptr, size,
               event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props =
        GetTransferProps("zeCommandListAppendMemoryCopy", size, (srcptr ? context : nullptr),
                         srcptr, (dstptr ? context : nullptr), dstptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendMemoryFill(
    ze_command_list_handle_t hCommandList, void* ptr, const void* pattern, size_t pattern_size,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendMemoryFill, target_command_list, ptr, pattern,
               pattern_size, size, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props =
        GetTransferProps("zeCommandListAppendMemoryFill", size, (pattern ? context : nullptr),
                         pattern, (ptr ? context : nullptr), ptr, pattern_size);
    visitor->commands_.back()->callback_id_ = zeCommandListAppendMemoryFill_id;
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendMemoryCopyRegion(
    ze_command_list_handle_t hCommandList, void* dstptr, const ze_copy_region_t* dstRegion,
    uint32_t dstPitch, uint32_t dstSlicePitch, const void* srcptr,
    const ze_copy_region_t* srcRegion, uint32_t srcPitch, uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendMemoryCopyRegion, target_command_list, dstptr, dstRegion,
               dstPitch, dstSlicePitch, srcptr, srcRegion, srcPitch, srcSlicePitch, event.Get(),
               numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    size_t bytes_transferred = 0;
    if (srcRegion != nullptr) {
      bytes_transferred = srcRegion->width * srcRegion->height * srcPitch;
      if (srcRegion->depth != 0) {
        bytes_transferred *= srcRegion->depth;
      }
    }
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props = GetTransferProps(
        "zeCommandListAppendMemoryCopyRegion", bytes_transferred, (srcptr ? context : nullptr),
        srcptr, (dstptr ? context : nullptr), dstptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendMemoryCopyFromContext(
    ze_command_list_handle_t hCommandList, void* dstptr, ze_context_handle_t hContextSrc,
    const void* srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendMemoryCopyFromContext, target_command_list, dstptr,
               hContextSrc, srcptr, size, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props = GetTransferProps("zeCommandListAppendMemoryCopyFromContext",
                                                        size, hContextSrc, srcptr, context, dstptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopy(
    ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendImageCopy, target_command_list, hDstImage, hSrcImage,
               event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    // Image size lives in the collector's image_size_map_, which the visitor cannot reach,
    // so bytes_transferred stays 0 (see ze_command_visitor_todo.md). GetTransferProps still
    // derives the route/device info from any host pointers (none here for image-to-image).
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props =
        GetTransferProps("zeCommandListAppendImageCopy", 0, context, nullptr, context, nullptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopyRegion(
    ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage,
    const ze_image_region_t* pDstRegion, const ze_image_region_t* pSrcRegion,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendImageCopyRegion, target_command_list, hDstImage,
               hSrcImage, pDstRegion, pSrcRegion, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    // bytes_transferred requires the collector's image_size_map_ (unavailable here); leave 0.
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props = GetTransferProps("zeCommandListAppendImageCopyRegion", 0,
                                                        context, nullptr, context, nullptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopyToMemory(
    ze_command_list_handle_t hCommandList, void* dstptr, ze_image_handle_t hSrcImage,
    const ze_image_region_t* pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendImageCopyToMemory, target_command_list, dstptr,
               hSrcImage, pSrcRegion, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    // Source image size lives in the collector's image_size_map_ (unavailable here); leave
    // bytes_transferred 0. The route/dst device are still derived from the host dst pointer.
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props = GetTransferProps("zeCommandListAppendImageCopyToMemory", 0,
                                                        context, nullptr, context, dstptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopyFromMemory(
    ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void* srcptr,
    const ze_image_region_t* pDstRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendImageCopyFromMemory, target_command_list, hDstImage,
               srcptr, pDstRegion, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    size_t bytes_transferred = 0;
    if (pDstRegion != nullptr) {
      bytes_transferred = pDstRegion->width * pDstRegion->height;
      if (pDstRegion->depth != 0) {
        bytes_transferred *= pDstRegion->depth;
      }
    }
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props =
        GetTransferProps("zeCommandListAppendImageCopyFromMemory", bytes_transferred,
                         (srcptr ? context : nullptr), srcptr, context, nullptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopyToMemoryExt(
    ze_command_list_handle_t hCommandList, void* dstptr, ze_image_handle_t hSrcImage,
    const ze_image_region_t* pSrcRegion, uint32_t destRowPitch, uint32_t destSlicePitch,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_VISITOR_COMMAND(visitor, zeCommandListAppendImageCopyToMemoryExt, target_command_list,
                       dstptr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch, event.Get(),
                       numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    // Mirrors zeCommandListAppendImageCopyToMemory: source image size lives in the collector's
    // image_size_map_ (unavailable here), so bytes_transferred stays 0; route/dst device are
    // still derived from the host dst pointer.
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props = GetTransferProps("zeCommandListAppendImageCopyToMemoryExt",
                                                        0, context, nullptr, context, dstptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendImageCopyFromMemoryExt(
    ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void* srcptr,
    const ze_image_region_t* pDstRegion, uint32_t srcRowPitch, uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
    void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_VISITOR_COMMAND(visitor, zeCommandListAppendImageCopyFromMemoryExt, target_command_list,
                       hDstImage, srcptr, pDstRegion, srcRowPitch, srcSlicePitch, event.Get(),
                       numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kMemory>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    // Mirrors zeCommandListAppendImageCopyFromMemory: bytes from the destination region geometry.
    size_t bytes_transferred = 0;
    if (pDstRegion != nullptr) {
      bytes_transferred = pDstRegion->width * pDstRegion->height;
      if (pDstRegion->depth != 0) {
        bytes_transferred *= pDstRegion->depth;
      }
    }
    const auto context = visitor->current_command_list_info_.context;
    visitor->commands_.back()->props =
        GetTransferProps("zeCommandListAppendImageCopyFromMemoryExt", bytes_transferred,
                         (srcptr ? context : nullptr), srcptr, context, nullptr);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendEventReset, target_command_list, hEvent);
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendBarrier, target_command_list, event.Get(), numWaitEvents,
               phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kCommand>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->props.name = "zeCommandListAppendBarrier";
    visitor->commands_.back()->num_wait_events = numWaitEvents;
    visitor->commands_.back()->callback_id_ = zeCommandListAppendBarrier_id;
  });
}

ze_result_t VISITOR_CCONV ZeCommandVisitor::VisitCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t* pRangeSizes,
    const void** pRanges, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
    ze_event_handle_t* phWaitEvents, void* userData) {
  auto* visitor = static_cast<ZeCommandVisitor*>(userData);
  return ExceptionHandler(visitor, __func__, [&] {
    auto event =
        visitor->event_pool_manager_->AcquireEvent(visitor->current_command_list_info_.context);

    auto* target_command_list = visitor->visit_desc_.hReappendTargetCmdList
                                    ? visitor->visit_desc_.hReappendTargetCmdList
                                    : hCommandList;

    ZE_COMMAND(visitor, zeCommandListAppendMemoryRangesBarrier, target_command_list, numRanges,
               pRangeSizes, pRanges, event.Get(), numWaitEvents, phWaitEvents);
    if (hSignalEvent) {
      auto res = A2AppendWaitAndSignalEvent(target_command_list, hSignalEvent, event.Get());
      if (!res) {
        SPDLOG_INFO("Failed to append wait and signal for event: {}",
                    static_cast<const void*>(hSignalEvent));
        visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
      }
    }

    visitor->commands_.emplace_back(MakeCommand<KernelCommandType::kCommand>(
        hCommandList, visitor->current_command_list_info_, visitor->current_device_desc_));
    visitor->commands_.back()->event_self = hSignalEvent ? hSignalEvent : event.Get();
    visitor->commands_.back()->event_swap = std::move(event);
    visitor->commands_.back()->props.name = "zeCommandListAppendMemoryRangesBarrier";
    visitor->commands_.back()->num_wait_events = numWaitEvents;
    visitor->commands_.back()->callback_id_ = zeCommandListAppendMemoryRangesBarrier_id;
  });
}

#undef ZE_VISITOR_COMMAND
#undef ZE_COMMAND
