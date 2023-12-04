//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_XPTI_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_XPTI_COLLECTOR_H_

#include "unicontrol.h"
#include "unievent.h"
#include "xpti/xpti_trace_framework.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

typedef void (*OnXptiLoggingCallback)(EVENT_TYPE etype, const char *name, uint64_t start_ts, uint64_t end_ts);

class XptiCollector {
 public: // Interface

  static XptiCollector *Create(OnXptiLoggingCallback xcallback = nullptr) {
    XptiCollector* collector = new XptiCollector(xcallback);

    if (collector == nullptr) {
      std::cerr << "[WARNING] Unable to create XPTI tracer" << std::endl;
    }

    return collector;
  }

  XptiCollector(const XptiCollector& that) = delete;

  XptiCollector& operator=(const XptiCollector& that) = delete;

  ~XptiCollector() {
  }

  void Log(EVENT_TYPE etype, const char *name, uint64_t start_ts, uint64_t end_ts) {
    if (!UniController::IsCollectionEnabled()) {
      return;
    }
    if (xcallback_) {
      xcallback_(etype, name, start_ts, end_ts);
    }
  }
 private: // Implementation

  XptiCollector(OnXptiLoggingCallback xcallback) : xcallback_(xcallback) {
  }

 private: // Data
  OnXptiLoggingCallback xcallback_ = nullptr;
};

static XptiCollector *xpti_collector = nullptr;

XPTI_CALLBACK_API void tpCallback(uint16_t trace_type,
                                  xpti::trace_event_data_t *parent,
                                  xpti::trace_event_data_t *event,
                                  uint64_t instance, const void *user_data);

XPTI_CALLBACK_API void xptiTraceInit(unsigned int major_version,
                                     unsigned int minor_version,
                                     const char *version_str,
                                     const char *stream_name) {
  if ((std::string(stream_name) == "sycl") ||
      (std::string(stream_name) == "sycl.pi") ||
      //(std::string(stream_name) == "sycl.pi.debug") ||
      (std::string(stream_name) == "sycl.experimental.buffer") ||
      (std::string(stream_name) == "sycl.experimental.mem_alloc")) {
    uint8_t stream = xptiRegisterStream(stream_name);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::function_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::function_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::task_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::task_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::wait_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::wait_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::barrier_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::barrier_end, tpCallback);

    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::graph_create, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::node_create, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::edge_create, tpCallback);

    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::region_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::region_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::lock_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::lock_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::transfer_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::transfer_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::thread_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::thread_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::signal, tpCallback);
     
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::mem_alloc_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::mem_alloc_end, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::mem_release_begin, tpCallback);
    xptiRegisterCallback(stream, (uint16_t)xpti::trace_point_type_t::mem_release_end, tpCallback);

  } else {
    // handle other streams;
  }
}

std::string truncate(std::string Name) {
  size_t Pos = Name.find_last_of(":");
  if (Pos != std::string::npos) {
    return Name.substr(Pos + 1);
  } else {
    return Name;
  }
}

XPTI_CALLBACK_API void xptiTraceFinish(const char *stream_name) {
  // We do nothing here
}

enum XPTI_EVENT {
  XPTI_EVENT_FUNC,
  XPTI_EVENT_FUNC_WITH_ARGS,
  XPTI_EVENT_TASK,
  XPTI_EVENT_WAIT,
  XPTI_EVENT_BARRIER,
  XPTI_EVENT_MEM_ALLOC,
  XPTI_EVENT_MEM_RELEASE,
  XPTI_EVENT_REGION,
  XPTI_EVENT_LOCK,
  XPTI_EVENT_THREAD,
  XPTI_EVENT_TRANSFER,
  XPTI_EVENT_LAST
};
  
static thread_local uint64_t xpti_event_start_ts[XPTI_EVENT_LAST];

