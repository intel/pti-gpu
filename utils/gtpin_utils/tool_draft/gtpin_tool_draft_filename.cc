//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "gtpin_tool_draft_filename.hpp"

namespace gtpin {
namespace gtpin_prof {

/********************
 * Requered functions - should be implemented
 */

PROF_STATUS GtpinToolDraftKernel::Accumulate(std::shared_ptr<ResultData> profilingResult,
                                             GTPinProfileRecord* record) {
  auto gtpinToolDraftRec = reinterpret_cast<GtpinToolDraftRecord*>(record);
  auto gtpinToolDraftResult = std::dynamic_pointer_cast<GtpinToolDraftResultData>(profilingResult);

  /// Accumulate data from GtpinToolDraftRec to GtpinToolDraftResult here.
  /// For each profiling results may be several records, data should be
  /// accumulated, not just transferred

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GtpinToolDraftKernel::AnalyzeKernel(IGtKernelInstrument& instrumentor) {
  const IGtKernel& kernel = instrumentor.Kernel();
  const IGtCfg& cfg = instrumentor.Cfg();
  const IGtGenArch& genArch = GTPin_GetCore()->GenArch();

  SetRecordSize(sizeof(GtpinToolDraftRecord));
  SetDefautBuckets(instrumentor);

  for (auto bblPtr : cfg.Bbls()) {
    for (auto insPtr : bblPtr->Instructions()) {
      const IGtIns& ins = *insPtr;
      const InstructionOffset offset = cfg.GetInstructionOffset(ins);
    }
  }
  /// Set number of records and store required data based on information from
  /// instrumentor
  SetRecordsNum(1);

  return PROF_STATUS_SUCCESS;
}

PROF_STATUS GtpinToolDraftKernel::Instrument(IGtKernelInstrument& instrumentor) {
  const IGtKernel& kernel = instrumentor.Kernel();
  const IGtCfg& cfg = instrumentor.Cfg();
  const IGtGenCoder& coder = instrumentor.Coder();
  IGtVregFactory& vregs = coder.VregFactory();
  IGtInsFactory& insF = coder.InstructionFactory();

  /// Code example
  // const IGtGenArch& genArch = GTPin_GetCore()->GenArch();
  // uint32_t grfRegSize = insF.GenModel().GrfRegSize();  /// bytes

  // GtReg tileIdReg =
  //     vregs.MakeScratch(VREG_TYPE_QWORD);  /// Scratch registers are available
  //                                          /// only inside specific procedure.
  // GtReg counterReg =
  //     vregs.Make(VREG_TYPE_QWORD);  /// General registers are allocated for all
  //                                   /// kernel and available at any time
  // GtReg addrReg =
  //     vregs.MakeMsgAddrScratch();  /// Allocate register which can be used as an
  //                                  /// address for memory access
  // GtReg msgDataReg =
  //     vregs.MakeMsgDataScratch();  /// Allocate register which can be used as an
  //                                  /// data storage for memory access

  // GtGenProcedure proc;

  // proc += insF.MakeMov(addrReg, 0);
  // coder.LoadTileId(proc, tileIdReg);

  // instrumentor.InstrumentEntries(proc); /// Inject procedure at every kernel
  // entry points instrumentor.InstrumentExits(proc); /// Inject procedure at
  // every kernel exit points instrumentor.InstrumentInstruction(const IGtIns&
  // ins, GtIpoint::Before(), proc); /// inject procedure before or after
  // specific assembly instruction instrumentor.InstrumentBbl(const IGtBbl& bbl,
  // GtIpoint::Before(), proc); /// inject procedure before or after specific
  // basic block

  return PROF_STATUS_SUCCESS;
}

/********************
 * Optional functions - may be changed or not, base on tool behaviour
 */

PROF_STATUS GtpinToolDraftKernel::InitResultData(std::shared_ptr<InvocationData> invocationData,
                                                 IGtKernelDispatch& dispatcher,
                                                 const GTPinKernelExecDesriptor& execDescr,
                                                 const std::shared_ptr<IToolFactory> factory) {
  auto invData = std::dynamic_pointer_cast<GtpinToolDraftInvocationData>(invocationData);
  PTI_ASSERT((invData != nullptr) && "Invocation data was wrongly initialized. Check factory.");

  for (size_t idx = 0; idx < this->GetRecordsNum(); idx++) {
    auto resData = factory->MakeResultData();
    auto gtpinToolDraftResult = std::dynamic_pointer_cast<GtpinToolDraftResultData>(resData);
    /// Place result specific initialization here if needed
    invData->data.push_back(gtpinToolDraftResult);
  }

  return PROF_STATUS_SUCCESS;
};

PROF_STATUS GtpinToolDraftKernel::PostProcData(std::shared_ptr<InvocationData> invocationData) {
  return PROF_STATUS_SUCCESS;
}

/**
 * GtpinToolDraft implementations
 */
std::vector<const char*> GtpinToolDraft::SetGtpinKnobs() const {
  /// Knob example:
  // return std::vector<const char*> {
  //     "--allow_sregs", "0",
  //     "--dump_cfg"};

  return std::vector<const char*>{};
};

/**
 * GtpinToolDraftFactory implementations
 */
std::shared_ptr<GTPinProfileKernel> GtpinToolDraftFactory::MakeKernel(
    IGtKernelInstrument& instrumentor, std::shared_ptr<KernelData> kernelData) {
  return std::make_shared<GtpinToolDraftKernel>(instrumentor, kernelData);
}

GTPinProfileRecord* GtpinToolDraftFactory::MakeRecord() {
  GtpinToolDraftRecord* rec = new GtpinToolDraftRecord();
  return rec;
};

std::shared_ptr<ProfilerData> GtpinToolDraftFactory::MakeProfilerData() {
  return std::make_shared<GtpinToolDraftProfilerData>();
};

std::shared_ptr<KernelData> GtpinToolDraftFactory::MakeKernelData(
    IGtKernelInstrument& instrumentor) {
  return std::make_shared<GtpinToolDraftKernelData>(instrumentor);
};

std::shared_ptr<InvocationData> GtpinToolDraftFactory::MakeInvocationData(
    const GTPinKernelExecDesriptor& execDescr) {
  return std::make_shared<GtpinToolDraftInvocationData>(execDescr);
};

std::shared_ptr<ResultData> GtpinToolDraftFactory::MakeResultData() {
  return std::make_shared<GtpinToolDraftResultData>();
};

}  // namespace gtpin_prof
}  // namespace gtpin