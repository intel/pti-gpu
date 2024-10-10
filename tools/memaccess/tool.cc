//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#if defined(_WIN32)
#define NOMINMAX
#endif

#include <string.h>

#include <iostream>
#include <memory>

#include "api/gtpin_api.h"
#include "knob_parser.h"
#include "memaccess.hpp"
#include "utils.h"

using namespace gtpin;
using namespace gtpin_prof;

// Tool specific implementation of Writer and Control /////////////////////////

class MemAccessTxtWriter : public MemAccessWriterBase, public TxtWriterBase {
 public:
  using TxtWriterBase::TxtWriterBase;
  virtual ~MemAccessTxtWriter() = default;

  bool WriteMemAccessKernelData(const MemAccessApplicationDataSPtr res,
                                const MemAccessKernelDataSPtr kernelData) {
    if (kernelData->GetInvocations().size() == 0) return true;
    const size_t tileId = 0;

    size_t resultsNum = kernelData->GetResultsNum();
    std::vector<size_t> instCount(resultsNum);
    std::vector<size_t> simdCount(resultsNum);
    std::vector<size_t> clCount(resultsNum);
    std::vector<size_t> clNotAlignedCount(resultsNum);
    std::vector<size_t> strideSum(resultsNum);
    std::vector<size_t> strideOvfLower(resultsNum);
    std::vector<size_t> strideOvfHigher(resultsNum);
    std::vector<std::map<int64_t, size_t>> strides(resultsNum);
    std::vector<std::vector<size_t>> addresses(resultsNum);

    auto resultDataCommon = kernelData->GetResultDataCommon();
    for (size_t idx = 0; idx < resultsNum; idx++) {
      auto rdc = std::dynamic_pointer_cast<MemAccessResultDataCommon>(resultDataCommon[idx]);
      bool firstInvocation = true;
      for (auto invocationDataPair : kernelData->GetInvocations()) {
        auto invocationData =
            std::dynamic_pointer_cast<MemAccessInvocationData>(invocationDataPair.second);
        const size_t tileId = 0;
        auto resultData = std::dynamic_pointer_cast<MemAccessResultData>(
            invocationData->GetResultData(tileId, idx));
        instCount[idx] += resultData->accessInstructionCounter;
        simdCount[idx] += resultData->simdLanesActiveCounter;
        clCount[idx] += resultData->cacheLinesCounter;
        clNotAlignedCount[idx] += resultData->clNotAlignedCounter;

        if (firstInvocation) {
          addresses[idx].resize(resultData->addresses.size());
          for (size_t addrIdx = 0; addrIdx < resultData->addresses.size(); addrIdx++) {
            addresses[idx][addrIdx] = resultData->addresses[addrIdx];
          }
        }
        firstInvocation = false;
        strideOvfLower[idx] += resultData->strideOverflowLowerCounter;
        strideOvfHigher[idx] += resultData->strideOverflowHigherCounter;
        strideSum[idx] +=
            resultData->strideOverflowLowerCounter + resultData->strideOverflowHigherCounter;
        for (auto stride : resultData->strideDistributionCounter) {
          strideSum[idx] += stride.second;
          if (strides[idx].find(stride.first) == strides[idx].end()) {
            strides[idx][stride.first] = 0;
          }
          strides[idx][stride.first] += stride.second;
        }
      }
    }

    auto assembly = kernelData->GetOrigAsm();
    for (size_t idx = 0; idx < resultsNum; idx++) {
      auto rdc = std::dynamic_pointer_cast<MemAccessResultDataCommon>(resultDataCommon[idx]);

      InstructionOffset offset = rdc->offset;
      std::cerr << std::string(80, '-') << "\n";
      std::cerr << "0x" << std::setw(6) << std::hex << std::setfill('0') << offset
                << std::setfill(' ') << std::dec << " : ";
      for (const auto& asmRecord : assembly) {
        if (asmRecord.GetInstructionOffset() == offset) {
          std::cerr << asmRecord.GetAsmLineOrig() << "\n";
          if (asmRecord.GetSourcePoint().IsValid()) {
            std::cerr << " (" << asmRecord.GetSourcePoint().GetFile() << ":"
                      << asmRecord.GetSourcePoint().GetLine() << ":"
                      << asmRecord.GetSourcePoint().GetColumn() << ")";
          } else {
            std::cerr << " (no source info)";
          }
          break;
        }
      }
      std::cerr << "\n";
      // Example: SIMD16 ExecSize:4 8Bytes X 1 Atomic scatter SLM A64 Write
      std::cerr << "  * " << (rdc->isEot ? "EOT " : "") << "SIMD" << (rdc->simdWidth) << " "
                << "ExecSize_" << (rdc->execSize) << " " << (rdc->elementSize) << " bytes "
                << "X" << (rdc->numOfElements) << " " << (rdc->isAtomic ? "Atomic " : "")
                << (rdc->isScatter ? "Scatter " : "") << (rdc->isSlm ? "SLM " : "")
                << (rdc->isScratch ? "Scratch " : "") << ((rdc->addrWidth == 8) ? "A64 " : "A32 ")
                << (rdc->isWrite ? "Write " : "Read ") << "\n";
      std::cerr << "  * Instruction executed: " << instCount[idx] << "\n";
      std::cerr << "  * SIMD lanes executed: " << simdCount[idx] << "\n";
      size_t usedBytes = simdCount[idx] * rdc->elementSize * rdc->numOfElements;
      size_t clCountMultiplier = (size_t(rdc->elementSize * rdc->numOfElements + CACHE_LINE_SIZE_BYTES - 1)) / CACHE_LINE_SIZE_BYTES; // Round up
      size_t transferredCacheLines = clCount[idx] * clCountMultiplier;
      size_t transferredBytes = transferredCacheLines * CACHE_LINE_SIZE_BYTES;
      float usedTransferredRatio =
          (transferredCacheLines > 0
               ? (100.0 * usedBytes / transferredBytes)
               : 0);
      std::cerr << "  * Cache line transferred: " << transferredCacheLines << " ( "
                << ( transferredBytes ) << " bytes)\n      " << usedTransferredRatio
                << " % used/transferred ratio\n";
      float clNotAlignedCountPercent =
          (instCount[idx] > 0 ? (100.0 * clNotAlignedCount[idx] / instCount[idx]) : 0);
      std::cerr << "  * Cache line not aligned: " << std::setprecision(4)
                << clNotAlignedCountPercent << " % (" << clNotAlignedCount[idx] << ")\n";

      std::cerr << "  * Stride distribution:\n";
      if (strideSum[idx] > 0) {
        std::map<size_t, int64_t> sortedStrides;  // map is a sorted container
        for (auto stride : strides[idx]) {
          if (sortedStrides.size() == 0 || sortedStrides.begin()->first < stride.second) {
            sortedStrides[stride.second] = stride.first;
            if (sortedStrides.size() > 5) sortedStrides.erase(sortedStrides.begin());
          }
        }
        size_t meaningfulStridesSum = strideOvfHigher[idx] + strideOvfLower[idx];
        for (auto it = sortedStrides.rbegin(); it != sortedStrides.rend(); ++it) {
          if (it->first == 0) continue;
          std::cerr << "      " << (100.0 * it->first / strideSum[idx]) << "% (" << it->first
                    << ") -> stride: " << it->second << " bytes ("
                    << ((rdc->elementSize != 0) ? it->second / rdc->elementSize : 0) << " units)"
                    << '\n';
          meaningfulStridesSum += it->first;
        }
        if (strideOvfHigher[idx] > 0) {
          std::cerr << "      " << (100.0 * strideOvfHigher[idx] / strideSum[idx])
                    << "% -> overflow, stride higher than "
                    << (rdc->strideMin + rdc->strideNum * rdc->strideStep) << "\n";
        }
        if (strideOvfLower[idx] > 0) {
          std::cerr << "      " << (100.0 * strideOvfLower[idx] / strideSum[idx])
                    << "% -> overflow, stride lower than " << rdc->strideMin << "\n";
        }
        if ((strideSum[idx] - meaningfulStridesSum) > 0) {
          std::cerr << "      " << (strideSum[idx] - meaningfulStridesSum) << " " << strideSum[idx]
                    << " _ " << (100.0 * (strideSum[idx] - meaningfulStridesSum) / strideSum[idx])
                    << "% -> other strides" << '\n';
        }
      } else {
        std::cerr << "      No strides detected\n";
      }
      std::cerr << "  * Access addresses sample (SIMD" << rdc->simdWidth << "):";
      for (size_t addrIdx = 0; addrIdx < addresses[idx].size(); addrIdx++) {
        if ((addrIdx) % 4 == 0)
          std::cerr << "\n      Addr# " << std::setw(2) << std::dec << std::setfill(' ') << addrIdx
                    << "   ";
        std::cerr << "0x" << std::setw(( (rdc->addrWidth == 8) ? 16 : 8 )) << std::hex << std::setfill('0')
                  << addresses[idx][addrIdx] << " ";
      }
      std::cerr << std::dec << "\n";
      std::cerr << std::endl;
    }
    return true;
  }
};

