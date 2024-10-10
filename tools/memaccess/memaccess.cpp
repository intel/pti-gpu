//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "memaccess.hpp"

#include "api/gtpin_api.h"
#include "capsule.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
 * @brief This function analyzes kernel. It specifies sites of instrument and maps them with the
 * result data.
 * @param kernelData A shared pointer to the KernelData object.
 * @param instrumentor A reference to the IGtKernelInstrument object.
 * @return PROF_STATUS The status of the operation.
 */
PROF_STATUS MemAccessGTPinTool::AnalyzeKernel(std::shared_ptr<KernelData> kernelData,
                                              const gtpin::IGtKernelInstrument& instrumentor) {
  const gtpin::IGtCfg& cfg = instrumentor.Cfg();

  SetBucketsNum(kernelData, 16); // buckets num reduces due to overflow

  if (cfg.Bbls().empty()) {
    return PROF_STATUS::NOTHING_TO_INSTRUMENT;
  }

  auto memAccessControl = std::dynamic_pointer_cast<MemAccessControlDefault>(GetControl());
  if (memAccessControl == nullptr) return PROF_STATUS::WRONG_CONTROL;

  for (auto bblPtr : cfg.Bbls()) {
    for (auto insPtr : bblPtr->Instructions()) {
      const gtpin::IGtIns& instruction = *insPtr;

      if (!instruction.IsSendMessage() || instruction.IsEot() || instruction.IsSync()) continue;

      const InstructionOffset offset = cfg.GetInstructionOffset(instruction);

      if (!memAccessControl->ShouldCollectAccess(offset, instruction)) continue;

      std::shared_ptr<MemAccessResultDataCommon> rdc;
      rdc = std::make_shared<MemAccessResultDataCommon>(
          offset, instruction, memAccessControl->GetStrideMin(), memAccessControl->GetStrideNum(),
          memAccessControl->GetStrideStep());

      auto siteMemAccess = std::make_shared<MemAccessSiteOfInstrument>(instruction, rdc);

      AddSiteOfInstrument(kernelData, siteMemAccess);

      auto rdIdx = AddResultData(kernelData, rdc);

      MapResultData(siteMemAccess, rdIdx);
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
PROF_STATUS MemAccessGTPinTool::Instrument(std::shared_ptr<KernelData> kernelData,
                                           gtpin::IGtKernelInstrument& instrumentor) {
  const gtpin::IGtCfg& cfg = instrumentor.Cfg();

  auto memAccessControl = std::dynamic_pointer_cast<MemAccessControlDefault>(GetControl());
  if (memAccessControl == nullptr) return PROF_STATUS::WRONG_CONTROL;

  for (size_t idx = 0; idx < kernelData->GetSiteOfInstrumentNum(); idx++) {
    auto site =
        std::dynamic_pointer_cast<MemAccessSiteOfInstrument>(GetSiteOfInstrument(kernelData, idx));
    PTI_ASSERT(site != nullptr);

    const InstructionOffset offset = cfg.GetInstructionOffset(site->instruction);

    Capsule capsule(instrumentor, GetProfileArray(kernelData), idx);

    Analysis::InstructionCounter(capsule, offsetof(MemAccessRawRecord, memAccessCounter));
    Analysis::SimdActiveCounter(capsule, site->instruction,
                                offsetof(MemAccessRawRecord, simdLanesActiveCounter));
    if (site->addrPayload > 0) {
      Analysis::CacheLineAlignedCounter(capsule, site->instruction,
                                        offsetof(MemAccessRawRecord, clNotAlignedCounter));
      if (site->isScatter) {
        if (memAccessControl->ShouldCollectStrideDistribution(offset, site->instruction)) {
          Analysis::StrideDistrCalc(capsule, site->instruction, site->strideMin, site->strideNum,
                                    site->strideStep,
                                    offsetof(MemAccessRawRecord, strideOverflowLowerCounter),
                                    sizeof(MemAccessRawRecord::strideOverflowLowerCounter));
        }

        if (memAccessControl->ShouldSampleAddresses(offset, site->instruction))
          Analysis::DumpFirstAddresses(capsule, site->instruction,
                                       offsetof(MemAccessRawRecord, addresses),
                                       offsetof(MemAccessRawRecord, memAccessCounter));

        if (!site->isSlm &&
            memAccessControl->ShouldCollectCacheLinesNumber(offset, site->instruction)) {
          Analysis::CacheLineCounter(capsule, site->instruction,
                                     offsetof(MemAccessRawRecord, cacheLinesCounter));
        }
      }
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
PROF_STATUS MemAccessGTPinTool::Accumulate(std::shared_ptr<KernelData> kernelData,
                                           std::shared_ptr<ResultData> profilingResult,
                                           std::shared_ptr<SiteOfInstrument> siteOfInstrument,
                                           RawRecord* record) {
  auto memAccessRawRec = reinterpret_cast<MemAccessRawRecord*>(record);
  auto memAccessResultData = std::dynamic_pointer_cast<MemAccessResultData>(profilingResult);
  auto site = std::dynamic_pointer_cast<MemAccessSiteOfInstrument>(siteOfInstrument);

  memAccessResultData->accessInstructionCounter += memAccessRawRec->memAccessCounter;
  memAccessResultData->simdLanesActiveCounter += memAccessRawRec->simdLanesActiveCounter;
  memAccessResultData->cacheLinesCounter += memAccessRawRec->cacheLinesCounter;
  memAccessResultData->clNotAlignedCounter += memAccessRawRec->clNotAlignedCounter;
  memAccessResultData->strideOverflowLowerCounter += memAccessRawRec->strideOverflowLowerCounter;
  memAccessResultData->strideOverflowHigherCounter += memAccessRawRec->strideOverflowHigherCounter;

  auto rdc = std::dynamic_pointer_cast<MemAccessResultDataCommon>(memAccessResultData->GetCommon());
  PTI_ASSERT(rdc != nullptr);

  PTI_ASSERT(site->strideMin == rdc->strideMin);
  PTI_ASSERT(site->strideNum == rdc->strideNum);
  PTI_ASSERT(site->strideStep == rdc->strideStep);
  for (size_t idx = 0; idx < rdc->strideNum; idx++) {
    int64_t stride = rdc->strideMin + idx * rdc->strideStep;
    memAccessResultData->strideDistributionCounter[stride] += memAccessRawRec->strideDistr[idx];
  }

  PTI_ASSERT(memAccessResultData->addresses.size() <=
             sizeof(memAccessRawRec->addresses) / sizeof(memAccessRawRec->addresses[0]));

  for (size_t addrIdx = 0; addrIdx < memAccessResultData->addresses.size(); addrIdx++) {
    if (memAccessRawRec->addresses[addrIdx] /* Do not fill with zeros */
        && memAccessResultData->addresses[addrIdx] ==
               0 /* Do not overwrite already filled addresses */)
      memAccessResultData->addresses[addrIdx] = memAccessRawRec->addresses[addrIdx];
  }
  return PROF_STATUS::SUCCESS;
}

std::shared_ptr<GTPinTool> MemAccessFactory::MakeGTPinTool() const {
  auto memAccessControl = std::dynamic_pointer_cast<MemAccessControlDefault>(m_control);
  strideNum = memAccessControl->GetStrideNum();
  return std::make_shared<MemAccessGTPinTool>(std::make_shared<MemAccessFactory>(*this));
}

/// Next functions casts data structures to MemAccess types and calls MemAccess specific functions
/// for writing the data
bool MemAccessWriterBase::WriteApplicationData(const ApplicationDataSPtr res) {
  auto memAccessApplicationData = std::dynamic_pointer_cast<MemAccessApplicationData>(res);
  PTI_ASSERT(memAccessApplicationData != nullptr);
  return WriteMemAccessApplicationData(memAccessApplicationData);
}

bool MemAccessWriterBase::WriteKernelData(const ApplicationDataSPtr res,
                                          const KernelDataSPtr kernelData) {
  auto memAccessApplicationData = std::dynamic_pointer_cast<MemAccessApplicationData>(res);
  PTI_ASSERT(memAccessApplicationData != nullptr);

  auto memAccessKernelData = std::dynamic_pointer_cast<MemAccessKernelData>(kernelData);
  PTI_ASSERT(memAccessKernelData != nullptr);

  return WriteMemAccessKernelData(memAccessApplicationData, memAccessKernelData);
}

bool MemAccessWriterBase::WriteInvocationData(const ApplicationDataSPtr res,
                                              const KernelDataSPtr kernelData,
                                              const InvocationDataSPtr invocationData) {
  auto memAccessApplicationData = std::dynamic_pointer_cast<MemAccessApplicationData>(res);
  PTI_ASSERT(memAccessApplicationData != nullptr);

  auto memAccessKernelData = std::dynamic_pointer_cast<MemAccessKernelData>(kernelData);
  PTI_ASSERT(memAccessKernelData != nullptr);

  auto memAccessInvocationData = std::dynamic_pointer_cast<MemAccessInvocationData>(invocationData);
  PTI_ASSERT(memAccessInvocationData != nullptr);

  return WriteMemAccessInvocationData(memAccessApplicationData, memAccessKernelData,
                                      memAccessInvocationData);
}

bool MemAccessWriterBase::WriteResultData(const ApplicationDataSPtr res,
                                          const KernelDataSPtr kernelData,
                                          const InvocationDataSPtr invocationData,
                                          const ResultDataSPtr resultData,
                                          const ResultDataCommonSPtr resultDataCommon,
                                          size_t tileId) {
  auto memAccessApplicationData = std::dynamic_pointer_cast<MemAccessApplicationData>(res);
  PTI_ASSERT(memAccessApplicationData != nullptr);

  auto memAccessKernelData = std::dynamic_pointer_cast<MemAccessKernelData>(kernelData);
  PTI_ASSERT(memAccessKernelData != nullptr);

  auto memAccessInvocationData = std::dynamic_pointer_cast<MemAccessInvocationData>(invocationData);
  PTI_ASSERT(memAccessInvocationData != nullptr);

  auto memAccessResultData = std::dynamic_pointer_cast<MemAccessResultData>(resultData);
  PTI_ASSERT(memAccessResultData != nullptr);

  auto memAccessResultDataCommon =
      std::dynamic_pointer_cast<MemAccessResultDataCommon>(resultDataCommon);
  PTI_ASSERT(memAccessResultDataCommon != nullptr);

  return WriteMemAccessResultData(memAccessApplicationData, memAccessKernelData,
                                  memAccessInvocationData, memAccessResultData,
                                  memAccessResultDataCommon, tileId);
}
