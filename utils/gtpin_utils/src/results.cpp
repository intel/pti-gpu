//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file results.cpp
 * @brief Contains the implementation of classes related to result data and kernel data.
 */

#include "results.hpp"

#include "api/gtpin_api.h"
#include "capsule.hpp"

using namespace gtpin_prof;

ResultData::ResultData(ResultDataCommonSPtr resultDataCommon)
    : m_resultDataCommon(resultDataCommon){};
const ResultDataCommonSPtr ResultData::GetCommon() const { return m_resultDataCommon; }
size_t ResultData::GetTileId() const { return m_tileId; };

InvocationData::InvocationData(const KernelExecDescriptor& execDescr)
    : m_runNum(execDescr.runIdx),
      m_globalRunNum(execDescr.runGlobalIdx),
      m_dispatchId(execDescr.dispatchId){};
bool InvocationData::IsValid() const { return (m_dispatchId != -1); }
const KernelRun InvocationData::GetRunNum() const { return m_runNum; }
const KernelRun InvocationData::GetGlobalRunNum() const { return m_globalRunNum; }
const DispatchId InvocationData::GetDispatchId() const { return m_dispatchId; }
size_t InvocationData::GetCollectedTilesNum() const { return m_tileResultData.size(); }
const std::vector<ResultDataSPtr> InvocationData::GetResults(size_t tileId) const {
  PTI_ASSERT(tileId < m_tileResultData.size());
  return m_tileResultData[tileId];
}
const ResultDataSPtr InvocationData::GetResultData(size_t tileId, size_t idx) const {
  auto res = GetResults(tileId);
  PTI_ASSERT(idx < res.size());
  return res[idx];
}
bool InvocationData::IsCollected() const { return m_collected; }

SourcePoint::SourcePoint(const std::string file, const int line, const int column,
                         const std::string func)
    : m_file(file), m_line(line), m_column(column), m_function(func) {}
bool SourcePoint::IsValid() const {
  return (m_file != "") || (m_line != -1) || (m_column != -1) || (m_function != "");
}
const std::string SourcePoint::GetFile() const { return m_file; }
const int SourcePoint::GetLine() const { return m_line; }
const int SourcePoint::GetColumn() const { return m_column; }
const std::string SourcePoint::GetFunction() const { return m_function; }

AsmRecord::AsmRecord(const InstructionOffset instructionOffset, const std::string asmLineOrig,
                     const SourcePoint sourcePoint)
    : m_instructionOffset(instructionOffset),
      m_asmLineOrig(asmLineOrig),
      m_sourcePoint(sourcePoint){};
const InstructionOffset AsmRecord::GetInstructionOffset() const { return m_instructionOffset; }
const std::string AsmRecord::GetAsmLineOrig() const { return m_asmLineOrig; }
const SourcePoint AsmRecord::GetSourcePoint() const { return m_sourcePoint; }

KernelData::KernelData(const gtpin::IGtKernelInstrument& instrumentor)
    : m_kernelName(instrumentor.Kernel().Name()),
      m_kernelId(instrumentor.Kernel().Id()),
      m_tilesNum(Macro::GetNumTiles(instrumentor)) {
  PTI_ASSERT(m_tilesNum > 0);
  // origAsm
  const gtpin::IGtCfg& cfg = instrumentor.Cfg();
  for (const auto& bblPtr : cfg.Bbls()) {
    for (const auto& insPtr : bblPtr->Instructions()) {
      const gtpin::IGtIns& ins = *insPtr;
      const InstructionOffset offset = cfg.GetInstructionOffset(ins);
      m_origAsm.emplace_back(offset, insPtr->ToString());
    }
  }
  // origBinary
  auto binary = instrumentor.Kernel().Binary();
  m_origBinary.resize(binary.size());
  std::copy(binary.begin(), binary.end(), m_origBinary.begin());
  // origSource
}
const std::string KernelData::GetKernelName() const { return m_kernelName; }
const KernelId KernelData::GetKernelId() const { return m_kernelId; }
const std::vector<AsmRecord> KernelData::GetOrigAsm() const { return m_origAsm; }
const std::vector<uint8_t> KernelData::GetOrigBinary() const { return m_origBinary; }
const size_t KernelData::GetKernelRun() const { return m_kernelRuns; }
const std::unordered_map<DispatchId, InvocationDataSPtr> KernelData::GetInvocations() const {
  return m_invocations;
}
const uint32_t KernelData::GetRecordSize() const { return m_recordSize; }
const size_t KernelData::GetSiteOfInstrumentNum() const { return m_sitesOfInterest.size(); }
const size_t KernelData::GetResultsNum() const { return m_resultDataCommon.size(); }
const size_t KernelData::GetBucketsNum() const { return m_buckets; }
const size_t KernelData::GetTilesNum() const { return m_tilesNum; }
const size_t KernelData::GetCollectedTilesNum() const { return m_collectedTilesNum; }
const SiteOfInstrumentSPtr KernelData::GetSiteOfInstrument(size_t idx) const {
  PTI_ASSERT(idx < m_sitesOfInterest.size());
  return m_sitesOfInterest.at(idx);
}
std::vector<ResultDataCommonSPtr> KernelData::GetResultDataCommon() const {
  return m_resultDataCommon;
}
ResultDataCommonSPtr KernelData::GetResultDataCommon(size_t idx) const {
  PTI_ASSERT(idx < m_resultDataCommon.size());
  return m_resultDataCommon[idx];
}
bool KernelData::IsRecordSizeSet() { return m_recordSize != -1; }
bool KernelData::IsBucketsSet() { return m_buckets != 0; }

const std::string ApplicationData::GetToolName() const { return m_toolName; }
const std::string ApplicationData::GetApplicationName() const { return m_applicationName; }
const std::unordered_map<KernelId, KernelDataSPtr> ApplicationData::GetKernels() const {
  return m_kernels;
}
const KernelDataSPtr ApplicationData::GetKernel(const KernelId& kernelId) const {
  return m_kernels.at(kernelId);
}