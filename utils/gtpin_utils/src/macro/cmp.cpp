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

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, GtCondModifier, GtReg, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    CmpFunctionsTable = {};

GtGenProcedure Macro::Cmp(const IGtKernelInstrument& instrumentor, GtCondModifier cond,
                          GtReg flagReg, const GtRegRegion& src0, const GtRegRegion& src1,
                          GtExecMask execMask, GtPredicate predicate) {
#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (CmpFunctionsTable.find(HwModel) != CmpFunctionsTable.end()) {
    return CmpFunctionsTable[HwModel](instrumentor, cond, flagReg, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  insF.MakeCmp(cond, flagReg, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure CmpiXeHpc(const IGtKernelInstrument& instrumentor, GtCondModifier cond,
                         GtReg flagReg, const GtRegRegion& src0, const GtImm& srcI1,
                         GtExecMask execMask, GtPredicate predicate) {
  uint64_t mask = Macro::GetMaskBySizeBytes(4);
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc +=
      insF.MakeCmp(cond, flagReg, src0, GtImm(srcI1, Macro::GetGedIntDataTypeBytes(4)), execMask)
          .SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, GtCondModifier, GtReg,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    CmpiFunctionsTable = {
        {GED_MODEL_XE_HP, &CmpiXeHpc}, {GED_MODEL_XE_HPC, &CmpiXeHpc}, {GED_MODEL_XE2, &CmpiXeHpc}};

GtGenProcedure Macro::Cmp(const IGtKernelInstrument& instrumentor, GtCondModifier cond,
                          GtReg flagReg, const GtRegRegion& src0, const GtImm& srcI1,
                          GtExecMask execMask, GtPredicate predicate) {
#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (CmpiFunctionsTable.find(HwModel) != CmpiFunctionsTable.end()) {
    return CmpiFunctionsTable[HwModel](instrumentor, cond, flagReg, src0, srcI1, execMask,
                                       predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  insF.MakeCmp(cond, flagReg, src0,
               GtImm(srcI1, Macro::GetGedIntDataTypeBytes(src0.DataType().Size())), execMask)
      .SetPredicate(predicate);
  return proc;
}
