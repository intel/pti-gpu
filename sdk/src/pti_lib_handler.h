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

#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_metrics.h"
#include "pti/pti_metrics_scope.h"
#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"
#include "utils/library_loader.h"
#include "utils/platform_strings.h"
#include "utils/utils.h"
#include "xpti_adapter.h"

#if defined(PTI_CCL_ITT_COMPILE)
#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE

#include <ittnotify.h>
#include <ittnotify_config.h>

#include "itt/itt_adapter.h"
#include "itt/itt_metadata.h"

#endif  // PTI_CCL_ITT_COMPILE

namespace pti {

inline std::string GetPathToPtiModule() {
  auto pti_dir = ::utils::GetPathToSharedObject(GetPathToPtiModule);

  pti_dir = ::utils::GetFilePath(pti_dir);
  if (pti_dir.empty()) {
    return pti_dir;
  }

  pti_dir += pti::strings::kModuleSubdir;
  pti_dir += pti::strings::kOsPathSeparator;

  return pti_dir;
}

#if defined(__linux__)

// Upper bound on load attempts before giving up: a dependency chain costs one attempt
// per library in it, plus one per retry of the library that depends on it.
inline constexpr int kMaxDependencyLoadAttempts = 32;

// Extracts the soname the dynamic loader could not find, e.g. out of
// "libxptifw.so: cannot open shared object file: No such file or directory". Empty when
// the failure is not a missing dependency (undefined symbol, wrong ELF class) or when
// the loader names a path - neither leaves anything to pre-load.
inline std::string MissingSonameFromLoaderError(std::string_view loader_error) {
  constexpr std::string_view kNotFound = ": cannot open shared object file";

  const auto pos = loader_error.find(kNotFound);
  if (pos == std::string_view::npos) {
    SPDLOG_TRACE("Loader error names no missing dependency: {}", loader_error);
    return std::string{};
  }

  const auto soname = loader_error.substr(0, pos);
  if (soname.empty() || soname.find_first_of("/\\") != std::string_view::npos) {
    SPDLOG_TRACE("Loader error names a path rather than a soname, nothing to pre-load: {}",
                 loader_error);
    return std::string{};
  }

  SPDLOG_TRACE("Loader error names missing dependency {}", soname);
  return std::string{soname};
}

// Fallback directories for a dependency the loader will not resolve by soname: the PTI
// module directory, and its parent where the interface library and the runtime libraries
// shipped next to it live.
inline std::vector<utils::filesystem::path> DependencySearchDirs() {
  const auto module_dir = utils::filesystem::path{GetPathToPtiModule()}.parent_path();
  if (module_dir.empty()) {
    SPDLOG_TRACE("PTI module directory is unknown: no fallback search directories");
    return {};
  }

  SPDLOG_TRACE("Fallback dependency search directories: {} and {}", module_dir.string(),
               module_dir.parent_path().string());
  return {module_dir, module_dir.parent_path()};
}

// Pre-loads the given soname, so that a subsequent load can resolve it as a dependency.
// By soname first, letting the loader search as it normally would: that picks the same
// copy every other participant in the process would resolve. Then by full path from the
// fallback directories, for deployments no RUNPATH covers.
//
// A candidate that fails because it needs something else first reports that soname in
// next_missing, for the caller to satisfy before coming back to this one. Reported only
// once every candidate has been tried: another copy of the same soname need not depend
// on the same things.
//
// NOTE: a pre-load satisfies a NEEDED entry only if the file's SONAME equals the string
// we were given - the loader matches an already loaded object by name, not by path. That
// holds for the libraries this is used for, whose file names are their sonames; for a
// conventionally versioned library the pre-load would succeed and satisfy nothing.
// Harmless, as the caller stops retrying a soname it has pre-loaded.
inline std::unique_ptr<LibraryLoader> PreloadDependency(const std::string& soname,
                                                        std::string& next_missing) {
  next_missing.clear();

  SPDLOG_TRACE("Resolving dependency {}", soname);

  std::vector<std::string> candidates{soname};
  std::error_code error_code;
  for (const auto& dir : DependencySearchDirs()) {
    const auto candidate = dir / soname;
    if (utils::filesystem::exists(candidate, error_code)) {
      candidates.push_back(candidate.string());
    } else {
      SPDLOG_TRACE("No {} in {}", soname, dir.string());
    }
  }

  // Held rather than returned right away, so every candidate still gets its turn.
  std::string deeper_missing;

  for (const auto& candidate : candidates) {
    SPDLOG_TRACE("Trying {}", candidate);

    std::string error;
    auto library = LibraryLoader::Preload(candidate, error);
    if (library != nullptr) {
      if (candidate == soname) {
        SPDLOG_DEBUG("Pre-loaded dependency {} through the loader's search path", soname);
      } else {
        // Bypassing the loader's search binds this copy to the soname process-wide,
        // including for participants that would have resolved it somewhere else.
        SPDLOG_WARN("Pre-loaded dependency {} by full path; it now satisfies {} process-wide",
                    candidate, soname);
      }
      return library;
    }
    SPDLOG_DEBUG("Pre-load of {} failed: {}", candidate, error);

    auto missing = MissingSonameFromLoaderError(error);
    if (missing.empty() || missing == soname) {
      // Either not a missing dependency at all, or the loader could not find what we
      // asked for rather than something it needs first - reporting that would loop.
      continue;
    }

    // Keep the first report: it comes from the copy the loader itself resolved, the one
    // we would rather end up using.
    SPDLOG_DEBUG("Pre-load of {} needs {} first", candidate, missing);
    if (deeper_missing.empty()) {
      deeper_missing = std::move(missing);
    }
  }

  if (!deeper_missing.empty()) {
    SPDLOG_DEBUG("No copy of {} loaded; the one the loader resolves needs {} first", soname,
                 deeper_missing);
    next_missing = std::move(deeper_missing);
  } else {
    SPDLOG_DEBUG(
        "{} is reachable neither by soname nor from the PTI module directory or its parent",
        soname);
  }
  return nullptr;
}

inline void ReleasePreloadedLibraries(
    std::vector<std::unique_ptr<LibraryLoader>>& preloaded_libraries) {
  for (auto it = preloaded_libraries.rbegin(); it != preloaded_libraries.rend(); ++it) {
    it->reset();
  }
  preloaded_libraries.clear();
}

// Loads the PTI Core library, satisfying dependencies the dynamic loader cannot find by
// itself. In a Python wheel the Core library's RPATH is $ORIGIN only, leaving it unable
// to resolve dependencies that sit one directory up, next to the PTI interface library.
// The loader raises those before any Core library code runs, so they cannot be handled
// from there; they are pre-loaded from this side of the load instead.
//
// This has to stay on the interface library side of the load: dlopen searches with the
// RUNPATH of the calling object, and only the interface library's $ORIGIN points at the
// directory those libraries are deployed in.
//
// It also relies on pre-loading libxptifw.so not making XPTI dispatch to its
// subscribers. XPTI loads them on the first functional call into the framework, not from
// its library initializers. Were that to change, the pre-load would reach xptiTraceInit
// while we are still inside the PtiLibHandler constructor this is called from, with its
// forwarding pointers not yet resolved.
inline std::unique_ptr<LibraryLoader> LoadCoreLibrary(const std::string& core_lib_path) {
  std::string last_error;
  std::vector<std::string> pending;                 // sonames left to satisfy, deepest last
  std::unordered_set<std::string> preloaded_names;  // sonames already pre-loaded
  std::vector<std::unique_ptr<LibraryLoader>> preloaded_libraries;

  SPDLOG_TRACE("Loading PTI Core library {}", core_lib_path);

  int attempt = 0;
  for (; attempt < kMaxDependencyLoadAttempts; ++attempt) {
    if (pending.empty()) {
      try {
        auto core_lib = std::make_unique<LibraryLoader>(core_lib_path);
        [[maybe_unused]] const auto preloaded_count = preloaded_names.size();
        // Core now owns references through its DT_NEEDED entries.
        ReleasePreloadedLibraries(preloaded_libraries);
        SPDLOG_TRACE("Loaded {} after pre-loading {} dependencies", core_lib_path, preloaded_count);
        return core_lib;
      } catch (const std::exception& e) {
        last_error = e.what();
      }
      SPDLOG_TRACE("Load of {} failed: {}", core_lib_path, last_error);

      auto missing = MissingSonameFromLoaderError(last_error);
      if (missing.empty()) {
        SPDLOG_TRACE("Not a missing dependency: pre-loading cannot help, giving up");
        break;
      }
      if (preloaded_names.count(missing) != 0) {
        SPDLOG_TRACE(
            "{} is still reported missing after we pre-loaded it - its SONAME most likely differs "
            "from the name depended on; giving up",
            missing);
        break;
      }

      SPDLOG_TRACE("Queueing {} to be pre-loaded", missing);
      pending.push_back(std::move(missing));
    }

    std::string next_missing;
    auto preloaded_library = PreloadDependency(pending.back(), next_missing);
    if (preloaded_library != nullptr) {
      SPDLOG_TRACE("{} satisfied, {} left to satisfy", pending.back(), pending.size() - 1);
      preloaded_libraries.push_back(std::move(preloaded_library));
      preloaded_names.insert(std::move(pending.back()));
      pending.pop_back();
      continue;
    }

    // What we just tried needs something else first: satisfy that and come back to this
    // one - unless there is nothing to try, or it would send us in circles.
    if (next_missing.empty()) {
      SPDLOG_TRACE("Nothing further to try for {}: giving up", pending.back());
      break;
    }
    if (preloaded_names.count(next_missing) != 0) {
      SPDLOG_TRACE("{} wants {}, which we have already pre-loaded: giving up", pending.back(),
                   next_missing);
      break;
    }
    if (std::find(pending.begin(), pending.end(), next_missing) != pending.end()) {
      SPDLOG_TRACE("{} wants {}, which is already waiting on it: dependency cycle, giving up",
                   pending.back(), next_missing);
      break;
    }

    SPDLOG_TRACE("Queueing {} ahead of {}", next_missing, pending.back());
    pending.push_back(std::move(next_missing));
  }

  if (attempt == kMaxDependencyLoadAttempts) {
    SPDLOG_TRACE(
        "Stopped after {} attempts: the dependency chain is longer than we are willing "
        "to follow",
        attempt);
  }
  SPDLOG_TRACE("Giving up on {}, reporting: {}", core_lib_path, last_error);
  ReleasePreloadedLibraries(preloaded_libraries);

  throw std::runtime_error(last_error);
}
#else
inline std::unique_ptr<LibraryLoader> LoadCoreLibrary(const std::string& core_lib_path) {
  return std::make_unique<LibraryLoader>(core_lib_path);
}
#endif  // __linux__

// API implemented in PTI Core library
// to pass from PTI Interface library status of detected "foreign" XPTI subscribers,
// if such were detected at the library load time
void PtiSetXPTIEnvironmentDetails(bool is_foreign_subscriber, bool is_likely_unitrace_subscriber);
class PtiLibHandler {
#if defined(PTI_CCL_ITT_COMPILE)
  using IttCollectorSetCclGlobalAndDomainFn = void(ITTAPI*)(__itt_global*, const __itt_domain*);
#endif  // PTI_CCL_ITT_COMPILE

