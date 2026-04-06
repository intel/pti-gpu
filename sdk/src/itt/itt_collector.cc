//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "itt_collector.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdio>
#include <stack>
#include <string>
#include <vector>

#include "itt_common_helpers.h"
#include "utils.h"

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
              (desc.domain && desc.domain->nameA ? desc.domain->nameA : "NULL") +
              "::" + (desc.name && desc.name->strA ? desc.name->strA : "NULL") +
              " start_time: " + std::to_string(desc.start_time) + "\n";
  }

  result += "  Total task descriptors: " + std::to_string(elements.size()) + "\n";
  return result;
}

static std::atomic<__itt_global *> itt_global{nullptr};

void IttCollector::ScanExistingDomainsAndFindCclDomain() {
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    SPDLOG_DEBUG("{}(): itt_global is NULL, no domains to scan", __FUNCTION__);
    return;
  }

  SPDLOG_DEBUG("{}(): Scanning existing domains in itt_global", __FUNCTION__);

  __itt_mutex_lock(&(global_ptr->mutex));
  for (__itt_domain *d = global_ptr->domain_list; d != nullptr; d = d->next) {
    // This will cache the domain if it's the CCL domain
    GetCclDomain(d);
    if (d->nameA != nullptr) {
      SPDLOG_DEBUG("{}(): Found domain '{}'", __FUNCTION__, d->nameA);
    }
  }
  __itt_mutex_unlock(&(global_ptr->mutex));

  [[maybe_unused]] const __itt_domain *cached = itt_ccl_domain_.load(std::memory_order_relaxed);
  SPDLOG_DEBUG("{}(): CCL domain {}", __FUNCTION__, cached ? "found and cached" : "not found");
}

ITT_EXTERN_C void ITTAPI __itt_api_init_impl(__itt_global *p,
                                             [[maybe_unused]] __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() - Collector: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p != nullptr) {
    itt_global.store(p, std::memory_order_release);
    // Scan for domains that may have been created before collector initialized
    IttCollector::Instance().ScanExistingDomainsAndFindCclDomain();
  }
}

[[maybe_unused]] static std::string __itt_domain_dump() {
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    return "itt_global is NULL";
  }

  std::string result = "\nDomain list:\n";
  __itt_mutex_lock(&(global_ptr->mutex));

  int count = 0;
  for (__itt_domain *h = global_ptr->domain_list; h != nullptr; h = h->next) {
    result += "  [" + std::to_string(count) + "] " + (h->nameA ? h->nameA : "NULL") +
              " <flag: " + std::to_string(h->flags) + ">\n";
    count++;
  }

  if (count == 0) {
    result += "(empty)\n";
  } else {
    result += "  Total domains: " + std::to_string(count);
  }

  __itt_mutex_unlock(&(global_ptr->mutex));
  return result;
}

ITT_EXTERN_C __itt_domain *ITTAPI __itt_domain_create_impl(const char *name) {
  SPDLOG_DEBUG("{}() Collector - name: {}", __FUNCTION__, name ? name : "NULL");
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr || name == nullptr) {
    return nullptr;
  }

  __itt_domain *h_tail = nullptr, *h = nullptr;
  __itt_mutex_lock(&(global_ptr->mutex));
  for (h_tail = nullptr, h = global_ptr->domain_list; h != nullptr; h_tail = h, h = h->next) {
    if (h->nameA != nullptr && !__itt_fstrcmp(h->nameA, name)) break;
  }
  if (h == nullptr) {
    NEW_DOMAIN_A(global_ptr, h, h_tail, name);
    // Only enable CCL domain if tracing is currently enabled
    bool is_ccl_domain = IttCollector::Instance().GetCclDomain(h) == h;
    h->flags = is_ccl_domain && IttCollector::Instance().IsTraceEnabled() ? 1 : 0;
  }
  __itt_mutex_unlock(&(global_ptr->mutex));

  return h;
}

[[maybe_unused]] static std::string __itt_string_handle_dump() {
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    return "itt_global is NULL";
  }

  std::string result = "\nString handle list:\n";
  __itt_mutex_lock(&(global_ptr->mutex));

  int count = 0;
  for (__itt_string_handle *h = global_ptr->string_list; h != nullptr; h = h->next) {
    result += "  [" + std::to_string(count) + "] " + (h->strA ? h->strA : "NULL") + "\n";
    count++;
  }

  if (count == 0) {
    result += "  (empty string handle list)\n";
  } else {
    result += "  Total string handles: " + std::to_string(count);
  }

  __itt_mutex_unlock(&(global_ptr->mutex));
  return result;
}

ITT_EXTERN_C __itt_string_handle *ITTAPI __itt_string_handle_create_impl(const char *name) {
  SPDLOG_DEBUG("{}() - name: {}", __FUNCTION__, name ? name : "NULL");

  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    SPDLOG_WARN("{}(): itt_global is NULL", __FUNCTION__);
    return nullptr;
  }

  __itt_string_handle *h = itt_helpers::FindOrCreateStringHandle(global_ptr, name);

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, __itt_string_handle_dump());
  return h;
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_impl(const __itt_domain *domain,
                                               [[maybe_unused]] __itt_id taskid,
                                               [[maybe_unused]] __itt_id parentid,
                                               __itt_string_handle *name) {
  SPDLOG_DEBUG("{}() Collector - domain: {}, name: {}", __FUNCTION__,
               domain ? domain->nameA : "NULL", name ? name->strA : "NULL");

  if ((domain == nullptr) || (IttCollector::Instance().GetCclDomain(domain) != domain)) {
    return;
  }

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, __itt_domain_dump());
  SPDLOG_DEBUG("{}() {}", __FUNCTION__, __itt_string_handle_dump());

  IttCollector::task_desc_.push(ThreadTaskDescriptor(domain, name, utils::GetTime()));

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, IttCollector::DumpTaskDescriptor());
}

