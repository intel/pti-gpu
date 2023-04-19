//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "gpu_inst_count_collector.h"

static GpuInstCountCollector* collector = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT
void Usage() {
  std::cout <<
    "Usage: ./gpu_inst_count[.exe] <application> <args>" <<
    std::endl;
}

extern "C" PTI_EXPORT
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C" PTI_EXPORT
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP", "1");
  utils::SetEnv("ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1");
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
  GpuInstCountCollector::PrintResults(kernel_data_map);
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  PTI_ASSERT(collector == nullptr);
  collector = GpuInstCountCollector::Create();
}

void DisableProfiling() {
  if (collector != nullptr) {
    PrintResults();
    delete collector;
  }
}