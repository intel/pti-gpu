//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "cl_debug_info_collector.h"

static ClDebugInfoCollector* collector = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT
void Usage() {
  std::cout <<
    "Usage: ./cl_debug_info[.exe] <application> <args>" <<
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

  const KernelDebugInfoMap& debug_info_map =
    collector->GetKernelDebugInfoMap();
  if (debug_info_map.size() == 0) {
    return;
  }

  std::cerr << std::endl;
  for (auto pair : debug_info_map) {
    ClDebugInfoCollector::PrintKernelDebugInfo(pair.first, pair.second);
  }
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cerr << "[WARNING] Unable to find target GPU device for tracing" <<
      std::endl;
    return;
  }

  collector = ClDebugInfoCollector::Create(device);
}

void DisableProfiling() {
  if (collector != nullptr) {
    collector->DisableTracing();
    PrintResults();
    delete collector;
  }
}