class MemAccessJsonWriter : public MemAccessWriterBase, public JsonWriterBase {
 public:
  using JsonWriterBase::JsonWriterBase;
  virtual ~MemAccessJsonWriter() = default;

  bool WriteMemAccessResultData(const MemAccessApplicationDataSPtr res,
                                const MemAccessKernelDataSPtr kernelData,
                                const MemAccessInvocationDataSPtr invocationData,
                                const MemAccessResultDataSPtr resultData,
                                const MemAccessResultDataCommonSPtr resultDataCommon,
                                size_t tileId) final {
    // MemAccessResultDataSPtr
    GetStream() << "\"access_instruction_counter\":" << resultData->accessInstructionCounter;
    GetStream() << ",\"simd_lanes_active_counter\":" << resultData->simdLanesActiveCounter;
    GetStream() << ",\"cache_lines_counter\":" << resultData->cacheLinesCounter;
    GetStream() << ",\"cl_not_aligned_counter\":" << resultData->clNotAlignedCounter;
    GetStream() << ",\"stride_overflow_higher_counter\":"
                << resultData->strideOverflowHigherCounter;
    GetStream() << ",\"stride_overflow_lower_counter\":" << resultData->strideOverflowLowerCounter;
    GetStream() << ",\"addresses\":[";
    for (size_t addrIdx = 0; addrIdx < resultData->addresses.size(); addrIdx++) {
      if (addrIdx > 0) GetStream() << ",";
      GetStream() << "\"" << resultData->addresses[addrIdx] << "\"";
    }
    GetStream() << "],\"stride_distribution\":{";
    bool first = true;
    for (auto stride : resultData->strideDistributionCounter) {
      if (stride.second == 0) continue;
      if (!first) GetStream() << ",";
      first = false;
      GetStream() << "\"" << stride.first << "\":" << stride.second;
    }
    GetStream() << "}";

    ///// MemAccessResultDataCommon
    GetStream() << ",\"offset\":" << resultDataCommon->offset;
    GetStream() << ",\"stride_min\":" << resultDataCommon->strideMin;
    GetStream() << ",\"stride_num\":" << resultDataCommon->strideNum;
    GetStream() << ",\"stride_step\":" << resultDataCommon->strideStep;
    GetStream() << ",\"element_size\":" << resultDataCommon->elementSize;
    GetStream() << ",\"num_of_elements\":" << resultDataCommon->numOfElements;
    GetStream() << ",\"is_write\":" << resultDataCommon->isWrite;
    GetStream() << ",\"is_scatter\":" << resultDataCommon->isScatter;
    GetStream() << ",\"is_bts\":" << resultDataCommon->isBts;
    GetStream() << ",\"is_slm\":" << resultDataCommon->isSlm;
    GetStream() << ",\"is_scratch\":" << resultDataCommon->isScratch;
    GetStream() << ",\"is_atomic\":" << resultDataCommon->isAtomic;
    GetStream() << ",\"addr_width\":" << resultDataCommon->addrWidth;
    GetStream() << ",\"simd_width\":" << resultDataCommon->simdWidth;
    GetStream() << ",\"bti\":" << resultDataCommon->bti;
    GetStream() << ",\"addr_payload\":" << resultDataCommon->addrPayload;
    GetStream() << ",\"is_eot\":" << resultDataCommon->isEot;
    GetStream() << ",\"is_media\":" << resultDataCommon->isMedia;
    GetStream() << ",\"exec_size\":" << resultDataCommon->execSize;
    GetStream() << ",\"channel_offset\":" << resultDataCommon->channelOffset;

    return false;
  }
};

