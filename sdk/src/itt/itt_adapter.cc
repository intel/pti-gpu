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
#include <spdlog/spdlog.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "pti_lib_handler.h"
#include "utils/utils.h"

namespace {
static std::atomic<__itt_global *> g_itt_global_of_ccl_domain{nullptr};
static std::atomic<const __itt_domain *> g_ccl_domain_cached{nullptr};

static const char *kNoIttGlobalOfCclDomainString = "No IttGlobalOfCclDomain available";
static const char *const kCclDomainString = "oneCCL::API";

inline const __itt_domain *GetCacheCclDomainAdapter(const __itt_domain *domain) {
  const __itt_domain *cached = g_ccl_domain_cached.load(std::memory_order_relaxed);
  if (cached != nullptr) return cached;
  if (domain != nullptr && domain->nameA != nullptr &&
      !std::strcmp(domain->nameA, kCclDomainString)) {
    const __itt_domain *expected = nullptr;
    g_ccl_domain_cached.compare_exchange_strong(expected, domain, std::memory_order_relaxed);
    return domain;
  }
  return nullptr;
}

}  // namespace

__itt_global *GetIttGlobalOfCclDomainAdapter() {
  __itt_global *g = g_itt_global_of_ccl_domain.load(std::memory_order_acquire);
  return g;
}

const __itt_domain *GetIttCclDomainAdapter() { return GetCacheCclDomainAdapter(nullptr); }

// Helper template to forward ITT calls with domain check (only forwards oneCCL::API domain)
template <typename Func, typename... Args>
static void ForwardIttCallIfCclDomain(const __itt_domain *domain, Func handler_member_ptr,
                                      Args &&...args) {
  if (domain) {
    if (GetCacheCclDomainAdapter(domain) == domain) {
      if (pti::PtiLibHandler::IsSuccessfullyInitialized()) {
        auto &handler = pti::PtiLibHandler::Instance();
        auto func = handler.*handler_member_ptr;
        if (func) {
          func(std::forward<Args>(args)...);
        }
      }

    } else {
      SPDLOG_DEBUG(
          "{}() Adapter Non-standard domain (global unknown): domain_name: {}, "
          "domain ptr: {} - Apply domain->flags = 0 ",
          __FUNCTION__, domain->nameA, static_cast<const void *>(domain));
      // this is a data race - but we better shut down this domain, to avoid overhead
      auto *mutable_domain = const_cast<__itt_domain *>(domain);
      mutable_domain->flags = 0;
    }
  }
}

// Adapter functions that wrap handler calls
static void ITTAPI itt_task_begin_adapter(const __itt_domain *domain, __itt_id taskid,
                                          __itt_id parentid, __itt_string_handle *name) {
  SPDLOG_DEBUG("{}() Adapter - domain_name: {}, domain ptr: {}, task_name: {}", __FUNCTION__,
               domain ? domain->nameA : "NULL", static_cast<const void *>(domain),
               name ? name->strA : "NULL");
  ForwardIttCallIfCclDomain(domain, &pti::PtiLibHandler::itt_task_begin_, domain, taskid, parentid,
                            name);
}

static void ITTAPI itt_task_end_adapter(const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() Adapter - domain_name: {}, domain ptr: {}", __FUNCTION__,
               domain ? domain->nameA : "NULL", static_cast<const void *>(domain));
  ForwardIttCallIfCclDomain(domain, &pti::PtiLibHandler::itt_task_end_, domain);
}

static void ITTAPI itt_metadata_add_adapter(const __itt_domain *domain, __itt_id id,
                                            __itt_string_handle *key, __itt_metadata_type type,
                                            size_t count, void *data) {
  SPDLOG_DEBUG("{}() Adapter - domain: {}, key: {}", __FUNCTION__, domain ? domain->nameA : "NULL",
               key ? key->strA : "NULL");
  ForwardIttCallIfCclDomain(domain, &pti::PtiLibHandler::itt_metadata_add_, domain, id, key, type,
                            count, data);
}

