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
inline thread_local SyclUrFuncT current_func_task_info;

inline constexpr static std::array<const char* const, 13> kSTraceType = {
    "TaskBegin",           "TaskEnd",     "Signal",    "NodeCreate", "FunctionWithArgsBegin",
    "FunctionWithArgsEnd", "Metadata",    "WaitBegin", "WaitEnd",    "FunctionBegin",
    "FunctionEnd",         "QueueCreate", "Other"};

enum class ApiType { kInvalid = 0, kKernel = 1, kMemory = 2 };

inline static const std::unordered_map<std::string, ApiType> kCoreApis = {
    {"piextUSMEnqueueFill", ApiType::kMemory},
    {"piextUSMEnqueueFill2D", ApiType::kMemory},
    {"piextUSMEnqueueMemcpy", ApiType::kMemory},
    {"piextUSMEnqueueMemset", ApiType::kMemory},
    {"piextUSMEnqueueMemcpy2D", ApiType::kMemory},
    {"piextUSMEnqueueMemset2D", ApiType::kMemory},

    {"piEnqueueKernelLaunch", ApiType::kKernel},
    {"piextEnqueueKernelLaunchCustom", ApiType::kKernel},
    {"piextEnqueueCooperativeKernelLaunch", ApiType::kKernel},

    {"piEnqueueMemBufferRead", ApiType::kMemory},
    {"piEnqueueMemBufferWrite", ApiType::kMemory},
    {"piextUSMSharedAlloc", ApiType::kMemory},
    {"piextUSMHostAlloc", ApiType::kMemory},
    {"piextUSMDeviceAlloc", ApiType::kMemory},

    {"urEnqueueUSMFill", ApiType::kMemory},
    {"urEnqueueUSMFill2D", ApiType::kMemory},
    {"urEnqueueUSMMemcpy", ApiType::kMemory},
    {"urEnqueueUSMMemcpy2D", ApiType::kMemory},

    {"urEnqueueKernelLaunch", ApiType::kKernel},
    {"urEnqueueKernelLaunchCustomExp", ApiType::kKernel},
    {"urEnqueueCooperativeKernelLaunchExp", ApiType::kKernel},

    {"urEnqueueMemBufferFill", ApiType::kMemory},
    {"urEnqueueMemBufferRead", ApiType::kMemory},
    {"urEnqueueMemBufferWrite", ApiType::kMemory},
    {"urEnqueueMemBufferCopy", ApiType::kMemory},
    {"urUSMHostAlloc", ApiType::kMemory},
    {"urUSMSharedAlloc", ApiType::kMemory},
    {"urUSMDeviceAlloc", ApiType::kMemory}};

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

inline bool InKernelCoreApis(const char* function_name) {
  const auto it = kCoreApis.find(function_name);
  if (it != kCoreApis.end()) {
    return it->second == ApiType::kKernel;
  }
  return false;
}

inline bool InMemoryCoreApis(const char* function_name) {
  const auto it = kCoreApis.find(function_name);
  if (it != kCoreApis.end()) {
    return it->second == ApiType::kMemory;
  }
  return false;
}

inline bool IsMemoryOperation(xpti::string_id_t xpti_metadata_key) {
  const auto* const xpti_metadata_name = xptiLookupString(xpti_metadata_key);

  constexpr const char* const kMemorySubStr = "memory";

  return std::strstr(xpti_metadata_name, kMemorySubStr) != nullptr;
}

