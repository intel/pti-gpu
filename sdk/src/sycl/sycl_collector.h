//
// TODO(juf): need to put our own license here
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
#ifndef PTI_TOOLS_SYCL_COLLECTOR_H_
#define PTI_TOOLS_SYCL_COLLECTOR_H_
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <xpti/xpti_trace_framework.hpp>

#include "library_loader.h"
#include "platform_strings.h"
#include "pti_api_ids_state_maps.h"
#include "unikernel.h"
#include "utils.h"

inline static constexpr std::string_view kStashedSymbolName = "xptiGetStashedTuple";
inline static constexpr std::string_view kUnknownFunctionName = "<unknown>";

using StashedFuncPtr = decltype(&xptiGetStashedTuple);

inline constexpr auto kMaxFuncNameLen = static_cast<std::size_t>(2048);
static_assert(
    kUnknownFunctionName.size() < kMaxFuncNameLen,
    "Placeholder function name size must be less than the size of the max function name length");
inline constexpr uint64_t kDefaultQueueId = PTI_INVALID_QUEUE_ID;

using OnSyclRuntimeViewCallback = void (*)(void* data, ZeKernelCommandExecutionRecord& kcexec);

enum class SyclImpl { kPi, kUr };

inline thread_local std::map<uint64_t, uint64_t> node_q_map = {};
struct SyclUrFuncT {
  std::array<char, kMaxFuncNameLen> func_name;
  uint32_t func_pid;
  uint32_t func_tid;
};

inline thread_local bool framework_finalized = false;

inline constexpr static std::array<const char* const, 13> kSTraceType = {
    "TaskBegin",           "TaskEnd",     "Signal",    "NodeCreate", "FunctionWithArgsBegin",
    "FunctionWithArgsEnd", "Metadata",    "WaitBegin", "WaitEnd",    "FunctionBegin",
    "FunctionEnd",         "QueueCreate", "Other"};

enum class ApiType { kInvalid = 0, kKernel = 1, kMemory = 2 };

inline static const std::unordered_map<pti_api_id_runtime_sycl, ApiType> kCoreApis = {
    {pti_api_id_runtime_sycl::urEnqueueUSMFill_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueUSMFill2D_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueUSMMemcpy_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueUSMMemcpy2D_id, ApiType::kMemory},

    {pti_api_id_runtime_sycl::urEnqueueKernelLaunch_id, ApiType::kKernel},
    {pti_api_id_runtime_sycl::urEnqueueKernelLaunchCustomExp_id, ApiType::kKernel},
    {pti_api_id_runtime_sycl::urEnqueueCooperativeKernelLaunchExp_id, ApiType::kKernel},
    {pti_api_id_runtime_sycl::urEnqueueKernelLaunchWithArgsExp_id, ApiType::kKernel},

    {pti_api_id_runtime_sycl::urEnqueueMemBufferFill_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueMemBufferRead_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueMemBufferWrite_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urEnqueueMemBufferCopy_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urUSMHostAlloc_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urUSMSharedAlloc_id, ApiType::kMemory},
    {pti_api_id_runtime_sycl::urUSMDeviceAlloc_id, ApiType::kMemory}};

inline const char* GetTracePointTypeString(xpti::trace_point_type_t trace_type) {
  switch (trace_type) {
    case xpti::trace_point_type_t::task_begin:
      return kSTraceType[0];
    case xpti::trace_point_type_t::task_end:
      return kSTraceType[1];
    case xpti::trace_point_type_t::signal:
      return kSTraceType[2];
    case xpti::trace_point_type_t::node_create:
      return kSTraceType[3];
    case xpti::trace_point_type_t::function_with_args_begin:
      return kSTraceType[4];
    case xpti::trace_point_type_t::function_with_args_end:
      return kSTraceType[5];
    case xpti::trace_point_type_t::metadata:
      return kSTraceType[6];
    case xpti::trace_point_type_t::wait_begin:
      return kSTraceType[7];
    case xpti::trace_point_type_t::wait_end:
      return kSTraceType[8];
    case xpti::trace_point_type_t::function_begin:
      return kSTraceType[9];
    case xpti::trace_point_type_t::function_end:
      return kSTraceType[10];
    case xpti::trace_point_type_t::queue_create:
      return kSTraceType[11];
    default:
      return kSTraceType[12];
  }
}

