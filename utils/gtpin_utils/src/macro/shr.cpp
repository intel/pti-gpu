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

GtGenProcedure ShrXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                        const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                        GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  if (dst.DataType().Size() == 1) {
    proc += insF.MakeShr(dst, src0, src1, execMask).SetPredicate(predicate);

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);

    proc += insF.MakeMov(tmpReg, GtImm(0x100, GED_DATA_TYPE_uw), execMask).SetPredicate(predicate);
    proc += insF.MakeShr(tmpReg, tmpReg,
                         GtReg(src1.Reg(), ((src1.DataType().Size() > 1) ? 2 : 1), 0), execMask)
                .SetPredicate(predicate);
    proc += Macro::Add(instrumentor, tmpReg, tmpReg, GtImm(-1 & 0xFFFF, GED_DATA_TYPE_uw), execMask,
                       predicate);
    GtReg tmpRegL = {tmpReg, 1, 0};
    proc += Macro::And(instrumentor, dst, dst.Reg(), tmpRegL, execMask, predicate);
    return proc;
  }

  proc += insF.MakeShr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    ShrFunctionsTable = {{GED_MODEL_XE_HP, &ShrXeHpc}, {GED_MODEL_XE_HPC, &ShrXeHpc}};

GtGenProcedure Macro::Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  // Destination size may be less than source 2 size

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShrFunctionsTable.find(HwModel) != ShrFunctionsTable.end()) {
    return ShrFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeShr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure ShriXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1 || dst.DataType().Size() == 1) {
    proc += insF.MakeShr(dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_d), execMask)
                .SetPredicate(predicate);
    if (dst.DataType().Size() == 1) {
      proc +=
          Macro::And(instrumentor, dst, dst.Reg(),
                     GtImm(Macro::GetMaskBySize(8 - srcI1), GED_DATA_TYPE_ub), execMask, predicate);
    }
    return proc;
  }

  if (srcI1.DataType().Size() == 8) {
    proc += insF.MakeShr(dst, src0, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeShr(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtImm&, GtExecMask, GtPredicate)>
    ShriFunctionsTable = {{GED_MODEL_XE_HP, &ShriXeHpc}, {GED_MODEL_XE_HPC, &ShriXeHpc}};

GtGenProcedure Macro::Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(srcI1.Value() <= dst.DataType().Size() * 8 &&
             "Shift value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShriFunctionsTable.find(HwModel) != ShriFunctionsTable.end()) {
    return ShriFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeShr(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
