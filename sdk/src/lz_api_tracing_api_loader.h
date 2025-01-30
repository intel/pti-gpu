//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_LZ_API_TRACING_API_LOADER_H_
#define SRC_LZ_API_TRACING_API_LOADER_H_

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "utils/library_loader.h"
#include "utils/platform_strings.h"
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

  bool ViewAvailable() const { return api_dlsyms_lib_ != nullptr; }

  // Forward to implementation in core library
#include <tracing_api_dlsym_public.gen>  // Auto-generated callbacks

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
#include <tracing_api_dlsym_private.gen>               // Auto-generated callbacks
#undef LEVEL_ZERO_LOADER_GET_SYMBOL
  }
  std::unique_ptr<LibraryLoader> api_dlsyms_lib_ = nullptr;
};
}  // namespace pti

#endif  // SRC_LZ_API_TRACING_API_LOADER_H_
