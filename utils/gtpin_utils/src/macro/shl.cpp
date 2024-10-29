//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <climits>

#include <algorithm>
#include <map>

#include "api/gtpin_api.h"
#include "capsule.hpp"
#include "def_gpu.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
dst: register, src0: register, src1: register
*/
GtGenProcedure ShlTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  GtReg src1Dw = {src1.Reg(),
                  std::min(static_cast<uint32_t>(sizeof(uint32_t)), src1.DataType().Size()), 0};

  if (dst.DataType().Size() == 8) {
    PTI_ASSERT(false && "Not supported");
    return proc;
  }

  if (src1.DataType().Size() == 8) {
    proc += insF.MakeShl(dst, src0, src1Dw, execMask).SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeShl(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    ShlFunctionsTable = {{GED_MODEL_TGL, &ShlTgl}};

GtGenProcedure Macro::Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  // Destination size may be less than source 1 size

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShlFunctionsTable.find(HwModel) != ShlFunctionsTable.end()) {
    return ShlFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeShl(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure ShliTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  GtImm srcI1Dw = GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud);

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};

    // move high part
    if (src0.DataType().Size() == 8 && srcI1Dw.Value() < (sizeof(uint32_t) * CHAR_BIT)) {
      GtReg src0H = {src0.Reg(), 4, 1};
      proc += insF.MakeShl(dstH, src0H, GtImm((srcI1Dw.Value()) & 0xFFFF'FFFF, GED_DATA_TYPE_ud),
                           execMask)
                  .SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstH, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    }

    // middle part
    if (srcI1Dw.Value() > 64) {
      proc += insF.MakeMov(dstL, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    } else if (srcI1Dw.Value() > 32) {
      proc += insF.MakeShl(dstH, src0L,
                           GtImm((srcI1Dw.Value() - 32) & 0xFFFF'FFFF, GED_DATA_TYPE_ud), execMask)
                  .SetPredicate(predicate);
    } else {  // srcI1Dw.Value() <= 32
      size_t ovfShift = 32 - srcI1Dw.Value();
      GtReg tmpReg = dstL;
      if (src0L.RegNum() == dstL.RegNum() && src0L.SubRegNum() == dstL.SubRegNum()) {
        tmpReg = instrumentor.Coder().VregFactory().MakeMsgDataScratch(VREG_TYPE_DWORD);
      }
      proc += insF.MakeShr(tmpReg, src0L, GtImm(ovfShift, GED_DATA_TYPE_ud), execMask)
                  .SetPredicate(predicate);
      proc += insF.MakeOr(dstH, dstH, tmpReg, execMask).SetPredicate(predicate);
    }

    // move low part
    if (srcI1Dw.Value() < (sizeof(uint32_t) * CHAR_BIT)) {
      proc += insF.MakeShl(dstL, src0L, srcI1Dw, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstL, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    }

    return proc;
  }

  proc += insF.MakeShl(dst, src0, srcI1Dw, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure ShliXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += ShliXeHpc(instrumentor, dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_ud),
                      execMask, predicate);
    return proc;
  }

  if (srcI1.DataType().Size() == 8) {
    proc += insF.MakeShl(dst, src0, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeShl(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    ShliFunctionsTable = {{GED_MODEL_TGL, &ShliTgl},
                          {GED_MODEL_XE_HP, &ShliXeHpc},
                          {GED_MODEL_XE_HPC, &ShliXeHpc},
                          {GED_MODEL_XE2, &ShliXeHpc}};

GtGenProcedure Macro::Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3I
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(srcI1.Value() <= dst.DataType().Size() * CHAR_BIT &&
             "Shift value is too large for the destination size");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (srcI1 == 0) {
    proc += Macro::Mov(instrumentor, dst, src0, execMask, predicate);
    return proc;
  }
  if (srcI1 > dst.DataType().Size() * CHAR_BIT) {
    proc += Macro::Mov(instrumentor, dst, GtImm(0, GED_DATA_TYPE_ud), execMask, predicate);
    return proc;
  }

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShliFunctionsTable.find(HwModel) != ShliFunctionsTable.end()) {
    return ShliFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  proc += insF.MakeShl(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
