//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SDK_SRC_COLLECTOR_OPTIONS_
#define SDK_SRC_COLLECTOR_OPTIONS_

struct CollectorOptions {
  // collector present in the process but does nothing, ready to start
  bool disabled_mode = true;
  // collector shifts to this mode only when introspection apis available and only when user forces
  // to be so.
  bool hybrid_mode = false;

  bool kernel_tracing = false;
  bool api_tracing = false;
  bool demangle = true;
};

#endif  // SDK_SRC_COLLECTOR_OPTIONS_
