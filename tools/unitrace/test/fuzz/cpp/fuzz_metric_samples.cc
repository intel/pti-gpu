//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the checks on what the driver returns for the sampled metrics, in place of
// a Level Zero mock: ze_metrics.h CalculatedSamplesFit(), which ze_metrics.h
// ComputeStallMetrics() and ComputeTimedMetrics() call on the counts of
// zetMetricGroupCalculateMultipleMetricValuesExp before reading the values, and
// UnwrapTimestamp() on the sampling timestamps. When the counts are accepted, the
// values are read row by row as both loops in ze_metrics.h read them, so ASan
// reports a read past the buffer. The loops are copies (the real ones call Level
// Zero); the stall array they fill is bounded by ComputeStallMetrics() itself,
// which rejects more than max_num_of_stall_types stall metrics up front.
// Input: the timestamps (uint64 previous, ts, span), then num_samples, num_values
// (uint16 each), row_size (uint8), then the samples[] entries (uint32 each).

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "levelzero/ze_metrics.h"

// Takes the next sizeof(T) bytes of the input as out (unaligned, host byte order)
// and advances data and size past them. Returns false, leaving everything as it
// is, when fewer bytes are left.
template <typename T>
static bool Take(const uint8_t*& data, size_t& size, T& out) {
  if (size < sizeof(T)) return false;
  memcpy(&out, data, sizeof(T));
  data += sizeof(T);
  size -= sizeof(T);
  return true;
}

// libFuzzer entry point: one input is one set of driver results.
//   data, size  previous, ts, span (uint64 each), then num_samples, num_values
//               (uint16 each), row_size (uint8), then the samples[] entries
//               (uint32 each)
// Checks UnwrapTimestamp() against the loop it replaced, then, if
// CalculatedSamplesFit() accepts the counts, reads the values as both ze_metrics.h
// loops do. Returns 0; the trap or an ASan read past the values is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  uint64_t previous = 0, ts = 0, span = 0;
  if (Take(data, size, previous) && Take(data, size, ts) && Take(data, size, span)) {
    const uint64_t unwrapped = UnwrapTimestamp(ts, previous, span);
    // The result is the first whole number of spans on that is not before previous,
    // as the loop it replaced (while (previous > ts) ts += span) computed; where that
    // does not fit a uint64_t, previous.
    if (span != 0 && ts < previous) {
      const unsigned __int128 expected =
          ts + (static_cast<unsigned __int128>((previous - ts - 1) / span) + 1) * span;
      if (unwrapped != (expected <= UINT64_MAX ? static_cast<uint64_t>(expected) : previous)) {
        fprintf(stderr, "[FUZZ] UnwrapTimestamp(%lu, %lu, %lu) = %lu\n", ts, previous, span, unwrapped);
        __builtin_trap();
      }
    }
  }

  uint16_t num_samples = 0, num_values = 0;
  uint8_t row_size = 0;
  if (!Take(data, size, num_samples) || !Take(data, size, num_values) || !Take(data, size, row_size)) {
    return 0;
  }
  // The buffers ze_metrics.h allocates from the first call's counts; the driver's
  // second call returns the counts under test.
  std::vector<uint32_t> samples(size / sizeof(uint32_t));
  if (!samples.empty()) memcpy(samples.data(), data, samples.size() * sizeof(uint32_t));
  const size_t values_capacity = samples.size() * 4;
  std::vector<uint64_t> values(values_capacity);

  if (!CalculatedSamplesFit(samples.data(), num_samples, samples.size(), num_values,
                                           values.size(), row_size)) {
    return 0;
  }
  volatile uint64_t sink = 0;
  const uint64_t* value = values.data();
  for (uint32_t i = 0; i < num_samples; ++i) {
    // ComputeStallMetrics: rows of row_size values from j.
    for (uint32_t j = 0; j + row_size <= samples[i]; j += row_size) {
      for (uint32_t k = 0; k < row_size; ++k) sink += value[j + k];
    }
    // ComputeTimedMetrics: samples[i] / row_size rows.
    for (uint32_t j = 0; j < samples[i] / row_size; ++j) {
      for (uint32_t k = 0; k < row_size; ++k) sink += value[j * row_size + k];
    }
    value += samples[i];
  }
  return 0;
}
