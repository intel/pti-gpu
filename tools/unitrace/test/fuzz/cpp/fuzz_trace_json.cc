//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the strings unitrace writes into JSON: kernel, ITT and device names
// (chromelogger.h JsonNameField(), utils_host.h JsonString()) and the
// command line written to the --result-dir config log (unitrace.cc
// CreateConfigLog() and StringifyJsonArray()). Names come from the
// module binary (GetZeKernelCommandName quotes them), the application (ITT) and
// the driver. Input: kernel name \0 device or ITT name \0 argv[0] \0 argv[1] ...
// Oracle: every record must be valid JSON.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "unitrace.cc"  // UNITRACE_FUZZ leaves out its main()
#include "tracer.h"  // chromelogger.h, after the headers it relies on
#include "json_check.h"

// Traps, after printing it, if json is not valid JSON: the harness's oracle.
static void Check(const std::string& json) {
  if (!json_check::IsValid(json)) {
    fprintf(stderr, "[FUZZ] invalid JSON emitted: %s\n", json.c_str());
    __builtin_trap();
  }
}

// libFuzzer entry point: one input is one set of names and arguments.
//   data, size  kernel name \0 device or ITT name \0 argv[0] \0 argv[1] ...
// Builds the trace and config-log records unitrace writes for them (kernel events
// with and without a tile prefix, process/thread metadata, ITT arguments, the
// app_args line) and checks each with Check(). Returns 0; the trap is the
// finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  std::vector<std::string> part;
  size_t pos = 0;
  while (true) {
    size_t nul = input.find('\0', pos);
    part.push_back(input.substr(pos, nul == std::string::npos ? std::string::npos : nul - pos));
    if (nul == std::string::npos) break;
    pos = nul + 1;
  }
  const std::string& kernel = part[0];
  const std::string other = part.size() > 1 ? part[1] : "";

  // Kernel event, with and without implicit scaling (chromelogger.h FlushDeviceBuffer).
  Check("{\"ph\": \"X\"" + JsonNameField(kernel) + "}");
  Check("{\"ph\": \"X\"" + JsonNameField(kernel, "Tile #0: ") + "}");
  // Device process/thread metadata, ITT argument key and string value.
  Check("{\"args\": {\"name\": " + JsonString(other) + "}}");
  Check("{\"args\": {" + JsonString(other) + ":[" + JsonString(other) + "]}}");

  // Config log: app and unitrace arguments (C strings, so no embedded NUL).
  std::vector<char*> argv;
  for (size_t k = 2; k < part.size(); ++k) {
    argv.push_back(&part[k][0]);
  }
  argv.push_back(nullptr);
  Check("{" + StringifyJsonArray("app_args", argv.data()) + "\"pid\": 0}");
  return 0;
}
