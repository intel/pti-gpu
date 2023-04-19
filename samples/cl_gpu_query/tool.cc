//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iomanip>
#include <iostream>
#include <set>

#include "cl_metric_collector.h"

static ClMetricCollector* collector = nullptr;
static std::chrono::steady_clock::time_point start;

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT
void Usage() {
  std::cout <<
    "Usage: ./cl_gpu_query[.exe] <application> <args>" <<
    std::endl;
}

extern "C" PTI_EXPORT
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C" PTI_EXPORT
void SetToolEnv() {}

// Internal Tool Functionality ////////////////////////////////////////////////

static void PrintResults() {
  PTI_ASSERT(collector != nullptr);

  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  const ClKernelInfoMap& kernel_map = collector->GetKernelInfoMap();
  if (kernel_map.empty()) {
    return;
  }

  uint64_t total_duration = 0;
  for (auto& value : kernel_map) {
    total_duration += value.second.total_time;
  }
  PTI_ASSERT(total_duration > 0);

  std::cerr << std::endl;
  std::cerr << "=== Device Metrics: ===" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Total Execution Time (ns): " << time.count() << std::endl;
  std::cerr << "Total Kernel Time (ns): " << total_duration << std::endl;
  std::cerr << std::endl;
  ClMetricCollector::PrintKernelsTable(kernel_map);
  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cerr << "[WARNING] Unable to find target GPU device for tracing" <<
      std::endl;
    return;
  }

  collector = ClMetricCollector::Create(device, "ComputeBasic");
  if (collector == nullptr) {
    return;
  }

  start = std::chrono::steady_clock::now();
}

void DisableProfiling() {
  if (collector != nullptr) {
    collector->DisableTracing();
    PrintResults();
    delete collector;
  }
}