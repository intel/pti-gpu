
#include "gpu_inst_count.hpp"

using namespace gtpin_prof;
using namespace gtpin;

/********************
 * Requered functions - should be implemented
 */

PROF_STATUS GpuInstCountKernel::Accumulate(std::shared_ptr<ResultData> profilingResult,
                                           GTPinProfileRecord* record) {
  auto gpuInstCountRec = reinterpret_cast<GpuInstCountRecord*>(record);
  auto gpuInstCountResult = std::dynamic_pointer_cast<GpuInstCountResultData>(profilingResult);

  /// Accumulate data from GpuInstCountRec to GpuInstCountResult here.
  /// For each profiling results may be several records, data should be
  /// accumulated, not just transferred
  gpuInstCountResult->count += gpuInstCountRec->count;

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GpuInstCountKernel::AnalyzeKernel(IGtKernelInstrument& instrumentor) {
  const IGtKernel& kernel = instrumentor.Kernel();
  const IGtCfg& cfg = instrumentor.Cfg();
  const IGtGenArch& genArch = GTPin_GetCore()->GenArch();

  SetRecordSize(sizeof(GpuInstCountRecord));
  SetDefautBuckets(instrumentor);

  for (auto bblPtr : cfg.Bbls()) {
    for (auto insPtr : bblPtr->Instructions()) {
      const IGtIns& ins = *insPtr;
      const InstructionOffset offset = cfg.GetInstructionOffset(ins);

      bblData.emplace(offset, bblPtr->FirstIns());
    }
  }
  /// Set number of records and store required data based on information from
  /// instrumentor
  SetRecordsNum(bblData.size());

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GpuInstCountKernel::Instrument(IGtKernelInstrument& instrumentor) {
  const IGtKernel& kernel = instrumentor.Kernel();
  const IGtCfg& cfg = instrumentor.Cfg();
  const IGtGenCoder& coder = instrumentor.Coder();
  IGtVregFactory& vregs = coder.VregFactory();
  IGtInsFactory& insF = coder.InstructionFactory();

  const IGtGenArch& genArch = GTPin_GetCore()->GenArch();
  uint32_t grfRegSize = insF.GenModel().GrfRegSize();  // bytes

  GtGenProcedure proc;

  size_t bblIdx = 0;
  for (auto it = bblData.begin(); it != bblData.end(); it++, bblIdx++) {
    GtGenProcedure proc;
    PointOfInterest poi(instrumentor, m_profileArray, bblIdx);
    poi.InstructionCounterAnalysis(offsetof(GpuInstCountRecord, count));
    poi.ClosePOI(proc);
    instrumentor.InstrumentInstruction(it->second, GtIpoint::Before(), proc);
  }

  return PROF_STATUS_SUCCESS;
}

/********************
 * Optional functions - may be changed or not, base on tool behaviour
 */

PROF_STATUS GpuInstCountKernel::InitResultData(std::shared_ptr<InvocationData> invocationData,
                                               IGtKernelDispatch& dispatcher,
                                               const GTPinKernelExecDesriptor& execDescr,
                                               const std::shared_ptr<IToolFactory> factory) {
  auto invData = std::dynamic_pointer_cast<GpuInstCountInvocationData>(invocationData);
  PTI_ASSERT((invData != nullptr) && "Invocation data was wrongly initialized. Check factory.");

  size_t idx = 0;
  for (auto it = bblData.begin(); it != bblData.end(); it++, idx++) {
    auto resData = factory->MakeResultData();
    auto gpuInstCountResult = std::dynamic_pointer_cast<GpuInstCountResultData>(resData);
    gpuInstCountResult->instructionOffset = it->first;
    invData->data.push_back(gpuInstCountResult);
  }

  return PROF_STATUS_SUCCESS;
};

PROF_STATUS GpuInstCountKernel::PostProcData(std::shared_ptr<InvocationData> invocationData) {
  return PROF_STATUS_SUCCESS;
}

/**
 * GpuInstCount implementations
 */
std::vector<const char*> GpuInstCount::SetGtpinKnobs() const {
  return std::vector<const char*>{"--no_empty_profile_dir"};
};

/**
 * GpuInstCountFactory implementations
 */
std::shared_ptr<GTPinProfileKernel> GpuInstCountFactory::MakeKernel(
    IGtKernelInstrument& instrumentor, std::shared_ptr<KernelData> kernelData) {
  return std::make_shared<GpuInstCountKernel>(instrumentor, kernelData);
}

GTPinProfileRecord* GpuInstCountFactory::MakeRecord() {
  GpuInstCountRecord* rec = new GpuInstCountRecord();
  return rec;
};

std::shared_ptr<ProfilerData> GpuInstCountFactory::MakeProfilerData() {
  return std::make_shared<GpuInstCountProfilerData>();
};

std::shared_ptr<KernelData> GpuInstCountFactory::MakeKernelData(IGtKernelInstrument& instrumentor) {
  return std::make_shared<GpuInstCountKernelData>(instrumentor);
};

std::shared_ptr<InvocationData> GpuInstCountFactory::MakeInvocationData(
    const GTPinKernelExecDesriptor& execDescr) {
  return std::make_shared<GpuInstCountInvocationData>(execDescr);
};

std::shared_ptr<ResultData> GpuInstCountFactory::MakeResultData() {
  return std::make_shared<GpuInstCountResultData>();
};