inline std::string Truncate(const std::string& name) {
  size_t pos = name.find_last_of(":");
  if (pos != std::string::npos) {
    return name.substr(pos + 1);
  }
  return name;
}

inline ApiType GetApiType(const pti_api_id_runtime_sycl api_id) noexcept {
  const auto it = kCoreApis.find(api_id);
  return (it != kCoreApis.end()) ? it->second : ApiType::kInvalid;
}

inline bool InKernelCoreApis(const pti_api_id_runtime_sycl api_id) noexcept {
  return GetApiType(api_id) == ApiType::kKernel;
}

inline bool InMemoryCoreApis(const pti_api_id_runtime_sycl api_id) noexcept {
  return GetApiType(api_id) == ApiType::kMemory;
}

// Metadata key classification enum and cache
enum class MetadataKeyType : uint8_t { kUnknown = 0, kKernel = 1, kMemory = 2 };
inline thread_local std::unordered_map<xpti::string_id_t, MetadataKeyType> metadata_key_type_cache;

inline MetadataKeyType ClassifyMetadataKey(xpti::string_id_t xpti_metadata_key) {
  auto it = metadata_key_type_cache.find(xpti_metadata_key);
  if (it != metadata_key_type_cache.end()) {
    return it->second;
  }

  const auto* const xpti_metadata_name = xptiLookupString(xpti_metadata_key);
  MetadataKeyType type = MetadataKeyType::kUnknown;

  if (std::strcmp(xpti_metadata_name, "kernel_name") == 0) {
    type = MetadataKeyType::kKernel;
  } else if (std::strstr(xpti_metadata_name, "memory") != nullptr) {
    type = MetadataKeyType::kMemory;
  }

  metadata_key_type_cache[xpti_metadata_key] = type;
  return type;
}

inline bool IsMemoryOperation(xpti::string_id_t xpti_metadata_key) {
  return ClassifyMetadataKey(xpti_metadata_key) == MetadataKeyType::kMemory;
}

inline bool IsKernelOperation(xpti::string_id_t xpti_metadata_key) {
  return ClassifyMetadataKey(xpti_metadata_key) == MetadataKeyType::kKernel;
}

class SyclCollector {
 public:
  // Variables to indicate the presence of a foreign subscriber to XPTI.
  // To be set via call to ptiSetXPTIEnvironmentDetails before SyclCollector created
  static inline bool foreign_subscriber_ = false;
  static inline bool likely_unitrace_subscriber_ = false;

  static auto& Instance() {
    static SyclCollector sycl_collector{nullptr};
    return sycl_collector;
  }

  void EnableTracing() {
    // Do not change the behaviour here depending on foreign_subscriber_!
    // The current behavior ensures that in the absense of XPTI subscription (and Sycl records)
    // PTI generates to-called Special Records
    // About Special Records: see comments in unitrace.h
    enabled_ = true;
    xptiForceSetTraceEnabled(enabled_);
  }

  void DisableTracing() {
    enabled_ = false;
    if (streams_found_) {
      // Don't allow the collector to be disabled unless all the xpti streams we require are found.
      xptiForceSetTraceEnabled(enabled_);
    }
  }

  bool Enabled() const { return enabled_; }

  void StreamsInitialized() { streams_found_ = true; }

  //  For compiler versions < 2024.1.1. Manually load xptiGetStashedTuple.
  inline static StashedFuncPtr GetStashedFuncPtrFromSharedObject() {
    StashedFuncPtr xptiGetStashedTuple = nullptr;  // NOLINT
    try {
      auto xpti_lib = LibraryLoader{pti::strings::kXptiLibName};
      xptiGetStashedTuple = xpti_lib.GetSymbol<StashedFuncPtr>(kStashedSymbolName.data());
    } catch ([[maybe_unused]] const std::runtime_error& e) {
      xptiGetStashedTuple = nullptr;
      SPDLOG_ERROR("exception caught while trying to get {}: {}", kStashedSymbolName, e.what());
    } catch (...) {
      xptiGetStashedTuple = nullptr;
      SPDLOG_ERROR("unknown exception caught while trying to get {}", kStashedSymbolName);
    }
    return xptiGetStashedTuple;
  }

  void SetCallback(const OnSyclRuntimeViewCallback callback) { acallback_ = callback; }

