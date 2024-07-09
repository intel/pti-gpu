//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "instcount.hpp"

#include <map>

#include "api/gtpin_api.h"
#include "capsule.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
 * @class SimdInstrArgs
 * @brief This class is used to encapsulate the arguments of a SIMD instruction.
 * This class is used to encapsulate the arguments of a SIMD instruction.
 * It contains the following members:
 * - maskCtrl: A boolean indicating whether the write mask is enabled or not.
 * - execMask: A 32-bit integer representing the execution mask.
 * - pred: A GtPredicate object representing the predicate of the instruction.
 * - isSendIns: A boolean indicating whether the instruction is a send message instruction or not.
 *
 * The class also provides a less than operator for comparison purposes. This is used
 * to enable the use of this class as a key in a std::map.
 */
class SimdInstrArgs {
 public:
  SimdInstrArgs(const gtpin::IGtIns& ins)
      : maskCtrl(!ins.IsWriteMaskEnabled()),
        execMask(ins.ExecMask().Bits()),
        pred(ins.Predicate()),
        isSendIns(ins.IsSendMessage()) {}

  bool operator<(const SimdInstrArgs& other) const {
    return std::make_tuple(maskCtrl, execMask, pred, isSendIns) <
           std::make_tuple(other.maskCtrl, other.execMask, other.pred, other.isSendIns);
  }

  bool maskCtrl;
  uint32_t execMask;
  gtpin::GtPredicate pred;
  bool isSendIns;
};

using SimdInsMap = std::map<SimdInstrArgs, std::shared_ptr<InstCountSiteOfInstrument>>;

/**
 * @brief This function analyzes kernel. It specifies sites of instrument and maps them with the
 * result data.
 * @param kernelData A shared pointer to the KernelData object.
 * @param instrumentor A reference to the IGtKernelInstrument object.
 * @return PROF_STATUS The status of the operation.
 */
PROF_STATUS InstCountGTPinTool::AnalyzeKernel(KernelDataSPtr kernelData,
                                              const gtpin::IGtKernelInstrument& instrumentor) {
  const gtpin::IGtCfg& cfg = instrumentor.Cfg();

  SetDefaultBuckets(kernelData, instrumentor);

  if (cfg.Bbls().empty()) {
    return PROF_STATUS::NOTHING_TO_INSTRUMENT;
  }

  auto instCountControl = std::dynamic_pointer_cast<InstCountControlDefault>(GetControl());
  bool simdActiveLanesEnable =
      (instCountControl == nullptr || instCountControl->ShouldCollectSimdWidth());

  // std::map with similar SIMD instructions inside one region. This algorithm allows to do only
  // instrumentation for all SIMD instructions with same SIMD width inside one region
  SimdInsMap simdInstrMap;
  for (const auto& bblPtr : cfg.Bbls()) {
    if (simdActiveLanesEnable)
      simdInstrMap.clear();  // new region starts with the beginning of the basic block

    const gtpin::IGtIns& firstInstruction = bblPtr->FirstIns();
    const InstructionOffset firstInstructionOffset = cfg.GetInstructionOffset(firstInstruction);
    auto siteBblCount = std::make_shared<InstCountSiteOfInstrument>(
        firstInstruction, InstCountSiteOfInstrument::Type::Count);
    AddSiteOfInstrument(kernelData, siteBblCount);

    for (const auto& insPtr : bblPtr->Instructions()) {
      const gtpin::IGtIns& instruction = *insPtr;
      const InstructionOffset offset = cfg.GetInstructionOffset(instruction);

      auto rdc = std::make_shared<InstCountResultDataCommon>(offset, bblPtr->Id());
      auto rdIdx = AddResultData(kernelData, rdc);
      MapResultData(siteBblCount, rdIdx);

      if (simdActiveLanesEnable) {
        SimdInstrArgs simdArgs(instruction);
        auto it = simdInstrMap.find(simdArgs);
        if (it == simdInstrMap.end()) {  // if region does not contain similar SIMD instruction, add
                                         // new site of instrument
          auto siteSimdCount = std::make_shared<InstCountSiteOfInstrument>(
              instruction, InstCountSiteOfInstrument::Type::Simd);
          AddSiteOfInstrument(kernelData, siteSimdCount);
          MapResultData(siteSimdCount, rdIdx);
          simdInstrMap[simdArgs] = siteSimdCount;
        } else {
          // if region contains similar SIMD instruction, map the data to the same site of
          // instrument
          MapResultData(it->second, rdIdx);
        }
        if (instruction.IsFlagModifier()) {
          simdInstrMap.clear();  // new region starts after flag modifier
        }
      }
    }
  }

  return PROF_STATUS::SUCCESS;
}

/**
 * @brief This function instruments the kernel data.
 * It instruments the kernel binary based on the results of the "AnalyzeKernel" function. It applies
 * the instrumentation for all sites of instrument based on information stored in it.
 * @param kernelData A shared pointer to the KernelData object.
 * @param instrumentor A reference to the IGtKernelInstrument object.
 * @return PROF_STATUS The status of the operation.
 */
PROF_STATUS InstCountGTPinTool::Instrument(KernelDataSPtr kernelData,
                                           gtpin::IGtKernelInstrument& instrumentor) {
  for (size_t idx = 0; idx < kernelData->GetSiteOfInstrumentNum(); idx++) {
    auto site =
        std::dynamic_pointer_cast<InstCountSiteOfInstrument>(GetSiteOfInstrument(kernelData, idx));
    PTI_ASSERT(site != nullptr);

    Capsule capsule(instrumentor, GetProfileArray(kernelData), idx);
    if (site->type == InstCountSiteOfInstrument::Type::Count) {
      Analysis::InstructionCounter(capsule, offsetof(InstCountRawRecord, count));
    } else if (site->type == InstCountSiteOfInstrument::Type::Simd) {
      Analysis::SimdActiveCounter(capsule, site->instruction, offsetof(InstCountRawRecord, count));
    }

    instrumentor.InstrumentInstruction(site->instruction, GtIpoint::Before(),
                                       capsule.GetProcedure());
  }
  return PROF_STATUS::SUCCESS;
}

/**
 * @brief This function accumulates the data from the RawRecord to the ResultData.
 * It accumulates data from RawRecord to ResultData. For each profiling results may be several
 * records, data should be accumulated, not just transferred.
 * @param kernelData A shared pointer to the KernelData object.
 * @param profilingResult A shared pointer to the ResultData object.
 * @param siteOfInstrument A shared pointer to the SiteOfInstrument object.
 * @param record A pointer to the RawRecord object.
 * @return PROF_STATUS The status of the operation.
 */
PROF_STATUS InstCountGTPinTool::Accumulate(KernelDataSPtr kernelData,
                                           ResultDataSPtr profilingResult,
                                           SiteOfInstrumentSPtr siteOfInstrument,
                                           RawRecord* record) {
  auto instCountRawRec = reinterpret_cast<InstCountRawRecord*>(record);
  auto instCountResultData = std::dynamic_pointer_cast<InstCountResultData>(profilingResult);
  PTI_ASSERT(instCountResultData != nullptr);
  auto site = std::dynamic_pointer_cast<InstCountSiteOfInstrument>(siteOfInstrument);
  PTI_ASSERT(site != nullptr);

  if (site->type == InstCountSiteOfInstrument::Type::Count) {
    instCountResultData->instructionCounter += instCountRawRec->count;
  } else if (site->type == InstCountSiteOfInstrument::Type::Simd) {
    instCountResultData->simdActiveLaneCounter += instCountRawRec->count;
  }

  return PROF_STATUS::SUCCESS;
}
