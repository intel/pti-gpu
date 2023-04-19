//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iomanip>
#include <iostream>
#include <set>

#include "utils.h"
#include "ze_kernel_collector.h"

static ZeKernelCollector* collector = nullptr;
static std::chrono::steady_clock::time_point start;

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT
void Usage() {
  std::cout <<
    "Usage: ./ze_hot_kernels[.exe] <application> <args>" <<
    std::endl;
}

extern "C" PTI_EXPORT
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C" PTI_EXPORT
void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static void PrintResults() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  PTI_ASSERT(collector != nullptr);
  const ZeKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
  if (kernel_info_map.size() == 0) {
    return;
  }

  uint64_t total_duration = 0;
  for (auto& value : kernel_info_map) {
    total_duration += value.second.total_time;
  }

  std::cerr << std::endl;
  std::cerr << "=== Device Timing Results: ===" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Total Execution Time (ns): " << time.count() << std::endl;
  std::cerr << "Total Device Time (ns): " << total_duration << std::endl;
  std::cerr << std::endl;

  if (total_duration > 0) {
    ZeKernelCollector::PrintKernelsTable(kernel_info_map);
  }

  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  collector = ZeKernelCollector::Create();
  start = std::chrono::steady_clock::now();
}

void DisableProfiling() {
  if (collector != nullptr) {
    collector->DisableTracing();
    PrintResults();
    delete collector;
  }
}