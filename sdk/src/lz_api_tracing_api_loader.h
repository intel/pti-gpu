//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef LZ_API_TRACING_API_LOADER_H_
#define LZ_API_TRACING_API_LOADER_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/layers/zel_tracing_register_cb.h>
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <level_zero/zet_api.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "utils/library_loader.h"
#include "utils/utils.h"

namespace pti {

#if defined(_WIN32)
inline static constexpr const char* const kLevelZeroLoaderName = "ze_loader.dll";
#else
inline static constexpr const char* const kLevelZeroLoaderName = "libze_loader.so.1";
#endif

class PtiLzTracerLoader {
 public:
  static auto& Instance() {
    static PtiLzTracerLoader instance{};
    return instance;
  }

  void Unload() { api_dlsyms_lib_.reset(nullptr); }

  bool Available() const { return api_dlsyms_lib_ != nullptr; }

  decltype(&zeInitDrivers) zeInitDrivers_ = nullptr;                              // NOLINT
  decltype(&zesDriverGetDeviceByUuidExp) zesDriverGetDeviceByUuidExp_ = nullptr;  // NOLINT
  decltype(&zeKernelGetBinaryExp) zeKernelGetBinaryExp_ = nullptr;                // NOLINT
  decltype(&zeExecutableGraphGetSourceGraphExt) zeExecutableGraphGetSourceGraphExt_ =
      nullptr;                                                                            // NOLINT
  decltype(&zeGraphGetPrimaryCommandListExt) zeGraphGetPrimaryCommandListExt_ = nullptr;  // NOLINT
  decltype(&zeGraphCreateExt) zeGraphCreateExt_ = nullptr;                                // NOLINT
  decltype(&zeCommandListBeginCaptureIntoGraphExt) zeCommandListBeginCaptureIntoGraphExt_ =
      nullptr;                                                                            // NOLINT
  decltype(&zeCommandListEndGraphCaptureExt) zeCommandListEndGraphCaptureExt_ = nullptr;  // NOLINT
  decltype(&zeGraphInstantiateExt) zeGraphInstantiateExt_ = nullptr;                      // NOLINT

  // Forward to implementation in core library
#include "tracing_api_dlsym_public.gen"  // Auto-generated callbacks

#define APPEND_COMMAND(X) \
  decltype(&zeCommandListAppend##X) zeCommandListAppend##X##_ = nullptr;  // NOLINT
#include "levelzero/ze_visitor_commands.inc"
#undef APPEND_COMMAND

 private:
  PtiLzTracerLoader() {
    try {
      api_dlsyms_lib_ = std::make_unique<LibraryLoader>(kLevelZeroLoaderName);
    } catch (const std::exception& e) {
      SPDLOG_ERROR("Unable to load {} because {}", kLevelZeroLoaderName, e.what());
      api_dlsyms_lib_ = nullptr;
      return;
    }
#define LEVEL_ZERO_LOADER_GET_SYMBOL(X) \
  X##_ = api_dlsyms_lib_->GetSymbol<decltype(&X)>(#X)  // NOLINT
#include "tracing_api_dlsym_private.gen"               // Auto-generated callbacks
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeInitDrivers);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zesDriverGetDeviceByUuidExp);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeKernelGetBinaryExp);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeExecutableGraphGetSourceGraphExt);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeGraphGetPrimaryCommandListExt);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeGraphCreateExt);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeCommandListBeginCaptureIntoGraphExt);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeCommandListEndGraphCaptureExt);
    LEVEL_ZERO_LOADER_GET_SYMBOL(zeGraphInstantiateExt);
#undef LEVEL_ZERO_LOADER_GET_SYMBOL
#define APPEND_COMMAND(X)                                                                    \
  zeCommandListAppend##X##_ = api_dlsyms_lib_->GetSymbol<decltype(&zeCommandListAppend##X)>( \
      "zeCommandListAppend" #X);  // NOLINT
#include "levelzero/ze_visitor_commands.inc"
#undef APPEND_COMMAND
  }
  std::unique_ptr<LibraryLoader> api_dlsyms_lib_ = nullptr;
};
}  // namespace pti

#endif  // LZ_API_TRACING_API_LOADER_H_
