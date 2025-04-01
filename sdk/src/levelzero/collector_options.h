//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SDK_SRC_COLLECTOR_OPTIONS_
#define SDK_SRC_COLLECTOR_OPTIONS_

#include <atomic>

// Structure holds flags that convey to collector that the associated viewKind is enabled by user.
//    Enabled flags trigger callbacks to view_handler to issue buffer record.
struct ViewsBufferable {
  std::atomic<bool> synch_enabled = false;      // is synchronization viewkind enabled by user.
  std::atomic<bool> api_calls_enabled = false;  // are driver api calls enabled by user.

  ViewsBufferable() : synch_enabled(false), api_calls_enabled(false) {}

  // to copy atomics
  ViewsBufferable(ViewsBufferable& other)
      : synch_enabled(other.synch_enabled.load()),
        api_calls_enabled(other.api_calls_enabled.load()) {}

  ViewsBufferable& operator=(const ViewsBufferable&) = delete;  // Not used.
  ViewsBufferable(ViewsBufferable&&) = delete;
  ViewsBufferable& operator=(const ViewsBufferable&&) = delete;
  virtual ~ViewsBufferable() = default;
};

struct CollectorOptions {
  // collector present in the process but does nothing, ready to start
  bool disabled_mode = true;
  // collector shifts to this mode only when introspection apis available and only when user forces
  // to be so.
  bool hybrid_mode = false;

  bool kernel_tracing = false;
  bool api_tracing = false;
  bool demangle = true;

  ViewsBufferable lz_enabled_views;
};

#endif  // SDK_SRC_COLLECTOR_OPTIONS_
