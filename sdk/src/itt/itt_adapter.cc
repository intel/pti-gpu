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
#include <unordered_map>

#include "itt_common_helpers.h"
#include "itt_pti_view_export.h"
#include "pti_lib_handler.h"
#include "utils/utils.h"

// Forward declarations of adapter wrapper functions
static void ITTAPI IttTaskBegin(const __itt_domain *domain, __itt_id taskid, __itt_id parentid,
                                __itt_string_handle *name);
static void ITTAPI IttTaskEnd(const __itt_domain *domain);
static __itt_string_handle *ITTAPI IttStringHandleCreate(const char *name);
static void ITTAPI IttMetadataAdd(const __itt_domain *domain, __itt_id id, __itt_string_handle *key,
                                  __itt_metadata_type type, size_t count, void *data);
static __itt_domain *ITTAPI IttDomainCreate(const char *name);
static void ITTAPI IttApiInitAdapter(__itt_global *p, __itt_group_id init_groups);

static void FillFuncPtrPerLib(__itt_global *p) {
  // Ensure this function only runs once
  static std::atomic<bool> initialized{false};

  // Fast path: already initialized
  if (initialized.load(std::memory_order_acquire)) {
    return;
  }

  SPDLOG_TRACE("{}() Adapter:", __FUNCTION__);
  __itt_mutex_lock(&(p->mutex));

  // Double-check after acquiring lock
  if (initialized.load(std::memory_order_relaxed)) {
    __itt_mutex_unlock(&(p->mutex));
    return;
  }

  __itt_api_info *api_list = static_cast<__itt_api_info *>(p->api_list_ptr);

  // Static lookup table for ITT function mappings to adapter wrappers
  static const std::unordered_map<std::string_view, void *> itt_function_map = {
      {"__itt_task_begin", reinterpret_cast<void *>(&IttTaskBegin)},
      {"__itt_task_end", reinterpret_cast<void *>(&IttTaskEnd)},
      {"__itt_string_handle_create", reinterpret_cast<void *>(&IttStringHandleCreate)},
      {"__itt_metadata_add", reinterpret_cast<void *>(&IttMetadataAdd)},
      {"__itt_domain_create", reinterpret_cast<void *>(&IttDomainCreate)},
      {"__itt_api_init", reinterpret_cast<void *>(&IttApiInitAdapter)}};

  for (int i = 0; api_list[i].name != nullptr; i++) {
    void *func_ptr = nullptr;

    // Try to find in ITT function map first
    auto it = itt_function_map.find(api_list[i].name);
    if (it != itt_function_map.end()) {
      func_ptr = it->second;
    }

    // Use null_func if we didn't find a valid function pointer
    if (!func_ptr) {
      func_ptr = api_list[i].null_func;
    }

    *(api_list[i].func_ptr) = func_ptr;
  }

  // Mark as initialized
  initialized.store(true, std::memory_order_release);
  __itt_mutex_unlock(&(p->mutex));
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

// Cache for ITT init parameters
static struct {
  std::atomic<__itt_global *> global_ptr{nullptr};
  __itt_group_id init_groups{__itt_group_none};
} cached_init_params_;

// Retrieve cached ITT init parameters for PtiLibHandler
void PtiGetCachedIttInitParams(__itt_global **out_global_ptr, __itt_group_id *out_init_groups) {
  if (out_global_ptr != nullptr) {
    *out_global_ptr = cached_init_params_.global_ptr.load(std::memory_order_acquire);
  }
  if (out_init_groups != nullptr) {
    *out_init_groups = cached_init_params_.init_groups;
  }
}

// Adapter wrapper functions that check initialization before forwarding

static void ITTAPI IttTaskBegin(const __itt_domain *domain, __itt_id taskid, __itt_id parentid,
                                __itt_string_handle *name) {
  SPDLOG_TRACE("{}() Adapter, domain: {}, task name: {}", __FUNCTION__,
               domain ? domain->nameA : "NULL", name ? name->strA : "NULL");
  if (!pti::PtiLibHandler::IsInitialized()) {
    SPDLOG_DEBUG("{}(): PtiLibHandler not initialized, ignoring call", __FUNCTION__);
    return;
  }
  auto &handler = pti::PtiLibHandler::Instance();
  if (handler.__itt_task_begin_) {
    handler.__itt_task_begin_(domain, taskid, parentid, name);
  }
}

static void ITTAPI IttTaskEnd(const __itt_domain *domain) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  if (!pti::PtiLibHandler::IsInitialized()) {
    SPDLOG_DEBUG("{}(): PtiLibHandler not initialized, ignoring call", __FUNCTION__);
    return;
  }
  auto &handler = pti::PtiLibHandler::Instance();
  if (handler.__itt_task_end_) {
    handler.__itt_task_end_(domain);
  }
}

