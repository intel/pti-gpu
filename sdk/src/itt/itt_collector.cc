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

#include "utils.h"

//
// The ITT collector only supports oneCCL usage. The said
// library always uses the same domain name "oneCCL::API" for all its calls.
// By inserting the oneCCL::API in the domain list
// operations on domain are made on pointer equalities in contrast
// with expensive string equalities.
//
static const char *GetCclDomainString() {
  static const char *const itt_ccl_domain_cstr = []() {
    auto domain = __itt_domain_create("oneCCL::API");
    SPDLOG_DEBUG("GetCclDomainString() - CCL domain: {}", domain->nameA);
    return domain->nameA;
  }();
  return itt_ccl_domain_cstr;
}

//
// Helper function to determine if we should return early from ITT
// API calls based on tracing state and domain filtering
//
static inline bool IttReturnEarly(const __itt_domain *domain = nullptr) {
  if (!IttCollector::Instance().IsTraceEnabled()) {
    SPDLOG_TRACE("{}() Collector - tracing disabled, returning immediately", __FUNCTION__);
    return true;
  }
  if (domain && !(domain->nameA && domain->nameA == GetCclDomainString())) {
    return true;
  }
  return false;
}

thread_local std::stack<ThreadTaskDescriptor> task_desc;

// Static method definitions
std::string ThreadTaskDescriptor::DumpTaskDescriptor() noexcept {
  if (task_desc.empty()) {
    return "Task descriptor: (empty stack)";
  }

  std::string result = "\nTask descriptor stack:\n";
  std::stack<ThreadTaskDescriptor> temp_stack = task_desc;
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
    result += "  [" + std::to_string(count) + "] " + (h->nameA ? h->nameA : "NULL") + "\n";
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

  if (IttReturnEarly(domain)) {
    return;
  }

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, __itt_domain_dump());
  SPDLOG_DEBUG("{}() {}", __FUNCTION__, __itt_string_handle_dump());

  task_desc.push(ThreadTaskDescriptor(domain, name, utils::GetTime()));

  SPDLOG_DEBUG("Combined domain::name: {}::{} start_time: {}", domain->nameA, name->strA,
               utils::GetTime());
  SPDLOG_DEBUG("{}() {}", __FUNCTION__, ThreadTaskDescriptor::DumpTaskDescriptor());
}

ITT_EXTERN_C void ITTAPI __itt_task_end_impl(const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() Collector - domain: {}", __FUNCTION__, domain ? domain->nameA : "NULL");

  if (IttReturnEarly(domain)) {
    return;
  }

  auto end = utils::GetTime();

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, ThreadTaskDescriptor::DumpTaskDescriptor());

  if (task_desc.empty()) {
    return;
  }

  auto task = task_desc.top();
  task_desc.pop();

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
  if (IttReturnEarly(domain)) {
    return;
  }

  if (data == nullptr || type != __itt_metadata_u64 || count != 1) {
    return;
  }

  SPDLOG_DEBUG("{}() Collector - domain: {}, key: {}, send_size: 0x{:#x}", __FUNCTION__,
               domain ? domain->nameA : "NULL", key ? key->strA : "NULL",
               *static_cast<uint64_t *>(data));

  //
  // This is the ID of the CCL object associated with this metadata.
  // We only care about this ID.
  //
  if (type != __itt_metadata_u64 || count != 1) {
    return;
  }

  if (!task_desc.empty() && task_desc.top().domain == domain) {
    SPDLOG_DEBUG("{}() Collector - FOUND!!", __FUNCTION__);
    task_desc.top().metadata_size = *static_cast<uint64_t *>(data);
    return;
  }
  SPDLOG_DEBUG("{}() Collector - NOT FOUND!!", __FUNCTION__);
}

//