  static XPTI_CALLBACK_API void TpCallback(uint16_t TraceType, xpti::trace_event_data_t* /*Parent*/,
                                           xpti::trace_event_data_t* Event, uint64_t /*instance*/,
                                           const void* UserData) {
    const auto trace_type = static_cast<xpti::trace_point_type_t>(TraceType);

    switch (trace_type) {
      case xpti::trace_point_type_t::function_with_args_begin:
        HandleFunctionWithArgsBegin(UserData);
        break;
      case xpti::trace_point_type_t::function_with_args_end:
        HandleFunctionWithArgsEnd(UserData);
        break;
      case xpti::trace_point_type_t::task_begin:
        HandleTaskBegin(Event);
        break;
      case xpti::trace_point_type_t::node_create:
        HandleNodeCreate(Event);
        break;
      case xpti::trace_point_type_t::task_end:
      case xpti::trace_point_type_t::queue_create:
      default:
        break;
    }
  }

 private:
  static void HandleFunctionWithArgsBegin(const void* UserData) {
    sycl_data_kview.cid_ = UniCorrId::GetUniCorrId();
    sycl_data_mview.cid_ = sycl_data_kview.cid_;
    SyclCollector::Instance().sycl_runtime_rec_.cid_ = sycl_data_kview.cid_;

    if (!UserData) return;

    const auto* args = static_cast<const xpti::function_with_args_t*>(UserData);
    const auto api_id = static_cast<pti_api_id_runtime_sycl>(args->function_id);

    auto& runtime_rec = SyclCollector::Instance().sycl_runtime_rec_;
    runtime_rec.pid_ = thread_local_pid_tid_info.pid;
    runtime_rec.tid_ = thread_local_pid_tid_info.tid;
    runtime_rec.callback_id_ = args->function_id;

    const ApiType api_type = GetApiType(api_id);

    runtime_rec.start_time_ = utils::GetTime();
    if (api_type == ApiType::kKernel) {
      sycl_data_kview.sycl_enqk_begin_time_ = runtime_rec.start_time_;
    } else if (api_type == ApiType::kMemory) {
      sycl_data_mview.sycl_task_begin_time_ = runtime_rec.start_time_;
    }
  }

  static void HandleFunctionWithArgsEnd(const void* UserData) {
    if (!UserData) return;

    const auto* args = static_cast<const xpti::function_with_args_t*>(UserData);
    const auto api_id = static_cast<pti_api_id_runtime_sycl>(args->function_id);
    auto time = utils::GetTime();

    const ApiType api_type = GetApiType(api_id);
    auto& runtime_rec = SyclCollector::Instance().sycl_runtime_rec_;

    if (api_type == ApiType::kKernel) {
      runtime_rec.kid_ = sycl_data_kview.kid_;
    } else if (api_type == ApiType::kMemory) {
      runtime_rec.kid_ = sycl_data_mview.kid_;
      runtime_rec.tid_ = sycl_data_mview.tid_;
    }

    runtime_rec.end_time_ = time;

    // Check callback and invoke if needed
    auto& instance = SyclCollector::Instance();
    if (instance.acallback_ != nullptr) {
      const std::lock_guard<std::mutex> lock(sycl_set_granularity_map_mtx);
      const uint32_t id_enabled = pti_api_id_runtime_sycl_state[args->function_id];
      const int32_t trace_all = instance.trace_all_env_value_;

      if ((trace_all > 0) || ((trace_all < 0) && id_enabled)) {
        if (instance.enabled_ && !framework_finalized) {
          (instance.acallback_.load())(nullptr, runtime_rec);
        }
      }

      // Reset state
      runtime_rec.kid_ = 0;
      sycl_data_kview.kid_ = 0;
      sycl_data_kview.tid_ = 0;
      sycl_data_kview.cid_ = 0;
      sycl_data_mview.kid_ = 0;
      sycl_data_mview.tid_ = 0;
      sycl_data_mview.cid_ = 0;
    }
  }