static __itt_string_handle *ITTAPI IttStringHandleCreate(const char *name) {
  SPDLOG_TRACE("{}() Adapter: {}", __FUNCTION__, name ? name : "NULL");
  if (!pti::PtiLibHandler::IsInitialized()) {
    auto itt_global = cached_init_params_.global_ptr.load(std::memory_order_acquire);
    if (itt_global == nullptr) {
      SPDLOG_DEBUG("{}(): PtiLibHandler not initialized and no cached global, returning nullptr",
                   __FUNCTION__);
      return nullptr;
    }

    SPDLOG_DEBUG("{}(): PtiLibHandler not initialized, create handle in libpti_view", __FUNCTION__);
    return itt_helpers::FindOrCreateStringHandle(itt_global, name);
  }

  auto &handler = pti::PtiLibHandler::Instance();
  if (handler.__itt_string_handle_create_) {
    return handler.__itt_string_handle_create_(name);
  }
  return nullptr;
}

static void ITTAPI IttMetadataAdd(const __itt_domain *domain, __itt_id id, __itt_string_handle *key,
                                  __itt_metadata_type type, size_t count, void *data) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  if (!pti::PtiLibHandler::IsInitialized()) {
    SPDLOG_DEBUG("{}(): PtiLibHandler not initialized, ignoring call", __FUNCTION__);
    return;
  }
  auto &handler = pti::PtiLibHandler::Instance();
  if (handler.__itt_metadata_add_) {
    handler.__itt_metadata_add_(domain, id, key, type, count, data);
  }
}

static __itt_domain *ITTAPI IttDomainCreate(const char *name) {
  SPDLOG_TRACE("{}() Adapter: domain name: {}", __FUNCTION__, name ? name : "NULL");

  if (!pti::PtiLibHandler::IsInitialized()) {
    // If PtiLibHandler is not initialized, we can still create domains but we need to do it here
    // and also disable them by default (flags = 0) since we don't want to pay any overhead for
    // task until collector (in libpti) is enabled
    SPDLOG_TRACE("{}(): PtiLibHandler not initialized, processing in libpti_view", __FUNCTION__);

    auto itt_global = cached_init_params_.global_ptr.load(std::memory_order_acquire);

    if (itt_global == nullptr || name == nullptr) {
      return nullptr;
    }

    __itt_domain *h_tail = nullptr, *h = nullptr;
    __itt_mutex_lock(&(itt_global->mutex));
    for (h_tail = nullptr, h = itt_global->domain_list; h != nullptr; h_tail = h, h = h->next) {
      if (h->nameA != nullptr && !__itt_fstrcmp(h->nameA, name)) break;
    }
    if (h == nullptr) {
      SPDLOG_DEBUG("{}(): Creating domain '{}' in libpti_view", __FUNCTION__, name);
      NEW_DOMAIN_A(itt_global, h, h_tail, name);
      // Turn off all domains
      h->flags = 0;
    }
    __itt_mutex_unlock(&(itt_global->mutex));

    return h;
  }

  auto &handler = pti::PtiLibHandler::Instance();
  if (handler.__itt_domain_create_) {
    return handler.__itt_domain_create_(name);
  }
  return nullptr;
}

static void ITTAPI IttApiInitAdapter(__itt_global *p, __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() Adapter: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p == nullptr) {
    return;
  }

  // Cache parameters
  cached_init_params_.init_groups = init_groups;
  cached_init_params_.global_ptr.store(p, std::memory_order_release);

  // If PtiLibHandler is already initialized, forward immediately to collector
  if (pti::PtiLibHandler::IsInitialized()) {
    SPDLOG_DEBUG("{}(): PtiLibHandler initialized, forwarding immediately", __FUNCTION__);
    auto &handler = pti::PtiLibHandler::Instance();
    if (handler.__itt_api_init_) {
      handler.__itt_api_init_(p, init_groups);
    }
  } else {
    SPDLOG_DEBUG("{}(): PtiLibHandler not initialized, caching init parameters", __FUNCTION__);
    // Parameters will be forwarded later by CommunicateCachedIttInit() when PtiLibHandler is
    // created
  }
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global *p, __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() Adapter: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p != nullptr) {
    FillFuncPtrPerLib(p);
    IttApiInitAdapter(p, init_groups);
    IttDomainClearFlags(p);
  }
}
