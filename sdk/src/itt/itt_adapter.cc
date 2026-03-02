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

#include "pti_lib_handler.h"
#include "utils/utils.h"

static __itt_global *itt_global_adapter = NULL;

static void fill_func_ptr_per_lib(__itt_global *p) {
  SPDLOG_TRACE("{}() Adapter:", __FUNCTION__);
  __itt_api_info *api_list = static_cast<__itt_api_info *>(p->api_list_ptr);

  for (int i = 0; api_list[i].name != NULL; i++) {
    *(api_list[i].func_ptr) = static_cast<void *>(__itt_get_proc(p->lib, api_list[i].name));
    if (*(api_list[i].func_ptr) == NULL) {
      *(api_list[i].func_ptr) = api_list[i].null_func;
    }
  }
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global *p, __itt_group_id init_groups) {
  SPDLOG_DEBUG("{}() Adapter: {}", __FUNCTION__, p ? "non-NULL" : "NULL");
  if (p != NULL) {
    fill_func_ptr_per_lib(p);
    itt_global_adapter = p;
  }
  pti::PtiLibHandler::Instance().__itt_api_init_(p, init_groups);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin(const __itt_domain *domain, __itt_id taskid,
                                          __itt_id parentid, __itt_string_handle *name) {
  SPDLOG_DEBUG("{}() Adapter - domain: {}, name: {}", __FUNCTION__, domain ? domain->nameA : "NULL",
               name ? name->strA : "NULL");
  pti::PtiLibHandler::Instance().__itt_task_begin_(domain, taskid, parentid, name);
}

ITT_EXTERN_C void ITTAPI __itt_task_end(const __itt_domain *domain) {
  SPDLOG_DEBUG("{}() Adapter - domain: {}", __FUNCTION__, domain ? domain->nameA : "NULL");
  pti::PtiLibHandler::Instance().__itt_task_end_(domain);
}

ITT_EXTERN_C __itt_string_handle *ITTAPI __itt_string_handle_create(const char *name) {
  SPDLOG_TRACE("{}() Adapter - name: {}", __FUNCTION__, name ? name : "NULL");
  return pti::PtiLibHandler::Instance().__itt_string_handle_create_(name);
}

ITT_EXTERN_C void ITTAPI __itt_metadata_add(const __itt_domain *domain, __itt_id id,
                                            __itt_string_handle *key, __itt_metadata_type type,
                                            size_t count, void *data) {
  SPDLOG_DEBUG("{}() Adapter - domain: {}, key: {}", __FUNCTION__, domain ? domain->nameA : "NULL",
               key ? key->strA : "NULL");
  return pti::PtiLibHandler::Instance().__itt_metadata_add_(domain, id, key, type, count, data);
}

ITT_EXTERN_C __itt_domain *ITTAPI __itt_domain_create(const char *name) {
  SPDLOG_DEBUG("{}() Adapter - name: {}", __FUNCTION__, name ? name : "NULL");
  if (itt_global_adapter == NULL) {
    return NULL;
  }

  __itt_domain *h_tail = NULL, *h = NULL;

  __itt_mutex_lock(&(itt_global_adapter->mutex));
  for (h_tail = NULL, h = itt_global_adapter->domain_list; h != NULL; h_tail = h, h = h->next) {
    if (h->nameA != NULL && !__itt_fstrcmp(h->nameA, name)) break;
  }
  if (h == NULL) {
    NEW_DOMAIN_A(itt_global_adapter, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global_adapter->mutex));

  return h;
}