[[maybe_unused]] static std::string DumpIttStringHandlesOfCclDomainGlobal() {
  __itt_global *g = g_itt_global_of_ccl_domain.load(std::memory_order_acquire);
  if (g == nullptr) {
    return kNoIttGlobalOfCclDomainString;
  }

  std::string result = "\nString handle list:\n";
  __itt_mutex_lock(&(g->mutex));

  int count = 0;
  for (__itt_string_handle *h = g->string_list; h != nullptr; h = h->next) {
    result += "  [" + std::to_string(count) + "] " + (h->strA ? h->strA : "NULL") + "\n";
    count++;
  }

  if (count == 0) {
    result += "  (empty string handle list)\n";
  } else {
    result += "  Total string handles: " + std::to_string(count);
  }

  __itt_mutex_unlock(&(g->mutex));
  return result;
}

static __itt_string_handle *ITTAPI itt_string_handle_create_adapter(const char *name) {
  SPDLOG_DEBUG("{}() - name: {}", __FUNCTION__, name ? name : "NULL");

  __itt_global *g = g_itt_global_of_ccl_domain.load(std::memory_order_acquire);
  if (g == nullptr || name == nullptr) {
    return nullptr;
  }

  __itt_string_handle *h_tail = nullptr, *h = nullptr;

  __itt_mutex_lock(&(g->mutex));
  for (h_tail = nullptr, h = g->string_list; h != nullptr; h_tail = h, h = h->next) {
    if (h->strA != nullptr && !__itt_fstrcmp(h->strA, name)) break;
  }
  if (h == nullptr) {
    NEW_STRING_HANDLE_A(g, h, h_tail, name);

    if (!__itt_fstrcmp(name, itt_metadata::kSendSizeString)) {
      itt_metadata::GetSendSizeHandle().store(h);
    } else if (!__itt_fstrcmp(name, itt_metadata::kRecvSizeString)) {
      itt_metadata::GetRecvSizeHandle().store(h);
    } else if (!__itt_fstrcmp(name, itt_metadata::kCommIdString)) {
      itt_metadata::GetCommIdHandle().store(h);
    }
  }
  __itt_mutex_unlock(&(g->mutex));

  SPDLOG_DEBUG("{}() {}", __FUNCTION__, DumpIttStringHandlesOfCclDomainGlobal());
  return h;
}

