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

#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
inline static constexpr const char* const kLevelZeroLoaderName = "ze_loader.dll";
inline static constexpr const char* const kLevelZeroDriverName =
    kLevelZeroLoaderName;  // Use ze_loader.dll. ze_intel_gpu64 is the driver if this is an issue in
                           // the future.
#else
inline static constexpr const char* const kLevelZeroLoaderName = "libze_loader.so.1";
inline static constexpr const char* const kLevelZeroDriverName = "libze_intel_gpu.so.1";
#endif

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

#define LOADER_LOAD_AND_DEBUG_PRINT(name)                        \
  fptr_##name##_ = l0_loader_.GetSymbol<fptr_##name##_t>(#name); \
  SPDLOG_DEBUG("Found fptr_{}_: {}", #name, (void*)*fptr_##name##_);

#define DRIVER_LOAD_AND_DEBUG_PRINT(name)                        \
  fptr_##name##_ = l0_driver_.GetSymbol<fptr_##name##_t>(#name); \
  SPDLOG_DEBUG("Found fptr_{}_: {}", #name, (void*)*fptr_##name##_);

class Level0Wrapper {
 public:
  Level0Wrapper() {
    try {
      l0_loader_ = LibraryLoader{kLevelZeroLoaderName};
      LOADER_LOAD_AND_DEBUG_PRINT(zelEnableTracingLayer);
      LOADER_LOAD_AND_DEBUG_PRINT(zelDisableTracingLayer);

      l0_driver_ = LibraryLoader{kLevelZeroDriverName};
      DRIVER_LOAD_AND_DEBUG_PRINT(zeEventPoolGetFlags);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandListGetDeviceHandle);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandListGetContextHandle);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandListGetOrdinal);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandListImmediateGetIndex);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandListIsImmediate);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandQueueGetIndex);
      DRIVER_LOAD_AND_DEBUG_PRINT(zeCommandQueueGetOrdinal);
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

  ze_result_t w_zeCommandListImmediateGetIndex(ze_command_list_handle_t command_list,
                                               uint32_t* index) const {
    if (nullptr != fptr_zeCommandListImmediateGetIndex_) {
      return fptr_zeCommandListImmediateGetIndex_(command_list, index);
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
        nullptr == fptr_zeCommandListGetContextHandle_ ||
        nullptr == fptr_zeCommandListIsImmediate_ ||
        nullptr == fptr_zeCommandListImmediateGetIndex_ ||
        nullptr == fptr_zeCommandQueueGetIndex_ || nullptr == fptr_zeCommandQueueGetOrdinal_ ||
        nullptr == fptr_zeCommandListGetOrdinal_) {
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
  LibraryLoader l0_driver_;
  LibraryLoader l0_loader_;
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
#undef LOADER_LOAD_AND_DEBUG_PRINT
#undef DRIVER_LOAD_AND_DEBUG_PRINT
#endif
