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
#include <mutex>
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

#include "itt_metadata.h"

struct ThreadTaskDescriptor {
  __itt_domain* domain_;
  __itt_string_handle* name_;

  uint64_t start_time_;
  uint64_t send_size_;
  uint64_t recv_size_;
  uint64_t communicator_id_;

  ThreadTaskDescriptor(const __itt_domain* d = nullptr, const __itt_string_handle* n = nullptr,
                       uint64_t start = 0)
      : domain_(const_cast<__itt_domain*>(d)),
        name_(const_cast<__itt_string_handle*>(n)),
        start_time_(start),
        send_size_(0),
        recv_size_(0),
        communicator_id_(0) {}
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

    // Clear any stale stack state for clean start
    if (!task_desc_.empty()) {
      SPDLOG_DEBUG("Clearing {} stale tasks on EnableTrace", task_desc_.size());
      std::stack<ThreadTaskDescriptor> empty_stack;
      task_desc_.swap(empty_stack);
    }

    EnableCollection();
  }
  void DisableTrace() {
    SPDLOG_DEBUG("{}(): TID: {}", __PRETTY_FUNCTION__, utils::GetTid());

    DisableCollection();

    if (!task_desc_.empty()) {
      SPDLOG_DEBUG("Purging {} incomplete tasks from thread-local stack", task_desc_.size());
      std::stack<ThreadTaskDescriptor> empty_stack;
      task_desc_.swap(empty_stack);
    }
  }

  bool IsCollectionEnabled() const {
    return collection_state_.load(std::memory_order_acquire) == CollectionState::kEnabled;
  }

  void SetCallback(const OnIttLoggingCallback callback) {
    record_dispatcher_.store(callback, std::memory_order_relaxed);
  }

  // Debug utility for dumping thread-local task stack
  static std::string DumpTaskDescriptor() noexcept;

  // Thread-local task stack - public for implementation functions access
  inline static thread_local std::stack<ThreadTaskDescriptor> task_desc_;

  ~IttCollector() {}

  void CallbackUser(ThreadTaskDescriptor& rec, uint64_t end) {
    SPDLOG_DEBUG("{}() - name: {}, start_ts: {}, end_ts: {}, send_size: {:#x}, recv_size: {:#x}",
                 __FUNCTION__, rec.name_ ? rec.name_->strA : "NULL", rec.start_time_, end,
                 rec.send_size_, rec.recv_size_);

    // Single atomic load with relaxed ordering
    auto cb = record_dispatcher_.load(std::memory_order_relaxed);
    if (cb) {
      itt_runtime_rec_.pid_ = PidTidInfo::Get().pid;
      itt_runtime_rec_.tid_ = PidTidInfo::Get().tid;
      itt_runtime_rec_.start_time_ = rec.start_time_;
      itt_runtime_rec_.end_time_ = end;
      itt_runtime_rec_.send_size_ = rec.send_size_;
      itt_runtime_rec_.recv_size_ = rec.recv_size_;
      itt_runtime_rec_.communicator_id_ = rec.communicator_id_;
      itt_runtime_rec_.name_ = rec.name_ ? rec.name_->strA : nullptr;

      cb(nullptr, itt_runtime_rec_);
    }
  }

  mutable std::mutex initialization_mutex_;

 private:  // Implementation
  enum class CollectionState : int { kDisabled = 0, kEnabled = 1 };

  void EnableCollection();
  void DisableCollection();

  inline static thread_local CommunicationRecord itt_runtime_rec_;
  IttCollector(OnIttLoggingCallback callback) : record_dispatcher_(callback) {}

 private:  // Data
  std::atomic<OnIttLoggingCallback> record_dispatcher_ = nullptr;
  std::atomic<CollectionState> collection_state_{CollectionState::kDisabled};
};

#endif  // ITT_ITT_COLLECTOR_H_
