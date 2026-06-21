//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "itt_collector.h"

#include <ittnotify_config.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdio>
#include <stack>
#include <string>
#include <vector>

#include "utils.h"

namespace {
static std::atomic<__itt_global *> g_itt_global_of_ccl_domain_adaptor{nullptr};
static std::atomic<const __itt_domain *> g_ccl_domain_adaptor{nullptr};

inline const __itt_domain *GetCacheCclDomainCollector(const __itt_domain *domain) {
  static const char *const kCclDomain = "oneCCL::API";
  const __itt_domain *cached = g_ccl_domain_adaptor.load(std::memory_order_relaxed);
  if (cached != nullptr) return cached;
  if (domain != nullptr && domain->nameA != nullptr && !std::strcmp(domain->nameA, kCclDomain)) {
    g_ccl_domain_adaptor.store(domain, std::memory_order_relaxed);
    return domain;
  }
  return nullptr;
}
}  // namespace

// Static method definitions
std::string IttCollector::DumpTaskDescriptor() noexcept {
  if (task_desc_.empty()) {
    return "Task descriptor: (empty stack)";
  }

  std::string result = "\nTask descriptor stack:\n";
  std::stack<ThreadTaskDescriptor> temp_stack = task_desc_;
  std::vector<ThreadTaskDescriptor> elements;

  // Extract all elements from the copy (this reverses the order)
  while (!temp_stack.empty()) {
    elements.push_back(temp_stack.top());
    temp_stack.pop();
  }

  // Display elements in reverse order (original stack order: bottom to top)
  for (size_t i = 0; i < elements.size(); ++i) {
    const auto &desc = elements[elements.size() - 1 - i];
    result += "  [" + std::to_string(i) + "] " +
              (desc.domain_ && desc.domain_->nameA ? desc.domain_->nameA : "NULL") +
              "::" + (desc.name_ && desc.name_->strA ? desc.name_->strA : "NULL") +
              " start_time: " + std::to_string(desc.start_time_) + "\n";
  }

  result += "  Total task descriptors: " + std::to_string(elements.size()) + "\n";
  return result;
}

ITT_EXTERN_C void ITTAPI IttCollectorSetCclGlobalAndDomain(__itt_global *g,
                                                           const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() domain: {}", __FUNCTION__, static_cast<const void *>(domain));
  std::lock_guard<std::mutex> lock(IttCollector::Instance().initialization_mutex_);

  g_ccl_domain_adaptor.store(domain, std::memory_order_release);
  g_itt_global_of_ccl_domain_adaptor.store(g, std::memory_order_release);
  if (g != nullptr && domain != nullptr) {
    auto *mutable_domain = const_cast<__itt_domain *>(domain);
    __itt_mutex_lock(&(g->mutex));
    mutable_domain->flags = IttCollector::Instance().IsCollectionEnabled() ? 1 : 0;
    __itt_mutex_unlock(&(g->mutex));
  }
}

ITT_EXTERN_C void ITTAPI itt_task_begin_collector(const __itt_domain *domain,
                                                  [[maybe_unused]] __itt_id taskid,
                                                  [[maybe_unused]] __itt_id parentid,
                                                  __itt_string_handle *name) {
  SPDLOG_DEBUG("{}() Collector - domain: {}, name: {}", __FUNCTION__,
               domain ? domain->nameA : "NULL", name ? name->strA : "NULL");

  if (!IttCollector::Instance().IsCollectionEnabled() || (domain == nullptr) ||
      (GetCacheCclDomainCollector(domain) != domain)) {
    return;
  }

  IttCollector::task_desc_.push(ThreadTaskDescriptor(domain, name, utils::GetTime()));

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, IttCollector::DumpTaskDescriptor());
}

ITT_EXTERN_C void ITTAPI itt_task_end_collector(const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() Collector - domain: {}", __FUNCTION__, domain ? domain->nameA : "NULL");

  if (!IttCollector::Instance().IsCollectionEnabled() || (domain == nullptr) ||
      (GetCacheCclDomainCollector(domain) != domain)) {
    return;
  }

  auto end = utils::GetTime();

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, IttCollector::DumpTaskDescriptor());

  if (IttCollector::task_desc_.empty()) {
    return;
  }

  auto task = IttCollector::task_desc_.top();
  IttCollector::task_desc_.pop();

  SPDLOG_DEBUG("{}() line {} - {}::{}, start: {}, end: {}, send_size: {:#x}, recv_size: {:#x}",
               __FUNCTION__, __LINE__, task.domain_ ? task.domain_->nameA : "NULL",
               task.name_ ? task.name_->strA : "NULL", task.start_time_, end, task.send_size_,
               task.recv_size_);

  IttCollector::Instance().CallbackUser(task, end);
}