 public:
  static bool IsSuccessfullyInitialized() {
    return successfully_initialized_instance_.load(std::memory_order_acquire) != nullptr;
  }

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
  decltype(&ptiMetricsGetCalculatedData) ptiMetricsGetCalculatedData_ = nullptr;          // NOLINT
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

  decltype(&ptiPcSamplingEnable) ptiPcSamplingEnable_ = nullptr;        // NOLINT
  decltype(&ptiPcSamplingConfigure) ptiPcSamplingConfigure_ = nullptr;  // NOLINT
  decltype(&ptiPcSamplingQueryCollectionBufferSize) ptiPcSamplingQueryCollectionBufferSize_ =
      nullptr;  // NOLINT
  decltype(&ptiPcSamplingSetCollectionBufferSize) ptiPcSamplingSetCollectionBufferSize_ =
      nullptr;                                                                            // NOLINT
  decltype(&ptiPcSamplingStartCollection) ptiPcSamplingStartCollection_ = nullptr;        // NOLINT
  decltype(&ptiPcSamplingStopCollection) ptiPcSamplingStopCollection_ = nullptr;          // NOLINT
  decltype(&ptiPcSamplingGetStallReasons) ptiPcSamplingGetStallReasons_ = nullptr;        // NOLINT
  decltype(&ptiPcSamplingGetProfiledDevices) ptiPcSamplingGetProfiledDevices_ = nullptr;  // NOLINT
  decltype(&ptiPcSamplingGetObservedKernelHandles) ptiPcSamplingGetObservedKernelHandles_ =
      nullptr;  // NOLINT
  decltype(&ptiPcSamplingGetObservedKernelInfo) ptiPcSamplingGetObservedKernelInfo_ =
      nullptr;  // NOLINT
  decltype(&ptiPcSamplingGetSamplesPerInstruction) ptiPcSamplingGetSamplesPerInstruction_ =
      nullptr;                                                                      // NOLINT
  decltype(&ptiPcSamplingGetDeviceStatus) ptiPcSamplingGetDeviceStatus_ = nullptr;  // NOLINT
  decltype(&ptiPcSamplingDisable) ptiPcSamplingDisable_ = nullptr;                  // NOLINT

#if defined(PTI_CCL_ITT_COMPILE)
  decltype(&__itt_task_begin) itt_task_begin_ = nullptr;                             // NOLINT
  decltype(&__itt_task_end) itt_task_end_ = nullptr;                                 // NOLINT
  decltype(&__itt_metadata_add) itt_metadata_add_ = nullptr;                         // NOLINT
  IttCollectorSetCclGlobalAndDomainFn IttCollectorSetCclGlobalAndDomain_ = nullptr;  // NOLINT
#endif  // PTI_CCL_ITT_COMPILE

