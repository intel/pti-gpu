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

#include <atomic>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "pti_lib_handler.h"
#include "utils/utils.h"

// Helper template to forward ITT calls through handler
template <typename Func, typename... Args>
static void ForwardIttCall(Func handler_member_ptr, Args &&...args) {
  if (pti::PtiLibHandler::IsSuccessfullyInitialized()) {
    auto &handler = pti::PtiLibHandler::Instance();
    auto func = handler.*handler_member_ptr;
    if (func) {
      func(std::forward<Args>(args)...);
    }
  }
}

// Helper template to forward ITT calls with return values through handler
template <typename ReturnType, typename Func, typename... Args>
static ReturnType ForwardIttCallWithReturn(Func handler_member_ptr, Args &&...args) {
  ReturnType result = nullptr;

  if (pti::PtiLibHandler::IsSuccessfullyInitialized()) {
    auto &handler = pti::PtiLibHandler::Instance();
    auto func = handler.*handler_member_ptr;
    if (func) {
      result = func(std::forward<Args>(args)...);
    }
  }

  return result;
}

// Adapter functions that wrap handler calls
static void ITTAPI itt_task_begin_adapter(const __itt_domain *domain, __itt_id taskid,
                                          __itt_id parentid, __itt_string_handle *name) {
  ForwardIttCall(&pti::PtiLibHandler::__itt_task_begin_, domain, taskid, parentid, name);
}

static void ITTAPI itt_task_end_adapter(const __itt_domain *domain) {
  ForwardIttCall(&pti::PtiLibHandler::__itt_task_end_, domain);
}

static void ITTAPI itt_metadata_add_adapter(const __itt_domain *domain, __itt_id id,
                                            __itt_string_handle *key, __itt_metadata_type type,
                                            size_t count, void *data) {
  ForwardIttCall(&pti::PtiLibHandler::__itt_metadata_add_, domain, id, key, type, count, data);
}

static __itt_string_handle *ITTAPI itt_string_handle_create_adapter(const char *name) {
  return ForwardIttCallWithReturn<__itt_string_handle *>(
      &pti::PtiLibHandler::__itt_string_handle_create_, name);
}

static __itt_domain *ITTAPI itt_domain_create_adapter(const char *name) {
  return ForwardIttCallWithReturn<__itt_domain *>(&pti::PtiLibHandler::__itt_domain_create_, name);
}

static void ITTAPI itt_api_init_adapter(__itt_global *p, __itt_group_id init_groups) {
  ForwardIttCall(&pti::PtiLibHandler::__itt_api_init_, p, init_groups);
}

static void FillFuncPtrPerLib(__itt_global *p) {
  SPDLOG_TRACE("{}() Adapter:", __FUNCTION__);
  __itt_api_info *api_list = static_cast<__itt_api_info *>(p->api_list_ptr);

  // Static lookup table for ITT function mappings to adapter functions
  static const std::unordered_map<std::string_view, void *> itt_function_map = {
      {"__itt_task_begin", reinterpret_cast<void *>(&itt_task_begin_adapter)},
      {"__itt_task_end", reinterpret_cast<void *>(&itt_task_end_adapter)},
      {"__itt_string_handle_create", reinterpret_cast<void *>(&itt_string_handle_create_adapter)},
      {"__itt_metadata_add", reinterpret_cast<void *>(&itt_metadata_add_adapter)},
      {"__itt_domain_create", reinterpret_cast<void *>(&itt_domain_create_adapter)},
      {"__itt_api_init", reinterpret_cast<void *>(&itt_api_init_adapter)}};

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
  if (p == nullptr) {
    return;
  }

  FillFuncPtrPerLib(p);
  itt_api_init_adapter(p, init_groups);
  IttDomainClearFlags(p);
}
