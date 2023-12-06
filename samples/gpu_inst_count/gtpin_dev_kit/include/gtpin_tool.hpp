#ifndef GTPIN_TOOL_H
#define GTPIN_TOOL_H

#include <memory>
#include <vector>

#include "api/gtpin_api.h"
#include "def_gpu_gtpin.hpp"
#include "filter.hpp"
#include "kernel_exec_config.h"
#include "knob_parser.h"
#include "results_gtpin.hpp"
#include "writer.hpp"

/**
 * @file Declarations of classes that forms GTPin tool
 */

//
// "Filter" - object that defines what should be profiled.
// "Writer" - object that describes how the data should be stored.
// "GTPin profiling buffer" - memory buffer that is by binary instrumentation.
// Raw data can be
//      stored here from the kernel's instrumentation.
// "Profiling data (results)" - tree of classes, that stores the results of
// profiling. Base
//      classes are desribed in results header, tools have it's own,
//      tool-specific, delivered versions with extended fields. This class-tree
//      is passed to writer to save results of profiling.

namespace gtpin_prof {

using namespace gtpin;

class IToolFactory;

/**
 * @brief GTPinProfileRecord is a base class that is used as indivisible unit of
 * profiling. Usually one record is used for one instrumentation point of
 * interest.
 */
struct GTPinProfileRecord {};

/**
 * @brief GTPinProfileKernel class provides per-kernel functionality for profiling. It
 * implements memory management, data copy, instrumentaiton functions. Binary
 * instrumentaion- specific functions are virtual and should be instrumented in
 * tool-specific libraries.
 */
class GTPinProfileKernel {
 public:
  GTPinProfileKernel(IGtKernelInstrument& instrumentor, std::shared_ptr<KernelData> kernelData);
  virtual ~GTPinProfileKernel() = default;

  /**
   * Instrumentation-specific functions, which have different behaviour and are
   * implemented for each tool in tool's source file.
   *
   * The GTPin-based profiling process is divided into three parts:
   * instrumentation (once per applicaiton run), kernel run, and kernel
   * complete.
   * - During instrumentation, kernels are analyzed (AnalyzeKernel), additional
   * instructions injected (Instrument).
   * - On kernel run, profiling buffers are allocated, results are prepared.
   * "InitResultData" is used for results data initialization.
   * - After completing the kernel execution, the data copied from profiling
   * buffer into tool specific result classes (ProfilerData, KernelData,
   * InvocationData, ResultData). The "Accumulate" function is used for this
   * purposes. "PostProcData" is optional and may be used for post
   * process/normalization purposes after data copy.
   *
   * See tool draft header for more information
   */
  virtual PROF_STATUS AnalyzeKernel(IGtKernelInstrument& instrumentor) = 0;
  virtual PROF_STATUS Instrument(IGtKernelInstrument& instrumentor) = 0;
  virtual PROF_STATUS InitResultData(std::shared_ptr<InvocationData> invocationData,
                                     IGtKernelDispatch& dispatcher,
                                     const GTPinKernelExecDesriptor& execDescr,
                                     const std::shared_ptr<IToolFactory> factory) = 0;
  virtual PROF_STATUS Accumulate(std::shared_ptr<ResultData> profilingResult,
                                 GTPinProfileRecord* record) = 0;
  virtual PROF_STATUS PostProcData(std::shared_ptr<InvocationData> invocationResult);

