//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_CAPSULES_H
#define PTI_GTPIN_CAPSULES_H

#include "def_gpu_gtpin.hpp"
#include "gen_send_decoder.h"
#include "prof_lib_gpu_gtpin.hpp"

#define ANNOTATION(proc) \
  for (auto ins : proc) ins->AppendAnnotation(__FUNCTION__);
#define EXEC_MASK_1_0 GtExecMask(GtExecSize(1), GtChannelOffset(0))
#define INVALID_NUM static_cast<size_t>(-1)

namespace gtpin {
namespace gtpin_prof {

/// Get HW & instrumentation info
/* inline */ bool Is64BitCountersSupport(IGtKernelInstrument& instrumentor);
/* inline */ size_t GetCounterSizeBytes(IGtKernelInstrument& instrumentor);
/* inline */ GED_DATA_TYPE GetCounterDataType(IGtKernelInstrument& instrumentor);
/* inline */ GED_DATA_TYPE GetCounterDataTypeSigned(IGtKernelInstrument& instrumentor);
/* inline */ GtVregType GetCounterVregDataType(IGtKernelInstrument& instrumentor);
/* inline */ uint32_t GetNumTiles(IGtKernelInstrument& instrumentor);
/* inline */ GtVregType GetVregDataType(size_t sizeBits);
/* inline */ GtVregType GetVregDataTypeBytes(size_t sizeBytes);
/* inline */ GED_DATA_TYPE GetGedIntDataType(size_t sizeBits);
/* inline */ GED_DATA_TYPE GetGedIntDataTypeBytes(size_t sizeBytes);
/* inline */ GED_DATA_TYPE GetGedIntDataTypeSigned(size_t sizeBites);
/* inline */ GED_DATA_TYPE GetGedIntDataTypeSigned(size_t sizeBytes);

/// Simple instructions
// namespace instr {
// GtGenProcedure Mov(IGtKernelInstrument& instrumentor, GtReg dst, GtReg src,
//                      GtPredicate predicate = GtPredicate::MakeNone());
// GtGenProcedure Mov(IGtKernelInstrument& instrumentor, GtReg dst, int64_t src,
//                      GtPredicate predicate = GtPredicate::MakeNone());
// }

GtGenProcedure Mov64(IGtKernelInstrument& instrumentor, GtReg dst, GtReg src,
                     GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Mov64(IGtKernelInstrument& instrumentor, GtReg dst, int64_t srci,
                     GtPredicate predicate = GtPredicate::MakeNone());

/// Procedures
GtGenProcedure CounterIncProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                              GtReg baseAddrReg, GtReg tempAddrReg, uint64_t dataOffsetBytes,
                              GtReg offserBytesReg = NullReg(),
                              GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure CounterAddProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                              GtReg baseAddrReg, GtReg tempAddrReg, GtReg dataReg,
                              uint64_t dataOffsetBytes, GtReg offserBytesReg = NullReg(),
                              GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure ComputeSimdMaskProc(IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                                   bool maskCtrl, uint32_t execMask, GtPredicate pred);
/* inline */ GtGenProcedure ComputeSimdMaskProc(IGtKernelInstrument& instrumentor,
                                                GtReg simdMaskReg, const IGtIns& gtpinIns);

GtGenProcedure IsCacheLineAlignedProc(IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                      size_t channelOffset, GtReg simdMaskReg, GtReg tempData1Reg,
                                      GtReg tempData2Reg, size_t mathWidthBytes = 2);
/* inline */ GtGenProcedure IsCacheLineAlignedProc(IGtKernelInstrument& instrumentor,
                                                   const IGtIns& gtpinIns, GtReg simdMaskReg,
                                                   GtReg tempData1Reg, GtReg tempData2Reg,
                                                   size_t mathWidthBytes = 2);
GtGenProcedure IsCacheLineAlignedFlagProc(IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                          size_t channelOffset, GtReg simdMaskReg,
                                          GtReg tempData1Reg, GtReg flagReg = FlagReg(0),
                                          size_t mathWidthBytes = 2);
/* inline */ GtGenProcedure IsCacheLineAlignedFlagProc(IGtKernelInstrument& instrumentor,
                                                       const IGtIns& gtpinIns, GtReg simdMaskReg,
                                                       GtReg tempData1Reg,
                                                       GtReg flagReg = FlagReg(0),
                                                       size_t mathWidthBytes = 2);
/// TODO: Add is cahce line aligned function based on flag register

GtGenProcedure CalcBaseAddrProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                                GtReg baseAddrReg, size_t recordIndex, size_t numTiles);

GtGenProcedure DistributionIntIncProc(IGtKernelInstrument& instrumentor, GtReg dataReg,
                                      uint64_t overflowLowOffset, uint64_t overflowHighOffset,
                                      uint64_t baseOffset, uint64_t lowerValue,
                                      uint64_t higherValue);

GtGenProcedure CacheLinesCountProc(IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                                   GtReg simdMaskReg, uint32_t execSize, GtReg addrRegCheckReg,
                                   size_t channelOffset, const uint32_t addrWidthBytes,
                                   GtReg flagReg = FlagReg(0));
/* inline */ GtGenProcedure CacheLinesCountProc(IGtKernelInstrument& instrumentor,
                                                GtReg clCounterReg, GtReg simdMaskReg,
                                                const IGtIns& gtpinIns, GtReg flagReg = FlagReg(0));

/** Helper functions which is used in instrumentation process. */
inline bool Is64BitCountersSupport(IGtKernelInstrument& instrumentor) {
  return (instrumentor.Coder().InstructionFactory().CanAccessAtomically(GED_DATA_TYPE_uq));
}
inline size_t GetCounterSizeBytes(IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? 8 : 4);
}
inline GED_DATA_TYPE GetCounterDataType(IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud);
}
inline GED_DATA_TYPE GetCounterDataTypeSigned(IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_q : GED_DATA_TYPE_d);
}
inline GtVregType GetCounterVregDataType(IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GtVregType(VREG_TYPE_QWORD)
                                                 : GtVregType(VREG_TYPE_DWORD));
}
inline uint32_t GetNumTiles(IGtKernelInstrument& instrumentor) {
  // Disabled by now due to problems in addr calculation
  // return ((instrumentor.Coder().IsTileIdSupported()) ?
  //         GTPin_GetCore()->GenArch().MaxTiles(instrumentor.Kernel().GpuPlatform())
  //         : 1);
  // TODO: enable after fix MUL operation
  return 1;
}
inline GtVregType GetVregDataType(size_t sizeBits) {
  PTI_ASSERT(((sizeBits || 0b111) == 0) && "Incorrect size");
  return GtVregType::MakeBySize((sizeBits >> 3));  // sizeBits // 8
}
inline GtVregType GetVregDataTypeBytes(size_t sizeBytes) {
  return GtVregType::MakeBySize(sizeBytes);
}
inline GED_DATA_TYPE GetGedIntDataType(size_t sizeBits) {
  switch (sizeBits) {
    case 8:
      return GED_DATA_TYPE_ub;
    case 16:
      return GED_DATA_TYPE_uw;
    case 32:
      return GED_DATA_TYPE_ud;
    case 64:
      return GED_DATA_TYPE_uq;
    default:
      PTI_ASSERT(false &&
                 (std::string("Incorrect data size in Bits: ") + std::to_string(sizeBits)).c_str());
  }
  return GED_DATA_TYPE_INVALID;
}
inline GED_DATA_TYPE GetGedIntDataTypeBytes(size_t sizeBytes) {
  return GetGedIntDataType(sizeBytes * 8);
}
inline GED_DATA_TYPE GetGedIntDataTypeSigned(size_t sizeBits) {
  switch (sizeBits) {
    case 8:
      return GED_DATA_TYPE_b;
    case 16:
      return GED_DATA_TYPE_w;
    case 32:
      return GED_DATA_TYPE_d;
    case 64:
      return GED_DATA_TYPE_q;
    default:
      PTI_ASSERT(false &&
                 (std::string("Incorrect data size in Bits: ") + std::to_string(sizeBits)).c_str());
  }
  return GED_DATA_TYPE_INVALID;
}
inline GED_DATA_TYPE GetGedIntDataTypeBytesSigned(size_t sizeBytes) {
  return GetGedIntDataTypeSigned(sizeBytes * 8);
}

