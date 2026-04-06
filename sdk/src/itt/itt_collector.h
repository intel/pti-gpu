//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef ITT_ITT_COLLECTOR_H_
#define ITT_ITT_COLLECTOR_H_

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <stack>
#include <string>

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
  uint64_t communicator_id_;

  ThreadTaskDescriptor(const __itt_domain* d = nullptr, const __itt_string_handle* n = nullptr,
                       uint64_t start = 0)
      : domain(const_cast<__itt_domain*>(d)),
        name(const_cast<__itt_string_handle*>(n)),
        start_time(start),
        metadata_size(0),
        communicator_id_(0) {}
};

void IttEnableCclDomain(void);
void IttDisableCclDomain(void);

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

    trace_enabled_.store(true, std::memory_order_release);

    // Clear any stale stack state for clean start
    if (!task_desc_.empty()) {
      SPDLOG_DEBUG("Clearing {} stale tasks on EnableTrace", task_desc_.size());
      std::stack<ThreadTaskDescriptor> empty_stack;
      task_desc_.swap(empty_stack);
    }

    IttEnableCclDomain();
  }
  void DisableTrace() {
    SPDLOG_DEBUG("{}(): TID: {}", __PRETTY_FUNCTION__, utils::GetTid());

    IttDisableCclDomain();

    trace_enabled_.store(false, std::memory_order_release);

    if (!task_desc_.empty()) {
      SPDLOG_DEBUG("Purging {} incomplete tasks from thread-local stack", task_desc_.size());
      std::stack<ThreadTaskDescriptor> empty_stack;
      task_desc_.swap(empty_stack);
      // empty_stack destructor handles cleanup automatically
    }
  }

  void SetCallback(const OnIttLoggingCallback callback) {
    record_dispatcher_.store(callback, std::memory_order_relaxed);
  }

  bool IsTraceEnabled() const { return trace_enabled_.load(std::memory_order_acquire); }

  // Debug utility for dumping thread-local task stack
  static std::string DumpTaskDescriptor() noexcept;

  /**
   * @brief Scan existing domains in itt_global and cache CCL domain if found
   *
   * This method scans the global ITT domain list to find and cache the CCL domain.
   * It's safe to call multiple times (idempotent).
   */
  void ScanExistingDomainsAndFindCclDomain();

  /**
   * @brief Check if a domain is the CCL domain and cache it if found
   * @param domain Domain to check (can be nullptr to just retrieve cached value)
   * @return Pointer to CCL domain if found/cached, nullptr otherwise
   */
  inline const __itt_domain* GetCclDomain(const __itt_domain* domain) {
    static const char* const kcclDomain = "oneCCL::API";

    // Fast path: if already initialized, return the cached domain
    const __itt_domain* cached = itt_ccl_domain_.load(std::memory_order_relaxed);
    if (cached != nullptr) {
      return cached;
    }

    // Slow path: check if this domain matches oneCCL and try to cache it
    if (domain != nullptr && domain->nameA != nullptr && !std::strcmp(domain->nameA, kcclDomain)) {
      const __itt_domain* expected = nullptr;
      // Only one thread will successfully update itt_ccl_domain_ from nullptr to domain
      // If compare_exchange_weak fails, another thread already set it, which is fine
      itt_ccl_domain_.compare_exchange_weak(expected, domain, std::memory_order_relaxed);
      return domain;
    }

    return nullptr;
  }

  // Thread-local task stack - public for implementation functions access
  inline static thread_local std::stack<ThreadTaskDescriptor> task_desc_;

  ~IttCollector() {}

  void CallbackUser(ThreadTaskDescriptor& rec, uint64_t end) {
    SPDLOG_DEBUG("{}() - name: {}, start_ts: {}, end_ts: {}, metadata_size: {:#x}", __FUNCTION__,
                 rec.name ? rec.name->strA : "NULL", rec.start_time, end, rec.metadata_size);

    // Single atomic load with relaxed ordering
    auto cb = record_dispatcher_.load(std::memory_order_relaxed);
    if (cb) {
      itt_runtime_rec_.pid_ = PidTidInfo::Get().pid;
      itt_runtime_rec_.tid_ = PidTidInfo::Get().tid;
      itt_runtime_rec_.start_time_ = rec.start_time;
      itt_runtime_rec_.end_time_ = end;
      itt_runtime_rec_.metadata_size_ = rec.metadata_size;
      itt_runtime_rec_.communicator_id_ = 12345;  // Placeholder
      itt_runtime_rec_.name_ = rec.name ? rec.name->strA : nullptr;

      cb(nullptr, itt_runtime_rec_);
    }
  }

 private:  // Implementation
  inline static thread_local CommunicationRecord itt_runtime_rec_;

  IttCollector(OnIttLoggingCallback callback) : record_dispatcher_(callback) {
    ScanExistingDomainsAndFindCclDomain();
  }

 private:  // Data
  std::atomic<OnIttLoggingCallback> record_dispatcher_ = nullptr;
  std::atomic<const __itt_domain*> itt_ccl_domain_{nullptr};
  std::atomic<bool> trace_enabled_{false};
};

#endif  // ITT_ITT_COLLECTOR_H_
