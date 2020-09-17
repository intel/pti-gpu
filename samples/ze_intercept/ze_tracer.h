//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_INTERCEPT_ZE_TRACER_H_
#define PTI_SAMPLES_ZE_INTERCEPT_ZE_TRACER_H_

#include <tracing.gen>

class ZeTracer {
 public:
  ZeTracer(ze_context_handle_t context, ZeIntercept* intercept) {
    PTI_ASSERT(context != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, intercept};
    status = zetTracerExpCreate(context, &tracer_desc, &tracer_);
    if (status != ZE_RESULT_SUCCESS) {
      tracer_ = nullptr;
    }
  }
  
  ~ZeTracer() {
    if (tracer_ != nullptr) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zetTracerExpDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  bool IsValid() const {
    return (tracer_ != nullptr);
  }

  bool Enable() {
    if (!IsValid()) {
      return false;
    }

    ze_tracing::SetTracingFunctions(tracer_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(tracer_, true);
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
    status = zetTracerExpSetEnabled(tracer_, false);
    if (status != ZE_RESULT_SUCCESS) {
      return false;
    }

    return true;
  }

 private:
  zet_tracer_exp_handle_t tracer_ = nullptr;
};

#endif // PTI_SAMPLES_ZE_INTERCEPT_ZE_TRACER_H_