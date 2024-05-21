//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT

#ifndef ZE_WRAPPERS_H_
#define ZE_WRAPPERS_H_

#if defined(_WIN32)
#else
#include <dlfcn.h>
#endif

#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

/*
 * Wrappers for L0 Instrospection APIs and for Loader Enable/Disable Tracing.
 * They enable graceful handling (starting from library loading)
 * when L0 on the system doesn't have
 * the Introspection API implementation or Dynamic Tracing Enable/Disable
 */

typedef ze_result_t (*fptr_zeEventPoolGetFlags_t)(ze_event_pool_handle_t hEventPool,
                                                  ze_event_pool_flags_t* pFlags);

typedef ze_result_t (*fptr_zeCommandListGetDeviceHandle_t)(ze_command_list_handle_t command_list,
                                                           ze_device_handle_t* device);

typedef ze_result_t (*fptr_zeCommandListGetContextHandle_t)(ze_command_list_handle_t command_list,
                                                            ze_context_handle_t* context);

typedef ze_result_t (*fptr_zeCommandListIsImmediate_t)(ze_command_list_handle_t command_list,
                                                       ze_bool_t* isImmediate);

typedef ze_result_t (*fptr_zeCommandListImmediateGetIndex_t)(ze_command_list_handle_t command_list,
                                                             uint32_t* index);

typedef ze_result_t (*fptr_zeCommandListGetOrdinal_t)(ze_command_list_handle_t command_list,
                                                      uint32_t* ordinal);
typedef ze_result_t (*fptr_zeCommandQueueGetIndex_t)(ze_command_queue_handle_t command_queue,
                                                     uint32_t* index);

typedef ze_result_t (*fptr_zeCommandQueueGetOrdinal_t)(ze_command_queue_handle_t command_list,
                                                       uint32_t* ordinal);

typedef ze_result_t (*fptr_zelEnableTracingLayer_t)();

typedef ze_result_t (*fptr_zelDisableTracingLayer_t)();

class Level0Wrapper {
 public:
  Level0Wrapper(const Level0Wrapper&) = delete;
  Level0Wrapper(Level0Wrapper&&) = delete;
  Level0Wrapper& operator=(const Level0Wrapper&) = delete;
  Level0Wrapper& operator=(Level0Wrapper&&) = delete;
  Level0Wrapper() {}
  ~Level0Wrapper() {
    if (l0_handle_) {
#if defined(_WIN32)
#else
      dlclose(l0_handle_);
#endif
    }
    if (l0_loader_handle_) {
#if defined(_WIN32)
#else
      dlclose(l0_loader_handle_);
#endif
    }
  }