XPTI_CALLBACK_API void tpCallback(uint16_t TraceType,
                                  xpti::trace_event_data_t *Parent,
                                  xpti::trace_event_data_t *Event,
                                  uint64_t Instance, const void *UserData) {
  if (xpti_collector) {
    switch (TraceType) {
      case (uint16_t)xpti::trace_point_type_t::function_begin:
        xpti_event_start_ts[XPTI_EVENT_FUNC] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::function_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          const char *name = nullptr;
          if (UserData) {
            name = (const char *)UserData;
          }
          xpti_collector->Log(EVENT_COMPLETE, (name ? name : "unknwon"), xpti_event_start_ts[XPTI_EVENT_FUNC], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::function_with_args_begin:
        xpti_event_start_ts[XPTI_EVENT_FUNC_WITH_ARGS] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::function_with_args_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          const char *name = nullptr;
          if (UserData) {
            xpti::function_with_args_t *args = (xpti::function_with_args_t *)UserData;
            name = args->function_name;
          }
          xpti_collector->Log(EVENT_COMPLETE, name ? name : "unknwon", xpti_event_start_ts[XPTI_EVENT_FUNC_WITH_ARGS], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::task_begin:
        xpti_event_start_ts[XPTI_EVENT_TASK] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::task_end:
        {
#if 0
          // TODO: get kernel information. name is always nullptr in current implmentation
          const char *name = nullptr;
          xpti::metadata_t *Metadata = xptiQueryMetadata(Event);
          for (auto &item : *Metadata) {
            const char *key = xptiLookupString(item.first);
            if (!strcmp(key, "kernel_name") || !strcmp(key, "memory_object")) {
              name = xptiLookupString(item.second);
            }
          }
#endif /* 0 */
          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_COMPLETE, "submit", xpti_event_start_ts[XPTI_EVENT_TASK], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::wait_begin:
        xpti_event_start_ts[XPTI_EVENT_WAIT] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::wait_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          const char *name = nullptr;
          if (UserData) {
            name = (const char *)UserData;
          }
          xpti_collector->Log(EVENT_COMPLETE, (name ? name : "unknown"), xpti_event_start_ts[XPTI_EVENT_WAIT], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::barrier_begin:
        xpti_event_start_ts[XPTI_EVENT_WAIT] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::barrier_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          const char *name = nullptr;
          if (UserData) {
            name = (const char *)UserData;
          }
          xpti_collector->Log(EVENT_COMPLETE, (name ? name : "unknown"), xpti_event_start_ts[XPTI_EVENT_BARRIER], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::graph_create:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_MARK, "graph_create", ts, 0 /* not used */);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::node_create:
        {
#if 0
          // TODO: get kernel information. name is always nullptr in current implmentation
          const char *name = nullptr;
          xpti::metadata_t *Metadata = xptiQueryMetadata(Event);
          for (auto &item : *Metadata) {
            const char *key = xptiLookupString(item.first);
            if (!strcmp(key, "kernel_name") || !strcmp(key, "memory_object")) {
              name = xptiLookupString(item.second);
            }
          }
#endif /* 0 */

          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_MARK, "node_create", ts, 0);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::edge_create:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_MARK, "edge_create", ts, 0);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::mem_alloc_begin:
        xpti_event_start_ts[XPTI_EVENT_MEM_ALLOC] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::mem_alloc_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_COMPLETE, "mem_alloc", xpti_event_start_ts[XPTI_EVENT_MEM_ALLOC], ts);
        }
        break;
      case (uint16_t)xpti::trace_point_type_t::mem_release_begin:
        xpti_event_start_ts[XPTI_EVENT_MEM_RELEASE] = UniTimer::GetHostTimestamp();
        break;
      case (uint16_t)xpti::trace_point_type_t::mem_release_end:
        {
          uint64_t ts = UniTimer::GetHostTimestamp();
          xpti_collector->Log(EVENT_COMPLETE, "mem_release", xpti_event_start_ts[XPTI_EVENT_MEM_RELEASE], ts);
        }
        break;

      case (uint16_t)xpti::trace_point_type_t::region_begin:
      case (uint16_t)xpti::trace_point_type_t::region_end:
      case (uint16_t)xpti::trace_point_type_t::lock_begin:
      case (uint16_t)xpti::trace_point_type_t::lock_end:
      case (uint16_t)xpti::trace_point_type_t::transfer_begin:
      case (uint16_t)xpti::trace_point_type_t::transfer_end:
      case (uint16_t)xpti::trace_point_type_t::thread_begin:
      case (uint16_t)xpti::trace_point_type_t::thread_end:
      case (uint16_t)xpti::trace_point_type_t::signal:
        break;

      default:
        break;
    }
  }
}

#endif // PTI_TOOLS_UNITRACE_XPTI_COLLECTOR_H_
