//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <climits>

#include <map>

#include "api/gtpin_api.h"
#include "capsule.hpp"
#include "def_gpu.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
dst: register, src0: register, src1: register
*/

GtGenProcedure ShrTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  GtReg src1Dw = {src1.Reg(),
                  std::min(static_cast<size_t>(src1.DataType().Size()), sizeof(uint32_t)), 0};

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = NullReg();
    if (src0.DataType().Size() == 8) {
      src0H = {src0.Reg(), 4, 1};
    }

    // move low part
    proc += insF.MakeShr(dstL, src0L, src1Dw, execMask).SetPredicate(predicate);

    // middle part
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_DWORD);

    proc += insF.MakeAdd(tmpReg, src1Dw, GtImm(dst.DataType().Size() * CHAR_BIT, GED_DATA_TYPE_ud),
                         execMask)
                .SetPredicate(predicate);
    proc.back()->SetSrcModifier(0, GED_SRC_MOD_Negative);

    proc += insF.MakeShl(tmpReg, src0H, tmpReg, execMask).SetPredicate(predicate);
    proc += insF.MakeOr(dstL, dstL, tmpReg, execMask).SetPredicate(predicate);

    // move high part
    if (src0.DataType().Size() == 8) {
      proc += insF.MakeShr(dstH, src0H, src1Dw, execMask).SetPredicate(predicate);
    } else {
      proc += Macro::Mov(instrumentor, dstH, GtImm(0, GED_DATA_TYPE_ud), execMask, predicate);
    }

    return proc;
  }

  if (dst.DataType().Size() == 1) {
    proc += insF.MakeShr(dst, src0, src1Dw, execMask).SetPredicate(predicate);

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

  proc += insF.MakeShr(dst, src0, src1Dw, execMask).SetPredicate(predicate);
  return proc;
}

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

GtGenProcedure ShrXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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

    proc += insF.MakeAdd(tmpReg, tmpReg, GtImm(0xFFFF, GED_DATA_TYPE_uw), execMask)
                .SetPredicate(predicate);
    GtReg tmpRegL = {tmpReg, 1, 0};

    proc += insF.MakeAnd(dst, dst.Reg(), tmpRegL, execMask).SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeShr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    ShrFunctionsTable = {{GED_MODEL_TGL, &ShrTgl},
                         {GED_MODEL_XE_HP, &ShrXeHpc},
                         {GED_MODEL_XE_HPC, &ShrXeHpc},
                         {GED_MODEL_XE2, &ShrXe2}};

GtGenProcedure Macro::Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  // Destination size may be less than source 1 size

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

GtGenProcedure ShriTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  GtImm srcI1DW = GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud);

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = NullReg();
    if (src0.DataType().Size() == 8) {
      src0H = {src0.Reg(), 4, 1};
    }

    // move low part
    proc += insF.MakeShr(dstL, src0L, srcI1DW, execMask).SetPredicate(predicate);

    // middle part
    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeScratch(VREG_TYPE_DWORD);
    proc +=
        insF.MakeShl(tmpReg, src0H,
                     GtImm(dst.DataType().Size() * CHAR_BIT - srcI1DW.Value(), GED_DATA_TYPE_ud),
                     execMask)
            .SetPredicate(predicate);
    proc += insF.MakeOr(dstL, dstL, tmpReg, execMask).SetPredicate(predicate);

    // move high part
    if (src0.DataType().Size() == 8) {
      proc += insF.MakeShr(dstH, src0H, srcI1DW, execMask).SetPredicate(predicate);
    } else {
      proc += Macro::Mov(instrumentor, dstH, GtImm(0, GED_DATA_TYPE_ud), execMask, predicate);
    }

    return proc;
  }

  if (dst.DataType().Size() == 1) {
    proc += insF.MakeShr(dst, src0, srcI1DW, execMask).SetPredicate(predicate);
    proc += Macro::And(instrumentor, dst, dst.Reg(),
                       GtImm(Macro::GetMaskBySize(CHAR_BIT - srcI1.Value()), GED_DATA_TYPE_ub),
                       execMask, predicate);
    return proc;
  }

  proc += insF.MakeShr(dst, src0, srcI1DW, execMask).SetPredicate(predicate);
  return proc;
}

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

GtGenProcedure ShriXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 1) {
    proc += insF.MakeShr(dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_d), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeAnd(dst, dst.Reg(),
                         GtImm(GtImm(Macro::GetMaskBySize(8 - srcI1)), GED_DATA_TYPE_uw), execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeShr(dst, src0, GtImm(srcI1, GED_DATA_TYPE_w), execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    ShriFunctionsTable = {{GED_MODEL_TGL, &ShriTgl},
                          {GED_MODEL_XE_HP, &ShriXeHpc},
                          {GED_MODEL_XE_HPC, &ShriXeHpc},
                          {GED_MODEL_XE2, &ShriXe2}};

GtGenProcedure Macro::Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShriFunctionsTable.find(HwModel) != ShriFunctionsTable.end()) {
    return ShriFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  proc += insF.MakeShr(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
