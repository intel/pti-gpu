//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_TOOL_H
#define PTI_GTPIN_TOOL_H

#include <memory>
#include <vector>

#include "api/gtpin_api.h"
#include "control.hpp"
#include "def_gpu.hpp"
#include "results.hpp"
#include "tool_factory.hpp"
#include "writer.hpp"

/**
 * @file tool.hpp
 * @brief This file contains the declaration of the GTPinTool class, which implements the gtpin
 * IGtTool interface for tool registration in the GTPin framework. It also includes the declaration
 * of the RawRecord struct, which is used as the base class for profiling records.
 */

namespace gtpin_prof {

/**
 * @brief RawRecord is a base class that is used as an indivisible unit of profiling. One record is
 * used for one instrumentation site of instrument.
 */
struct RawRecord {};

/**
 * @class GTPinTool
 * @brief GTPinTool class implements the gtpin IGtTool interface, which is used for tool
 * registration in the GTPin framework. It provides common functions for tool implementation. And
 * encapsulates general logic of most GTPin profiling, providing an interface for tool-specific
 * implementation.
 */
class GTPinTool : public gtpin::IGtTool {
 public:
  /**
   * @brief Constructs a GTPinTool object.
   * @param factory A shared pointer to the ToolFactory object.
   * @param control A shared pointer to the ControlBase object. Defaults to a DefaultControl object.
   */
  GTPinTool(const ToolFactorySPtr factory);

  ~GTPinTool() override = default;

  GTPinTool(const GTPinTool&) = delete;
  GTPinTool& operator=(const GTPinTool&) = delete;

  /**
   * @brief Runs the writer after profiling finishes.
   * @param writer A shared pointer to the WriterBase object.
   * @return The status of the operation.
   */
  PROF_STATUS RunWriter(const WriterBaseSPtr writer) const;

  /**
   * @brief Gets the general GTPin knobs.
   * @return A vector of const char pointers representing the GTPin knobs typical for all
   * GTPin-based tools.
   */
  std::vector<const char*> GetCommonGTPinKnobs() const;

  /**
   * @brief Virtual function that may be optionally used for specifying GTPin knobs from the tool.
   * By default, it does nothing.
   * @return A vector of const char pointers representing the GTPin knobs specific for exact tool.
   */
  virtual std::vector<const char*> GetGTPinKnobs() const;

  /**
   * @brief Gets the global KernelRun object.
   * @return The global KernelRun object.
   */
  const KernelRun GetGlobalRun() const;

  /**
   * @brief Gets the profiling data for the application.
   * @return A shared pointer to the ApplicationData object.
   */
  const ApplicationDataSPtr GetProfilingData() const;

  // IGtTool interface
  const char* Name() const override = 0;
  void OnKernelBuild(gtpin::IGtKernelInstrument& instrumentor) final;
  void OnKernelRun(gtpin::IGtKernelDispatch& dispatcher) final;
  void OnKernelComplete(gtpin::IGtKernelDispatch& dispatcher) final;
  uint32_t ApiVersion() const final {
    // Implementation should be in header to catch if special version is required by
    // tool
#ifdef PTI_GTPIN_GTPIN_API_VERSION
    return PTI_GTPIN_GTPIN_API_VERSION;
#else
    return GTPIN_API_VERSION;
#endif
  }

 protected:
  /// Tool specific implementation of the GTPinTool interface. Functions that should be implemented
  /// in the tool and describes tool-specific behaviour.

  /**
   * @brief Analyzes the kernel binary and prepares it for instrumentation.
   * This function is called prior to kernel instrumentation. It examines the kernel binary,
   * determines the number of instrumentation sites, and number of the result data objects.
   * @param kernelData A shared pointer to the KernelData object.
   * @param instrumentor A reference to the IGtKernelInstrument object.
   * @return The status of the operation.
   */
  virtual PROF_STATUS AnalyzeKernel(KernelDataSPtr kernelData,
                                    const gtpin::IGtKernelInstrument& instrumentor) = 0;

  /**
   * @brief This function instruments the kernel data.
   * The function instruments kernel binary based on the results of the "AnalyzeKernel" function,
   * this function modifies the kernel binary to insert instrumentation sites. It instruments the
   * kernel binary based on the results of the "AnalyzeKernel" function. It applies the
   * instrumentation for all sites of instrument based on information stored in it.
   * @param kernelData A shared pointer to the KernelData object.
   * @param instrumentor A reference to the IGtKernelInstrument object.
   * @return The status of the operation.
   */
  virtual PROF_STATUS Instrument(KernelDataSPtr kernelData,
                                 gtpin::IGtKernelInstrument& instrumentor) = 0;

