//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the intermediate metric query file (.q) reader, ze_metrics.h
// ReadMetricQueryRecord(), as ze_metrics.h ComputeMetricsQueried() calls it.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "levelzero/ze_metrics.h"

// libFuzzer entry point: one input is one .q metric query file.
//   data, size  the file contents
// Reads every record until the reader stops. Returns 0; a crash, an allocation
// beyond -malloc_limit_mb or a sanitizer report is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size),
                        std::ios::binary);
  MetricQueryRecord record;
  while (ReadMetricQueryRecord(in, record)) {
  }
  return 0;
}
