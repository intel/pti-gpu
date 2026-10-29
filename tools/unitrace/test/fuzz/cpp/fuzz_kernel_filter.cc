//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the kernel filter parser (--include-kernels[-file], --exclude-kernels[-file]),
// ze_collector.h ParseFilterList(), as ze_collector.h calls it. The first input
// line is the --*-kernels string, the rest is the --*-kernels-file contents.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "levelzero/ze_collector.h"

// libFuzzer entry point: one input is one kernel filter.
//   data, size  first line: the --include-kernels string; the rest: the contents of
//               the --include-kernels-file
// Checks that every item ParseFilterList() returns is non-empty and has no leading
// or trailing whitespace. Returns 0; the trap is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  size_t nl = input.find('\n');
  std::string filter = input.substr(0, nl);
  std::istringstream file(nl == std::string::npos ? std::string() : input.substr(nl + 1));
  auto items = ::ParseFilterList(&file, filter);
  for (const auto& item : items) {
    // An item is never empty and never starts or ends with whitespace.
    if (item.empty() || isspace(static_cast<unsigned char>(item.front())) ||
        isspace(static_cast<unsigned char>(item.back()))) {
      __builtin_trap();
    }
  }
  return 0;
}