static void FillFuncPtrPerLib(__itt_global *p) {
  SPDLOG_TRACE("{}() Adapter:", __FUNCTION__);
  __itt_api_info *api_list = static_cast<__itt_api_info *>(p->api_list_ptr);

  // Static lookup table for ITT function mappings to adapter functions
  static const std::unordered_map<std::string_view, void *> itt_function_map = {
      {"__itt_task_begin", reinterpret_cast<void *>(&itt_task_begin_adapter)},
      {"__itt_task_end", reinterpret_cast<void *>(&itt_task_end_adapter)},
      {"__itt_string_handle_create", reinterpret_cast<void *>(&itt_string_handle_create_adapter)},
      {"__itt_metadata_add", reinterpret_cast<void *>(&itt_metadata_add_adapter)}};

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

//
// Design note: how to capture the correct __itt_global for oneCCL::API.
//
// Every library that links ittnotify statically owns its own __itt_global.
// We need the one belonging to libccl.so because its mutex is the correct
// lock to use when toggling domain->flags, which is the switch that
// enables and disables oneCCL ITT calls issued for that domain.
//
// Key invariant PTI relies on: ITT provider in shared library
// creates all its domains before any other itt call issued, and the domains are present in
// its __itt_global when __itt_api_init is called.
// For oneCCL this should be always true:
// "oneCCL::API" is ALWAYS present in p_ccl->domain_list
// when __itt_api_init(p_ccl) is called oneCCL ITT initialization code.
// The domain variable in oneCCL sched_timer.cpp (as of June 20'2026)
// is a namespace-scope static, guaranteed by the C++ standard
// to be initialized before any external caller can invoke a
// function in libccl.so. domain_create's init stub does not trigger
// init_ittlib — it only allocates the node in p_ccl->domain_list.
// __itt_api_init is only triggered later by the first ITT_STUB call
// (e.g. task_begin), which can only happen after libccl.so is fully
// loaded. Therefore the domain node is always present when we walk
// itt_global_of_ccl->domain_list inside IttGlobalInitAndCclIfPresent.
//
// Assuming the key invariant - we do not intercept itt_domain_create.
//
// Scenario 1 (simple case): oneCCL is the only ITT library.
// The only __itt_global is the itt_global of ccl. We find the domain,
// set g_itt_global_of_ccl_domain to itt_global of ccl.
//
// Scenario 2: oneCCL loads last (e.g. p_tbb initializes first).
// p_tbb's __itt_api_init runs first:
// no CCL domain found, g_itt_global_of_ccl_domain is not set (remains nullptr),
// g_ccl_domain_cached is not set (remains nullptr).
// p_ccl's __itt_api_init runs next:
// CCL domain found, g_ccl_domain_cached CAS succeeds, g_itt_global_of_ccl_domain CAS succeeds also
//
// Scenario 3: oneCCL loads first (e.g. p_ccl initializes before p_tbb).
// p_ccl's __itt_api_init runs first: CCL domain found, g_ccl_domain_cached
// and g_itt_global both set to p_ccl. p_tbb's __itt_api_init runs later:
// we check that there is no oneCCL domain and
// do not attempt to overwrite g_itt_global_of_ccl_domain, and g_ccl_domain_cached
// as it is already set to p_ccl.
// The non-CCL domains in p_tbb get their flags zeroed out,
// as they are not relevant for our collector
//
// Scenario 4: Some libraries might not follow the key invariant
// (which assumes that all domains are present at __itt_api_init )
// Then we might get such domains tasks and other calls (as domain->flags is non-zero).
// In this case we make a data race and apply domain->flags = 0 to non-CCL domains,
// which is not ideal, but should not cause correctness issues,
// It helps avoid the overhead of the ITT calls issuing and processing.
//
static void ZeroAllDomainsFlagsAndStoreCclGlobalAndDomainIfSeeThem(__itt_global *p) {
  [[maybe_unused]] int count = 0;

  __itt_mutex_lock(&(p->mutex));

  for (__itt_domain *h = p->domain_list; h != nullptr; h = h->next) {
    h->flags = 0;
    if (h->nameA != nullptr && !std::strcmp(h->nameA, kCclDomainString)) {
      const __itt_domain *expected = nullptr;
      g_ccl_domain_cached.compare_exchange_strong(expected, h, std::memory_order_relaxed);
      __itt_global *expected_global = nullptr;
      g_itt_global_of_ccl_domain.compare_exchange_strong(expected_global, p,
                                                         std::memory_order_release);
    }
    count++;
  }
  __itt_mutex_unlock(&(p->mutex));
  SPDLOG_DEBUG("{}() processed {} domains", __FUNCTION__, count);
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global *p,
                                        [[maybe_unused]] __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() Adapter: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p == nullptr) {
    return;
  }

  FillFuncPtrPerLib(p);
  ZeroAllDomainsFlagsAndStoreCclGlobalAndDomainIfSeeThem(p);

  if (pti::PtiLibHandler::IsSuccessfullyInitialized()) {
    SPDLOG_DEBUG("{}() PTI_LIB is successfully initialized, setting CCL Global {} and Domain {}",
                 __FUNCTION__,
                 static_cast<void *>(g_itt_global_of_ccl_domain.load(std::memory_order_acquire)),
                 static_cast<const void *>(g_ccl_domain_cached.load(std::memory_order_acquire)));
    auto &handler = pti::PtiLibHandler::Instance();

    if (handler.IttCollectorSetCclGlobalAndDomain_ != nullptr) {
      handler.IttCollectorSetCclGlobalAndDomain_(
          g_itt_global_of_ccl_domain.load(std::memory_order_acquire),
          g_ccl_domain_cached.load(std::memory_order_acquire));
    } else {
      SPDLOG_WARN(
          "{}() IttCollectorSetCclGlobalAndDomain_ is NULL"
          ", cannot set CCL global and domain in collector",
          __FUNCTION__);
    }
  } else {
    SPDLOG_DEBUG(
        "{}() PTI_LIB is NOT initialized"
        ", skipping setting CCL Global and Domain in collector",
        __FUNCTION__);
  }
}
