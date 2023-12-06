
#include "gtpin_tool.hpp"

#include "results_gtpin.hpp"

/**
 * @file Implementation of GTPin tool classes
 */

using namespace gtpin;
namespace gtpin_prof {

/**
 * InvocationData class implementation
 */

InvocationData::InvocationData(const GTPinKernelExecDesriptor& execDescr)
    : kernelId(execDescr.id),
      runNum(execDescr.runIdx),
      globalRunNum(execDescr.runGlobalIdx),
      gtpinDispatchId(execDescr.gtpinDispatchId) {}

/**
 * AsmRecord class implementation
 */

AsmRecord::AsmRecord(const InstructionOffset instructionOffset, const std::string asmLineOrig)
    : instructionOffset(instructionOffset), asmLineOrig(asmLineOrig){};

/**
 * KernelData class implementation
 */

KernelData::KernelData(const std::string name, const GtKernelId id,
                       const std::vector<AsmRecord> origAsm)
    : kernelName(name), kernelId(id), origAsm(origAsm) {}

std::vector<AsmRecord> GetAsm(IGtKernelInstrument& instrumentor) {
  const IGtCfg& cfg = instrumentor.Cfg();
  std::vector<AsmRecord> asmRecords;

  for (auto bblPtr : cfg.Bbls()) {
    BblId bblId = bblPtr->Id();
    for (auto insPtr : bblPtr->Instructions()) {
      const InstructionOffset offset = cfg.GetInstructionOffset(*insPtr);
      asmRecords.emplace_back(offset, std::string(insPtr->ToString()));
    }
  }
  return asmRecords;
};

KernelData::KernelData(IGtKernelInstrument& instrumentor)
    : KernelData(instrumentor.Kernel().Name(), instrumentor.Kernel().Id(), GetAsm(instrumentor)){};

/**
 * GTPinTool class implementation
 */

GTPinTool::GTPinTool(const std::shared_ptr<IToolFactory> toolFactory,
                     const std::shared_ptr<GTPinDataWriterBase> writer,
                     const std::shared_ptr<GTPinFilterBase> filter)
    : gtpin::IGtTool(), m_factory(toolFactory), m_writer(writer), m_filter(filter) {
  m_globalRun = 0;
  m_profilingData = this->GetFactory()->MakeProfilerData();
}

const std::shared_ptr<IToolFactory> GTPinTool::GetFactory() {
  PTI_ASSERT(m_factory != nullptr && "Tool wrongly initialized (factory)");
  return m_factory;
}

std::shared_ptr<GTPinDataWriterBase> GTPinTool::GetWriter() {
  PTI_ASSERT(m_writer != nullptr && "Tool wrongly initialized (writer)");
  return m_writer;
}

const std::shared_ptr<GTPinFilterBase> GTPinTool::GetFilter() {
  PTI_ASSERT(m_filter != nullptr && "Tool wrongly initialized (filter)");
  return m_filter;
}

std::vector<const char*> GTPinTool::SetCommonGtpinKnobs() const {
  // return std::vector<const char*>{"--allow_sregs", "0",
  //  "-d"};
  return std::vector<const char*>{};
}

std::vector<const char*> GTPinTool::SetGtpinKnobs() const { return std::vector<const char*>{}; }

void GTPinTool::OnKernelBuild(IGtKernelInstrument& instrumentor) {
  PROF_STATUS error;

  std::shared_ptr<GTPinProfileKernel> kernelProfile = this->CreateKernelInStorage(instrumentor);

  if (!this->GetFilter()->ShouldInstrument(instrumentor)) {
    return;
  }

  error = kernelProfile->AnalyzeKernel(instrumentor);
  if (error == PROF_STATUS_NOTHING_TO_INSTRUMENT) {
    return;
  }
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to analyze kernel");
  /// TODO: ("check for updating array size")

  error = kernelProfile->AllocateResources(instrumentor);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to allocate resources");

  error = kernelProfile->Instrument(instrumentor);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to instrument kernel");

  /// TODO: ("set HW profiling based on filter and SetProfileFilter")
  // m_filter->GetHWProfileMask()
}

void GTPinTool::OnKernelRun(IGtKernelDispatch& dispatcher) {
  PROF_STATUS error;
  GtKernelExecDesc gtExecDesc;
  dispatcher.GetExecDescriptor(gtExecDesc);
  if ((!dispatcher.Kernel().IsInstrumented() &&
       IsKernelExecProfileEnabled(gtExecDesc, dispatcher.Kernel().GpuPlatform()))) {
    return;
  }

  this->IncrementGlobalRuns();

  /// Get kernel from storage. Warning if kernel not in storage, but not profile
  if (!this->IsKernelInStorage(dispatcher.Kernel().Id())) {
    /// TODO: ("Assert-like problem here")
    /// TODO: ("Warning, enqueue will not be profiled")
    dispatcher.SetProfilingMode(false);
    return;
  }
  std::shared_ptr<GTPinProfileKernel> kernelProfile = this->GetKernel(dispatcher.Kernel().Id());
  kernelProfile->IncrementKernelRuns();

  /// TODO: check that bug fixed, use next execDescr instead of enqueue-based
  /// one
  // auto execDescr = GTPinKernelExecDesriptor(
  //     dispatcher, GetGlobalRun(), kernelProfile->GetKernelRun());
  uint64_t dispatchId = 0;
  GtKernelExecDesc GtExecDesc;
  dispatcher.GetExecDescriptor(GtExecDesc);
  switch (dispatcher.Kernel().GpuPlatform()) {
    case GPU_PLATFORM_OCL: {
      dispatchId = GtExecDesc.oclExecDesc.enqueue_index;
      break;
    }
    case GPU_PLATFORM_L0: {
      dispatchId = GtExecDesc.l0ExecDesc.enqueue_index;
      break;
    }
    default:
      dispatchId = 0;
  }
  auto execDescr =
      GTPinKernelExecDesriptor(dispatcher.Kernel().Name().Get(), dispatcher.Kernel().Id(),
                               dispatchId, GetGlobalRun(), kernelProfile->GetKernelRun());

  if (!this->GetFilter()->ShouldProfile(execDescr)) {
    return;
  }

  error = kernelProfile->InitProfileData(dispatcher, execDescr, m_factory);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to initiate result");

  error = kernelProfile->InitProfileBuffer(dispatcher);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to initiate array");

  dispatcher.SetProfilingMode(true);
}

void GTPinTool::OnKernelComplete(IGtKernelDispatch& dispatcher) {
  PROF_STATUS error;

  // Get kernel from storage. Warning if kernel not in storage, but not profile
  /// TODO: assert-like problem
  if (!this->IsKernelInStorage(dispatcher.Kernel().Id())) {
    /// TODO: ("Warning, enqueue will not be profiled")
    return;
  }

  std::shared_ptr<GTPinProfileKernel> kernelProfile = this->GetKernel(dispatcher.Kernel().Id());

  if (!dispatcher.Kernel().IsInstrumented() || !dispatcher.IsProfilingEnabled()) {
    return;
  }

  /// TODO: check that bug fixed, use next execDescr instead of enqueue-based
  /// one
  // auto execDescr = GTPinKernelExecDesriptor(
  //     dispatcher, GetGlobalRun(), kernelProfile->GetKernelRun());
  uint64_t dispatchId = 0;
  GtKernelExecDesc GtExecDesc;
  dispatcher.GetExecDescriptor(GtExecDesc);
  switch (dispatcher.Kernel().GpuPlatform()) {
    case GPU_PLATFORM_OCL: {
      dispatchId = GtExecDesc.oclExecDesc.enqueue_index;
      break;
    }
    case GPU_PLATFORM_L0: {
      dispatchId = GtExecDesc.l0ExecDesc.enqueue_index;
      break;
    }
    default:
      dispatchId = 0;
  }
  auto execDescr =
      GTPinKernelExecDesriptor(dispatcher.Kernel().Name().Get(), dispatcher.Kernel().Id(),
                               dispatchId, GetGlobalRun(), kernelProfile->GetKernelRun());

  error = kernelProfile->ReadProfileData(dispatcher, execDescr, m_factory);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error && "Fail to read data");
}