// Design note: why pointer-based caching of __itt_string_handle is safe here.
//
// Every .so that links ittnotify statically gets its own __itt_global. Our
// collector's __itt_api_init is therefore called once per static part (p_sycl,
// p_tbb, p_ccl, ...). Each library creates its domains and string handles
// exclusively through its own p, so they accumulate in that p's lists.
//
// The key guarantee: __itt_metadata_add is always downstream of domain
// creation. oneCCL must create "oneCCL::API" before it can annotate any
// metadata. It also creates its metadata key strings ("send_size", "recv_size",
// "comm_id") through the same p_ccl. All three therefore live in p_ccl's
// string_list at the time we discover the domain in p_ccl->domain_list.
//
// Any other string handles from other libraries are irrelevant to our collector.
// So there is no need to track them.
//
ITT_EXTERN_C void ITTAPI itt_metadata_add_collector(const __itt_domain *domain,
                                                    [[maybe_unused]] __itt_id id,
                                                    [[maybe_unused]] __itt_string_handle *key,
                                                    __itt_metadata_type type, size_t count,
                                                    void *data) {
  if (!IttCollector::Instance().IsCollectionEnabled() || (domain == nullptr) ||
      (GetCacheCclDomainCollector(domain) != domain)) {
    return;
  }

  if (data == nullptr || type != __itt_metadata_u64 || count != 1) {
    return;
  }

  SPDLOG_DEBUG("{}() Collector - domain: {}, key: {}, send_size: 0x{:#x}", __FUNCTION__,
               domain ? domain->nameA : "NULL", key ? key->strA : "NULL",
               *static_cast<uint64_t *>(data));

  if (!IttCollector::task_desc_.empty() && IttCollector::task_desc_.top().domain_ == domain) {
    SPDLOG_DEBUG("{}() Collector - FOUND!!", __FUNCTION__);

    // Helper lambda to check and capture string handles
    auto check_and_capture = [&](auto &handle, const char *target_string) -> bool {
      __itt_string_handle *current_handle = handle.load();
      if (current_handle == key) return true;  // Fast path
      if (key == nullptr) return false;
      if (current_handle == nullptr && key->strA != nullptr &&
          !__itt_fstrcmp(key->strA, target_string)) {
        // Attempt atomic capture - use compare_exchange to avoid races
        __itt_string_handle *expected = nullptr;
        if (handle.compare_exchange_strong(expected, key)) {
          return true;  // Successfully captured
        }
        // Another thread captured it, check if it matches our key
        return handle.load() == key;
      }
      return false;
    };

    if (check_and_capture(itt_metadata::GetSendSizeHandle(), itt_metadata::kSendSizeString)) {
      IttCollector::task_desc_.top().send_size_ = *static_cast<uint64_t *>(data);
    } else if (check_and_capture(itt_metadata::GetRecvSizeHandle(),
                                 itt_metadata::kRecvSizeString)) {
      IttCollector::task_desc_.top().recv_size_ = *static_cast<uint64_t *>(data);
    } else if (check_and_capture(itt_metadata::GetCommIdHandle(), itt_metadata::kCommIdString)) {
      IttCollector::task_desc_.top().communicator_id_ = *static_cast<uint64_t *>(data);
    }
    return;
  }
  SPDLOG_DEBUG("{}() Collector - NOT FOUND!!", __FUNCTION__);
}

void IttCollector::EnableCollection() {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);

  auto *domain = const_cast<__itt_domain *>(GetCacheCclDomainCollector(nullptr));
  if (domain == nullptr) {
    SPDLOG_DEBUG("{}() Collector: oneCCL domain not yet cached", __FUNCTION__);
  } else {
    auto *g = g_itt_global_of_ccl_domain_adaptor.load(std::memory_order_acquire);
    if (g != nullptr) {
      __itt_mutex_lock(&(g->mutex));
      domain->flags = 1;
      __itt_mutex_unlock(&(g->mutex));
    }  // else - don't know global for Ccl domain -
       // so rely only on collection_state_ flag to enable collection
  }

  collection_state_.store(CollectionState::kEnabled, std::memory_order_release);
}

void IttCollector::DisableCollection() {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);

  collection_state_.store(CollectionState::kDisabled, std::memory_order_release);

  auto *domain = const_cast<__itt_domain *>(GetCacheCclDomainCollector(nullptr));
  if (domain == nullptr) {
    SPDLOG_DEBUG("{}() Collector: oneCCL domain not yet cached", __FUNCTION__);
    return;
  }
  auto *g = g_itt_global_of_ccl_domain_adaptor.load(std::memory_order_acquire);
  if (g != nullptr) {
    __itt_mutex_lock(&(g->mutex));
    domain->flags = 0;
    __itt_mutex_unlock(&(g->mutex));
  }  // else - don't know global for Ccl domain -
     // so rely only on collection_state_ flag to disable collection
}