  static void HandleTaskBegin(xpti::trace_event_data_t* Event) {
    if (!Event) return;

    const uint64_t ID = Event->unique_id;
    const uint64_t Instance_ID = Event->instance_id;
    const auto* metadata = xptiQueryMetadata(Event);
    auto time = utils::GetTime();

    for (const auto& item : *metadata) {
      if (IsKernelOperation(item.first)) {
        const auto* payload = xptiQueryPayload(Event);
        if (payload) {
          if (payload->source_file) {
            sycl_data_kview.source_file_name_ = std::string{payload->source_file};
          }
          sycl_data_kview.source_line_number_ = payload->line_no;
        }

        sycl_data_kview.sycl_node_id_ = ID;
        const auto queue_id_loc = node_q_map.find(ID);
        sycl_data_kview.sycl_queue_id_ =
            (queue_id_loc != std::end(node_q_map)) ? queue_id_loc->second : kDefaultQueueId;
        sycl_data_kview.sycl_invocation_id_ = static_cast<uint32_t>(Instance_ID);
        sycl_data_kview.sycl_task_begin_time_ = time;
        return;
      }

      if (IsMemoryOperation(item.first)) {
        const auto queue_id_loc = node_q_map.find(ID);
        sycl_data_mview.sycl_queue_id_ =
            (queue_id_loc != std::end(node_q_map)) ? queue_id_loc->second : kDefaultQueueId;
        sycl_data_mview.sycl_task_begin_time_ = time;
        return;
      }
    }
  }

  static void HandleNodeCreate(xpti::trace_event_data_t* Event) {
    if (!Event) return;

    const uint64_t ID = Event->unique_id;
    char* stashed_key = nullptr;
    uint64_t stashed_value = 0;

    auto& instance = SyclCollector::Instance();
    if (instance.xptiGetStashedKV_ && (instance.xptiGetStashedKV_(&stashed_key, stashed_value) ==
                                       xpti::result_t::XPTI_RESULT_SUCCESS)) {
      if (std::strcmp(stashed_key, "queue_id") == 0) {
        node_q_map[ID] = stashed_value;
      }
    } else {
      node_q_map[ID] = kDefaultQueueId;
    }

    const uint64_t queue_id = node_q_map[ID];
    const auto* metadata = xptiQueryMetadata(Event);

    for (const auto& item : *metadata) {
      if (IsKernelOperation(item.first)) {
        sycl_data_kview.sycl_queue_id_ = queue_id;
        return;
      }
      if (IsMemoryOperation(item.first)) {
        sycl_data_mview.sycl_queue_id_ = queue_id;
        return;
      }
    }
  }

 private:
  static constexpr const char* const kWarnForeignSubscriber =
      "Another subscriber already subscribed to Sycl runtime events, "
      "so PTI will not subscribe to them. It will affect correctness of PTI profile: "
      "e.g. report zero XPU time for CPU callers of GPU kernels.";

  static constexpr const char* const kWarnLikelyUnitraceSubscriber =
      " Likely the application running under Unitrace. "
      "To get correct PTI profile - run without Unitrace.";

  explicit SyclCollector(OnSyclRuntimeViewCallback buffer_callback)
      : acallback_(buffer_callback), xptiGetStashedKV_(GetStashedFuncPtrFromSharedObject()) {
    if (foreign_subscriber_) {
      SPDLOG_WARN("{}{}", kWarnForeignSubscriber,
                  likely_unitrace_subscriber_ ? kWarnLikelyUnitraceSubscriber : "");
    }
  }

  int32_t trace_all_env_value_ = utils::IsSetEnv("PTI_VIEW_RUNTIME_API");
  inline static thread_local ZeKernelCommandExecutionRecord sycl_runtime_rec_;
  std::atomic<OnSyclRuntimeViewCallback> acallback_ = nullptr;
  std::atomic<bool> enabled_ = false;
  std::atomic<bool> streams_found_ = false;
  StashedFuncPtr xptiGetStashedKV_ = nullptr;
};

class XptiStreamRegistrationHandler {
 public:
  // We register two streams. One for "sycl" and another for the sycl implementation (e.g. plugin
  // interface or unified runtime)
  constexpr static size_t kNumberOfStreams = 2;

  static auto& Instance() {
    static XptiStreamRegistrationHandler handler{};
    return handler;
  }

  bool RegistrationComplete() const { return stream_count_ >= kNumberOfStreams; }

