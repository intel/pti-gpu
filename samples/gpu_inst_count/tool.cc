//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#if defined(_WIN32)
#define NOMINMAX
#endif

#include "gpu_inst_count.hpp"
#include "utils.h"

using namespace gtpin_prof;
using namespace gtpin;

class GpuInstCountTxtWriter : public StreamGTPinDataWriter {
 public:
  using StreamGTPinDataWriter::StreamGTPinDataWriter;

  bool Init() final {
    if (sh == nullptr) {
      sh = new StreamHolder(std::cout);
    }
    if (sh == nullptr) return false;
    return true;
  }

  void Write(const std::shared_ptr<ProfilerData> res) {
    auto gpuInstCountProfRes = std::dynamic_pointer_cast<GpuInstCountProfilerData>(res);
    for (auto kerData : res->kernels) {
      auto gpuInstCountKernRes = std::dynamic_pointer_cast<GpuInstCountKernelData>(kerData.second);
      if (gpuInstCountKernRes->invocations.size() == 0) {
        continue;
      }
      GetStream() << "=== " << gpuInstCountKernRes->kernelName << "(runs "
                  << gpuInstCountKernRes->invocations.size() << " times) ===\n";
      std::map<InstructionOffset, size_t> instCount;
      size_t maxCount = 0;
      for (auto invoc : gpuInstCountKernRes->invocations) {
        for (auto data : invoc.second->data) {
          auto gpuInstCountResult = std::dynamic_pointer_cast<GpuInstCountResultData>(data);
          if (instCount.find(gpuInstCountResult->instructionOffset) == instCount.end()) {
            instCount[gpuInstCountResult->instructionOffset] = 0;
          }
          instCount[gpuInstCountResult->instructionOffset] += gpuInstCountResult->count;
          maxCount = std::max(maxCount, instCount[gpuInstCountResult->instructionOffset]);
        }
      }
      std::string maxCountStr = std::to_string(maxCount);

      for (auto asmLine : gpuInstCountKernRes->origAsm) {
        auto it = instCount.lower_bound(asmLine.instructionOffset + 1);
        if (it != instCount.begin()) it--;
        GetStream() << "[" << std::dec << std::setw(maxCountStr.size() + 1) << it->second << "] 0x"
                    << std::setw(6) << std::hex << std::setfill('0') << asmLine.instructionOffset
                    << std::setfill(' ') << " : " << asmLine.asmLineOrig << "\n";
      }
      GetStream() << "\n";
    }
  }
};

GpuInstCountProfiler* toolHandle = nullptr;

std::shared_ptr<GpuInstCountTxtWriter> txtWriter =
    std::make_shared<GpuInstCountTxtWriter>(std::cerr);
std::shared_ptr<DefaultGTPinFilter> filter = std::make_shared<DefaultGTPinFilter>();

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT void Usage() {
  std::cout << "Usage: ./gpu_inst_count[.exe] <application> <args>" << std::endl;
}

extern "C" PTI_EXPORT int ParseArgs(int argc, char* argv[]) { return 1; }

extern "C" PTI_EXPORT void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
  utils::SetEnv("ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static void PrintResults() {
  PTI_ASSERT(toolHandle != nullptr);

  toolHandle->Stop();
  delete toolHandle;
  toolHandle = nullptr;

  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  std::cerr << std::endl;
  PTI_ASSERT(toolHandle == nullptr);

  toolHandle = new GpuInstCountProfiler(txtWriter, filter);
  auto status = toolHandle->Start();

  if (PROF_STATUS_SUCCESS != status) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
  }
}

void DisableProfiling() {
  std::cerr << std::endl;
  if (toolHandle != nullptr) {
    PrintResults();
    delete toolHandle;
  }
}

void OnFini() {
  if (toolHandle != nullptr) {
    toolHandle->Stop();
    delete toolHandle;
    toolHandle = nullptr;
  }
}

EXPORT_C_FUNC void GTPin_Entry(int argc, const char* argv[]) {
  ConfigureGTPin(argc, argv);
  atexit(OnFini);

  toolHandle = new GpuInstCountProfiler(txtWriter);
  auto status = toolHandle->Start();

  if (PROF_STATUS_SUCCESS != status) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
  }
}
