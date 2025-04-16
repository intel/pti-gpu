//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_PTI_LIB_HANDLER_H_
#define SRC_PTI_LIB_HANDLER_H_

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

#include "pti/pti.h"
#include "pti/pti_view.h"
#include "utils/library_loader.h"
#include "utils/platform_strings.h"
#include "utils/utils.h"
#include "xpti_adapter.h"

namespace pti {

#if defined(_WIN32)
inline std::string MoveUpOneDirectory(std::string_view dir) {
  auto path_pos = dir.find_last_of("\\");
  if (path_pos == std::string_view::npos) {
    return "";
  }
  return std::string{dir.substr(0, path_pos)};
}

inline std::string GetPathToWindowsLibraryDirectory() {
  auto pti_dir = ::utils::GetPathToSharedObject(GetPathToWindowsLibraryDirectory);

  pti_dir = MoveUpOneDirectory(pti_dir);
  if (pti_dir.empty()) {
    return pti_dir;
  }
  pti_dir = MoveUpOneDirectory(pti_dir);
  if (pti_dir.empty()) {
    return pti_dir;
  }

  pti_dir += "\\lib\\";

  return pti_dir;
}
#endif

class PtiLibHandler {
 public:
  static auto& Instance() {
    static PtiLibHandler instance{};
    return instance;
  }

  void Unload() { pti_view_lib_.reset(nullptr); }

  bool ViewAvailable() const { return pti_view_lib_ != nullptr; }

  // Forward to implementation in core library
  decltype(&xptiTraceInit) xptiTraceInit_ = nullptr;  // NOLINT
  // Forward to implementation in core library
  decltype(&xptiTraceFinish) xptiTraceFinish_ = nullptr;                                  // NOLINT
  decltype(&ptiViewEnable) ptiViewEnable_ = nullptr;                                      // NOLINT
  decltype(&ptiViewDisable) ptiViewDisable_ = nullptr;                                    // NOLINT
  decltype(&ptiViewGPULocalAvailable) ptiViewGPULocalAvailable_ = nullptr;                // NOLINT
  decltype(&ptiViewOverheadKindToString) ptiViewOverheadKindToString_ = nullptr;          // NOLINT
  decltype(&ptiViewMemoryTypeToString) ptiViewMemoryTypeToString_ = nullptr;              // NOLINT
  decltype(&ptiViewMemcpyTypeToString) ptiViewMemcpyTypeToString_ = nullptr;              // NOLINT
  decltype(&ptiViewSetCallbacks) ptiViewSetCallbacks_ = nullptr;                          // NOLINT
  decltype(&ptiViewGetNextRecord) ptiViewGetNextRecord_ = nullptr;                        // NOLINT
  decltype(&ptiFlushAllViews) ptiFlushAllViews_ = nullptr;                                // NOLINT
  decltype(&ptiViewPushExternalCorrelationId) ptiViewPushExternalCorrelationId_ =         // NOLINT
      nullptr;                                                                            // NOLINT
  decltype(&ptiViewPopExternalCorrelationId) ptiViewPopExternalCorrelationId_ = nullptr;  // NOLINT
  decltype(&ptiViewGetTimestamp) ptiViewGetTimestamp_ = nullptr;                          // NOLINT
  decltype(&ptiViewSetTimestampCallback) ptiViewSetTimestampCallback_ = nullptr;          // NOLINT
  decltype(&ptiViewGetApiIdName) ptiViewGetApiIdName_ = nullptr;                          // NOLINT
  decltype(&ptiViewEnableDriverApi) ptiViewEnableDriverApi_ = nullptr;                    // NOLINT
  decltype(&ptiViewEnableDriverApiClass) ptiViewEnableDriverApiClass_ = nullptr;          // NOLINT
  decltype(&ptiViewEnableRuntimeApi) ptiViewEnableRuntimeApi_ = nullptr;                  // NOLINT
  decltype(&ptiViewEnableRuntimeApiClass) ptiViewEnableRuntimeApiClass_ = nullptr;        // NOLINT

 private:
  PtiLibHandler() {
    try {
      spdlog::set_level(spdlog::level::off);

      auto env_string = ::utils::GetEnv("PTILOG_LEVEL");
      if (!env_string.empty()) {
        spdlog::cfg::helpers::load_levels(env_string);
      }

      std::string pti_dir;
#if defined(_WIN32)
      pti_dir += GetPathToWindowsLibraryDirectory();
#endif
      pti_dir += strings::kPtiViewLib;
      pti_view_lib_ = std::make_unique<LibraryLoader>(pti_dir);
    } catch (const std::exception& e) {
      SPDLOG_ERROR("Unable to load {} because {}", strings::kPtiViewLib, e.what());
      pti_view_lib_ = nullptr;
      return;
    }
#define PTI_VIEW_GET_SYMBOL(X) X##_ = pti_view_lib_->GetSymbol<decltype(&X)>(#X)  // NOLINT
    PTI_VIEW_GET_SYMBOL(xptiTraceInit);
    PTI_VIEW_GET_SYMBOL(xptiTraceFinish);
    PTI_VIEW_GET_SYMBOL(ptiViewEnable);
    PTI_VIEW_GET_SYMBOL(ptiViewDisable);
    PTI_VIEW_GET_SYMBOL(ptiViewGPULocalAvailable);
    PTI_VIEW_GET_SYMBOL(ptiViewOverheadKindToString);
    PTI_VIEW_GET_SYMBOL(ptiViewMemoryTypeToString);
    PTI_VIEW_GET_SYMBOL(ptiViewMemcpyTypeToString);
    PTI_VIEW_GET_SYMBOL(ptiViewSetCallbacks);
    PTI_VIEW_GET_SYMBOL(ptiViewGetNextRecord);
    PTI_VIEW_GET_SYMBOL(ptiFlushAllViews);
    PTI_VIEW_GET_SYMBOL(ptiViewPushExternalCorrelationId);
    PTI_VIEW_GET_SYMBOL(ptiViewPopExternalCorrelationId);
    PTI_VIEW_GET_SYMBOL(ptiViewGetTimestamp);
    PTI_VIEW_GET_SYMBOL(ptiViewSetTimestampCallback);
    PTI_VIEW_GET_SYMBOL(ptiViewGetApiIdName);
    PTI_VIEW_GET_SYMBOL(ptiViewEnableDriverApi);
    PTI_VIEW_GET_SYMBOL(ptiViewEnableDriverApiClass);
    PTI_VIEW_GET_SYMBOL(ptiViewEnableRuntimeApi);
    PTI_VIEW_GET_SYMBOL(ptiViewEnableRuntimeApiClass);
#undef PTI_VIEW_GET_SYMBOL
  }
  std::unique_ptr<LibraryLoader> pti_view_lib_ = nullptr;
};
}  // namespace pti

#endif  // SRC_PTI_LIB_HANDLER_H_
