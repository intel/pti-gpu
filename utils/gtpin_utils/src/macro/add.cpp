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
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    AddFunctionsTable = {

};

GtGenProcedure Macro::Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
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
    AddiFunctionsTable = {
        {GED_MODEL_XE_HP, &AddiXeHpc}, {GED_MODEL_XE_HPC, &AddiXeHpc}, {GED_MODEL_XE2, AddiXe2}};

GtGenProcedure Macro::Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (AddiFunctionsTable.find(HwModel) != AddiFunctionsTable.end()) {
    return AddiFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeAdd(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
