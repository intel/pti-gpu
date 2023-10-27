//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_
#define PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_

struct CollectorOptions {
  bool kernel_tracing = false;
  bool api_tracing = false;
  bool call_logging = false;
  bool need_tid = false;
  bool need_pid = false;
  bool verbose = false;
  bool demangle = true;
  bool kernels_per_tile = false;
  bool metric_query = false;
};

#endif  // PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_
