//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_RESULTS_H
#define PTI_GTPIN_RESULTS_H

#include <api/gtpin_api.h>

#include <set>
#include <unordered_map>
#include <vector>

#include "def_gpu.hpp"

/**
 * @file results.hpp
 * @brief This file includes base classes for profiling results. Derived classes can add
 * tool-specific information.
 */

namespace gtpin_prof {

/**
 * @class ResultDataCommon
 * @brief A base class for common result data.
 * This class contains common data for each ResultData object associated with different invocations.
 */

class ResultDataCommon {
 public:
  ResultDataCommon() = default;
  virtual ~ResultDataCommon() = default;
  ResultDataCommon(ResultDataCommon&) = delete;
  ResultDataCommon operator=(ResultDataCommon&) = delete;
};

/**
 * @class ResultData
 * @brief This class represents human-readable information passed to the writer.
 */
class ResultData {
 public:
  ResultData(ResultDataCommonSPtr resultDataCommon);
  virtual ~ResultData() = default;
  ResultData(ResultData&) = delete;
  ResultData operator=(ResultData&) = delete;
  const ResultDataCommonSPtr GetCommon() const;
  size_t GetTileId() const;

 private:
  const ResultDataCommonSPtr m_resultDataCommon;
  size_t m_tileId;
  friend class GTPinTool;
};

/**
 * @class SiteOfInstrument
 * @brief This class describes the site of the instrument.
 * It stores information about the site of the instrument, a place in the code that should be
 * modified by binary instrumentation.
 */
class SiteOfInstrument {
 public:
  SiteOfInstrument() = default;
  virtual ~SiteOfInstrument() = default;
  SiteOfInstrument(SiteOfInstrument&) = delete;
  SiteOfInstrument operator=(SiteOfInstrument&) = delete;

 private:
  std::vector<size_t> m_results;
  friend class GTPinTool;
};

/**
 * @class InvocationData
 * @brief This class contains data for each kernel run.
 */
class InvocationData {
 public:
  InvocationData(const KernelExecDescriptor& execDescr);
  virtual ~InvocationData() = default;
  InvocationData(InvocationData&) = delete;
  InvocationData& operator=(InvocationData&) = delete;

  bool IsValid() const;

  // Getters
  const KernelRun GetRunNum() const;
  const KernelRun GetGlobalRunNum() const;
  const DispatchId GetDispatchId() const;
  size_t GetCollectedTilesNum() const;
  const std::vector<ResultDataSPtr> GetResults(size_t tileId) const;
  const ResultDataSPtr GetResultData(size_t tileId, size_t idx) const;
  bool IsCollected() const;

 private:
  KernelRun m_runNum = 0;
  KernelRun m_globalRunNum = 0;
  DispatchId m_dispatchId = -1;
  bool m_collected = false;

  std::vector<std::vector<ResultDataSPtr>> m_tileResultData;

  friend class GTPinTool;
};

/**
 * @class SourcePoint
 * @brief Represents a source code location.
 */
class SourcePoint {
 public:
  SourcePoint(const std::string file = "", const int line = -1, const int column = -1,
              const std::string func = "");
  bool IsValid() const;
  const std::string GetFile() const;
  const int GetLine() const;
  const int GetColumn() const;
  const std::string GetFunction() const;

 private:
  const std::string m_file;
  const int m_line;
  const int m_column;
  const std::string m_function;
};

/**
 * @class AsmRecord
 * @brief Represents an assembly location.
 */
class AsmRecord {
 public:
  AsmRecord(const InstructionOffset instructionOffset, const std::string asmLineOrig = "",
            const SourcePoint sourcePoint = SourcePoint());
  // Getters
  const InstructionOffset GetInstructionOffset() const;
  const std::string GetAsmLineOrig() const;
  const SourcePoint GetSourcePoint() const;

 private:
  const InstructionOffset m_instructionOffset;
  const std::string m_asmLineOrig;
  const SourcePoint m_sourcePoint;
  friend class GTPinTool;
};

/**
 * @class KernelData
 * @brief Data for each kernel.
 */
class KernelData {
 public:
  KernelData(const gtpin::IGtKernelInstrument& instrumentor);
  virtual ~KernelData() = default;
  KernelData(KernelData&) = delete;
  KernelData& operator=(KernelData&) = delete;

  // Getters
  const std::string GetKernelName() const;
  const KernelId GetKernelId() const;
  const std::vector<AsmRecord> GetOrigAsm() const;
  const std::vector<uint8_t> GetOrigBinary() const;
  const size_t GetKernelRun() const;
  const std::unordered_map<DispatchId, InvocationDataSPtr> GetInvocations() const;
  const uint32_t GetRecordSize() const;
  const size_t GetSiteOfInstrumentNum() const;
  const size_t GetResultsNum() const;
  const size_t GetBucketsNum() const;
  const size_t GetTilesNum() const;  ///> HW number of tiles
  const size_t GetCollectedTilesNum() const;
  const SiteOfInstrumentSPtr GetSiteOfInstrument(size_t idx) const;
  std::vector<ResultDataCommonSPtr> GetResultDataCommon() const;
  ResultDataCommonSPtr GetResultDataCommon(size_t idx) const;

  bool IsRecordSizeSet();
  bool IsBucketsSet();

 private:
  const std::string m_kernelName;
  const KernelId m_kernelId;
  std::vector<AsmRecord> m_origAsm;
  std::vector<uint8_t> m_origBinary;

  std::vector<SiteOfInstrumentSPtr> m_sitesOfInterest;

  std::unordered_map<DispatchId, InvocationDataSPtr> m_invocations;
  std::vector<ResultDataCommonSPtr> m_resultDataCommon;
  uint32_t m_recordSize = -1;
  size_t m_buckets = 0;
  size_t m_kernelRuns = 0;
  size_t m_tilesNum = 0;
  size_t m_collectedTilesNum = 0;
  gtpin::GtProfileArray m_profileArray;

  friend class GTPinTool;
};

/**
 * @class ApplicationData
 * @brief Application data.
 */
class ApplicationData {
 public:
  ApplicationData() = default;
  virtual ~ApplicationData() = default;
  ApplicationData(ApplicationData&) = delete;
  ApplicationData& operator=(ApplicationData&) = delete;

  // Getters
  const std::string GetToolName() const;
  const std::string GetApplicationName() const;
  const std::unordered_map<KernelId, KernelDataSPtr> GetKernels() const;
  const KernelDataSPtr GetKernel(const KernelId& kernelId) const;

 protected:
  std::string m_toolName;
  std::string m_applicationName;
  std::unordered_map<KernelId, KernelDataSPtr> m_kernels;

  friend class GTPinTool;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_RESULTS_H
