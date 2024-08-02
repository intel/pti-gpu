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

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtRegRegion&, GtExecMask, GtPredicate)>
    ShlFunctionsTable = {};

GtGenProcedure Macro::Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtRegRegion& src1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  // Destination size may be less than source 2 size

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

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, const GtDstRegion&, const GtRegRegion&,
                                       const GtImm&, GtExecMask, GtPredicate)>
    ShliFunctionsTable = {{GED_MODEL_XE_HP, &ShliXeHpc}, {GED_MODEL_XE_HPC, &ShliXeHpc}};

GtGenProcedure Macro::Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                          const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask,
                          GtPredicate predicate) {
  PTI_ASSERT(dst.DataType().Size() >= src0.DataType().Size() &&
             "Destination size should be no less than source size");
  PTI_ASSERT(srcI1.Value() <= dst.DataType().Size() * 8 &&
             "Shift value is too large for the destination size");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (ShliFunctionsTable.find(HwModel) != ShliFunctionsTable.end()) {
    return ShliFunctionsTable[HwModel](instrumentor, dst, src0, srcI1, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  proc += insF.MakeShl(dst, src0, GtImm(srcI1, dst.DataType()), execMask).SetPredicate(predicate);
  return proc;
}
