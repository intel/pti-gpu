//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <map>

#include "api/gtpin_api.h"
#include "capsule.hpp"
#include "def_gpu.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
dst: register, src0: register, src1: register
*/

std::map<GED_MODEL, GtGenProcedure (*)(IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    SubFunctionsTable = {

};

GtGenProcedure Macro::Sub(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(dst.DataType().Size() >= src1.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (SubFunctionsTable.find(HwModel) != SubFunctionsTable.end()) {
    return SubFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeSub(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure SubiXeHpc(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  auto& coder = instrumentor.Coder();
  auto& vregs = coder.VregFactory();
  GtReg tmpReg = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(srcI1.DataType().Size()));
  proc += Macro::Mov(instrumentor, tmpReg, srcI1, execMask, predicate);
  proc += insF.MakeSub(dst, src0, tmpReg, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtImm&, GtExecMask, GtPredicate)>
    SubiFunctionsTable = {{GED_MODEL_XE_HP, &SubiXeHpc}, {GED_MODEL_XE_HPC, &SubiXeHpc}};

GtGenProcedure Macro::Sub(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (SubiFunctionsTable.find(HwModel) != SubiFunctionsTable.end()) {
    return SubiFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  insF.MakeSub(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