  // Forward to implementation in core library
  decltype(&xptiQuerySubscriberStreamDetailLevel) xptiQuerySubscriberStreamDetailLevel_ =  // NOLINT
      nullptr;                                                                             // NOLINT

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

      auto pti_dir = GetPathToPtiModule();
      pti_dir += strings::kPtiViewLib;
      pti_view_lib_ = LoadCoreLibrary(pti_dir);
    } catch (const std::exception& e) {
      SPDLOG_ERROR("Unable to load {} because {}", strings::kPtiViewLib, e.what());
      pti_view_lib_ = nullptr;
      // Do NOT mark as successfully initialized on failure - ITT calls will safely no-op
      return;
    }
#define PTI_VIEW_GET_SYMBOL(X) X##_ = pti_view_lib_->GetSymbol<decltype(&X)>(#X)  // NOLINT
    PTI_VIEW_GET_SYMBOL(xptiTraceInit);
    PTI_VIEW_GET_SYMBOL(xptiTraceFinish);
    PTI_VIEW_GET_SYMBOL(xptiQuerySubscriberStreamDetailLevel);
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
    PTI_VIEW_GET_SYMBOL(ptiMetricsGetCalculatedData);

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

    PTI_VIEW_GET_SYMBOL(ptiPcSamplingEnable);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingConfigure);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingQueryCollectionBufferSize);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingSetCollectionBufferSize);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingStartCollection);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingStopCollection);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetStallReasons);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetProfiledDevices);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetObservedKernelHandles);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetObservedKernelInfo);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetSamplesPerInstruction);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingGetDeviceStatus);
    PTI_VIEW_GET_SYMBOL(ptiPcSamplingDisable);