inline GtGenProcedure ComputeSimdMaskProc(IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                                          const IGtIns& gtpinIns) {
  return ComputeSimdMaskProc(instrumentor, simdMaskReg, !gtpinIns.IsWriteMaskEnabled(),
                             gtpinIns.ExecMask().Bits(), gtpinIns.Predicate());
}

inline GtGenProcedure IsCacheLineAlignedProc(IGtKernelInstrument& instrumentor,
                                             const IGtIns& gtpinIns, GtReg simdMaskReg,
                                             GtReg tempData1Reg, GtReg tempData2Reg,
                                             size_t mathWidthBytes) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, mathWidthBytes);

  return IsCacheLineAlignedProc(instrumentor, addrRegCheckReg, msg.ChannelOffset(), simdMaskReg,
                                tempData1Reg, tempData2Reg, mathWidthBytes);
}

inline GtGenProcedure IsCacheLineAlignedFlagProc(IGtKernelInstrument& instrumentor,
                                                 const IGtIns& gtpinIns, GtReg simdMaskReg,
                                                 GtReg tempData1Reg, GtReg flagReg,
                                                 size_t mathWidthBytes) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, mathWidthBytes);

  return IsCacheLineAlignedFlagProc(instrumentor, addrRegCheckReg, msg.ChannelOffset(), simdMaskReg,
                                    tempData1Reg, flagReg, mathWidthBytes);
}

