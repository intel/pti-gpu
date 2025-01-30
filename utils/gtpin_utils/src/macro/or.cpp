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

GtGenProcedure OrTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                     const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                     GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    PTI_ASSERT(execMask.ExecSize() == 1 && execMask.ChannelOffset() == 0 &&
               "Limited functionality of 64 bit mov on this HW");

    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    if (src0.DataType().Size() == 8 && src1.DataType().Size() == 8) {
      GtReg src0L = {src0.Reg(), 4, 0};
      GtReg src0H = {src0.Reg(), 4, 1};
      GtReg src1L = {src1.Reg(), 4, 0};
      GtReg src1H = {src1.Reg(), 4, 1};
      proc += insF.MakeOr(dstL, src0L, src1L, execMask).SetPredicate(predicate);
      proc += insF.MakeOr(dstH, src0H, src1H, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeOr(dstL, src0, src1, execMask).SetPredicate(predicate);
      proc += insF.MakeOr(dstH, src0, src1, execMask).SetPredicate(predicate);
    }

    return proc;
  }

  proc += insF.MakeOr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure OrXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8 && src1.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    GtReg src1L = {src1.Reg(), 4, 0};
    GtReg src1H = {src1.Reg(), 4, 1};

    proc += insF.MakeOr(dstL, src0L, src1L, execMask).SetPredicate(predicate);
    proc += insF.MakeOr(dstH, src0H, src1H, execMask).SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() >= 4 && src1.DataType().Size() == 4) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};

    proc += insF.MakeOr(dstL, src0L, src1, execMask).SetPredicate(predicate);
    proc += insF.MakeMov(dstH, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeOr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure OrXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                     const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                     GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8 && src1.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    GtReg src1L = {src1.Reg(), 4, 0};
    GtReg src1H = {src1.Reg(), 4, 1};

    proc += insF.MakeOr(dstL, src0L, src1L, execMask).SetPredicate(predicate);
    proc += insF.MakeOr(dstH, src0H, src1H, execMask).SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeOr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    OrFunctionsTable = {{GED_MODEL_TGL, &OrTgl},
                        {GED_MODEL_XE_HP, &OrXeHpc},
                        {GED_MODEL_XE_HPC, &OrXeHpc},
                        {GED_MODEL_XE2, &OrXe2}};

GtGenProcedure Macro::Or(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                         GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(dst.DataType().Size() >= src1.DataType().Size() &&
             "Destination size should be no less than source size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (OrFunctionsTable.find(HwModel) != OrFunctionsTable.end()) {
    return OrFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeOr(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure OriTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    if (src0.DataType().Size() == 8) {
      GtReg src0L = {src0.Reg(), 4, 0};
      GtReg src0H = {src0.Reg(), 4, 1};
      proc +=
          insF.MakeOr(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
      proc += insF.MakeOr(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                          execMask)
                  .SetPredicate(predicate);
    } else {
      proc += insF.MakeOr(dstL, src0, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                  .SetPredicate(predicate);
      proc +=
          insF.MakeMov(dstH, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
    }

    return proc;
  }

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeOr(dst, src0, GtImm(srcI1.Value(), GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeOr(dst, src0,
                      GtImm(srcI1.Value(), Macro::GetGedIntDataTypeBytes(srcI1.DataType().Size())),
                      execMask)
              .SetPredicate(predicate);
  return proc;
}

GtGenProcedure OriXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                        const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                        GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeOr(dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    proc += insF.MakeOr(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeOr(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                        execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeOr(dst, src0,
                      GtImm(srcI1.Value(), Macro::GetGedIntDataTypeBytes(srcI1.DataType().Size())),
                      execMask)
              .SetPredicate(predicate);
  return proc;
}

GtGenProcedure OriXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeOr(dst, src0, GtImm(srcI1 & 0xFF, GED_DATA_TYPE_uw), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    proc += insF.MakeOr(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeOr(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                        execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeOr(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    OriFunctionsTable = {{GED_MODEL_TGL, &OriTgl},
                         {GED_MODEL_XE_HP, &OriXeHpc},
                         {GED_MODEL_XE_HPC, &OriXeHpc},
                         {GED_MODEL_XE2, &OriXe2}};

GtGenProcedure Macro::Or(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (OriFunctionsTable.find(HwModel) != OriFunctionsTable.end()) {
    return OriFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeOr(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
