//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef ITT_ITT_COLLECTOR_H_
#define ITT_ITT_COLLECTOR_H_

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <set>
#include <stack>
#include <string>
#include <thread>
#include <tuple>

#include "unikernel.h"

using OnIttLoggingCallback = void (*)(void* data, CommunicationRecord& rec);

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE

#if defined(PTI_CCL_ITT_COMPILE)
#define ITTAPI_CDECL __attribute__((visibility("default")))
#else
#define ITTAPI_CDECL
#endif  // PTI_CCL_ITT_COMPILE

#include <ittnotify.h>
#include <ittnotify_config.h>

struct ThreadTaskDescriptor {
  __itt_domain* domain;
  __itt_string_handle* name;

  uint64_t start_time;
  uint64_t metadata_size;

  ThreadTaskDescriptor(const __itt_domain* d = nullptr, const __itt_string_handle* n = nullptr,
                       uint64_t start = 0)
      : domain(const_cast<__itt_domain*>(d)),
        name(const_cast<__itt_string_handle*>(n)),
        start_time(start),
        metadata_size(0) {}

  // Static method declarations with improved const correctness and cleaner types
  static std::string DumpTaskDescriptor() noexcept;
};

class IttCollector {
 public:  // Interface
  inline static auto& Instance(OnIttLoggingCallback callback_ = nullptr) {
    static IttCollector itt_collector{callback_};
    return itt_collector;
  }

  IttCollector(const IttCollector& copy) = delete;
  IttCollector& operator=(const IttCollector& copy) = delete;
  IttCollector(IttCollector&&) = delete;
  IttCollector& operator=(IttCollector&&) = delete;

  void EnableTrace() {
    SPDLOG_DEBUG("{}(): TID: {}", __PRETTY_FUNCTION__, utils::GetTid());
    collection_enabled_.store(true, std::memory_order_relaxed);
  }
  void DisableTrace() {
    SPDLOG_DEBUG("{}(): TID: {}", __PRETTY_FUNCTION__, utils::GetTid());
    collection_enabled_.store(false, std::memory_order_relaxed);
  }
  inline bool IsTraceEnabled() { return collection_enabled_.load(std::memory_order_relaxed); }

  std::atomic<OnIttLoggingCallback> acallback_ = nullptr;
  void SetCallback(const OnIttLoggingCallback callback) { acallback_.store(callback); }

  ~IttCollector() {}

  void CallbackUser(ThreadTaskDescriptor& rec, uint64_t end) {
    SPDLOG_DEBUG("{}() - name: {}, start_ts: {}, end_ts: {}, metadata_size: {:#x}", __FUNCTION__,
                 rec.name ? rec.name->strA : "NULL", rec.start_time, end, rec.metadata_size);
    if (acallback_.load()) {
      itt_runtime_rec_.pid_ = PidTidInfo::Get().pid;
      itt_runtime_rec_.tid_ = PidTidInfo::Get().tid;
      itt_runtime_rec_.start_time_ = rec.start_time;
      itt_runtime_rec_.end_time_ = end;
      itt_runtime_rec_.metadata_size_ = rec.metadata_size;
      itt_runtime_rec_.name_ = rec.name ? rec.name->strA : nullptr;

      auto cb = acallback_.load();
      if (cb) cb(nullptr, itt_runtime_rec_);
    }
  }

 private:  // Implementation
  inline static thread_local CommunicationRecord itt_runtime_rec_;
  IttCollector(OnIttLoggingCallback callback) : acallback_(callback) {}

 private:  // Data
  std::atomic<bool> collection_enabled_{false};
};

#endif  // ITT_ITT_COLLECTOR_H_
