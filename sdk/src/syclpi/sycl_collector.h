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
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <xpti/xpti_trace_framework.hpp>

#include "unikernel.h"
#include "utils.h"

// Work-around for ensuring XPTI_SUBSCRIBERS and XPTI_FRAMEWORK_DISPATCHER are
// set before xptiTraceInit() is called.
// Warning: Do not add a dependency on another static variable or there is a
// risk of undefined behavior.
// TODO(matthew.schilling@intel.com): Fix when there's a better solution.
class GlobalSyclInitializer {
 public:
  inline static bool Initialize() {
    // https://stackoverflow.com/questions/48650674/c-c-how-to-find-out-the-own-library-name
    utils::SetEnv("XPTI_SUBSCRIBERS", utils::GetPathToSharedObject(Initialize).c_str());
    utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", utils::GetPathToSharedObject(xptiReset).c_str());
    utils::SetEnv("XPTI_TRACE_ENABLE", "1");
    return true;
  }

  inline static bool result_ = Initialize();
};

inline constexpr auto kMaxFuncNameLen = 2048;

typedef void (*OnSyclRuntimeViewCallback)(void* data, ZeKernelCommandExecutionRecord& kcexec);

#ifdef PTI_DEBUG

// keeping this for future SYCL nodes/tasks debug
struct sycl_node_t {
  uint64_t _id;
  uint64_t _node_create_time;
  std::string _source_file_name;
  uint32_t _source_line_number;
  std::string _name;
  uint32_t _task_begin_count;
  uint32_t _task_end_count;
  sycl_node_t(uint64_t id)
      : _id(id),
        _node_create_time(0ULL),
        _source_file_name("<unknown>"),
        _source_line_number(0),
        _task_begin_count(0),
        _task_end_count(0){};
};

thread_local std::map<uint64_t, std::unique_ptr<sycl_node_t>> s_node_map = {};

#endif

struct SyclPiFuncT {
  std::array<char, kMaxFuncNameLen> func_name;
  uint32_t func_pid;
  uint32_t func_tid;
};

thread_local bool framework_finalized = false;
thread_local SyclPiFuncT current_func_task_info;

inline constexpr static std::array<const char* const, 12> kSTraceType = {
    "TaskBegin",           "TaskEnd",  "Signal",    "NodeCreate", "FunctionWithArgsBegin",
    "FunctionWithArgsEnd", "Metadata", "WaitBegin", "WaitEnd",    "FunctionBegin",
    "FunctionEnd",         "Other"};

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
    default:
      return kSTraceType[11];
  }
}

std::string Truncate(const std::string& name) {
  size_t pos = name.find_last_of(":");
  if (pos != std::string::npos) {
    return name.substr(pos + 1);
  }
  return name;
}

class SyclCollector {
 public:
  inline static auto& Instance() {
    static SyclCollector sycl_collector{nullptr};
    return sycl_collector;
  }

  inline void EnableTracing() {
    enabled_ = true;
    xptiForceSetTraceEnabled(enabled_);
  }

  inline void DisableTracing() {
    enabled_ = false;
    if (sycl_pi_graph_created_) {
      xptiForceSetTraceEnabled(enabled_);
    }
  }

  inline void SetCallback(const OnSyclRuntimeViewCallback callback) { acallback_ = callback; }

