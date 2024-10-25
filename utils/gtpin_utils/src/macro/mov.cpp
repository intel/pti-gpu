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

GtGenProcedure MovTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (dst.DataType().Size() == 8) {
    PTI_ASSERT(execMask.ExecSize() == 1 && execMask.ChannelOffset() == 0 &&
               "Limited functionality of 64 bit mov on this HW");

    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    if (src0.DataType().Size() == 8) {
      GtReg src0L = {src0.Reg(), 4, 0};
      GtReg src0H = {src0.Reg(), 4, 1};
      proc += insF.MakeMov(dstL, src0L, execMask).SetPredicate(predicate);
      proc += insF.MakeMov(dstH, src0H, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstL, src0, execMask).SetPredicate(predicate);
      proc += insF.MakeMov(dstH, GtImm(0, GED_DATA_TYPE_uw), execMask).SetPredicate(predicate);
    }

    return proc;
  }

  proc += insF.MakeMov(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    MovFunctionsTable = {
        {GED_MODEL_TGL, &MovTgl},
};

GtGenProcedure Macro::Mov(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  MACRO_TRACING_2
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (MovFunctionsTable.find(HwModel) != MovFunctionsTable.end()) {
    return MovFunctionsTable[HwModel](instrumentor, dst, src0, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeMov(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: immediate
*/

GtGenProcedure MoviTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1 && dst.DataType().Size() == 1) {
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);
    proc +=
        insF.MakeMov(tmpReg, GtImm(srcI1.Value() & 0XFF, GED_DATA_TYPE_uw)).SetPredicate(predicate);
    proc += insF.MakeAnd(dst, dst.Reg(), GtImm(0xFF00, GED_DATA_TYPE_uw)).SetPredicate(predicate);
    proc += insF.MakeOr(dst, dst.Reg(), tmpReg).SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8) {
    // mov low
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};
    proc += insF.MakeMov(dstL, GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud))
                .SetPredicate(predicate);
    // mov high
    proc +=
        insF.MakeMov(dstH, GtImm(srcI1.Value() >> 32, GED_DATA_TYPE_ud)).SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeMov(dst, GtImm(srcI1.Value(), dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure MoviXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  if (srcI1.DataType().Size() == 1 && dst.DataType().Size() == 1) {
    GtGenProcedure proc;
    IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_WORD);
    proc +=
        insF.MakeMov(tmpReg, GtImm(srcI1.Value() & 0XFF, GED_DATA_TYPE_uw)).SetPredicate(predicate);
    proc += insF.MakeAnd(dst, dst.Reg(), GtImm(0xFF00, GED_DATA_TYPE_uw)).SetPredicate(predicate);
    proc += insF.MakeOr(dst, dst.Reg(), tmpReg).SetPredicate(predicate);
    return proc;
  }

  if (srcI1.DataType().Size() == 8) {
    GtGenProcedure proc;
    IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
    // mov low
    GtReg dstL = {dst.Reg(), 4, 0};
    proc += insF.MakeMov(dstL, GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud))
                .SetPredicate(predicate);
    // mov high
    GtReg dstH = {dst.Reg(), 4, 1};
    proc +=
        insF.MakeMov(dstH, GtImm(srcI1.Value() >> 32, GED_DATA_TYPE_ud)).SetPredicate(predicate);
    return proc;
  }

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeMov(dst, GtImm(srcI1.Value(), dst.DataType())).SetPredicate(predicate);
  return proc;
}

GtGenProcedure MoviXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (srcI1.DataType().Size() == 1 && dst.DataType().Size() == 1) {
    proc += insF.MakeMov(dst, GtImm(srcI1.Value(), Macro::GetGedIntDataTypeBytes(sizeof(uint16_t))))
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeMov(dst, GtImm(srcI1.Value(), dst.DataType())).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtImm&,
                                       GtExecMask, GtPredicate)>
    MoviFunctionsTable = {{GED_MODEL_TGL, &MoviTgl},
                          {GED_MODEL_XE_HP, &MoviXeHpc},
                          {GED_MODEL_XE_HPC, &MoviXeHpc},
                          {GED_MODEL_XE2, &MoviXe2}};

GtGenProcedure Macro::Mov(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  MACRO_TRACING_2I
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (MoviFunctionsTable.find(HwModel) != MoviFunctionsTable.end()) {
    return MoviFunctionsTable[HwModel](instrumentor, dst, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeMov(dst, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
