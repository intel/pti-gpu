//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "pti/pti_callback.h"

#define PTI_CASE_ENUM_TO_STRING(e) \
  case e:                          \
    return #e;

/**
 * The rest of Callback functions are in levelzero subdirectory files
 * because currently Callback API supports only Level-Zero.
 */

const char* ptiCallbackDomainTypeToString(pti_callback_domain domain) {
  switch (domain) {
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_API)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_CONTEXT_CREATED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_HOST_SYNCHRONIZATION)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_MODULE_LOADED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_DRIVER_MODULE_UNLOADED)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_INTERNAL_EVENT)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_INTERNAL_THREADS)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_INVALID)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_DOMAIN_MAX)
  }

  return "INVALID";
}

const char* ptiCallbackPhaseTypeToString(pti_callback_phase phase) {
  switch (phase) {
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_API_ENTER)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_API_EXIT)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_INTERNAL_THREAD_START)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_INTERNAL_THREAD_END)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_INTERNAL_EVENT)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_INVALID)
    PTI_CASE_ENUM_TO_STRING(PTI_CB_PHASE_MAX)
  }

  return "INVALID";
}
