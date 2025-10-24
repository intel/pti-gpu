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
#include "pti/pti_callback.h"
#include "pti/pti_metrics.h"
#include "pti/pti_metrics_scope.h"
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

// API implemented in PTI Core library
// to pass from PTI Interface library status of detected "foreign" XPTI subscribers,
// if such were detected at the library load time
void PtiSetXPTIEnvironmentDetails(bool is_foreign_subscriber, bool is_likely_unitrace_subscriber);

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

  decltype(&ptiMetricsConfigureCollection) ptiMetricsConfigureCollection_ = nullptr;      // NOLINT
  decltype(&ptiMetricsStopCollection) ptiMetricsStopCollection_ = nullptr;                // NOLINT
  decltype(&ptiMetricsGetMetricGroups) ptiMetricsGetMetricGroups_ = nullptr;              // NOLINT
  decltype(&ptiMetricsGetMetricsProperties) ptiMetricsGetMetricsProperties_ = nullptr;    // NOLINT
  decltype(&ptiMetricsStartCollectionPaused) ptiMetricsStartCollectionPaused_ = nullptr;  // NOLINT
  decltype(&ptiMetricsPauseCollection) ptiMetricsPauseCollection_ = nullptr;              // NOLINT
  decltype(&ptiMetricsResumeCollection) ptiMetricsResumeCollection_ = nullptr;            // NOLINT
  decltype(&ptiMetricsGetDevices) ptiMetricsGetDevices_ = nullptr;                        // NOLINT
  decltype(&ptiMetricsStartCollection) ptiMetricsStartCollection_ = nullptr;              // NOLINT
  decltype(&ptiMetricGetCalculatedData) ptiMetricGetCalculatedData_ = nullptr;            // NOLINT
  decltype(&ptiCallbackSubscribe) ptiCallbackSubscribe_ = nullptr;                        // NOLINT
  decltype(&ptiCallbackUnsubscribe) ptiCallbackUnsubscribe_ = nullptr;                    // NOLINT
  decltype(&ptiCallbackEnableDomain) ptiCallbackEnableDomain_ = nullptr;                  // NOLINT
  decltype(&ptiCallbackDisableDomain) ptiCallbackDisableDomain_ = nullptr;                // NOLINT
  decltype(&ptiCallbackDisableAllDomains) ptiCallbackDisableAllDomains_ = nullptr;        // NOLINT
  decltype(&ptiCallbackDomainTypeToString) ptiCallbackDomainTypeToString_ = nullptr;      // NOLINT
  decltype(&ptiCallbackPhaseTypeToString) ptiCallbackPhaseTypeToString_ = nullptr;        // NOLINT
  decltype(&PtiSetXPTIEnvironmentDetails) PtiSetXPTIEnvironmentDetails_ = nullptr;        // NOLINT

  decltype(&ptiMetricsScopeEnable) ptiMetricsScopeEnable_ = nullptr;                    // NOLINT
  decltype(&ptiMetricsScopeConfigure) ptiMetricsScopeConfigure_ = nullptr;              // NOLINT
  decltype(&ptiMetricsScopeStartCollection) ptiMetricsScopeStartCollection_ = nullptr;  // NOLINT
  decltype(&ptiMetricsScopeStopCollection) ptiMetricsScopeStopCollection_ = nullptr;    // NOLINT
  decltype(&ptiMetricsScopeDisable) ptiMetricsScopeDisable_ = nullptr;                  // NOLINT
  decltype(&ptiMetricsScopeQueryCollectionBufferSize) ptiMetricsScopeQueryCollectionBufferSize_ =
      nullptr;  // NOLINT
  decltype(&ptiMetricsScopeSetCollectionBufferSize) ptiMetricsScopeSetCollectionBufferSize_ =
      nullptr;  // NOLINT
  decltype(&ptiMetricsScopeGetCollectionBuffersCount) ptiMetricsScopeGetCollectionBuffersCount_ =
      nullptr;  // NOLINT
  decltype(&ptiMetricsScopeGetCollectionBuffer) ptiMetricsScopeGetCollectionBuffer_ =
      nullptr;  // NOLINT
  decltype(&ptiMetricsScopeGetCollectionBufferProperties)
      ptiMetricsScopeGetCollectionBufferProperties_ = nullptr;                            // NOLINT
  decltype(&ptiMetricsScopeCalculateMetrics) ptiMetricsScopeCalculateMetrics_ = nullptr;  // NOLINT
  decltype(&ptiMetricsScopeQueryMetricsBufferSize) ptiMetricsScopeQueryMetricsBufferSize_ =
      nullptr;  // NOLINT
  decltype(&ptiMetricsScopeGetMetricsMetadata) ptiMetricsScopeGetMetricsMetadata_ =
      nullptr;  // NOLINT

 private:
  inline void CommunicateForeignXPTISubscriber() {
    // Passing information about XPTI subscriber to PTI Core library right after it is loaded.
    // This should be done before any other call to PTI Core, as it might create Sycl collector
    // and it should not enable it, in case if some foreign subscriber already subscribed for XPTI
    if (PtiSetXPTIEnvironmentDetails_) {
      auto [is_foreign_subscriber, is_likely_unitrace_subscriber] = IsForeignXPTISubscriber();
      PtiSetXPTIEnvironmentDetails_(is_foreign_subscriber, is_likely_unitrace_subscriber);
    } else {
      SPDLOG_DEBUG("PtiSetXPTIEnvironmentDetails_ is not available in the loaded library.");
    }
  }

  PtiLibHandler() {
    try {
      spdlog::set_level(spdlog::level::off);

      auto env_string = ::utils::GetEnv("PTILOG_LEVEL");
      if (!env_string.empty()) {
        spdlog::cfg::helpers::load_levels(env_string);
      }
      ::utils::SetGlobalSpdLogPattern();

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

    PTI_VIEW_GET_SYMBOL(ptiMetricsConfigureCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsStopCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsGetMetricGroups);
    PTI_VIEW_GET_SYMBOL(ptiMetricsGetMetricsProperties);
    PTI_VIEW_GET_SYMBOL(ptiMetricsStartCollectionPaused);
    PTI_VIEW_GET_SYMBOL(ptiMetricsPauseCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsResumeCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsGetDevices);
    PTI_VIEW_GET_SYMBOL(ptiMetricsStartCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricGetCalculatedData);

    PTI_VIEW_GET_SYMBOL(ptiCallbackSubscribe);
    PTI_VIEW_GET_SYMBOL(ptiCallbackUnsubscribe);
    PTI_VIEW_GET_SYMBOL(ptiCallbackEnableDomain);
    PTI_VIEW_GET_SYMBOL(ptiCallbackDisableDomain);
    PTI_VIEW_GET_SYMBOL(ptiCallbackDisableAllDomains);
    PTI_VIEW_GET_SYMBOL(ptiCallbackDomainTypeToString);
    PTI_VIEW_GET_SYMBOL(ptiCallbackPhaseTypeToString);

    PTI_VIEW_GET_SYMBOL(PtiSetXPTIEnvironmentDetails);

    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeEnable);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeConfigure);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeStartCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeStopCollection);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeDisable);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeQueryCollectionBufferSize);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeSetCollectionBufferSize);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeGetCollectionBuffersCount);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeGetCollectionBuffer);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeGetCollectionBufferProperties);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeCalculateMetrics);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeQueryMetricsBufferSize);
    PTI_VIEW_GET_SYMBOL(ptiMetricsScopeGetMetricsMetadata);

#undef PTI_VIEW_GET_SYMBOL
    CommunicateForeignXPTISubscriber();
  }
  std::unique_ptr<LibraryLoader> pti_view_lib_ = nullptr;
};
}  // namespace pti

#endif  // SRC_PTI_LIB_HANDLER_H_