inline GtGenProcedure CacheLinesCountProc(IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                                          GtReg simdMaskReg, const IGtIns& gtpinIns,
                                          GtReg flagReg) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, msg.AddrSize());

  return CacheLinesCountProc(instrumentor, clCounterReg, simdMaskReg, msg.ExecSize(),
                             addrRegCheckReg, msg.ChannelOffset(), msg.AddrSize(), flagReg);
}

class PointOfInterest {
 public:
  PointOfInterest(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                  size_t recordIndex);
  void FinishPOI(GtGenProcedure& proc);
  ~PointOfInterest() = default;

  void AppendProcedure(GtGenProcedure& proc);
  // operator +

  bool IsInit();
  bool IsFinalized();
  bool ResetPOI();
  GtGenProcedure GetProcedure();

  /// getters
  size_t GetNumTiles();
  GtReg GetBaseAddrReg();
  GtReg GetSimdMaskReg(const IGtIns& gtpinIns);
  GtReg GetSimdMaskReg(bool maskCtrl, uint32_t execMask, GtPredicate pred);
  GtReg GetTempAddrReg();
  GtReg GetTempData64Reg();
  GtReg GetMsgData64Reg();
  /// setters
  void SetTempAddrReg(GtReg tempAddrReg);
  void SetTempData64Reg(GtReg tempAddrReg);

  /// Utils
  void CalcBaseAddr();
  void IsCacheLineAligned(GtReg isCLAlignedReg, const IGtIns& gtpinIns);
  void IsCacheLineAligned(GtReg isCLAlignedReg, GtReg addressRegCheckReg, size_t channelOffset);
  void ComputeSimdMask(bool maskCtrl, uint32_t execMask, GtPredicate pred);
  void ComputeSimdMask(const IGtIns& gtpinIns);
  void CounterAddProcedure(GtReg dataReg, uint64_t dataOffsetBytes = 0,
                           GtReg offserBytesReg = NullReg(),
                           GtPredicate predicate = GtPredicate::MakeNone());
  void CounterIncProcedure(uint64_t dataOffsetBytes = 0, GtReg offserBytesReg = NullReg(),
                           GtPredicate predicate = GtPredicate::MakeNone());

  /// Analyses
  void InstructionCounterAnalysis(uint64_t dataOffsetBytes = 0);
  void SimdActiveCounterAnalysis(const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);
  void CacheLineAlignedCounterAnalysis(const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);
  void CacheLineCounterAnalysis(const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);
  void StrideDistrCalc(const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);

 private:
  IGtKernelInstrument& m_instrumentor;
  GtProfileArray& m_profileArray;
  size_t m_recordIndex;
  GtGenProcedure m_proc;
  size_t m_numTiles = INVALID_NUM;
  GtReg m_baseAddrReg = GtReg();
  GtReg m_simdMaskReg = GtReg();
  GtReg m_tempAddrReg = GtReg();
  GtReg m_tempData64Reg = GtReg();
  GtReg m_tempMsgData64Reg = GtReg();

  IGtVregFactory& m_vregs;
  IGtInsFactory& m_insF;
};

}  // namespace gtpin_prof
}  // namespace gtpin

#endif  // PTI_GTPIN_CAPSULES_H
