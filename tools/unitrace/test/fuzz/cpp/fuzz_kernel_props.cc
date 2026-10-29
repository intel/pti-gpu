//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the kernel properties (.kprops) and kernel times (.ktime) readers,
// ze_metrics.h ReadKernelPropsRecord() and ReadKernelTimeRecord(), as
// ze_metrics.h ReadModuleAndKernelProps() and ComputeTimedMetrics() call them.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "levelzero/ze_metrics.h"

// libFuzzer entry point: one input is read both as a .kprops and as a .ktime file.
//   data, size  the file contents
// Reads every record until the reader stops. Returns 0; a crash, hang or sanitizer
// report is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  {
    std::istringstream in(input);
    KernelPropsRecord record;
    while (ReadKernelPropsRecord(in, record)) {
    }
  }
  {
    std::istringstream in(input);
    KernelTimeRecord record;
    std::string kernel_name;
    // A .ktime record is the four fields, then the kernel name on its own line,
    // which ComputeTimedMetrics() reads after each record.
    while (ReadKernelTimeRecord(in, record) && std::getline(in, kernel_name)) {
    }
  }
  return 0;
}
