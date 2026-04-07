//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE
#define ITTAPI_CDECL __attribute__((visibility("default")))
#include <ittnotify.h>
#include <ittnotify_config.h>

#include <unordered_map>

#include "pti_lib_handler.h"
#include "utils/utils.h"

static void FillFuncPtrPerLib(__itt_global *p) {
  SPDLOG_TRACE("{}() Adapter:", __FUNCTION__);
  __itt_api_info *api_list = static_cast<__itt_api_info *>(p->api_list_ptr);
  auto &handler = pti::PtiLibHandler::Instance();

  // Static lookup table for ITT function mappings
  static const std::unordered_map<std::string_view, void *> itt_function_map = {
      {"__itt_task_begin", reinterpret_cast<void *>(handler.__itt_task_begin_)},
      {"__itt_task_end", reinterpret_cast<void *>(handler.__itt_task_end_)},
      {"__itt_string_handle_create", reinterpret_cast<void *>(handler.__itt_string_handle_create_)},
      {"__itt_metadata_add", reinterpret_cast<void *>(handler.__itt_metadata_add_)},
      {"__itt_domain_create", reinterpret_cast<void *>(handler.__itt_domain_create_)},
      {"__itt_api_init", reinterpret_cast<void *>(handler.__itt_api_init_)}};

  for (int i = 0; api_list[i].name != NULL; i++) {
    void *func_ptr = nullptr;

    // Try to find in ITT function map first
    auto it = itt_function_map.find(api_list[i].name);
    if (it != itt_function_map.end()) {
      func_ptr = it->second;
    } else {
      // Fallback to original behavior for non-ITT functions
      func_ptr = static_cast<void *>(__itt_get_proc(p->lib, api_list[i].name));
    }

    // Use null_func if we didn't find a valid function pointer
    if (!func_ptr) {
      func_ptr = api_list[i].null_func;
    }

    *(api_list[i].func_ptr) = func_ptr;
  }
}

static std::atomic<const __itt_domain *> itt_ccl_domain_{nullptr};

static const __itt_domain *GetCclDomain(const __itt_domain *domain) {
  static const char *const kcclDomain = "oneCCL::API";

  // Fast path: if already initialized, return the cached domain
  const __itt_domain *cached = itt_ccl_domain_.load(std::memory_order_relaxed);
  if (cached != nullptr) {
    return cached;
  }

  // Slow path: check if this domain matches oneCCL and try to cache it
  if (domain != nullptr && domain->nameA != nullptr && !std::strcmp(domain->nameA, kcclDomain)) {
    const __itt_domain *expected = nullptr;
    // Only one thread will successfully update itt_ccl_domain_ from nullptr to domain
    // If compare_exchange_weak fails, another thread already set it, which is fine
    itt_ccl_domain_.compare_exchange_weak(expected, domain, std::memory_order_relaxed);
    return domain;
  }

  return nullptr;
}

static void IttDomainClearFlags(__itt_global *p) {
  if (p == nullptr) {
    return;
  }

  __itt_mutex_lock(&(p->mutex));

  [[maybe_unused]] int count = 0;
  for (__itt_domain *h = p->domain_list; h != nullptr; h = h->next) {
    h->flags = GetCclDomain(h) == h;
    count++;
  }

  __itt_mutex_unlock(&(p->mutex));
  SPDLOG_DEBUG("{}() cleared flags for {} domains", __FUNCTION__, count);
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global *p, __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() Adapter: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p != NULL) {
    FillFuncPtrPerLib(p);
    pti::PtiLibHandler::Instance().__itt_api_init_(p, init_groups);
    IttDomainClearFlags(p);
  }
}
