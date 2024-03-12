//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SDK_SRC_COLLECTOR_OPTIONS_
#define SDK_SRC_COLLECTOR_OPTIONS_

struct CollectorOptions {
  bool kernel_tracing = false;
  bool api_tracing = false;
  bool demangle = true;
};

#endif  // SDK_SRC_COLLECTOR_OPTIONS_