  /**
   * @brief Accumulates the profiling results.
   * This function accumulates the data from the RawRecord to the ResultData.
   * It accumulates data from RawRecord to ResultData. For each profiling results may be several
   * records, data should be accumulated, not just transferred.
   * @param kernelData A shared pointer to the KernelData object.
   * @param profilingResult A shared pointer to the ResultData object.
   * @param siteOfInstrument A shared pointer to the SiteOfInstrument object.
   * @param record A pointer to the RawRecord object.
   * @return The status of the operation.
   */
  virtual PROF_STATUS Accumulate(KernelDataSPtr kernelData, ResultDataSPtr profilingResult,
                                 SiteOfInstrumentSPtr siteOfInstrument, RawRecord* record) = 0;

  /**
   * @brief Processes the profiling data after it has been collected.
   * @param kernelData A shared pointer to the KernelData object.
   * @param invocationResult A shared pointer to the InvocationData object.
   * @return The status of the operation.
   */
  virtual PROF_STATUS PostProcData(KernelDataSPtr kernelData, InvocationDataSPtr invocationResult);

  /**
   * @brief Allocates resources for profiling.
   * This function allocates resources for profiling based on the results of the "AnalyzeKernel"
   * function.
   * @param kernelData A shared pointer to the KernelData object.
   * @param instrumentor A reference to the IGtKernelInstrument object.
   * @return The status of the operation.
   */
  PROF_STATUS AllocateResources(KernelDataSPtr kernelData,
                                const gtpin::IGtKernelInstrument& instrumentor);

  /**
   * @brief Initializes the profiling data.
   * This function initializes the profiling data based on the results of the "AnalyzeKernel"
   * function. Optional function, default implementation does nothing.
   * @param kernelData A shared pointer to the KernelData object.
   * @param dispatcher A reference to the IGtKernelDispatch object.
   * @return The status of the operation.
   */
  PROF_STATUS InitProfileData(KernelDataSPtr kernelData, gtpin::IGtKernelDispatch& dispatcher);

  /**
   * @brief Initializes the buffer for profiling.
   * This function initializes the buffer for profiling based on the results of the "AnalyzeKernel"
   * function.
   * @param kernelData A shared pointer to the KernelData object.
   * @param dispatcher A reference to the IGtKernelDispatch object.
   * @return The status of the operation.
   */
  PROF_STATUS InitBuffer(KernelDataSPtr kernelData, gtpin::IGtKernelDispatch& dispatcher);

  /**
   * @brief Reads profiling data from the GTPin buffer into the profiling data (results) using the
   * "Accumulate" function.
   * @param kernelData A shared pointer to the KernelData object.
   * @param dispatcher A reference to the IGtKernelDispatch object.
   * @return The status of the operation.
   */
  PROF_STATUS ReadProfileData(KernelDataSPtr kernelData, gtpin::IGtKernelDispatch& dispatcher,
                              const ToolFactorySPtr factory);

  /// Functions for manipulating KernelData
  KernelDataSPtr CreateKernelInStorage(const gtpin::IGtKernelInstrument& instrumentor);
  bool IsKernelInStorage(const KernelId& kernelId) const;
  KernelDataSPtr GetKernel(const KernelId& kernelId) const;
  size_t GetKernelsNum();
  std::vector<KernelId> GetKernelIds();
  size_t AddResultData(KernelDataSPtr kernelData, ResultDataCommonSPtr resultDataCommon);
  void IncKernelRuns(KernelDataSPtr kernelData);
  void SetRecordSize(KernelDataSPtr kernelData, uint32_t recordSize);
  void SetCollectedTiles(KernelDataSPtr kernelData, uint32_t collectTilesNum);
  void SetBucketsNum(KernelDataSPtr kernelData, size_t buckets);
  void SetDefaultBuckets(KernelDataSPtr kernelData, const gtpin::IGtKernelInstrument& instrumentor);
  void AddSiteOfInstrument(KernelDataSPtr kernelData, SiteOfInstrumentSPtr siteOfInstrument);
  SiteOfInstrumentSPtr GetSiteOfInstrument(KernelDataSPtr kernelData, size_t idx);
  gtpin::GtProfileArray& GetProfileArray(KernelDataSPtr kernelData);
  void SetInvocationCollected(KernelDataSPtr kernelData, gtpin::IGtKernelDispatch& dispatcher);

  void MapResultData(SiteOfInstrumentSPtr siteOfInstrument, size_t resultDataIdx);
  std::vector<ResultDataSPtr> GetResultDataForSiteOfInstrument(
      InvocationDataSPtr invocation, SiteOfInstrumentSPtr siteOfInstrument);

  const ToolFactorySPtr GetFactory();
  const ControlBaseSPtr GetControl();

 private:
  /// Increment internal counter of global runs
  void IncGlobalRuns();

  /// Kernels invocation counter - total number of kernels runs
  KernelRun m_globalRun;
  /// Profiling data (results) storage. Passed to writer
  ApplicationDataSPtr const m_applicationData;

  const ToolFactorySPtr m_factory;
  const ControlBaseSPtr m_control;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_TOOL_H
