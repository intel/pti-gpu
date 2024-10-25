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

GtGenProcedure XorTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                      const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                      GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    PTI_ASSERT(execMask.ExecSize() == 1 && execMask.ChannelOffset() == 0 &&
               "Limited functionality of 64 bit mov on this HW");

    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), std::min(4u, src0.DataType().Size()), 0};
    GtReg src0H = NullReg();
    if (src0.DataType().Size() == 8) {
      src0H = {src0.Reg(), 4, 1};
    }

    GtReg src1L = {src1.Reg(), std::min(4u, src1.DataType().Size()), 0};
    GtReg src1H = NullReg();
    if (src1.DataType().Size() == 8) {
      src1H = {src1.Reg(), 4, 1};
    }

    proc += insF.MakeXor(dstL, src0L, src1L, execMask).SetPredicate(predicate);
    if (src0.DataType().Size() == 8 && src1.DataType().Size() == 8) {
      proc += insF.MakeXor(dstH, src0H, src1H, execMask).SetPredicate(predicate);
    } else if (src0.DataType().Size() == 8 /* && src1.DataType().Size() < 8 */) {
      proc += Macro::Mov(instrumentor, dstH, src0H, execMask, predicate);
    } else if (src1.DataType().Size() == 8 /* && src0.DataType().Size() < 8 */) {
      proc += Macro::Mov(instrumentor, dstH, src1H, execMask, predicate);
    } else /* src0.DataType().Size() < 8 && src1.DataType().Size() < 8 */ {
      proc += Macro::Mov(instrumentor, dstH, GtImm(0, GED_DATA_TYPE_ud), execMask, predicate);
    }

    return proc;
  }

  proc += insF.MakeXor(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure XorXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                        const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                        GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (dst.DataType().Size() == 8) {
    PTI_ASSERT(execMask.ExecSize() == 1 && execMask.ChannelOffset() == 0 &&
               "Limited functionality of 64 bit mov on this HW");

    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), std::min(4u, src0.DataType().Size()), 0};
    GtReg src0H = NullReg();
    if (src0.DataType().Size() == 8) {
      src0H = {src0.Reg(), 4, 1};
    }

    GtReg src1L = {src1.Reg(), std::min(4u, src1.DataType().Size()), 0};
    GtReg src1H = NullReg();
    if (src1.DataType().Size() == 8) {
      src1H = {src1.Reg(), 4, 1};
    }

    proc += insF.MakeXor(dstL, src0L, src1L, execMask).SetPredicate(predicate);

    if (src0.DataType().Size() == 8 && src1.DataType().Size() == 8) {
      proc += insF.MakeXor(dstH, src0H, src1H, execMask).SetPredicate(predicate);
    } else if (src0.DataType().Size() != 8 && src1.DataType().Size() == 8) {
      proc += insF.MakeMov(dstH, src1H, execMask).SetPredicate(predicate);
    } else if (src0.DataType().Size() == 8 && src1.DataType().Size() != 8) {
      proc += insF.MakeMov(dstH, src0H, execMask).SetPredicate(predicate);
    } else {
      proc += insF.MakeMov(dstH, GtImm(0, GED_DATA_TYPE_ud), execMask).SetPredicate(predicate);
    }

    return proc;
  }

  proc += insF.MakeXor(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure XorXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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

    proc += insF.MakeXor(dstL, src0L, src1L, execMask).SetPredicate(predicate);
    proc += insF.MakeXor(dstH, src0H, src1H, execMask).SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeXor(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL,
         GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                            const GtRegRegion&, GtExecMask, GtPredicate)>
    XorFunctionsTable = {{GED_MODEL_TGL, &XorTgl},
                         {GED_MODEL_XE_HP, &XorXeHpc},
                         {GED_MODEL_XE_HPC, &XorXeHpc},
                         {GED_MODEL_XE2, &XorXe2}};

GtGenProcedure Macro::Xor(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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
  if (XorFunctionsTable.find(HwModel) != XorFunctionsTable.end()) {
    return XorFunctionsTable[HwModel](instrumentor, dst, src0, src1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeXor(dst, src0, src1, execMask).SetPredicate(predicate);
  return proc;
}

/**
dst: register, src0: register, src1: immediate
*/

GtGenProcedure XoriTgl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
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
          insF.MakeXor(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
      proc += insF.MakeXor(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                           execMask)
                  .SetPredicate(predicate);
    } else {
      proc +=
          insF.MakeXor(dstL, src0, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
      proc +=
          insF.MakeMov(dstH, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
              .SetPredicate(predicate);
    }

    return proc;
  }

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeXor(dst, src0, GtImm(srcI1.Value(), GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeXor(dst, src0,
                       GtImm(srcI1.Value(), Macro::GetGedIntDataTypeBytes(srcI1.DataType().Size())),
                       execMask)
              .SetPredicate(predicate);
  return proc;
}

GtGenProcedure XoriXeHpc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                         const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                         GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1 && dst.DataType().Size() != 8) {
    proc += XoriXeHpc(instrumentor, dst, src0, GtImm(srcI1.Value() & 0xFF, GED_DATA_TYPE_ud),
                      execMask, predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    proc += insF.MakeXor(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeXor(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                         execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeXor(dst, src0, srcI1.Value(), execMask).SetPredicate(predicate);
  return proc;
}

GtGenProcedure XoriXe2(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                       const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                       GtPredicate predicate) {
  GtGenProcedure proc;
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();

  if (srcI1.DataType().Size() == 1) {
    proc += insF.MakeXor(dst, src0, GtImm(srcI1 & 0xFF, GED_DATA_TYPE_uw), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  if (dst.DataType().Size() == 8 && src0.DataType().Size() == 8) {
    GtReg dstL = {dst.Reg(), 4, 0};
    GtReg dstH = {dst.Reg(), 4, 1};

    GtReg src0L = {src0.Reg(), 4, 0};
    GtReg src0H = {src0.Reg(), 4, 1};

    proc += insF.MakeXor(dstL, src0L, GtImm(srcI1.Value() & 0xFFFFFFFF, GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    proc += insF.MakeXor(dstH, src0H, GtImm((srcI1.Value() >> 32) & 0xFFFFFFFF, GED_DATA_TYPE_ud),
                         execMask)
                .SetPredicate(predicate);

    return proc;
  }

  proc += insF.MakeXor(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&,
                                       const GtRegRegion&, const GtImm&, GtExecMask, GtPredicate)>
    XoriFunctionsTable = {{GED_MODEL_TGL, &XoriTgl},
                          {GED_MODEL_XE_HP, &XoriXeHpc},
                          {GED_MODEL_XE_HPC, &XoriXeHpc},
                          {GED_MODEL_XE2, &XoriXe2}};

GtGenProcedure Macro::Xor(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  MACRO_TRACING_3I
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  uint64_t mask = Macro::GetMaskBySizeBytes(dst.DataType().Size());
  PTI_ASSERT(srcI1.Value() <= mask && "Immediate value is too large for the destination size");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (srcI1 == 0) {
    proc += Macro::Mov(instrumentor, dst, src0, execMask, predicate);
    return proc;
  }

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (XoriFunctionsTable.find(HwModel) != XoriFunctionsTable.end()) {
    return XoriFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  proc += insF.MakeXor(dst, src0, srcI1, execMask).SetPredicate(predicate);
  return proc;
}
