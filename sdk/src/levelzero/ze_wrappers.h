//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT

#ifndef ZE_WRAPPERS_H_
#define ZE_WRAPPERS_H_

#include "library_loader.h"
#if defined(_WIN32)
#else
#include <dlfcn.h>
#endif

#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
inline static constexpr const char* const kLevelZeroLoaderName = "ze_loader.dll";
#else
inline static constexpr const char* const kLevelZeroLoaderName = "libze_loader.so.1";
#endif

/*
 * Wrappers for L0 Introspection APIs and for Loader Enable/Disable Tracing.
 * They enable graceful handling (starting from library loading)
 * when L0 on the system doesn't have
 * the Introspection API implementation or Dynamic Tracing Enable/Disable
 */

#define LOADER_LOAD_AND_DEBUG_PRINT(name)                          \
  do {                                                             \
    fptr_##name##_ = l0_loader_.GetSymbol<decltype(&name)>(#name); \
    SPDLOG_DEBUG("Found fptr_{}_", #name);                         \
  } while (0)

class Level0Wrapper {
 public:
  Level0Wrapper() {
    try {
      l0_loader_ = LibraryLoader{kLevelZeroLoaderName};
      LOADER_LOAD_AND_DEBUG_PRINT(zelEnableTracingLayer);
      LOADER_LOAD_AND_DEBUG_PRINT(zelDisableTracingLayer);
      LOADER_LOAD_AND_DEBUG_PRINT(zeEventPoolGetFlags);
      LOADER_LOAD_AND_DEBUG_PRINT(zeEventGetEventPool);
      LOADER_LOAD_AND_DEBUG_PRINT(zeEventPoolGetContextHandle);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListGetDeviceHandle);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListGetContextHandle);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListGetOrdinal);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListGetFlags);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListImmediateGetIndex);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListImmediateGetFlags);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListIsImmediate);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandQueueGetIndex);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandQueueGetOrdinal);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphCreateExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphDestroyExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphInstantiateExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphIsEmptyExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphDumpContentsExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphSetDestructionCallbackExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListBeginGraphCaptureExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListBeginCaptureIntoGraphExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListEndGraphCaptureExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListIsGraphCaptureEnabledExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListAppendGraphExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeCommandListGetGraphExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeGraphGetPrimaryCommandListExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeExecutableGraphGetSourceGraphExt);
      LOADER_LOAD_AND_DEBUG_PRINT(zeExecutableGraphDestroyExt);
    } catch ([[maybe_unused]] const std::runtime_error& e) {
      SPDLOG_ERROR("Error Loading Level Zero symbols: {}", e.what());
    }
  }

  ze_result_t w_zeEventPoolGetFlags(ze_event_pool_handle_t hEventPool,
                                    ze_event_pool_flags_t* pFlags) const {
    if (nullptr != fptr_zeEventPoolGetFlags_) {
      return fptr_zeEventPoolGetFlags_(hEventPool, pFlags);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeEventGetEventPool(ze_event_handle_t hEvent, ze_event_pool_handle_t* phEventPool) {
    if (nullptr != fptr_zeEventGetEventPool_) {
      return fptr_zeEventGetEventPool_(hEvent, phEventPool);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeEventPoolGetContextHandle(ze_event_pool_handle_t hEventPool,
                                            ze_context_handle_t* phContext) {
    if (nullptr != fptr_zeEventPoolGetContextHandle_) {
      return fptr_zeEventPoolGetContextHandle_(hEventPool, phContext);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetDeviceHandle(ze_command_list_handle_t command_list,
                                             ze_device_handle_t* device) const {
    if (nullptr != fptr_zeCommandListGetDeviceHandle_) {
      return fptr_zeCommandListGetDeviceHandle_(command_list, device);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetContextHandle(ze_command_list_handle_t command_list,
                                              ze_context_handle_t* context) const {
    if (nullptr != fptr_zeCommandListGetContextHandle_) {
      return fptr_zeCommandListGetContextHandle_(command_list, context);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListIsImmediate(ze_command_list_handle_t command_list,
                                         ze_bool_t* isImmediate) const {
    if (nullptr != fptr_zeCommandListIsImmediate_) {
      return fptr_zeCommandListIsImmediate_(command_list, isImmediate);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetFlags(ze_command_list_handle_t command_list,
                                      ze_command_list_flags_t* flags) const {
    if (nullptr != fptr_zeCommandListGetFlags_) {
      return fptr_zeCommandListGetFlags_(command_list, flags);
    }

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListImmediateGetIndex(ze_command_list_handle_t command_list,
                                               uint32_t* index) const {
    if (nullptr != fptr_zeCommandListImmediateGetIndex_) {
      return fptr_zeCommandListImmediateGetIndex_(command_list, index);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListImmediateGetFlags(ze_command_list_handle_t command_list,
                                               ze_command_queue_flags_t* flags) const {
    if (nullptr != fptr_zeCommandListImmediateGetFlags_) {
      return fptr_zeCommandListImmediateGetFlags_(command_list, flags);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetOrdinal(ze_command_list_handle_t command_list,
                                        uint32_t* ordinal) const {
    if (nullptr != fptr_zeCommandListGetOrdinal_) {
      return fptr_zeCommandListGetOrdinal_(command_list, ordinal);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandQueueGetIndex(ze_command_queue_handle_t command_queue,
                                       uint32_t* index) const {
    if (nullptr != fptr_zeCommandQueueGetIndex_) {
      return fptr_zeCommandQueueGetIndex_(command_queue, index);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandQueueGetOrdinal(ze_command_queue_handle_t command_queue,
                                         uint32_t* ordinal) const {
    if (nullptr != fptr_zeCommandQueueGetOrdinal_) {
      return fptr_zeCommandQueueGetOrdinal_(command_queue, ordinal);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphCreateExt(ze_context_handle_t hContext, const void* pNext,
                                 ze_graph_handle_t* phGraph) const {
    if (nullptr != fptr_zeGraphCreateExt_) {
      return fptr_zeGraphCreateExt_(hContext, pNext, phGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphDestroyExt(ze_graph_handle_t hGraph) const {
    if (nullptr != fptr_zeGraphDestroyExt_) {
      return fptr_zeGraphDestroyExt_(hGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphInstantiateExt(ze_graph_handle_t hGraph, const void* pNext,
                                      ze_executable_graph_handle_t* phExecutableGraph) const {
    if (nullptr != fptr_zeGraphInstantiateExt_) {
      return fptr_zeGraphInstantiateExt_(hGraph, pNext, phExecutableGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphIsEmptyExt(ze_graph_handle_t hGraph) const {
    if (nullptr != fptr_zeGraphIsEmptyExt_) {
      return fptr_zeGraphIsEmptyExt_(hGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphDumpContentsExt(ze_graph_handle_t hGraph, const char* filePath,
                                       const void* pNext) const {
    if (nullptr != fptr_zeGraphDumpContentsExt_) {
      return fptr_zeGraphDumpContentsExt_(hGraph, filePath, pNext);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphSetDestructionCallbackExt(ze_graph_handle_t hGraph,
                                                 zex_mem_graph_free_callback_fn_t pfnCallback,
                                                 void* pUserData, const void* pNext) const {
    if (nullptr != fptr_zeGraphSetDestructionCallbackExt_) {
      return fptr_zeGraphSetDestructionCallbackExt_(hGraph, pfnCallback, pUserData, pNext);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListBeginGraphCaptureExt(ze_command_list_handle_t hCommandList,
                                                  const void* pNext) const {
    if (nullptr != fptr_zeCommandListBeginGraphCaptureExt_) {
      return fptr_zeCommandListBeginGraphCaptureExt_(hCommandList, pNext);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListBeginCaptureIntoGraphExt(ze_command_list_handle_t hCommandList,
                                                      ze_graph_handle_t hGraph,
                                                      const void* pNext) const {
    if (nullptr != fptr_zeCommandListBeginCaptureIntoGraphExt_) {
      return fptr_zeCommandListBeginCaptureIntoGraphExt_(hCommandList, hGraph, pNext);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListEndGraphCaptureExt(ze_command_list_handle_t hCommandList,
                                                const void* pNext,
                                                ze_graph_handle_t* phGraph) const {
    if (nullptr != fptr_zeCommandListEndGraphCaptureExt_) {
      return fptr_zeCommandListEndGraphCaptureExt_(hCommandList, pNext, phGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListIsGraphCaptureEnabledExt(ze_command_list_handle_t hCommandList) const {
    if (nullptr != fptr_zeCommandListIsGraphCaptureEnabledExt_) {
      return fptr_zeCommandListIsGraphCaptureEnabledExt_(hCommandList);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListAppendGraphExt(ze_command_list_handle_t hCommandList,
                                            ze_executable_graph_handle_t hGraph, const void* pNext,
                                            ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                                            ze_event_handle_t* phWaitEvents) const {
    if (nullptr != fptr_zeCommandListAppendGraphExt_) {
      return fptr_zeCommandListAppendGraphExt_(hCommandList, hGraph, pNext, hSignalEvent,
                                               numWaitEvents, phWaitEvents);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetGraphExt(ze_command_list_handle_t hCommandList,
                                         ze_graph_handle_t* phGraph) const {
    if (nullptr != fptr_zeCommandListGetGraphExt_) {
      return fptr_zeCommandListGetGraphExt_(hCommandList, phGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeGraphGetPrimaryCommandListExt(ze_graph_handle_t hGraph,
                                                ze_command_list_handle_t* phCommandList) const {
    if (nullptr != fptr_zeGraphGetPrimaryCommandListExt_) {
      return fptr_zeGraphGetPrimaryCommandListExt_(hGraph, phCommandList);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeExecutableGraphGetSourceGraphExt(ze_executable_graph_handle_t hGraph,
                                                   ze_graph_handle_t* phSourceGraph) const {
    if (nullptr != fptr_zeExecutableGraphGetSourceGraphExt_) {
      return fptr_zeExecutableGraphGetSourceGraphExt_(hGraph, phSourceGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeExecutableGraphDestroyExt(ze_executable_graph_handle_t hGraph) const {
    if (nullptr != fptr_zeExecutableGraphDestroyExt_) {
      return fptr_zeExecutableGraphDestroyExt_(hGraph);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zelEnableTracingLayer() const {
    if (nullptr != fptr_zelEnableTracingLayer_) {
      return fptr_zelEnableTracingLayer_();
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zelDisableTracingLayer() const {
    if (nullptr != fptr_zelDisableTracingLayer_) {
      return fptr_zelDisableTracingLayer_();
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t InitIntrospectionWrappers() const {
    SPDLOG_DEBUG("In {}", __FUNCTION__);

    if (nullptr == fptr_zeEventPoolGetFlags_ || nullptr == fptr_zeCommandListGetDeviceHandle_ ||
        nullptr == fptr_zeEventGetEventPool_ || nullptr == fptr_zeEventPoolGetContextHandle_ ||
        nullptr == fptr_zeCommandListGetContextHandle_ ||
        nullptr == fptr_zeCommandListIsImmediate_ ||
        nullptr == fptr_zeCommandListImmediateGetIndex_ ||
        nullptr == fptr_zeCommandQueueGetIndex_ || nullptr == fptr_zeCommandQueueGetOrdinal_ ||
        nullptr == fptr_zeCommandListGetOrdinal_) {
      return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
  }

  ze_result_t InitGraphWrappers() const {
    SPDLOG_DEBUG("In {}", __FUNCTION__);

    if (nullptr == fptr_zeGraphCreateExt_ || nullptr == fptr_zeGraphDestroyExt_ ||
        nullptr == fptr_zeGraphInstantiateExt_ || nullptr == fptr_zeGraphIsEmptyExt_ ||
        nullptr == fptr_zeGraphDumpContentsExt_ ||
        nullptr == fptr_zeGraphSetDestructionCallbackExt_ ||
        nullptr == fptr_zeCommandListBeginGraphCaptureExt_ ||
        nullptr == fptr_zeCommandListBeginCaptureIntoGraphExt_ ||
        nullptr == fptr_zeCommandListEndGraphCaptureExt_ ||
        nullptr == fptr_zeCommandListIsGraphCaptureEnabledExt_ ||
        nullptr == fptr_zeCommandListAppendGraphExt_ || nullptr == fptr_zeCommandListGetGraphExt_ ||
        nullptr == fptr_zeGraphGetPrimaryCommandListExt_ ||
        nullptr == fptr_zeExecutableGraphGetSourceGraphExt_ ||
        nullptr == fptr_zeExecutableGraphDestroyExt_) {
      return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
  }

  ze_result_t InitDynamicTracingWrappers() const {
    SPDLOG_DEBUG("In {}", __FUNCTION__);

    if (nullptr == fptr_zelEnableTracingLayer_ || nullptr == fptr_zelDisableTracingLayer_) {
      return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
  }

 private:
  LibraryLoader l0_loader_;
  decltype(&zeEventPoolGetFlags) fptr_zeEventPoolGetFlags_ = nullptr;
  decltype(&zeEventGetEventPool) fptr_zeEventGetEventPool_ = nullptr;
  decltype(&zeEventPoolGetContextHandle) fptr_zeEventPoolGetContextHandle_ = nullptr;
  decltype(&zeCommandListGetDeviceHandle) fptr_zeCommandListGetDeviceHandle_ = nullptr;
  decltype(&zeCommandListGetContextHandle) fptr_zeCommandListGetContextHandle_ = nullptr;
  decltype(&zeCommandListGetFlags) fptr_zeCommandListGetFlags_ = nullptr;
  decltype(&zeCommandListIsImmediate) fptr_zeCommandListIsImmediate_ = nullptr;
  decltype(&zeCommandListImmediateGetIndex) fptr_zeCommandListImmediateGetIndex_ = nullptr;
  decltype(&zeCommandListImmediateGetFlags) fptr_zeCommandListImmediateGetFlags_ = nullptr;
  decltype(&zeCommandListGetOrdinal) fptr_zeCommandListGetOrdinal_ = nullptr;
  decltype(&zeCommandQueueGetIndex) fptr_zeCommandQueueGetIndex_ = nullptr;
  decltype(&zeCommandQueueGetOrdinal) fptr_zeCommandQueueGetOrdinal_ = nullptr;
  decltype(&zelEnableTracingLayer) fptr_zelEnableTracingLayer_ = nullptr;
  decltype(&zelDisableTracingLayer) fptr_zelDisableTracingLayer_ = nullptr;
  decltype(&zeGraphCreateExt) fptr_zeGraphCreateExt_ = nullptr;
  decltype(&zeGraphDestroyExt) fptr_zeGraphDestroyExt_ = nullptr;
  decltype(&zeGraphInstantiateExt) fptr_zeGraphInstantiateExt_ = nullptr;
  decltype(&zeGraphIsEmptyExt) fptr_zeGraphIsEmptyExt_ = nullptr;
  decltype(&zeGraphDumpContentsExt) fptr_zeGraphDumpContentsExt_ = nullptr;
  decltype(&zeGraphSetDestructionCallbackExt) fptr_zeGraphSetDestructionCallbackExt_ = nullptr;
  decltype(&zeCommandListBeginGraphCaptureExt) fptr_zeCommandListBeginGraphCaptureExt_ = nullptr;
  decltype(&zeCommandListBeginCaptureIntoGraphExt) fptr_zeCommandListBeginCaptureIntoGraphExt_ =
      nullptr;
  decltype(&zeCommandListEndGraphCaptureExt) fptr_zeCommandListEndGraphCaptureExt_ = nullptr;
  decltype(&zeCommandListIsGraphCaptureEnabledExt) fptr_zeCommandListIsGraphCaptureEnabledExt_ =
      nullptr;
  decltype(&zeCommandListAppendGraphExt) fptr_zeCommandListAppendGraphExt_ = nullptr;
  decltype(&zeCommandListGetGraphExt) fptr_zeCommandListGetGraphExt_ = nullptr;
  decltype(&zeGraphGetPrimaryCommandListExt) fptr_zeGraphGetPrimaryCommandListExt_ = nullptr;
  decltype(&zeExecutableGraphGetSourceGraphExt) fptr_zeExecutableGraphGetSourceGraphExt_ = nullptr;
  decltype(&zeExecutableGraphDestroyExt) fptr_zeExecutableGraphDestroyExt_ = nullptr;
};
#undef LOADER_LOAD_AND_DEBUG_PRINT
#endif