#if defined(PTI_CCL_ITT_COMPILE)
#define PTI_VIEW_GET_ITT_SYMBOL(X) \
  X##_ = pti_view_lib_->GetSymbol<decltype(&__##X)>(#X "_collector")
    PTI_VIEW_GET_ITT_SYMBOL(itt_task_begin);
    PTI_VIEW_GET_ITT_SYMBOL(itt_task_end);
    PTI_VIEW_GET_ITT_SYMBOL(itt_metadata_add);

    IttCollectorSetCclGlobalAndDomain_ =
        pti_view_lib_->GetSymbol<IttCollectorSetCclGlobalAndDomainFn>(
            "IttCollectorSetCclGlobalAndDomain");

    SPDLOG_DEBUG("{}() IttCollectorSetCclGlobalAndDomain_: {}", __FUNCTION__,
                 IttCollectorSetCclGlobalAndDomain_ != nullptr ? "available" : "not available");
    // If __itt_api_init fired before PtiLibHandler was constructed,
    // seed the collector's ITT Global that CCL domain belongs to and CCL domain now
    // Both could be nulls, but in that case it is not a problem,
    // as the collector will be able to set them later when they become available
    if (IttCollectorSetCclGlobalAndDomain_ != nullptr) {
      auto* g = GetIttGlobalOfCclDomainAdapter();
      auto* domain = GetIttCclDomainAdapter();
      SPDLOG_DEBUG("{}() IttGlobalCollector: {}, CCL domain: {}", __FUNCTION__,
                   static_cast<void*>(g), static_cast<const void*>(domain));
      IttCollectorSetCclGlobalAndDomain_(g, domain);
    }

#undef PTI_VIEW_GET_ITT_SYMBOL
#endif  // PTI_CCL_ITT_COMPILE

#undef PTI_VIEW_GET_SYMBOL
    CommunicateForeignXPTISubscriber();

    // Mark as fully initialized at the very end
    successfully_initialized_instance_.store(this, std::memory_order_release);
  }

  static inline std::atomic<PtiLibHandler*> successfully_initialized_instance_{nullptr};
  std::unique_ptr<LibraryLoader> pti_view_lib_ = nullptr;
};
}  // namespace pti

#endif  // SRC_PTI_LIB_HANDLER_H_
