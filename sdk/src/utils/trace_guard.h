//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_TRACE_GUARD_H_
#define PTI_TOOLS_CL_TRACER_TRACE_GUARD_H_

#include "pti_assert.h"

class TraceGuard {
 public:
  TraceGuard() { ++inactive_count_; }

  ~TraceGuard() {
    PTI_ASSERT(inactive_count_ > 0);
    --inactive_count_;
  }

  static bool Inactive() { return inactive_count_ > 0; }

 private:
  static thread_local int inactive_count_;
};

#endif  // PTI_TOOLS_CL_TRACER_TRACE_GUARD_H_