ITT_EXTERN_C void ITTAPI __itt_task_end_impl(const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() Collector - domain: {}", __FUNCTION__, domain ? domain->nameA : "NULL");

  if ((domain == nullptr) || (IttCollector::Instance().GetCclDomain(domain) != domain)) {
    return;
  }

  auto end = utils::GetTime();

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, IttCollector::DumpTaskDescriptor());

  if (IttCollector::task_desc_.empty()) {
    return;
  }

  auto task = IttCollector::task_desc_.top();
  IttCollector::task_desc_.pop();

  SPDLOG_DEBUG("{}() line {} - {}::{}, start: {}, end: {}, metadata_size: {:#x}", __FUNCTION__,
               __LINE__, task.domain ? task.domain->nameA : "NULL",
               task.name ? task.name->strA : "NULL", task.start_time, end, task.metadata_size);

  IttCollector::Instance().CallbackUser(task, end);
}
ITT_EXTERN_C void ITTAPI __itt_metadata_add_impl(const __itt_domain *domain,
                                                 [[maybe_unused]] __itt_id id,
                                                 [[maybe_unused]] __itt_string_handle *key,
                                                 __itt_metadata_type type, size_t count,
                                                 void *data) {
  if ((domain == nullptr) || (IttCollector::Instance().GetCclDomain(domain) != domain)) {
    return;
  }

  if (data == nullptr || type != __itt_metadata_u64 || count != 1) {
    return;
  }

  SPDLOG_DEBUG("{}() Collector - domain: {}, key: {}, send_size: 0x{:#x}", __FUNCTION__,
               domain ? domain->nameA : "NULL", key ? key->strA : "NULL",
               *static_cast<uint64_t *>(data));

  if (!IttCollector::task_desc_.empty() && IttCollector::task_desc_.top().domain == domain) {
    SPDLOG_DEBUG("{}() Collector - FOUND!!", __FUNCTION__);
    IttCollector::task_desc_.top().metadata_size = *static_cast<uint64_t *>(data);
    return;
  }
  SPDLOG_DEBUG("{}() Collector - NOT FOUND!!", __FUNCTION__);
}

void IttEnableCclDomain(void) {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    return;
  }

  // Scan for CCL domain in case it was created before collector initialized
  // or if this is called before __itt_api_init completed
  IttCollector::Instance().ScanExistingDomainsAndFindCclDomain();

  __itt_mutex_lock(&(global_ptr->mutex));
  auto cclDomain = IttCollector::Instance().GetCclDomain(nullptr);
  if (cclDomain != nullptr) {
    SPDLOG_DEBUG("{}() Enabling domain: {}", __FUNCTION__, cclDomain->nameA);
    const_cast<__itt_domain *>(cclDomain)->flags = 1;
  } else {
    SPDLOG_DEBUG("{}() CCL domain not found", __FUNCTION__);
  }
  __itt_mutex_unlock(&(global_ptr->mutex));
}

void IttDisableCclDomain(void) {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);
  __itt_global *global_ptr = itt_global.load(std::memory_order_acquire);
  if (global_ptr == nullptr) {
    return;
  }
  __itt_mutex_lock(&(global_ptr->mutex));
  auto cclDomain = IttCollector::Instance().GetCclDomain(nullptr);
  if (cclDomain != nullptr) {
    const_cast<__itt_domain *>(cclDomain)->flags = 0;
  }
  __itt_mutex_unlock(&(global_ptr->mutex));
}

// clang-format off
extern "C" {
  void
#if (defined(_WIN32) || defined(_WIN64))
  __declspec(dllexport)
#else
  __attribute__((visibility("default")))
#endif
  PtiSetCachedIttInit(__itt_global* p, [[maybe_unused]] __itt_group_id init_groups) {
    SPDLOG_DEBUG("{}() - p={}, init_groups={}", __FUNCTION__,
                 static_cast<void*>(p), static_cast<int>(init_groups));

    // Defensive: Handle NULL case (ITT not initialized yet)
    if (p == nullptr) {
      SPDLOG_DEBUG("{}() - NULL itt_global, ITT not initialized yet", __FUNCTION__);
      return;  // Will be called again later via __itt_api_init_impl
    }

    // Defensive: Avoid double-initialization
    __itt_global* current = itt_global.load(std::memory_order_acquire);
    if (current != nullptr && current != p) {
      SPDLOG_WARN("{}() - itt_global already set to different value!", __FUNCTION__);
      return;
    }

    if (current == nullptr) {
      SPDLOG_DEBUG("{}() - Setting itt_global and scanning domains", __FUNCTION__);
      itt_global.store(p, std::memory_order_release);
      IttCollector::Instance().ScanExistingDomainsAndFindCclDomain();
    } else {
      SPDLOG_DEBUG("{}() - itt_global already set, skipping", __FUNCTION__);
    }
  }
}
// clang-format on