static gtpin::Knob<bool> knobJsonOutput("json-output", false, "Print results in JSON format");

static gtpin::KnobVector<int> knobKernelRun("kernel-run", {}, "Kernel run to profile");
static gtpin::Knob<int> knobStrideMin("stride-min", STRIDE_MIN_DEFAULT,
                                      "Minimal detected stride (bytes)");
static gtpin::Knob<int> knobStrideNum("stride-num", STRIDE_NUM_DEFAULT,
                                      "Number of collected strides (buckets)");
static gtpin::Knob<int> knobStrideStep("stride-step", STRIDE_STEP_DEFAULT, "Stride step (bytes)");

class MemAccessGTPinControl : public MemAccessControlDefault {
 public:
  using MemAccessControlDefault::MemAccessControlDefault;

  bool ShouldProfileEnqueue(const KernelExecDescriptor& descr) const final {
    if (!gtpin::IsKernelExecProfileEnabled(descr.gtExecDesc, descr.gpuPlatform)) return false;

    if (!knobKernelRun.NumValues()) return true;

    for (uint32_t i = 0; i != knobKernelRun.NumValues(); ++i) {
      if (knobKernelRun.GetValue(i) == descr.runIdx) {
        return true;
      }
    }
    return false;
  }
  int32_t GetStrideMin() const { return knobStrideMin; }
  int32_t GetStrideNum() const { return knobStrideNum; }
  int32_t GetStrideStep() const { return knobStrideStep; }
};

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT void Usage() {
  std::cout << "Usage: ./memaccess";
#if defined(_WIN32)
  std::cout << "[.exe]";
#endif
  std::cout << " [options] <application> <args>" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "--kernel-run                   "
            << "Kernel run to profile" << std::endl;
  std::cout << "--stride-min                   "
            << "Minimal detected stride (bytes)" << std::endl;
  std::cout << "--stride-num                   "
            << "Number of collected strides (buckets)" << std::endl;
  std::cout << "--stride-step                  "
            << "Stride step (bytes)" << std::endl;
  std::cout << "--json-output                  "
            << "Print results in JSON format" << std::endl;
}

