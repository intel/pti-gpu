//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <dlfcn.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "../pc_sampling/pc_sampling_client.h"
#include "pti/pti.h"

namespace {

constexpr uint32_t kDefaultSamplingPeriodNs = 50'000;

bool g_active = false;

std::ofstream g_output_file;
std::ostream* g_output = &std::cerr;

// Set only by the launcher, which validates the range.
uint32_t SamplingPeriodFromEnvironment() {
  const char* value = std::getenv("PTI_PC_SAMPLING_PERIOD_NS");
  return value != nullptr ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10))
                          : kDefaultSamplingPeriodNs;
}

void ConfigureOutput() {
  const char* output_path = std::getenv("PTI_PC_SAMPLING_OUTPUT");
  if (output_path == nullptr) {
    return;
  }

  g_output_file.open(output_path, std::ios::out | std::ios::trunc);
  if (!g_output_file) {
    std::cerr << "Unable to open PC sampling output file '" << output_path
              << "'; writing to standard error" << std::endl;
    return;
  }
  g_output = &g_output_file;
  std::cerr << "PC sampling output file: " << output_path << std::endl;
}

// LD_PRELOAD is inherited, but a process enabling PC sampling blocks while another one samples,
// so only the first process started with the collector samples and its descendants do not.
bool ClaimCollection() {
  const std::string pid = std::to_string(getpid());
  const char* owner = std::getenv("PTI_PC_SAMPLING_PID");
  if (owner != nullptr) {
    return pid == owner;
  }
  return setenv("PTI_PC_SAMPLING_PID", pid.c_str(), 1) == 0;
}

void StopCollectionAndReport() {
  if (!g_active) {
    return;
  }
  g_active = false;

  if (StopPcSamplingCollection(*g_output)) {
    PrintPcSamplingData(*g_output);
  }
  DisablePcSampling(*g_output);
  CleanupProfilingEnvironment(*g_output);
  g_output->flush();
}

// Releases whatever it acquired on failure.
bool InitializeCollector() {
  ConfigureOutput();
  std::ostream& output = *g_output;
  output << "\n=== PC Sampling Profiler Collector ===" << std::endl;

  if (!InitializeProfilingEnvironment(output)) {
    return false;
  }

  if (EnablePcSampling(output) == PTI_SUCCESS) {
    if (ConfigurePcSampling(nullptr, 0, SamplingPeriodFromEnvironment(), output) == PTI_SUCCESS &&
        StartPcSamplingCollection(output)) {
      return true;
    }
    DisablePcSampling(output);
  }
  CleanupProfilingEnvironment(output);
  return false;
}

using MainFunction = int (*)(int, char**, char**);
MainFunction g_application_main = nullptr;

// Collection spans main(): PTI initializes Level Zero itself, so no runtime API is interposed,
// and the report runs before atexit handlers and static destructors tear the runtime down.
// glibc passes envp even to a two-parameter main, so the indirect call below is expected to
// mismatch the callee's declared type and -fsanitize=function must not flag it.
#ifdef __clang__
__attribute__((no_sanitize("function")))
#endif
int MainWrapper(int argc, char** argv, char** envp) {
  g_active = ClaimCollection() && InitializeCollector();
  const int status = g_application_main(argc, argv, envp);
  StopCollectionAndReport();
  return status;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) int __libc_start_main(
    MainFunction main_function, int argc, char** argv, MainFunction init, void (*fini)(void),
    void (*rtld_fini)(void), void* stack_end) {
  // glibc declares init as __typeof(main), hence MainFunction here; it is only forwarded.
  using LibcStartMainFunction =
      int (*)(MainFunction, int, char**, MainFunction, void (*)(void), void (*)(void), void*);
  static auto real_libc_start_main =
      reinterpret_cast<LibcStartMainFunction>(dlsym(RTLD_NEXT, "__libc_start_main"));
  if (real_libc_start_main == nullptr) {
    std::abort();
  }

  g_application_main = main_function;
  return real_libc_start_main(MainWrapper, argc, argv, init, fini, rtld_fini, stack_end);
}