  ze_result_t w_zeEventPoolGetFlags(ze_event_pool_handle_t hEventPool,
                                    ze_event_pool_flags_t* pFlags) {
    if (nullptr != fptr_zeEventPoolGetFlags_) {
      return fptr_zeEventPoolGetFlags_(hEventPool, pFlags);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetDeviceHandle(ze_command_list_handle_t command_list,
                                             ze_device_handle_t* device) {
    if (nullptr != fptr_zeCommandListGetDeviceHandle_) {
      return fptr_zeCommandListGetDeviceHandle_(command_list, device);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetContextHandle(ze_command_list_handle_t command_list,
                                              ze_context_handle_t* context) {
    if (nullptr != fptr_zeCommandListGetContextHandle_) {
      return fptr_zeCommandListGetContextHandle_(command_list, context);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListIsImmediate(ze_command_list_handle_t command_list,
                                         ze_bool_t* isImmediate) {
    if (nullptr != fptr_zeCommandListIsImmediate_) {
      return fptr_zeCommandListIsImmediate_(command_list, isImmediate);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListImmediateGetIndex(ze_command_list_handle_t command_list,
                                               uint32_t* index) {
    if (nullptr != fptr_zeCommandListImmediateGetIndex_) {
      return fptr_zeCommandListImmediateGetIndex_(command_list, index);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandListGetOrdinal(ze_command_list_handle_t command_list, uint32_t* ordinal) {
    if (nullptr != fptr_zeCommandListGetOrdinal_) {
      return fptr_zeCommandListGetOrdinal_(command_list, ordinal);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandQueueGetIndex(ze_command_queue_handle_t command_queue, uint32_t* index) {
    if (nullptr != fptr_zeCommandQueueGetIndex_) {
      return fptr_zeCommandQueueGetIndex_(command_queue, index);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zeCommandQueueGetOrdinal(ze_command_queue_handle_t command_queue,
                                         uint32_t* ordinal) {
    if (nullptr != fptr_zeCommandQueueGetOrdinal_) {
      return fptr_zeCommandQueueGetOrdinal_(command_queue, ordinal);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zelEnableTracingLayer() {
    if (nullptr != fptr_zelEnableTracingLayer_) {
      return fptr_zelEnableTracingLayer_();
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t w_zelDisableTracingLayer() {
    if (nullptr != fptr_zelDisableTracingLayer_) {
      return fptr_zelDisableTracingLayer_();
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

#define LOAD_AND_DEBUG_PRINT(name)                            \
  fptr_##name##_ = (fptr_##name##_t)dlsym(l0_handle_, #name); \
  SPDLOG_DEBUG("Found fptr_{}_: {}", #name, (void*)*fptr_##name##_);

  ze_result_t InitIntrospectionWrappers() {
    static char l0_so_name[] = "libze_intel_gpu.so.1";
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    l0_handle_ = dlopen(l0_so_name, RTLD_NOW);
    if (nullptr == l0_handle_) {
      SPDLOG_WARN("In {}, cannot load {}", __FUNCTION__, l0_so_name);
      return ZE_RESULT_ERROR_UNKNOWN;
    }

    LOAD_AND_DEBUG_PRINT(zeEventPoolGetFlags);
    LOAD_AND_DEBUG_PRINT(zeCommandListGetDeviceHandle);
    LOAD_AND_DEBUG_PRINT(zeCommandListGetContextHandle);
    LOAD_AND_DEBUG_PRINT(zeCommandListGetOrdinal);
    LOAD_AND_DEBUG_PRINT(zeCommandListImmediateGetIndex);
    LOAD_AND_DEBUG_PRINT(zeCommandListIsImmediate);
    LOAD_AND_DEBUG_PRINT(zeCommandQueueGetIndex);
    LOAD_AND_DEBUG_PRINT(zeCommandQueueGetOrdinal);

    if (nullptr == fptr_zeEventPoolGetFlags_ || nullptr == fptr_zeCommandListGetDeviceHandle_ ||
        nullptr == fptr_zeCommandListGetContextHandle_ ||
        nullptr == fptr_zeCommandListIsImmediate_ ||
        nullptr == fptr_zeCommandListImmediateGetIndex_ ||
        nullptr == fptr_zeCommandQueueGetIndex_ || nullptr == fptr_zeCommandQueueGetOrdinal_ ||
        nullptr == fptr_zeCommandListGetOrdinal_) {
      return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
  }

  ze_result_t InitDynamicTracingWrappers() {
    static char l0_loader_so_name[] = "libze_loader.so.1";
    SPDLOG_DEBUG("In {}", __FUNCTION__);
    l0_loader_handle_ = dlopen(l0_loader_so_name, RTLD_NOW);
    if (nullptr == l0_loader_handle_) {
      SPDLOG_WARN("In {}, cannot load {}", __FUNCTION__, l0_loader_so_name);
      return ZE_RESULT_ERROR_UNKNOWN;
    }

    LOAD_AND_DEBUG_PRINT(zelEnableTracingLayer);
    LOAD_AND_DEBUG_PRINT(zelDisableTracingLayer);

    if (nullptr == fptr_zelEnableTracingLayer_ || nullptr == fptr_zelDisableTracingLayer_) {
      return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
  }

 private:
  void* l0_handle_ = nullptr;
  void* l0_loader_handle_ = nullptr;
  fptr_zeEventPoolGetFlags_t fptr_zeEventPoolGetFlags_ = nullptr;
  fptr_zeCommandListGetDeviceHandle_t fptr_zeCommandListGetDeviceHandle_ = nullptr;
  fptr_zeCommandListGetContextHandle_t fptr_zeCommandListGetContextHandle_ = nullptr;
  fptr_zeCommandListIsImmediate_t fptr_zeCommandListIsImmediate_ = nullptr;
  fptr_zeCommandListImmediateGetIndex_t fptr_zeCommandListImmediateGetIndex_ = nullptr;
  fptr_zeCommandListGetOrdinal_t fptr_zeCommandListGetOrdinal_ = nullptr;
  fptr_zeCommandQueueGetIndex_t fptr_zeCommandQueueGetIndex_ = nullptr;
  fptr_zeCommandQueueGetOrdinal_t fptr_zeCommandQueueGetOrdinal_ = nullptr;
  fptr_zelEnableTracingLayer_t fptr_zelEnableTracingLayer_ = nullptr;
  fptr_zelDisableTracingLayer_t fptr_zelDisableTracingLayer_ = nullptr;
};
#endif