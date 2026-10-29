//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzzes the --devices-to-sample and --ranks-to-sample parsers and the MPI
// launcher environment they read (PMI_SIZE, PMI_RANK, ...), utils_ze.h
// ParseDeviceList() and ParseRankList(), as utils_ze.h GetZeDevicesToSample()
// and GetZeRanksToSample() call them. Input: list \0 size \0 rank.

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include "utils_ze.h"

// libFuzzer entry point: one input is one sample list and MPI environment.
//   data, size  list \0 MPI size \0 MPI rank
// Parses list as --devices-to-sample and as --ranks-to-sample. Returns 0; an
// uncaught exception or a crash is the finding.
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
  std::set<int> devices;
  (void)ParseDeviceList(part[0], devices);
  std::set<int> ranks;
  (void)ParseRankList(part[0], part[1], part[2], ranks);
  return 0;
}
