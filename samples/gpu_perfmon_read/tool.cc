//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "gpu_perfmon_collector.h"

static GpuPerfMonCollector* collector = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./gpu_perfmon_read[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
  utils::SetEnv("ZET_ENABLE_PROGRAM_INSTRUMENTATION=1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static void PrintResults() {
  PTI_ASSERT(collector != nullptr);

  const KernelDataMap& kernel_data_map = collector->GetKernelDataMap();
  if (kernel_data_map.size() == 0) {
    std::cerr << "[WARNING] No kernels were collected" << std::endl;
    return;
  }

  std::cerr << std::endl;
  GpuPerfMonCollector::PrintResults(kernel_data_map);
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  PTI_ASSERT(collector == nullptr);
  collector = GpuPerfMonCollector::Create();
}

void DisableProfiling() {
  if (collector != nullptr) {
    PrintResults();
    delete collector;
  }
}