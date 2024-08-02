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

GtGenProcedure AtomicStoreXeHpc(const IGtKernelInstrument& instrumentor, GtReg addrReg, GtReg dataReg,
                                GtExecMask execMask, GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;

  if (dataReg.ElementSize() == 8) {
    /// Warning, it will not be atomic store, just store.
    PTI_ASSERT(execMask.ExecSize() == 1 && execMask.ChannelOffset() == 0 &&
               "Limited functionality of 64 bit atomic store on this HW");

    GtReg dataRegL = {dataReg, 4, 0};
    GtReg dataRegH = {dataReg, 4, 1};
    proc += insF.MakeAtomicStore(addrReg, dataRegL, GtDataType(GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    // mov dataRegH to dataRegTemp
    GtReg dataRegTemp = instrumentor.Coder().VregFactory().MakeMsgDataScratch(VREG_TYPE_DWORD);
    proc += insF.MakeMov(dataRegTemp, dataRegH, execMask).SetPredicate(predicate);
    proc +=
        Macro::Add(instrumentor, addrReg, addrReg, GtImm(4, GED_DATA_TYPE_d), execMask, predicate);

    proc += insF.MakeAtomicStore(addrReg, dataRegTemp, GtDataType(GED_DATA_TYPE_ud), execMask)
                .SetPredicate(predicate);
    return proc;
  }

  proc += insF.MakeAtomicStore(addrReg, dataReg,
                               GtDataType(Macro::GetGedIntDataTypeBytes(dataReg.ElementSize())),
                               execMask)
              .SetPredicate(predicate);
  return proc;
}

std::map<GED_MODEL, GtGenProcedure (*)(const IGtKernelInstrument&, GtReg, GtReg, GtExecMask, GtPredicate)>
    AtomicStoreFunctionsTable = {{GED_MODEL_XE_HP, &AtomicStoreXeHpc},
                                 {GED_MODEL_XE_HPC, &AtomicStoreXeHpc}};

GtGenProcedure Macro::AtomicStore(const IGtKernelInstrument& instrumentor, GtReg addrReg, GtReg dataReg,
                                  GtExecMask execMask, GtPredicate predicate) {
  PTI_ASSERT(dataReg.SubRegNum() == 0 &&
             "Data register should have sub-register number == 0. Try to use "
             "MakeMsgDataScratch VregFactory interface to allocate data register.");

#ifndef DISABLE_MACRO_WORKAROUNDS
  // check if there is a specific implementation for the current model
  GED_MODEL HwModel = instrumentor.Coder().GenModel().Id();
  if (AtomicStoreFunctionsTable.find(HwModel) != AtomicStoreFunctionsTable.end()) {
    return AtomicStoreFunctionsTable[HwModel](instrumentor, addrReg, dataReg, execMask, predicate);
  }
  // else default behaviour
#endif  // DISABLE_MACRO_WORKAROUNDS

  auto dataGedType = Macro::GetGedIntDataTypeBytes(dataReg.ElementSize());
  PTI_ASSERT(instrumentor.Coder().InstructionFactory().CanAccessAtomically(dataGedType) &&
             "Atomic accesses with such size is not available on this HW");

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeAtomicStore(addrReg, dataReg, GtDataType(dataGedType), execMask)
              .SetPredicate(predicate);
  return proc;
}
