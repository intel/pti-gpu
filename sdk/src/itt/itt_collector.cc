//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "itt_collector.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <stack>
#include <string>
#include <vector>

#include "utils.h"

// String constants for specific handle identification
static constexpr const char *kSendSizeString = "send_size";
static std::atomic<__itt_string_handle *> send_size_handle{nullptr};
static constexpr const char *kCommIdString = "comm_id";
static std::atomic<__itt_string_handle *> comm_id_handle{nullptr};

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

static __itt_global *itt_global = nullptr;
ITT_EXTERN_C void ITTAPI __itt_api_init_impl(__itt_global *p,
                                             [[maybe_unused]] __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() - Collector: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p != nullptr) {
    itt_global = p;
  }
}

[[maybe_unused]] static std::string __itt_domain_dump() {
  if (itt_global == nullptr) {
    return "itt_global is NULL";
  }

  std::string result = "\nDomain list:\n";
  __itt_mutex_lock(&(itt_global->mutex));

  int count = 0;
  for (__itt_domain *h = itt_global->domain_list; h != nullptr; h = h->next) {
    result += "  [" + std::to_string(count) + "] " + (h->nameA ? h->nameA : "NULL") +
              " <flag: " + std::to_string(h->flags) + ">\n";
    count++;
  }

  if (count == 0) {
    result += "(empty)\n";
  } else {
    result += "  Total domains: " + std::to_string(count);
  }

  __itt_mutex_unlock(&(itt_global->mutex));
  return result;
}

ITT_EXTERN_C __itt_domain *ITTAPI __itt_domain_create_impl(const char *name) {
  SPDLOG_DEBUG("{}() Collector - name: {}", __FUNCTION__, name ? name : "NULL");
  if (itt_global == NULL || name == nullptr) {
    return NULL;
  }

  __itt_domain *h_tail = NULL, *h = NULL;

  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = NULL, h = itt_global->domain_list; h != NULL; h_tail = h, h = h->next) {
    if (h->nameA != NULL && !__itt_fstrcmp(h->nameA, name)) break;
  }
  if (h == NULL) {
    NEW_DOMAIN_A(itt_global, h, h_tail, name);
    // Turn off all domains except oneCCL:API
    h->flags = IttCollector::Instance().GetCclDomain(h) == h;
  }
  __itt_mutex_unlock(&(itt_global->mutex));

  return h;
}

[[maybe_unused]] static std::string __itt_string_handle_dump() {
  if (itt_global == nullptr) {
    return "itt_global is NULL";
  }

  std::string result = "\nString handle list:\n";
  __itt_mutex_lock(&(itt_global->mutex));

  int count = 0;
  for (__itt_string_handle *h = itt_global->string_list; h != nullptr; h = h->next) {
    result += "  [" + std::to_string(count) + "] " + (h->strA ? h->strA : "NULL") + "\n";
    count++;
  }

  if (count == 0) {
    result += "  (empty string handle list)\n";
  } else {
    result += "  Total string handles: " + std::to_string(count);
  }

  __itt_mutex_unlock(&(itt_global->mutex));
  return result;
}

ITT_EXTERN_C __itt_string_handle *ITTAPI __itt_string_handle_create_impl(const char *name) {
  SPDLOG_DEBUG("{}() - name: {}", __FUNCTION__, name ? name : "NULL");

  if (itt_global == nullptr) {
    SPDLOG_WARN("{}(): itt_global is NULL", __FUNCTION__);
    return nullptr;
  }

  __itt_string_handle *h_tail = nullptr, *h = nullptr;

  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = nullptr, h = itt_global->string_list; h != nullptr; h_tail = h, h = h->next) {
    if (h->strA != nullptr && !__itt_fstrcmp(h->strA, name)) break;
  }
  if (h == nullptr) {
    NEW_STRING_HANDLE_A(itt_global, h, h_tail, name);

    // Capture pointer address for specific strings
    if (name != nullptr) {
      if (!__itt_fstrcmp(name, kSendSizeString)) {
        send_size_handle.store(h);
      } else if (!__itt_fstrcmp(name, kCommIdString)) {
        comm_id_handle.store(h);
      }
    }
  }
  __itt_mutex_unlock(&(itt_global->mutex));

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
               __LINE__, task.domain_ ? task.domain_->nameA : "NULL",
               task.name_ ? task.name_->strA : "NULL", task.start_time_, end, task.metadata_size_);

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

    if (check_and_capture(send_size_handle, kSendSizeString)) {
      IttCollector::task_desc_.top().metadata_size_ = *static_cast<uint64_t *>(data);
    } else if (check_and_capture(comm_id_handle, kCommIdString)) {
      IttCollector::task_desc_.top().communicator_id_ = *static_cast<uint64_t *>(data);
    }
    return;
  }
  SPDLOG_DEBUG("{}() Collector - NOT FOUND!!", __FUNCTION__);
}

void IttEnableCclDomain(void) {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);
  if (itt_global == nullptr) {
    return;
  }
  __itt_mutex_lock(&(itt_global->mutex));
  auto cclDomain = IttCollector::Instance().GetCclDomain(nullptr);
  if (cclDomain != nullptr) {
    const_cast<__itt_domain *>(cclDomain)->flags = 1;
  }
  __itt_mutex_unlock(&(itt_global->mutex));
}

void IttDisableCclDomain(void) {
  SPDLOG_DEBUG("{}() Collector", __FUNCTION__);
  if (itt_global == nullptr) {
    return;
  }
  __itt_mutex_lock(&(itt_global->mutex));
  auto cclDomain = IttCollector::Instance().GetCclDomain(nullptr);
  if (cclDomain != nullptr) {
    const_cast<__itt_domain *>(cclDomain)->flags = 0;
  }
  __itt_mutex_unlock(&(itt_global->mutex));
}