  static XPTI_CALLBACK_API void TpCallback(uint16_t TraceType, xpti::trace_event_data_t* /*Parent*/,
                                           xpti::trace_event_data_t* Event, uint64_t,
                                           const void* UserData) {
    auto Payload = xptiQueryPayload(Event);
    uint64_t Time = utils::GetTime(CLOCK_MONOTONIC_RAW);
    std::string Name = "<unknown>";

    if (Payload) {
      if (Payload->name_sid() != xpti::invalid_id) {
        Name = Truncate(Payload->name);
      }
    }

    uint64_t ID = Event ? Event->unique_id : 0;
    uint64_t Instance_ID = Event ? Event->instance_id : 0;
    uint32_t pid = utils::GetPid();
    uint32_t tid = utils::GetTid();

    const auto trace_type = static_cast<xpti::trace_point_type_t>(TraceType);

    SPDLOG_DEBUG("{}: TraceType: {}", Time, GetTracePointTypeString(trace_type));
    SPDLOG_DEBUG(" Event_id: {}, Instance_id: {}, pid: {}, tid: {} name: {}", ID, Instance_ID, pid,
                 tid, Name.c_str());
    /*
    if (Event) {
      xpti::metadata_t *Metadata = xptiQueryMetadata(Event);
      for (const auto &Item : *Metadata) {
          std::cout << "    " << xptiLookupString(Item.first) << ": ";
          std::cout << xpti::readMetadata(Item) << "\n";
      }
    }
    */

    switch (trace_type) {
      case xpti::trace_point_type_t::graph_create:
        SyclCollector::Instance().sycl_pi_graph_created_ = true;
        // Until the user calls EnableTracing(), disable tracing when we are
        // able to capture the sycl runtime streams sycl and sycl.pi
        if (!SyclCollector::Instance().enabled_) {
          SyclCollector::Instance().DisableTracing();
        }
        break;
      case xpti::trace_point_type_t::function_begin:
        sycl_data_kview.cid_ = UniCorrId::GetUniCorrId();
        sycl_data_mview.cid_ = sycl_data_kview.cid_;
        if (UserData) {
          auto function_name = static_cast<const char*>(UserData);
          SPDLOG_DEBUG("\tSYCL.PI Function Begin: {}", function_name);
          if ((strlen(function_name) + 1) < kMaxFuncNameLen) {
            strcpy(current_func_task_info.func_name.data(), function_name);
          } else {
            strcpy(current_func_task_info.func_name.data(), "<unknown>");
          }
          current_func_task_info.func_pid = pid;
          current_func_task_info.func_tid = tid;
          if (strcmp(function_name, "piEnqueueKernelLaunch") == 0) {
            sycl_data_kview.sycl_enqk_begin_time_ = Time;
          }
          if ((strcmp(function_name, "piextUSMEnqueueMemcpy") == 0) ||
              (strcmp(function_name, "piextUSMEnqueueMemcpy2d") == 0)) {
            sycl_data_mview.sycl_task_begin_time_ = Time;
          }
          SyclCollector::Instance().sycl_runtime_rec_.pid_ = pid;
          SyclCollector::Instance().sycl_runtime_rec_.tid_ = tid;
          SyclCollector::Instance().sycl_runtime_rec_.start_time_ = Time;
          SyclCollector::Instance().sycl_runtime_rec_.sycl_func_name_ = function_name;
        }
        break;
      case xpti::trace_point_type_t::function_end:
        if (UserData) {
          auto function_name = static_cast<const char*>(UserData);
          SPDLOG_DEBUG("\tSYCL.PI Function End: {}", function_name);
          PTI_ASSERT(strcmp(current_func_task_info.func_name.data(), function_name) == 0);
          PTI_ASSERT(current_func_task_info.func_pid == pid);
          PTI_ASSERT(current_func_task_info.func_tid == tid);
          SPDLOG_DEBUG("\tVerified: func: {} - Pid: {} - Tid: {}",
                       current_func_task_info.func_name.data(), current_func_task_info.func_pid,
                       current_func_task_info.func_tid);
          SyclCollector::Instance().sycl_runtime_rec_.cid_ = sycl_data_kview.cid_;
          if (strcmp(function_name, "piEnqueueKernelLaunch") == 0) {
            SyclCollector::Instance().sycl_runtime_rec_.kid_ = sycl_data_kview.kid_;
          }
          if ((strcmp(function_name, "piextUSMEnqueueMemcpy") == 0) ||
              (strcmp(function_name, "piextUSMEnqueueMemcpy2d") == 0) ||
              (strcmp(function_name, "piEnqueueMemBufferRead") == 0) ||
              (strcmp(function_name, "piEnqueueMemBufferWrite") == 0)) {
            SyclCollector::Instance().sycl_runtime_rec_.kid_ = sycl_data_mview.kid_;
            SyclCollector::Instance().sycl_runtime_rec_.tid_ = sycl_data_mview.tid_;
          }
          SyclCollector::Instance().sycl_runtime_rec_.end_time_ = Time;
          if (SyclCollector::Instance().acallback_ != nullptr) {
            if (SyclCollector::Instance().enabled_ && !framework_finalized) {
              (SyclCollector::Instance().acallback_.load())(
                  nullptr, SyclCollector::Instance().sycl_runtime_rec_);
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
#ifdef PTI_DEBUG
        if (s_node_map.find(ID) != s_node_map.end()) {
          (s_node_map[ID]->_task_begin_count)++;
        } else {
          SPDLOG_WARN("Unexpected: Node not found at Task Begin, ID: {}, Name: {}", ID, Name);
        }
#endif
        if (Event) {
          xpti::metadata_t* Metadata = xptiQueryMetadata(Event);
          for (const auto& Item : *Metadata) {
            if (strcmp(xptiLookupString(Item.first), "kernel_name") == 0) {
              if (Payload) {
                if (Payload->source_file) {
                  sycl_data_kview.source_file_name_ = std::string{Payload->source_file};
                }
                sycl_data_kview.source_line_number_ = Payload->line_no;
              }
              sycl_data_kview.sycl_node_id_ = ID;
              sycl_data_kview.sycl_invocation_id_ = Instance_ID;
              sycl_data_kview.sycl_task_begin_time_ = Time;
              break;
            }
          }
        }
        break;
      case xpti::trace_point_type_t::task_end:
#ifdef PTI_DEBUG
        if (s_node_map.find(ID) != s_node_map.end()) {
          (s_node_map[ID]->_task_end_count)++;
        } else {
          SPDLOG_WARN("Unexpected: Node not found at Task End, ID: {}, Name {}", ID, Name);
        }
#endif
        break;
      case xpti::trace_point_type_t::node_create:
#ifdef PTI_DEBUG
        const char* source_file_name = nullptr;
        if (Payload) {
          source_file_name = Payload->source_file;
        }
        // From the experiments found that a "simple" Node Created once per
        // program so if a node (the same kernel task, defined at one specific
        // source location) is used at multiple threads - only one thread will
        // create its node. With that below warning is not relevant for "simple"
        // multi-threaded kernel submission.. but for some time will keep it
        // around
        if (s_node_map.find(ID) != s_node_map.end()) {
          SPDLOG_WARN("Unexpected: Node found before creation, ID: {}, Name: {}", ID, Name);
        }
        auto node = std::make_unique<sycl_node_t>(ID);
        if (source_file_name) {
          uint32_t chars = strlen(source_file_name);
          node->_source_file_name = (char*)source_file_name;
        }
        node->_source_line_number = source_line_number;
        node->_name = Name;
        node->_node_create_time = Time;
        s_node_map[ID] = std::move(node);
#endif
        if (Name.find("Memory Transfer (Copy)") != std::string::npos) {
          sycl_data_mview.sycl_task_begin_time_ = Time;
        }
        break;
      default:
        break;
    }
  }

 private:
  SyclCollector(OnSyclRuntimeViewCallback buffer_callback) : acallback_(buffer_callback){};

  inline static thread_local ZeKernelCommandExecutionRecord sycl_runtime_rec_;
  std::atomic<OnSyclRuntimeViewCallback> acallback_ = nullptr;
  bool sycl_pi_graph_created_ = false;
  std::atomic<bool> enabled_ = false;
};

XPTI_CALLBACK_API void xptiTraceInit(unsigned int /*major_version*/, unsigned int /*minor_version*/,
                                     const char* /*version_str*/, const char* stream_name) {
  static uint8_t stream_id = 0;

  spdlog::set_pattern("[%H:%M:%S] [thread %t] %v");
  if (strcmp(stream_name, "sycl") == 0) {
    // Register this stream to get the stream ID; This stream may already have
    // been registered by the framework and will return the previously
    // registered stream ID
    stream_id = xptiRegisterStream(stream_name);
    // Register our lone callback to all pre-defined trace point types
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::node_create),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::edge_create),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::region_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::region_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::task_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::task_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::barrier_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::barrier_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::lock_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::lock_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::transfer_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::transfer_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::thread_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::thread_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::wait_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::wait_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::metadata),
                         SyclCollector::TpCallback);
    SPDLOG_DEBUG("Registered callbacks for {}", stream_name);
  } else if (strcmp(stream_name, "sycl.pi") == 0) {
    stream_id = xptiRegisterStream(stream_name);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::graph_create),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::function_begin),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::function_end),
                         SyclCollector::TpCallback);
    xptiRegisterCallback(stream_id, static_cast<uint16_t>(xpti::trace_point_type_t::metadata),
                         SyclCollector::TpCallback);
    SPDLOG_DEBUG("Registering callbacks for {}", stream_name);
  } else {
    // handle the case when a bad stream name has been provided
    SPDLOG_DEBUG("Stream: {} no callbacks registered!", stream_name);
  }
}

XPTI_CALLBACK_API void xptiTraceFinish(const char* /*stream_name*/) {}
#if (defined(_WIN32) || defined(_WIN64))

#include <windows.h>

#include <string>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fwdReason, LPVOID lpvReserved) {
  switch (fwdReason) {
    case DLL_PROCESS_ATTACH:
      // printf("Framework initialization\n");
      break;
    case DLL_PROCESS_DETACH:
      //
      //  We cannot unload all subscribers here...
      //
      // printf("Framework finalization\n");
      break;
  }

  return TRUE;
}

#else  // Linux (possibly macOS?)

__attribute__((constructor)) static void framework_init() {}

__attribute__((destructor)) static void framework_fini() { framework_finalized = true; }

#endif
#endif