  /** Helper functions which is used in instrumentation process. */
  inline bool Is64BitCountersSupport(IGtKernelInstrument& instrumentor) {
    return (instrumentor.Coder().InstructionFactory().CanAccessAtomically(GED_DATA_TYPE_uq));
  }
  inline GED_DATA_TYPE GetCounterDataType(IGtKernelInstrument& instrumentor) {
    return ((this->Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud);
  }
  inline GED_DATA_TYPE GetCounterDataTypeSigned(IGtKernelInstrument& instrumentor) {
    return ((this->Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_q : GED_DATA_TYPE_d);
  }
  inline VREG_TYPE GetCounterVregDataType(IGtKernelInstrument& instrumentor) {
    return ((this->Is64BitCountersSupport(instrumentor)) ? VREG_TYPE_QWORD : VREG_TYPE_DWORD);
  }

  /** Next functions have same behaviour for different tools */

  /// Setups GTPin profile buffer, based on the work of the "AnalyzeKernel"
  /// function
  PROF_STATUS AllocateResources(IGtKernelInstrument& instrumentor);

  /// Increment internal counter of global runs
  inline void IncrementKernelRuns() { m_kernelData->totalRuns++; };

  /// Initializes tool's profiling data classes, which is used as input for a
  /// data writer
  PROF_STATUS InitProfileData(IGtKernelDispatch& dispatcher,
                              const GTPinKernelExecDesriptor& execDescr,
                              const std::shared_ptr<IToolFactory> factory);

  /// Initializes GTPin profiling buffer
  PROF_STATUS InitProfileBuffer(IGtKernelDispatch& dispatcher);

  /// Reads profiling data from GTPin buffer into profiling data (results) using
  /// "Accumulate" function
  PROF_STATUS ReadProfileData(IGtKernelDispatch& dispatcher,
                              const GTPinKernelExecDesriptor& execDescr,
                              const std::shared_ptr<IToolFactory> factory);

  /// Getters
  inline KernelRun GetKernelRun() { return m_kernelData->totalRuns; }
  inline int32_t GetRecordSize() { return m_recordSize; }
  inline size_t GetRecordsNum() { return m_recordsNum; }
  inline size_t GetBucketsNum() { return m_buckets; }
  inline GtKernelId GetKernelId() { return m_id; }

 protected:
  /**
   * Functions that configures GTPin profiling buffer. Next parameters should be
   * set during "AnalyzeKernel": RecordSize, RecordsNum, BucketsNum. Use
   * "SetDefautBuckets" to set number of buckets corresponding to number of
   * hardware threads.
   *
   * "RecordSize" - size of one GTPinProfileRecord. In general,
   * sizeof(ToolSpecificRecord) can be used. "RecordsNum" - number of records in
   * GTPin profile buffer. Usualy, same as number of instrumentation points
   * "BucketsNum" - number of "buckets", independant copies, to simplify
   * parallel access from different HW threads
   *
   * total GTPin profiling buffer size in bytes = RecordSize * RecordsNum *
   * BucketsNum
   */
  inline void SetRecordSize(uint32_t recordSize) { m_recordSize = recordSize; }
  inline void SetRecordsNum(size_t recordsNum) { m_recordsNum = recordsNum; }
  inline void SetBucketsNum(size_t buckets) { m_buckets = buckets; }
  inline const std::string GetKernelName() { return m_kernelData->kernelName; }
  inline void SetDefautBuckets(IGtKernelInstrument& instrumentor) {
    this->SetBucketsNum(instrumentor.Kernel().GenModel().MaxThreadBuckets());
  }

  /// storage for result kernel data, passed to writer
  std::shared_ptr<KernelData> m_kernelData;

  /// GTPin variables
  /// m_profileArray is object that incapsulates methods and data of GTPin
  /// profiling buffer
  GtProfileArray m_profileArray;

  /// Max number of tiles available to kernel which is compiled for specific
  /// device
  const uint32_t m_numTiles;

 private:
  // GTPin instrumentation resources
  uint32_t m_recordSize = -1;
  size_t m_recordsNum = -1;
  size_t m_buckets = 0;
  GtKernelId m_id;
};

/**
 * GTPinTool class implements gtpin IGtTool interface, that is used for tool
 * registration. It has mostly common functions. The functions that should be
 * redefined in tool are "Name()" and "SetGtpinKnobs()"
 */
class GTPinTool : public gtpin::IGtTool {
 public:
  GTPinTool(
      const std::shared_ptr<IToolFactory> kernelFactory,
      const std::shared_ptr<GTPinDataWriterBase> writer = std::make_shared<DefaultGTPinWriter>(),
      const std::shared_ptr<GTPinFilterBase> filter = std::make_shared<DefaultGTPinFilter>());
  ~GTPinTool() override = default;

  /// Runs writer, it is called after profiling finish
  void RunWriter();

  /// Sets general GTPin knobs
  std::vector<const char*> SetCommonGtpinKnobs() const;

  /// Virtual function that may be optionaly used for setting GTPin knobs from
  /// the tool. By default does nothing.
  virtual std::vector<const char*> SetGtpinKnobs() const;

 protected:
  /// Getters
  const std::shared_ptr<IToolFactory> GetFactory();
  std::shared_ptr<GTPinDataWriterBase> GetWriter();
  const std::shared_ptr<GTPinFilterBase> GetFilter();
  KernelRun GetGlobalRun() const;

  /// Profiling data (results) storage. Passed to writer
  std::shared_ptr<ProfilerData> m_profilingData;

 public:
  /// IGtTool interface
  const char* Name() const override = 0;
  void OnKernelBuild(IGtKernelInstrument& instrumentor) final;
  void OnKernelRun(IGtKernelDispatch& dispatcher) final;
  void OnKernelComplete(IGtKernelDispatch& dispatcher) final;
  uint32_t ApiVersion() const final {
// Implementation should be in header to catch if special version is requered by
// tool
#ifdef PLGG_GTPIN_API_VERSION
    return PLGG_GTPIN_API_VERSION;
#else
    return GTPIN_API_VERSION;
#endif
  }

 private:
  GTPinTool(const GTPinTool&) = delete;
  GTPinTool& operator=(const GTPinTool&) = delete;

  /// Storage for "GTPinProfileKernel" objects
  std::vector<std::shared_ptr<GTPinProfileKernel>> m_kernelStorage;

  /// Kernel storage functions
  std::shared_ptr<GTPinProfileKernel> CreateKernelInStorage(IGtKernelInstrument& instrumentor);
  bool IsKernelInStorage(const GtKernelId& id) const;
  std::shared_ptr<GTPinProfileKernel> GetKernel(const GtKernelId& id);

  /// Kernels invocation counter - total number of kerenls runs
  KernelRun m_globalRun;
  inline void IncrementGlobalRuns() { m_globalRun++; };

  const std::shared_ptr<IToolFactory> m_factory;
  const std::shared_ptr<GTPinDataWriterBase> m_writer;
  const std::shared_ptr<GTPinFilterBase> m_filter;
};

/**
 * IToolFactory interface. Factory is used for construction tool-specific
 * objects
 */
class IToolFactory {
 public:
  virtual std::shared_ptr<GTPinProfileKernel> MakeKernel(
      IGtKernelInstrument& instrumentor, std::shared_ptr<KernelData> kernelData) = 0;
  virtual GTPinProfileRecord* MakeRecord() = 0;
  virtual std::shared_ptr<ProfilerData> MakeProfilerData() = 0;
  virtual std::shared_ptr<KernelData> MakeKernelData(IGtKernelInstrument& instrumentor) = 0;
  virtual std::shared_ptr<InvocationData> MakeInvocationData(
      const GTPinKernelExecDesriptor& execDescr) = 0;
  virtual std::shared_ptr<ResultData> MakeResultData() = 0;
};

}  // namespace gtpin_prof

#endif  // GTPIN_TOOL_H
