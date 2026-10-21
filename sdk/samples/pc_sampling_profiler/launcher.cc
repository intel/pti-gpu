//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

constexpr uint32_t kMinSamplingPeriodNs = 1'000;
constexpr uint32_t kMaxSamplingPeriodNs = 10'000'000;

void Usage(const char* name) {
  std::cout << "Usage: " << name << " [options] -- <application> [arguments...]" << std::endl;
  std::cout << "\nOptions:" << std::endl;
  std::cout << "  --period [-p] <ns>  Sampling period in nanoseconds (range: "
            << kMinSamplingPeriodNs << "-" << kMaxSamplingPeriodNs << ")" << std::endl;
  std::cout << "  --output [-o] <file>  Write stall data to <file> instead of standard error"
            << std::endl;
  std::cout << "  --help [-h]         Display this help message" << std::endl;
  std::cout << "\nExample:" << std::endl;
  std::cout << "  " << name << " -- ./pc_sampling_profiler_l0_gemm ./gemm.spv" << std::endl;
  std::cout << "  " << name << " -- ./pc_sampling_profiler_gemm --size 2048 --repeat 3"
            << std::endl;
  std::cout << "  " << name << " -p 100000 -o stalls.txt -- ./my_application --arg value"
            << std::endl;
}

bool ParsePeriod(const char* argument, uint32_t* result) {
  char* end = nullptr;
  const unsigned long value = std::strtoul(argument, &end, 10);
  if (end == argument || *end != '\0' || value < kMinSamplingPeriodNs ||
      value > kMaxSamplingPeriodNs) {
    return false;
  }
  *result = static_cast<uint32_t>(value);
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  int target_index = 0;
  unsetenv("PTI_PC_SAMPLING_PERIOD_NS");
  unsetenv("PTI_PC_SAMPLING_OUTPUT");

  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "--") == 0) {
      target_index = index + 1;
      break;
    }
    if (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0) {
      Usage(argv[0]);
      return EXIT_SUCCESS;
    }
    if (std::strcmp(argument, "-p") == 0 || std::strcmp(argument, "--period") == 0) {
      uint32_t period = 0;
      if (++index >= argc || !ParsePeriod(argv[index], &period)) {
        std::cerr << "Error: --period requires a value in range [" << kMinSamplingPeriodNs << ", "
                  << kMaxSamplingPeriodNs << "] ns" << std::endl;
        Usage(argv[0]);
        return EXIT_FAILURE;
      }
      setenv("PTI_PC_SAMPLING_PERIOD_NS", std::to_string(period).c_str(), 1);
      continue;
    }
    if (std::strcmp(argument, "-o") == 0 || std::strcmp(argument, "--output") == 0) {
      if (++index >= argc || argv[index][0] == '\0') {
        std::cerr << "Error: --output requires a file path" << std::endl;
        Usage(argv[0]);
        return EXIT_FAILURE;
      }
      setenv("PTI_PC_SAMPLING_OUTPUT", argv[index], 1);
      continue;
    }

    std::cerr << "Error: unknown option '" << argument << "'" << std::endl;
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (target_index == 0 || target_index >= argc) {
    std::cerr << "Error: expected an application after --" << std::endl;
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (access(PC_SAMPLING_COLLECTOR_PATH, R_OK) != 0) {
    std::cerr << "Error: collector library '" << PC_SAMPLING_COLLECTOR_PATH
              << "' is not readable: " << std::strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }

  const char* previous_preload = std::getenv("LD_PRELOAD");
  std::string preload = PC_SAMPLING_COLLECTOR_PATH;
  if (previous_preload != nullptr && previous_preload[0] != '\0') {
    preload += ":";
    preload += previous_preload;
  }

  setenv("LD_PRELOAD", preload.c_str(), 1);
  // An inherited owner PID would keep the target from sampling; exec keeps this PID for it.
  unsetenv("PTI_PC_SAMPLING_PID");

  execvp(argv[target_index], argv + target_index);
  std::cerr << "Error: failed to execute '" << argv[target_index] << "': " << std::strerror(errno)
            << std::endl;
  return EXIT_FAILURE;
}
