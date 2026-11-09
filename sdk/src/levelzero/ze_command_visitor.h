//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_LEVELZERO_ZE_COMMAND_VISITOR_H_
#define PTI_LEVELZERO_ZE_COMMAND_VISITOR_H_

#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "overhead_kinds.h"
#include "pti_assert.h"
#include "ze_driver_init.h"
#include "ze_event_managers.h"
#include "ze_gpu_command.h"

struct ZeCommandListImmutableInfo {
  bool immediate = false;
  ze_command_list_flags_t flags = ZE_COMMAND_LIST_FLAG_FORCE_UINT32;
  ze_command_queue_flags_t immediate_flags = ZE_COMMAND_QUEUE_FLAG_FORCE_UINT32;
};

class ZeCommandVisitor {
 public:
  using Self = ZeCommandVisitor;
  using Command = std::shared_ptr<ZeKernelCommand>;
  using Commands = std::vector<Command>;
  using Result = std::pair<Commands, ze_result_t>;
  // Timestamp event & buffer
  using TimestampInfo = std::pair<ZeEventView<ZeEventPool>, utils::ze::TimestampBuffer>;

  ZeCommandVisitor(ZeExts::Visit visitor_extension, ZeEventPoolManager* event_pool_manager);

  [[nodiscard]] Result Visit(const ZeDeviceDescriptor& device_desc,
                             const ZeCommandListInfo& command_list_info,
                             ze_command_list_handle_t command_list,
                             ze_command_list_handle_t instrumented_command_list);

  [[nodiscard]] Result GraphVisit(const ZeDeviceDescriptor& device_desc,
                                  const ZeGraphInfo& graph_info, ze_graph_handle_t graph);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendLaunchKernel(
      ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
      const ze_group_count_t* pLaunchFuncArgs, ze_event_handle_t hSignalEvent,
      uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendLaunchKernelWithArguments(
      ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
      const ze_group_count_t groupCounts, const ze_group_size_t groupSizes, void** pArguments,
      const void* pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendLaunchKernelWithParameters(
      ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
      const ze_group_count_t* pGroupCounts, const void* pNext, ze_event_handle_t hSignalEvent,
      uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendLaunchCooperativeKernel(
      ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
      const ze_group_count_t* pLaunchFuncArgs, ze_event_handle_t hSignalEvent,
      uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendLaunchKernelIndirect(
      ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
      const ze_group_count_t* pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent,
      uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendMemoryCopy(
      ze_command_list_handle_t hCommandList, void* dstptr, const void* srcptr, size_t size,
      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
      void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendMemoryFill(
      ze_command_list_handle_t hCommandList, void* ptr, const void* pattern, size_t pattern_size,
      size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendMemoryCopyRegion(
      ze_command_list_handle_t hCommandList, void* dstptr, const ze_copy_region_t* dstRegion,
      uint32_t dstPitch, uint32_t dstSlicePitch, const void* srcptr,
      const ze_copy_region_t* srcRegion, uint32_t srcPitch, uint32_t srcSlicePitch,
      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
      void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendMemoryCopyFromContext(
      ze_command_list_handle_t hCommandList, void* dstptr, ze_context_handle_t hContextSrc,
      const void* srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopy(
      ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage,
      ze_image_handle_t hSrcImage, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopyRegion(
      ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage,
      ze_image_handle_t hSrcImage, const ze_image_region_t* pDstRegion,
      const ze_image_region_t* pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopyToMemory(
      ze_command_list_handle_t hCommandList, void* dstptr, ze_image_handle_t hSrcImage,
      const ze_image_region_t* pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopyFromMemory(
      ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void* srcptr,
      const ze_image_region_t* pDstRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopyToMemoryExt(
      ze_command_list_handle_t hCommandList, void* dstptr, ze_image_handle_t hSrcImage,
      const ze_image_region_t* pSrcRegion, uint32_t destRowPitch, uint32_t destSlicePitch,
      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
      void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendImageCopyFromMemoryExt(
      ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void* srcptr,
      const ze_image_region_t* pDstRegion, uint32_t srcRowPitch, uint32_t srcSlicePitch,
      ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t* phWaitEvents,
      void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendEventReset(
      ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendBarrier(
      ze_command_list_handle_t hCommandList, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  static ze_result_t ZE_VISITOR_CCONV VisitCommandListAppendMemoryRangesBarrier(
      ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t* pRangeSizes,
      const void** pRanges, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
      ze_event_handle_t* phWaitEvents, void* userData);

  constexpr bool HasError() const { return internal_error_ != ZE_RESULT_SUCCESS; }

 private:
  bool VisitingNativeGraph() const;

  Result CollectResultAndReset();

  template <pti_api_id_driver_levelzero OverheadId, typename Fn, typename... Args>
  void ZeCommand(Fn func, Args&&... args) {
    overhead::ScopedOverheadCollector scoped(OverheadId);
    auto result = func(std::forward<Args>(args)...);
    if (result != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("Command {} failed with result: {:x}", PTI_FUNCTION_NAME,
                   static_cast<std::uint32_t>(result));
      internal_error_ = result;
    }
  }

  template <typename T>
  static ze_result_t ExceptionHandler(Self* visitor, const char* function_name, T&& function) {
    try {
      std::forward<T>(function)();
      return ZE_RESULT_SUCCESS;
    } catch (const std::exception& ex) {
      SPDLOG_ERROR("Exception in {} : {}", function_name, ex.what());
    } catch (...) {
      SPDLOG_ERROR("Unknown Exception in {}", function_name);
    }
    visitor->internal_error_ = ZE_RESULT_ERROR_UNKNOWN;
    visitor->internal_exception_ = std::current_exception();
    return ZE_RESULT_SUCCESS;  // This return value is the return value of the callback, passed to
                               // the driver. Afaik, its unused.
  }

  std::optional<TimestampInfo> AppendQueryTimestamp(ze_command_list_handle_t command_list,
                                                    ze_event_handle_t event_to_query);

  void AppendSignalEvent(ze_command_list_handle_t command_list, ze_event_handle_t event_to_signal,
                         ze_event_handle_t wait_event);

  ZeExts::Visit visitor_extension_;
  ZeEventPoolManager* event_pool_manager_;
  ze_command_visit_ext_desc_t visit_desc_;
  ze_result_t internal_error_;
  std::exception_ptr internal_exception_ = nullptr;
  Commands commands_;
  ZeCommandListInfo current_command_list_info_;
  ZeDeviceDescriptor current_device_desc_;
  std::unordered_map<ze_command_list_handle_t, ZeCommandListImmutableInfo> command_list_info_map_;
};

#endif  // PTI_LEVELZERO_ZE_COMMAND_VISITOR_H_