  void InitializeStream(unsigned int /*major_version*/, unsigned int /*minor_version*/,
                        [[maybe_unused]] const char* version_str, const char* stream_name) {
    SPDLOG_TRACE("XPTI Stream: Found --->: {} v{}", stream_name, version_str);
    if (std::strcmp(stream_name, "sycl") == 0) {  // SYCL Stream
      stream_id_ = xptiRegisterStream(stream_name);
      RegisterSyclCallbacks(stream_id_);
      SPDLOG_DEBUG("Registered callbacks for {}", stream_name);
      ++stream_count_;
    } else if (std::strcmp(stream_name, "ur.call") == 0 ||
               std::strcmp(stream_name, "ur") == 0) {  // UNIFIED
                                                       // RUNTIME
      stream_id_ = xptiRegisterStream(stream_name);
      RegisterImplCallbacks(stream_id_);
      SPDLOG_DEBUG("Registering callbacks for {}", stream_name);
      ++stream_count_;
    } else {
      SPDLOG_DEBUG("XPTI Stream: {} v{} no callbacks registered!", stream_name, version_str);
    }
    CheckAndDisableCollectorIfNotEnabled();
  }

 private:
  // Helper to register a single callback and check result
  static void RegisterCallback(uint8_t stream_id, xpti::trace_point_type_t trace_point) {
    const auto result = xptiRegisterCallback(stream_id, static_cast<uint16_t>(trace_point),
                                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("For XPTI Stream ID: {} Callback Registration for trace point {} returned: {}",
                   static_cast<int32_t>(stream_id), static_cast<int32_t>(trace_point),
                   static_cast<int32_t>(result));
    }
  }

  static void RegisterSyclCallbacks(uint8_t stream_id) {
    // Register callback to predefined trace point types
    constexpr std::array<xpti::trace_point_type_t, 3> trace_points = {
        xpti::trace_point_type_t::node_create, xpti::trace_point_type_t::task_begin,
        xpti::trace_point_type_t::task_end};

    for (const auto trace_point : trace_points) {
      RegisterCallback(stream_id, trace_point);
    }
  }

  static void RegisterImplCallbacks(uint8_t stream_id) {
    RegisterCallback(stream_id, xpti::trace_point_type_t::function_with_args_begin);
    RegisterCallback(stream_id, xpti::trace_point_type_t::function_with_args_end);
    RegisterCallback(stream_id, xpti::trace_point_type_t::metadata);
  }

  //  Until the user calls EnableTracing(), disable tracing when we are
  //  able to capture the sycl runtime streams sycl and sycl.pi
  //  Empirically, we found the sycl.pi stream gets emitted after the sycl
  //  stream.
  void CheckAndDisableCollectorIfNotEnabled() const {
    if (RegistrationComplete()) {
      SyclCollector::Instance().StreamsInitialized();
      if (!SyclCollector::Instance().Enabled()) {
        SyclCollector::Instance().DisableTracing();
        // To remove effects of first callabacks that Sycl collector gets
        // until all streams registered
        // Zeroing cid_ -s is particularly important for correlation_id fields correctness
        sycl_data_kview.kid_ = 0;
        sycl_data_kview.tid_ = 0;
        sycl_data_kview.cid_ = 0;
        sycl_data_mview.kid_ = 0;
        sycl_data_mview.tid_ = 0;
        sycl_data_mview.cid_ = 0;
      }
    }
  }

  uint8_t stream_id_ = 0;
  std::atomic<size_t> stream_count_ = 0;
};

XPTI_CALLBACK_API void xptiTraceInit(unsigned int major_version, unsigned int minor_version,
                                     const char* version_str, const char* stream_name) {
  XptiStreamRegistrationHandler::Instance().InitializeStream(major_version, minor_version,
                                                             version_str, stream_name);
}

XPTI_CALLBACK_API void xptiTraceFinish(const char* /*stream_name*/) {}

// clang-format off
extern "C" {
  void
#if (defined(_WIN32) || defined(_WIN64))
  __declspec(dllexport)
#else
  __attribute__((visibility("default")))
#endif
  PtiSetXPTIEnvironmentDetails(bool is_foreign_subscriber,
                               bool is_likely_unitrace_subscriber) {
    SyclCollector::foreign_subscriber_ = is_foreign_subscriber;
    SyclCollector::likely_unitrace_subscriber_ = is_likely_unitrace_subscriber;
  }
}
// clang-format on

#if (defined(_WIN32) || defined(_WIN64))

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD fwdReason, LPVOID /*lpvReserved*/) {
  switch (fwdReason) {
    case DLL_PROCESS_ATTACH: {
      break;
    }
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      framework_finalized = true;
      break;
    case DLL_PROCESS_DETACH:
      break;
  }

  return TRUE;
}

#else  // Linux (possibly macOS?)

__attribute__((destructor)) static void framework_fini() { framework_finalized = true; }

#endif
#endif
