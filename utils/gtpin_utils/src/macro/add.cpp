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

GtGenProcedure AddTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    bool isDstAligned =
        ((dst.Reg().SubRegNum() == 0) &&
         (!dst.Reg().IsVirtual() || (vregs.Get(dst.Reg().VregNumber()).RequiredSubregNum() == 0)));
    if (!isDstAligned) {
      dstL = vregs.MakeMsgDataScratch(VREG_TYPE_DWORD);
      proc += insF.MakeMov(dstL, GtReg(dst.Reg(), sizeof(uint32_t), 0)).SetPredicate(predicate);
    }

    GtReg src0L = {src0.Reg(), std::min(static_cast<uint32_t>(sizeof(uint32_t)), src0.DataType().Size()), 0};

    GtReg src1L = {src1.Reg(), std::min(static_cast<uint32_t>(sizeof(uint32_t)), src1.DataType().Size()), 0};
    GtReg src1H = NullReg();
    if (src1.DataType().Size() == 8) {
      src1H = {src1.Reg(), 4, 1};
    }

    proc +=
        insF.MakeAddc(dstL, src0L, src1L, execMask).SetPredicate(predicate).SetAccWriteCtrl(true);

    proc += insF.MakeAdd(dstL, AccReg(0), dstL, execMask).SetPredicate(predicate);

    if (src0.DataType().Size() == 8) {
      GtReg src0H = {src0.Reg(), 4, 1};
      proc += insF.MakeAdd(dstH, src0H, dstL, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstH, dstL, execMask).SetPredicate(predicate);
    }

    proc += insF.MakeAdd(dstL, AccReg(0), dstL, execMask)
                .SetSrcModifier(0, GED_SRC_MOD_Negative)
                .SetPredicate(predicate);

    proc += insF.MakeSub(dstH, dstH, dstL, execMask).SetPredicate(predicate);

    if (src1.DataType().Size() == 8) {
      proc += insF.MakeAdd(dstH, dstH, src1H, execMask).SetPredicate(predicate);
    }

    if (!isDstAligned) {
      proc += insF.MakeMov(GtReg(dst.Reg(), sizeof(uint32_t), 0), dstL).SetPredicate(predicate);
    }
    return proc;
  }

  proc += insF.MakeAdd(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    AddFunctionsTable = {{GED_MODEL_TGL, &AddTgl}};

GtGenProcedure Macro::Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(dst.DataType().Size() >= src1.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (AddFunctionsTable.find(HwModel) != AddFunctionsTable.end()) {
    return AddFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeAdd(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure AddiTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    bool isDstAligned =
        ((dst.Reg().SubRegNum() == 0) &&
         (!dst.Reg().IsVirtual() || (vregs.Get(dst.Reg().VregNumber()).RequiredSubregNum() == 0)));
    if (!isDstAligned) {
      dstL = vregs.MakeMsgDataScratch(VREG_TYPE_DWORD);
      proc += insF.MakeMov(dstL, GtReg(dst.Reg(), sizeof(uint32_t), 0)).SetPredicate(predicate);
    }

    GtReg src0L = {src0.Reg(), std::min(static_cast<uint32_t>(sizeof(uint32_t)), src0.DataType().Size()), 0};

    proc +=
        insF.MakeAddc(dstL, src0L, GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud), execMask)
            .SetPredicate(predicate)
            .SetAccWriteCtrl(true);

    proc += insF.MakeAdd(dstL, AccReg(0), dstL, execMask).SetPredicate(predicate);

    if (src0.DataType().Size() == 8) {
      GtReg src0H = {src0.Reg(), 4, 1};
      proc += insF.MakeAdd(dstH, src0H, dstL, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstH, dstL, execMask).SetPredicate(predicate);
    }

    proc += insF.MakeAdd(dstL, AccReg(0), dstL, execMask)
                .SetSrcModifier(0, GED_SRC_MOD_Negative)
                .SetPredicate(predicate);

    proc += insF.MakeSub(dstH, dstH, dstL, execMask).SetPredicate(predicate);

    proc += insF.MakeAdd(dstH, dstH, GtImm((srcI1.Value() >> 32) & 0xFFFF'FFFF, GED_DATA_TYPE_ud),
                         execMask)
                .SetPredicate(predicate);

    if (!isDstAligned) {
      proc += insF.MakeMov(GtReg(dst.Reg(), sizeof(uint32_t), 0), dstL).SetPredicate(predicate);
    }
    return proc;
  }

  proc += insF.MakeAdd(dst, src0, GtImm(srcI1.Value() & 0xFFFF'FFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
  return proc;
}

GtGenProcedure AddiXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += AddiXeHpc(instrumentor, dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_ud),
                      execMask, predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8 && srcI1.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    proc +=
        insF.MakeAddc(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
            .SetPredicate(predicate);

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg = vregs.MakeMsgDataScratch(VREG_TYPE_DWORD);
    proc += insF.MakeAdd(tmpReg, AccReg(0), src0H, execMask).SetPredicate(predicate);
    proc += insF.MakeAdd(dstH, tmpReg, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                         execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeAdd(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure AddiXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeAdd(dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8) {
    if (srcI1.DataType().Size() == 8) {
      if (src0.DataType().Size() == 8) {
        GtReg dstL = {dst.Reg(), 4, 0};
        GtReg dstH = {dst.Reg(), 4, 1};

        GtReg src0L = {src0.Reg(), 4, 0};
        GtReg src0H = {src0.Reg(), 4, 1};

        proc += insF.MakeAddc(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                              execMask)
                    .SetPredicate(predicate);

        auto& coder = instrumentor.Coder();
        auto& vregs = coder.VregFactory();
        GtReg tmpReg = vregs.MakeMsgDataScratch(VREG_TYPE_DWORD);
        proc += insF.MakeAdd(tmpReg, AccReg(0), src0H, execMask).SetPredicate(predicate);
        proc += insF.MakeAdd(dstH, tmpReg,
                             GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                    .SetPredicate(predicate);
        return proc;
      }
      PTI_ASSERT(false && "Not Implemented Yet");
    }
    proc += insF.MakeAdd(dst, src0, srcI1, execMask).SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeAdd(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    AddiFunctionsTable = {{GED_MODEL_TGL, &AddiTgl},
                          {GED_MODEL_XE_HP, &AddiXeHpc},
                          {GED_MODEL_XE_HPC, &AddiXeHpc},
                          {GED_MODEL_XE2, AddiXe2}};

GtGenProcedure Macro::Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3I
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT((srcI1.Value() & (1 << (srcI1.DataType().Size() * CHAR_BIT - 1))) ||
             srcI1.Value() <= mask && "Immediate value is too large for the destination size");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (srcI1 == 0) {
    proc += Macro::Mov(instrumentor, dst, src0, execMask, predicate);
    return proc;
  }

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (AddiFunctionsTable.find(HwModel) != AddiFunctionsTable.end()) {
    return AddiFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  proc += insF.MakeAdd(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