std::shared_ptr<GTPinProfileKernel> GTPinTool::CreateKernelInStorage(
    IGtKernelInstrument& instrumentor) {
  const GtKernelId& id = instrumentor.Kernel().Id();

  // Check if kernel exist in storage
  PTI_ASSERT(this->IsKernelInStorage(id) == false && "Kernel is already instrumented");

  // Create kernel data in storage
  auto kernelData = this->GetFactory()->MakeKernelData(instrumentor);
  m_profilingData->kernels[id] = kernelData;

  auto kernel = this->GetFactory()->MakeKernel(instrumentor, kernelData);

  m_kernelStorage.push_back(kernel);

  return kernel;
}

bool GTPinTool::IsKernelInStorage(const GtKernelId& id) const {
  for (std::shared_ptr<GTPinProfileKernel> ker : m_kernelStorage) {
    if (ker->GetKernelId() == id) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<GTPinProfileKernel> GTPinTool::GetKernel(const GtKernelId& id) {
  for (std::shared_ptr<GTPinProfileKernel> ker : m_kernelStorage) {
    if (ker->GetKernelId() == id) {
      return ker;
    }
  }
  PTI_ASSERT(false && "Kernel is not in storage");
  return nullptr;
}

KernelRun GTPinTool::GetGlobalRun() const { return m_globalRun; }

void GTPinTool::RunWriter() {
  bool result = this->GetWriter()->Init();
  PTI_ASSERT(result && "Error during writer initialization");
  this->GetWriter()->Write(m_profilingData);
};

/**
 * GTPinProfileKernel class Implementation
 */

GTPinProfileKernel::GTPinProfileKernel(IGtKernelInstrument& instrumentor,
                                       std::shared_ptr<KernelData> kernelData)
    : m_kernelData(kernelData),
      m_id(instrumentor.Kernel().Id()),
      m_numTiles(((instrumentor.Coder().IsTileIdSupported())
                      ? GTPin_GetCore()->GenArch().MaxTiles(instrumentor.Kernel().GpuPlatform())
                      : 1)) {
  /// TODO: Copy next values:
  // Kernel's type (GtKernelType _type)
  // Kernel's platform (GtGpuPlatform _platform)
  // Kernel's hash identifier (uint64_t _hashId)
  // Kernel simd width (GtSimdWidth _simd)
}

PROF_STATUS GTPinProfileKernel::AllocateResources(IGtKernelInstrument& instrumentor) {
  PTI_ASSERT((m_recordSize != -1) &&
             "Record size not inititalized. Check \"AnalyzeKernel\" function");
  PTI_ASSERT((m_recordSize != 0) && "Zero record size. Check \"AnalyzeKernel\" function");
  PTI_ASSERT((m_recordsNum != -1) &&
             "Record num not inititalized. Check \"AnalyzeKernel\" function");
  PTI_ASSERT((m_recordsNum != 0) && "Zero record num. Check \"AnalyzeKernel\" function");

  if (m_buckets == 0) {
    SetDefautBuckets(instrumentor);
  }

  IGtProfileBufferAllocator& allocator = instrumentor.ProfileBufferAllocator();
  m_profileArray = GtProfileArray(m_recordSize, m_recordsNum, m_buckets);
  if (!m_profileArray.Allocate(allocator)) {
    std::cout << "GTPin has not initialized profile buffer.\n" << GTPIN_LAST_ERROR_STR << std::endl;
    return PROF_STATUS_ERROR;
  }

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GTPinProfileKernel::InitProfileData(IGtKernelDispatch& dispatcher,
                                                const GTPinKernelExecDesriptor& execDescr,
                                                const std::shared_ptr<IToolFactory> factory) {
  PROF_STATUS error = PROF_STATUS_ERROR;

  m_kernelData->invocations[execDescr.gtpinDispatchId] = factory->MakeInvocationData(execDescr);
  PTI_ASSERT(m_kernelData->invocations[execDescr.gtpinDispatchId] != nullptr &&
             "Invocation data was not initialized");

  error = this->InitResultData(m_kernelData->invocations[execDescr.gtpinDispatchId], dispatcher,
                               execDescr, factory);
  PTI_ASSERT(PROF_STATUS_SUCCESS == error &&
             "Fail to init result data during execution of \"InitResultData\" "
             "function");
  PTI_ASSERT(m_kernelData->invocations[execDescr.gtpinDispatchId]->data.size() > 0 &&
             "Result data was not initialized, check \"InitResultData\" function");

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GTPinProfileKernel::InitProfileBuffer(IGtKernelDispatch& dispatcher) {
  IGtProfileBuffer* buffer = dispatcher.CreateProfileBuffer();
  PTI_ASSERT(buffer != nullptr && "Profile buffer was not created");

  bool success = m_profileArray.Initialize(*buffer);

  return (success) ? PROF_STATUS_SUCCESS : PROF_STATUS_ERROR;
}

PROF_STATUS GTPinProfileKernel::ReadProfileData(IGtKernelDispatch& dispatcher,
                                                const GTPinKernelExecDesriptor& execDescr,
                                                const std::shared_ptr<IToolFactory> factory) {
  PROF_STATUS error;

  const IGtProfileBuffer* buffer = dispatcher.GetProfileBuffer();
  PTI_ASSERT((buffer != nullptr) && "Profile kernel was not found");

  auto profilingResults = m_kernelData->invocations[execDescr.gtpinDispatchId]->data;
  GTPinProfileRecord* record = factory->MakeRecord();

  for (size_t i = 0; i < m_recordsNum; i++) {
    for (uint32_t threadBucket = 0; threadBucket < m_profileArray.NumThreadBuckets();
         ++threadBucket) {
      if (!m_profileArray.Read(*buffer, record, i, 1, threadBucket)) {
        return PROF_STATUS_ERROR;
      } else {
        PTI_ASSERT((record != nullptr) && "Record is corrupted");
        error =
            this->Accumulate(profilingResults[i], reinterpret_cast<GTPinProfileRecord*>(record));
        PTI_ASSERT((PROF_STATUS_SUCCESS == error) && "Fail to accumulate result data");
      }
    }
  }

  error = this->PostProcData(m_kernelData->invocations[execDescr.gtpinDispatchId]);
  PTI_ASSERT((PROF_STATUS_SUCCESS == error) && "Fail to read data");

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GTPinProfileKernel::PostProcData(std::shared_ptr<InvocationData> invocationData) {
  return PROF_STATUS_SUCCESS;
};

}  // namespace gtpin_prof
