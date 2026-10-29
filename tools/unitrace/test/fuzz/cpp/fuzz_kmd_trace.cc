//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the raw KMD trace reader (--chrome-kmd-logging), unitrace.cc
// ReadKmdEvent() and KmdEventToJson(), as unitrace.cc DumpKmdTraceData() calls
// them. Oracle: every event written to oskmd.json must be valid JSON.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "unitrace.cc"  // UNITRACE_FUZZ leaves out its main()
#include "json_check.h"

// libFuzzer entry point: one input is one raw KMD trace (bpftrace output).
//   data, size  the trace
// Reads every event and checks that the Chrome trace event written for it is valid
// JSON. Returns 0; the trap is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
  KmdEvent event;
  while (ReadKmdEvent(in, event)) {
    std::string json = KmdEventToJson(event, event.timestamp_, event.duration_);
    if (!json_check::IsValid(json)) {
      fprintf(stderr, "[FUZZ] invalid JSON emitted: %s\n", json.c_str());
      __builtin_trap();
    }
  }
  return 0;
}
