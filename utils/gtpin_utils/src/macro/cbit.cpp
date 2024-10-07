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
dst: register, src0: register
*/

GtGenProcedure CbitXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  if (src0.DataType().Size() == 1 && dst.DataType().Size() == 1) {
    GtGenProcedure proc;
    IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);
    proc += insF.MakeCbit(tmpReg, src0, execMask).SetPredicate(predicate);
    proc += insF.MakeAnd(dst, dst.Reg(), GtImm(0xFFFF0000, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeOr(dst, dst.Reg(), tmpReg, execMask).SetPredicate(predicate);
    return proc;
  }

  if (src0.DataType().Size() == 8) {
    GtGenProcedure proc;
    IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);

    proc += insF.MakeCbit(dst, src0L, execMask).SetPredicate(predicate);
    proc += insF.MakeCbit(tmpReg, src0H, execMask).SetPredicate(predicate);
    if (dst.DataType().Size() == 1) {
      proc += Macro::Add(instrumentor, tmpReg, tmpReg, dst.Reg(), execMask, predicate);
      proc += Macro::Mov(instrumentor, dst, GtReg(tmpReg, 1, 0), execMask, predicate);
    } else {
      proc += Macro::Add(instrumentor, dst, dst.Reg(), tmpReg, execMask, predicate);
    }

    return proc;
  }

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeCbit(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure CbitXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (src0.DataType().Size() == 1 && dst.DataType().Size() == 1) {
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeMsgDataScratch(VREG_TYPE_WORD);
    proc += insF.MakeMov(tmpReg, src0, execMask).SetPredicate(predicate);
    proc += insF.MakeCbit(dst, tmpReg, execMask).SetPredicate(predicate);

    return proc;
  }

  if (src0.DataType().Size() == 8) {
    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);

    proc += insF.MakeCbit(dst, src0L, execMask).SetPredicate(predicate);
    proc += insF.MakeCbit(tmpReg, src0H, execMask).SetPredicate(predicate);
    proc += insF.MakeAdd(dst, dst.Reg(), tmpReg, execMask).SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeCbit(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    CbitFunctionsTable = {
        {GED_MODEL_XE_HP, &CbitXeHpc}, {GED_MODEL_XE_HPC, &CbitXeHpc}, {GED_MODEL_XE2, &CbitXe2}};

GtGenProcedure Macro::Cbit(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                           const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  // Destination size may be less than source size

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (CbitFunctionsTable.find(HwModel) != CbitFunctionsTable.end()) {
    return CbitFunctionsTable[HwModel](instrumentor, dst, src0, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeCbit(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: immediate
*/

GtGenProcedure Macro::Cbit(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                           const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  size_t val = srcI1.Value();
  size_t cbit = 0;
  for (cbit = 0; val; val >>= 1) {
    cbit += val & 1;
  }
  proc += Macro::Mov(instrumentor, dst, GtImm(cbit, dst.DataType()), execMask, predicate);

  return proc;
}
