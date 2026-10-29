//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the intermediate metric sampling file (.t) reader, ze_metrics.h
// ReadMetricSegment(), as ze_metrics.h ComputeStallMetrics() and
// ComputeTimedMetrics() call it. The buffer is MAX_METRIC_BUFFER as in the tool.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "levelzero/ze_metrics.h"

static constexpr uint64_t kMaxMetricBuffer = 8ULL * 1024ULL * 1024ULL;

// libFuzzer entry point: one input is one .t metric sampling file.
//   data, size  the file contents
// Reads every segment into a MAX_METRIC_BUFFER buffer and checks that none is
// reported larger than the buffer. Returns 0; the trap or an ASan write past the
// buffer is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static std::vector<uint8_t> buffer(kMaxMetricBuffer);
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size),
                        std::ios::binary);
  size_t raw_size = 0;
  while (ReadMetricSegment(in, buffer.data(), kMaxMetricBuffer, raw_size) ==
         SegmentStatus::kOk) {
    if (raw_size > kMaxMetricBuffer) __builtin_trap();
  }
  return 0;
}