inline bool IsKernelOperation(xpti::string_id_t xpti_metadata_key) {
  const auto* const xpti_metadata_name = xptiLookupString(xpti_metadata_key);

  return std::strcmp(xpti_metadata_name, "kernel_name") == 0;
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
    const uint64_t time = utils::GetTime();
    const auto* payload = xptiQueryPayload(Event);

    uint64_t ID = Event ? Event->unique_id : 0;
    uint64_t Instance_ID = Event ? Event->instance_id : 0;
    uint32_t pid = thread_local_pid_tid_info.pid;
    uint32_t tid = thread_local_pid_tid_info.tid;

    const auto trace_type = static_cast<xpti::trace_point_type_t>(TraceType);

    SPDLOG_TRACE("{}: TraceType: {} - id: {}", time, GetTracePointTypeString(trace_type),
                 TraceType);
    SPDLOG_TRACE(" Event_id: {}, Instance_id: {}, pid: {}, tid: {}", ID, Instance_ID, pid, tid);

    switch (trace_type) {
      case xpti::trace_point_type_t::function_with_args_begin:
        // case xpti::trace_point_type_t::function_begin:
        sycl_data_kview.cid_ = UniCorrId::GetUniCorrId();
        sycl_data_mview.cid_ = sycl_data_kview.cid_;
        SyclCollector::Instance().sycl_runtime_rec_.cid_ = sycl_data_kview.cid_;

        if (UserData) {
          const auto* args = static_cast<const xpti::function_with_args_t*>(UserData);

          // const auto* function_name = static_cast<const char*>(UserData);
          const auto* function_name = args->function_name;
          SPDLOG_TRACE("\tSYCL.UR Function Begin: {}, corr_id: {}", function_name,
                       sycl_data_kview.cid_);
          // TODO: Re-evaluate whether this is actually needed. I do not see what we are doing with
          // current_func_task_info.func_name.
          auto function_name_size = std::strlen(function_name) + 1;  // plus '\0'
          if (function_name_size < current_func_task_info.func_name.size()) {
            std::copy_n(function_name, function_name_size,
                        current_func_task_info.func_name.begin());
          } else {
            std::copy_n(kUnknownFunctionName.begin(), kUnknownFunctionName.size(),
                        current_func_task_info.func_name.begin());
            current_func_task_info.func_name[kUnknownFunctionName.size()] = '\0';
          }
          current_func_task_info.func_pid = pid;
          current_func_task_info.func_tid = tid;
          if (InKernelCoreApis(function_name)) {
            sycl_data_kview.sycl_enqk_begin_time_ = time;
          }
          if (InMemoryCoreApis(function_name)) {
            sycl_data_mview.sycl_task_begin_time_ = time;
          }
          SyclCollector::Instance().sycl_runtime_rec_.pid_ = pid;
          SyclCollector::Instance().sycl_runtime_rec_.tid_ = tid;
          SyclCollector::Instance().sycl_runtime_rec_.start_time_ = time;
          SyclCollector::Instance().sycl_runtime_rec_.sycl_func_name_ = function_name;
          SyclCollector::Instance().sycl_runtime_rec_.callback_id_ = args->function_id;
        }
        break;
      case xpti::trace_point_type_t::function_with_args_end:
        // case xpti::trace_point_type_t::function_end:
        if (UserData) {
          [[maybe_unused]] const char* api_name = nullptr;
          const auto* args = static_cast<const xpti::function_with_args_t*>(UserData);
          // const auto* function_name = static_cast<const char*>(UserData);
          const auto* function_name = args->function_name;
          SPDLOG_TRACE("\tSYCL.UR Function End: {}, corr_id: {}", function_name,
                       sycl_data_kview.cid_);
          PTI_ASSERT(std::strcmp(current_func_task_info.func_name.data(), function_name) == 0);
          // Following asserts check that the function_id arg matches the one in ur_api.h.
          //  These asserts only appear in debug builds and are filtered out for release builds.
          //  So no overhead in release builds.
          assert(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, args->function_id,
                                     &api_name) == pti_result::PTI_SUCCESS);
          assert(std::strcmp(api_name, function_name) == 0);
          PTI_ASSERT(current_func_task_info.func_pid == pid);
          PTI_ASSERT(current_func_task_info.func_tid == tid);
          SPDLOG_TRACE("\tVerified: func: {} - Pid: {} - Tid: {}",
                       current_func_task_info.func_name.data(), current_func_task_info.func_pid,
                       current_func_task_info.func_tid);
          if (InKernelCoreApis(function_name)) {
            SyclCollector::Instance().sycl_runtime_rec_.kid_ = sycl_data_kview.kid_;
          }
          if (InMemoryCoreApis(function_name)) {
            SyclCollector::Instance().sycl_runtime_rec_.kid_ = sycl_data_mview.kid_;
            SyclCollector::Instance().sycl_runtime_rec_.tid_ = sycl_data_mview.tid_;
          }
          SyclCollector::Instance().sycl_runtime_rec_.end_time_ = time;
          if (SyclCollector::Instance().acallback_ != nullptr) {
            {
              const std::lock_guard<std::mutex> lock(sycl_set_granularity_map_mtx);
              uint32_t id_enabled = 1;  // by default state maps are 1
              id_enabled = pti_api_id_runtime_sycl_state[args->function_id];

              int32_t trace_all = SyclCollector::Instance().trace_all_env_value_;
              if ((trace_all > 0) || ((trace_all < 0) && id_enabled)) {
                if (SyclCollector::Instance().enabled_ && !framework_finalized) {
                  (SyclCollector::Instance().acallback_.load())(
                      nullptr, SyclCollector::Instance().sycl_runtime_rec_);
                }
              }
            }
            SyclCollector::Instance().sycl_runtime_rec_.kid_ = 0;
            sycl_data_kview.kid_ = 0;
            sycl_data_kview.tid_ = 0;
            sycl_data_kview.cid_ = 0;
            sycl_data_mview.kid_ = 0;
            sycl_data_mview.tid_ = 0;
            sycl_data_mview.cid_ = 0;
          }
        }
        break;
      case xpti::trace_point_type_t::task_begin:
        if (Event) {
          const auto* metadata = xptiQueryMetadata(Event);
          for (const auto& item : *metadata) {
            if (IsKernelOperation(item.first)) {
              if (payload) {
                if (payload->source_file) {
                  sycl_data_kview.source_file_name_ = std::string{payload->source_file};
                }
                sycl_data_kview.source_line_number_ = payload->line_no;
              }
              sycl_data_kview.sycl_node_id_ = ID;
              const auto queue_id_loc = node_q_map.find(ID);
              sycl_data_kview.sycl_queue_id_ =
                  (queue_id_loc != std::cend(node_q_map)) ? queue_id_loc->second : kDefaultQueueId;
              sycl_data_kview.sycl_invocation_id_ = static_cast<uint32_t>(Instance_ID);
              sycl_data_kview.sycl_task_begin_time_ = time;
              break;  // no need to keep searching metadata
            }

            if (IsMemoryOperation(item.first)) {
              const auto queue_id_loc = node_q_map.find(ID);
              sycl_data_mview.sycl_queue_id_ =
                  (queue_id_loc != std::cend(node_q_map)) ? queue_id_loc->second : kDefaultQueueId;
              break;  // no need to keep searching metadata
            }
          }
        }
        break;
      case xpti::trace_point_type_t::task_end:
        break;
      case xpti::trace_point_type_t::queue_create:
        break;
      case xpti::trace_point_type_t::node_create: {
        if (Event) {
          char* stashed_key = nullptr;
          uint64_t stashed_value = 0;
          if (SyclCollector::Instance().xptiGetStashedKV_ &&
              SyclCollector::Instance().xptiGetStashedKV_(&stashed_key, stashed_value) ==
                  xpti::result_t::XPTI_RESULT_SUCCESS) {
            if (std::strcmp(stashed_key, "queue_id") == 0) {
              node_q_map[ID] = stashed_value;
            }
          } else {
            node_q_map[ID] = kDefaultQueueId;
          }
          const auto* metadata = xptiQueryMetadata(Event);
          for (const auto& item : *metadata) {
            if (IsKernelOperation(item.first)) {
              sycl_data_kview.sycl_queue_id_ = node_q_map[ID];
              break;
            }
            if (IsMemoryOperation(item.first)) {
              sycl_data_mview.sycl_queue_id_ = node_q_map[ID];
              break;
            }
          }
        }
        break;
      }
      default:
        break;
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
    } else if (std::strcmp(stream_name, "sycl.pi.debug") == 0) {  // PLUGIN INTERFACE
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
  static void RegisterSyclCallbacks(uint8_t stream_id) {
    // Register our lone callback to all pre-defined trace point types
    // TODO: Do something with result besides log.
    [[maybe_unused]] auto result = xptiRegisterCallback(
        stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::node_create),
        SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::queue_create),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::edge_create),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::region_begin),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::region_end),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::task_begin),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::task_end),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::barrier_begin),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::barrier_end),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::lock_begin),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::lock_end),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::transfer_begin),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::transfer_end),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(stream_id,
                                  static_cast<uint16_t>(xpti::trace_point_type_t::thread_begin),
                                  SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::thread_end),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::wait_begin),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::wait_end),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::metadata),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
  }

  static void RegisterImplCallbacks(uint8_t stream_id) {
    // TODO: Do something with result besides log.
    [[maybe_unused]] auto result = xptiRegisterCallback(
        stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::function_with_args_begin),
        SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result = xptiRegisterCallback(
        stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::function_with_args_end),
        SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
    result =
        xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::metadata),
                             SyclCollector::TpCallback);
    if (result != xpti::result_t::XPTI_RESULT_SUCCESS) {
      SPDLOG_ERROR("XPTI Callback Registration returned: {}", static_cast<int32_t>(result));
    }
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
