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

GtGenProcedure NotTgl(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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
      proc += insF.MakeNot(dstL, src0L, execMask).SetPredicate(predicate);
      proc += insF.MakeNot(dstH, src0H, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeNot(dstL, src0, execMask).SetPredicate(predicate);
      proc += insF.MakeNot(dstH, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    }

    return proc;
  }

  proc += insF.MakeMov(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure NotXeHpc(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                        const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8) {
    GtGenProcedure proc;
    IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg src0L = {src0.Reg(), 4, 0};
    proc += insF.MakeNot(dstL, src0L, execMask).SetPredicate(predicate);
    GtReg dstH = {dst.Reg(), 4, 1};
    GtReg src0H = {src0.Reg(), 4, 1};
    proc += insF.MakeNot(dstH, src0H, execMask).SetPredicate(predicate);
    return proc;
  }

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeNot(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       GtExecMask, GtPredicate)>
    NotFunctionsTable = {
        {GED_MODEL_TGL, &NotTgl}, {GED_MODEL_XE_HP, &NotXeHpc}, {GED_MODEL_XE_HPC, &NotXeHpc}};

GtGenProcedure Macro::Not(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, GtExecMask execMask, GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (NotFunctionsTable.find(HwModel) != NotFunctionsTable.end()) {
    return NotFunctionsTable[HwModel](instrumentor, dst, src0, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeNot(dst, src0, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: immediate
*/

GtGenProcedure Macro::Not(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtImm& srcI1, GtExecMask execMask, GtPredicate predicate) {
  size_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());  // 0b11111..111
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += Macro::Mov(instrumentor, dst, GtImm(((~((size_t)srcI1.Value())) & mask), dst.DataType()),
                     execMask, predicate);

  return proc;
}
