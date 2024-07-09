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

GtGenProcedure MulXeHpc(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                        const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                        GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (src1.DataType().Size() >= 4) {
    // dst size >= src0 size && dst size >= src1
    // dst > 4 bytes here

    GtReg a1 = src0.Reg();
    GtReg a2;
    GtReg a3;
    GtReg a4;

    GtReg b1 = src1.Reg();
    GtReg b2;
    GtReg b3;
    GtReg b4;

    if (src0.DataType().Size() >= 4) {
      a1 = GtReg(src0.Reg(), 2, 0);
      a2 = GtReg(src0.Reg(), 2, 1);
    }
    if (src0.DataType().Size() >= 8) {
      a3 = GtReg(src0.Reg(), 2, 2);
      a4 = GtReg(src0.Reg(), 2, 3);
    }

    if (src1.DataType().Size() >= 4) {
      b1 = GtReg(src1.Reg(), 2, 0);
      b2 = GtReg(src1.Reg(), 2, 1);
    }
    if (src1.DataType().Size() >= 8) {
      b3 = GtReg(src1.Reg(), 2, 2);
      b4 = GtReg(src1.Reg(), 2, 3);
    }

    GtReg tmpReg0;
    /// TODO: register collision check
    if (dst.Reg() == src0.Reg() || dst.Reg() == src1.Reg()) {
      auto& coder = instrumentor.Coder();
      auto& vregs = coder.VregFactory();
      tmpReg0 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));
    } else {
      tmpReg0 = dst.Reg();
    }

    proc += insF.MakeMul(tmpReg0, a1, b1, execMask).SetPredicate(predicate);

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg1 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));
    GtReg tmpReg2 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));

    if (b2.IsValid())
      proc += insF.MakeMul(tmpReg1, a1, b2, execMask).SetPredicate(predicate);
    else
      proc += Macro::Mov(instrumentor, tmpReg1, GtImm(0, GED_DATA_TYPE_uw), execMask, predicate);
    if (b1.IsValid() && a2.IsValid())
      proc += insF.MakeMul(tmpReg2, a2, b1, execMask).SetPredicate(predicate);
    if (b1.IsValid() && a2.IsValid())
      proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
    if (b1.IsValid() && a2.IsValid() || b2)
      proc += insF.MakeShl(tmpReg1, tmpReg1, 16, execMask).SetPredicate(predicate);

    if (b1.IsValid() && a2.IsValid() || b2)
      proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);

    if (dst.DataType().Size() == 8) {
      if (b3.IsValid())
        proc += insF.MakeMul(tmpReg1, a1, b3, execMask).SetPredicate(predicate);
      else
        proc += Macro::Mov(instrumentor, tmpReg1, GtImm(0, GED_DATA_TYPE_uw), execMask, predicate);
      if (b2.IsValid() && a2.IsValid())
        proc += insF.MakeMul(tmpReg2, a2, b2, execMask).SetPredicate(predicate);
      if (b2.IsValid() && a2.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b1.IsValid() && a3.IsValid())
        proc += insF.MakeMul(tmpReg2, a3, b1, execMask).SetPredicate(predicate);
      if (b1.IsValid() && a3.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b3.IsValid() || (b2.IsValid() && a2.IsValid()) || (b1.IsValid() && a3.IsValid()))
        proc += insF.MakeShl(tmpReg1, tmpReg1, 32, execMask).SetPredicate(predicate);
      if (b3.IsValid() || (b2.IsValid() && a2.IsValid()) || (b1.IsValid() && a3.IsValid()))
        proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);

      if (b4.IsValid())
        proc += insF.MakeMul(tmpReg1, a1, b4, execMask).SetPredicate(predicate);
      else
        proc += Macro::Mov(instrumentor, tmpReg1, GtImm(0, GED_DATA_TYPE_uw), execMask, predicate);
      if (b3.IsValid() && a2.IsValid())
        proc += insF.MakeMul(tmpReg2, a2, b3, execMask).SetPredicate(predicate);
      if (b3.IsValid() && a2.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b2.IsValid() && a3.IsValid())
        proc += insF.MakeMul(tmpReg2, a3, b2, execMask).SetPredicate(predicate);
      if (b2.IsValid() && a3.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b1.IsValid() && a4.IsValid())
        proc += insF.MakeMul(tmpReg2, a4, b1, execMask).SetPredicate(predicate);
      if (b1.IsValid() && a4.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);

      if (b4.IsValid() || (b3.IsValid() && a2.IsValid()) || (b2.IsValid() && a3.IsValid()) ||
          (b1.IsValid() && a4.IsValid()))
        proc += insF.MakeShl(tmpReg1, tmpReg1, 48, execMask).SetPredicate(predicate);
      if (b4.IsValid() || (b3.IsValid() && a2.IsValid()) || (b2.IsValid() && a3.IsValid()) ||
          (b1.IsValid() && a4.IsValid()))
        proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);
    }

    if (dst.Reg() == src0.Reg() || dst.Reg() == src1.Reg()) {
      proc += Macro::Mov(instrumentor, dst, tmpReg0, execMask, predicate);
    }

    return proc;
  }

  proc += insF.MakeMul(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    MulFunctionsTable = {{GED_MODEL_XE_HP, &MulXeHpc}, {GED_MODEL_XE_HPC, &MulXeHpc}};

GtGenProcedure Macro::Mul(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(dst.DataType().Size() >= src1.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (MulFunctionsTable.find(HwModel) != MulFunctionsTable.end()) {
    return MulFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeMul(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/
GtGenProcedure MuliXeHpc(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  auto v = srcI1.Value();
  if (v && !(v & (v - 1))) {  // imm is power of 2 => replace by shift left
    unsigned int r = 0;
    while (v >>= 1) {
      r++;
    }

    proc += Macro::Shl(instrumentor, dst, src0,
                       GtImm(r, Macro::GetGedIntDataTypeBytes(dst.DataType().Size())), execMask,
                       predicate);

    return proc;
  }

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeMul(dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_uw), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  if (srcI1.DataType().Size() >= 4) {
    // dst size >= src0 size && dst size >= src1
    // dst > 4 bytes here

    GtReg a1 = src0.Reg();
    GtReg a2;
    GtReg a3;
    GtReg a4;

    GtImm b1 = {srcI1.Value() & 0xFFFF, GED_DATA_TYPE_uw};
    GtImm b2 = {(srcI1.Value() >> 16) & 0xFFFF, GED_DATA_TYPE_uw};
    GtImm b3 = {(srcI1.Value() >> 32) & 0xFFFF, GED_DATA_TYPE_uw};
    GtImm b4 = {(srcI1.Value() >> 48) & 0xFFFF, GED_DATA_TYPE_uw};

    if (src0.DataType().Size() >= 4) {
      a1 = GtReg(src0.Reg(), 2, 0);
      a2 = GtReg(src0.Reg(), 2, 1);
    }
    if (src0.DataType().Size() >= 8) {
      a3 = GtReg(src0.Reg(), 2, 2);
      a4 = GtReg(src0.Reg(), 2, 3);
    }

    GtReg tmpReg0;
    // TodO: appropriate register collision check
    if (dst.Reg() == src0.Reg()) {
      auto& coder = instrumentor.Coder();
      auto& vregs = coder.VregFactory();
      tmpReg0 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));
    } else {
      tmpReg0 = dst.Reg();
    }

    proc += insF.MakeMul(tmpReg0, a1, b1, execMask).SetPredicate(predicate);

    auto& coder = instrumentor.Coder();
    auto& vregs = coder.VregFactory();
    GtReg tmpReg1 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));
    GtReg tmpReg2 = vregs.MakeMsgDataScratch(Macro::GetVregTypeBytes(dst.DataType().Size()));

    if (b2)
      proc += insF.MakeMul(tmpReg1, a1, b2, execMask).SetPredicate(predicate);
    else
      proc += Macro::Mov(instrumentor, a1, GtImm(0, GED_DATA_TYPE_uw), execMask, predicate);
    if (b1 && a2.IsValid()) proc += insF.MakeMul(tmpReg2, a2, b1, execMask).SetPredicate(predicate);
    if (b1 && a2.IsValid())
      proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
    if (b1 && a2.IsValid() || b2)
      proc += insF.MakeShl(tmpReg1, tmpReg1, 16, execMask).SetPredicate(predicate);

    if (b1 && a2.IsValid() || b2)
      proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);

    if (dst.DataType().Size() == 8) {
      proc += insF.MakeMul(tmpReg1, a1, b3, execMask).SetPredicate(predicate);
      if (b2 && a2.IsValid())
        proc += insF.MakeMul(tmpReg2, a2, b2, execMask).SetPredicate(predicate);
      if (b2 && a2.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b1 && a3.IsValid())
        proc += insF.MakeMul(tmpReg2, a3, b1, execMask).SetPredicate(predicate);
      if (b1 && a3.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b3 || (b2 && a2.IsValid()) || (b1 && a3.IsValid()))
        proc += insF.MakeShl(tmpReg1, tmpReg1, 32, execMask).SetPredicate(predicate);
      if (b3 || (b2 && a2.IsValid()) || (b1 && a3.IsValid()))
        proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);

      proc += insF.MakeMul(tmpReg1, a1, b4, execMask).SetPredicate(predicate);
      if (b3 && a2.IsValid())
        proc += insF.MakeMul(tmpReg2, a2, b3, execMask).SetPredicate(predicate);
      if (b3 && a2.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b2 && a3.IsValid())
        proc += insF.MakeMul(tmpReg2, a3, b2, execMask).SetPredicate(predicate);
      if (b2 && a3.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);
      if (b1 && a4.IsValid())
        proc += insF.MakeMul(tmpReg2, a4, b1, execMask).SetPredicate(predicate);
      if (b1 && a4.IsValid())
        proc += Macro::Add(instrumentor, tmpReg1, tmpReg1, tmpReg2, execMask, predicate);

      if (b4 || (b3 && a2.IsValid()) || (b2 && a3.IsValid()) || (b1 && a4.IsValid()))
        proc += insF.MakeShl(tmpReg1, tmpReg1, 48, execMask).SetPredicate(predicate);
      if (b4 || (b3 && a2.IsValid()) || (b2 && a3.IsValid()) || (b1 && a4.IsValid()))
        proc += Macro::Add(instrumentor, tmpReg0, tmpReg0, tmpReg1, execMask, predicate);
    }

    if (dst.Reg() == src0.Reg()) {
      proc += Macro::Mov(instrumentor, dst, tmpReg0, execMask, predicate);
    }

    return proc;
  }

  proc += insF.MakeMul(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtImm&, GtExecMask, GtPredicate)>
    MuliFunctionsTable = {{GED_MODEL_XE_HP, &MuliXeHpc}, {GED_MODEL_XE_HPC, &MuliXeHpc}};

GtGenProcedure Macro::Mul(IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (MuliFunctionsTable.find(HwModel) != MuliFunctionsTable.end()) {
    return MuliFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeMul(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
