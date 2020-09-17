//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_ZE_TRACER_H_
#define PTI_SAMPLES_UTILS_ZE_TRACER_H_

#include <set>

#include "pti_assert.h"

#include "tracing.gen"

#define ZE_FUNCTION_COUNT      (ze_tracing::ZE_FUNCTION_COUNT)
#define ZE_CALLBACK_SITE_ENTER (ze_tracing::ZE_CALLBACK_SITE_ENTER)
#define ZE_CALLBACK_SITE_EXIT  (ze_tracing::ZE_CALLBACK_SITE_EXIT)

using callback_data_t = ze_tracing::callback_data_t;
using function_id_t = ze_tracing::function_id_t;
using tracing_callback_t = ze_tracing::tracing_callback_t;

class ZeTracer {
 public:
  ZeTracer(ze_context_handle_t context,
           tracing_callback_t callback,
           void* user_data) {
    PTI_ASSERT(context != nullptr);

    data_.callback = callback;
    data_.user_data = user_data;

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, &data_};
    status = zetTracerExpCreate(context, &tracer_desc, &handle_);
    if (status != ZE_RESULT_SUCCESS) {
      handle_ = nullptr;
    }
  }

  ~ZeTracer() {
    if (IsValid()) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zetTracerExpDestroy(handle_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  bool SetTracingFunction(function_id_t function) {
    if (!IsValid()) {
      return false;
    }

    if (function >= 0 && function < ZE_FUNCTION_COUNT) {
      functions_.insert(function);
      return true;
    }

    return false;
  }

  bool Enable() {
    if (!IsValid()) {
      return false;
    }

    ze_tracing::SetTracingFunctions(handle_, functions_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(handle_, true);
    if (status != ZE_RESULT_SUCCESS) {
      return false;
    }

    return true;
  }

  bool Disable() {
    if (!IsValid()) {
      return false;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(handle_, false);
    if (status != ZE_RESULT_SUCCESS) {
      return false;
    }

    return true;
  }

  bool IsValid() const {
    return (handle_ != nullptr);
  }

 private:
  zet_tracer_exp_handle_t handle_ = nullptr;
  std::set<function_id_t> functions_;
  ze_tracing::global_data_t data_;
};

#endif // PTI_SAMPLES_UTILS_ZE_TRACER_H_