extern "C" PTI_EXPORT int ParseArgs(int argc, char* argv[]) {
  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--kernel-run") == 0) {
      if (argc < i + 1) {
        std::cerr << "Error: --kernel-run requires an argument" << std::endl;
        return -1;
      }
      utils::SetEnv("GMA_KernelRun", argv[i + 1]);
      app_index += 2;
    } else if (strcmp(argv[i], "--stride-min") == 0) {
      if (argc < i + 1) {
        std::cerr << "Error: --stride-min requires an argument" << std::endl;
        return -1;
      }
      utils::SetEnv("GMA_StrideMin", argv[i + 1]);
      app_index += 2;
    } else if (strcmp(argv[i], "--stride-num") == 0) {
      if (argc < i + 1) {
        std::cerr << "Error: --stride-num requires an argument" << std::endl;
        return -1;
      }
      utils::SetEnv("GMA_StrideNum", argv[i + 1]);
      app_index += 2;
    } else if (strcmp(argv[i], "--stride-step") == 0) {
      if (argc < i + 1) {
        std::cerr << "Error: --stride-step requires an argument" << std::endl;
        return -1;
      }
      utils::SetEnv("GMA_StrideStep", argv[i + 1]);
      app_index += 2;
    } else if (strcmp(argv[i], "--json-output") == 0) {
      utils::SetEnv("GMA_JsonOutput", "1");
      app_index++;
    } else if (strcmp(argv[i], "--version") == 0) {
#ifdef PTI_VERSION
      std::cout << TOSTRING(PTI_VERSION) << std::endl;
#endif
      return 0;
    } else {
      break;
    }
  }
  return app_index;
}

extern "C" PTI_EXPORT void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
  utils::SetEnv("ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1");
}

// Internal Tool Interface ////////////////////////////////////////////////////

std::shared_ptr<MemAccessWriterBase> writer;
std::shared_ptr<MemAccessControl> control;
std::shared_ptr<MemAccessGTPinProfiler> profiler;

void EnableProfiling() {
  PTI_ASSERT(profiler == nullptr);

  /// Apply options
  std::vector<const char*> args;
  std::string value;
  value = utils::GetEnv("GMA_KernelRun");
  if (!value.empty()) {
    args.push_back("--kernel-run");
    args.push_back(value.c_str());
  }
  value = utils::GetEnv("GMA_StrideMin");
  if (!value.empty()) {
    args.push_back("--stride-min");
    args.push_back(value.c_str());
  }
  value = utils::GetEnv("GMA_StrideNum");
  if (!value.empty()) {
    args.push_back("--stride-num");
    args.push_back(value.c_str());
  }
  value = utils::GetEnv("GMA_StrideStep");
  if (!value.empty()) {
    args.push_back("--stride-step");
    args.push_back(value.c_str());
  }
  value = utils::GetEnv("GMA_JsonOutput");
  if (!value.empty() && value == "1") {
    args.push_back("--json-output");
  }
  ConfigureGTPin(args.size(), args.data());

  if (knobJsonOutput) {
    writer = std::make_shared<MemAccessJsonWriter>(std::cerr);
  } else {
    writer = std::make_shared<MemAccessTxtWriter>(std::cerr);
  }

  control = std::make_shared<MemAccessGTPinControl>();
  profiler = std::make_shared<MemAccessGTPinProfiler>(writer, control);

  auto status = profiler->Start();
  if (PROF_STATUS::SUCCESS != status) {
    std::cerr << profiler->LastError() << std::endl;
  }
}

void DisableProfiling() {
  if (profiler == nullptr) {
    return;
  }
  PTI_ASSERT(profiler->Status() == PROF_STATUS::ACTIVE);

  auto status = profiler->Stop();
  if (PROF_STATUS::SUCCESS != status) {
    std::cerr << profiler->LastError() << std::endl;
  }
}

// GTPin loader interface allows to use library as a GTPin loader tool
EXPORT_C_FUNC PTI_EXPORT void GTPin_Entry(int argc, const char* argv[]) {
  ConfigureGTPin(argc, argv);
  // DisableProfiling() is registered as an atexit callback by PTI loader. No need to do it here
  EnableProfiling();
}
