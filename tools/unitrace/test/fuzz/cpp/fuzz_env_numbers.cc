//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the numbers unitrace and the collector read from the environment and the
// command line: UNITRACE_DeviceId, PTI_DEVICE_ID and PTI_SUB_DEVICE_ID
// (utils_ze.h ParseDeviceIndex(), unitrace.cc, utils_ze.h),
// UNITRACE_ChromeEventBufferSize from --chrome-event-buffer-size
// (utils_host.h ParseChromeEventBufferSize(), unitrace.cc, chromelogger.h TraceBuffer) and the
// devices to sample as the metric profiler reads them (DevicesStringToSet(),
// ze_metrics.h). The collector runs inside the application, so an exception here
// terminates it. Input: device index \0 buffer size \0 device list.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

#include "utils_ze.h"

// libFuzzer entry point: one input is three environment values.
//   data, size  device index \0 Chrome event buffer size \0 device list
// Checks that an accepted device index is exactly the decimal number given (not a
// prefix or a truncation of it) and an accepted buffer size is not below -1, and
// runs the profiler's device list through DevicesStringToSet(). Returns 0; an
// uncaught exception (it would terminate the application) or the trap is the
// finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  std::string part[3];
  size_t pos = 0;
  for (int k = 0; k < 3 && pos <= input.size(); ++k) {
    size_t nul = input.find('\0', pos);
    part[k] = input.substr(pos, nul == std::string::npos ? std::string::npos : nul - pos);
    if (nul == std::string::npos) break;
    pos = nul + 1;
  }

  // An accepted index must be the number written, not a prefix or a truncation of it.
  uint32_t device = 0;
  if (ParseDeviceIndex(part[0], device) && !part[0].empty() &&
      (!IsDigits(part[0]) || std::strtoull(part[0].c_str(), nullptr, 10) != device)) {
    fprintf(stderr, "[FUZZ] device index %s accepted as %u\n", part[0].c_str(), device);
    __builtin_trap();
  }

  int capacity = 0;
  if (ParseChromeEventBufferSize(part[1], capacity) && capacity < -1) {
    fprintf(stderr, "[FUZZ] chrome event buffer size %s accepted as %d\n", part[1].c_str(), capacity);
    __builtin_trap();
  }

  std::set<int> devices;
  DevicesStringToSet(part[2], devices);
  return 0;
}
