//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "gtpin_capsules.hpp"

namespace gtpin {
namespace gtpin_prof {

GtGenProcedure CounterIncProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                              GtReg baseAddrReg, GtReg tempAddrReg, uint64_t dataOffsetBytes,
                              GtReg offserBytesReg, GtPredicate predicate) {
  GED_DATA_TYPE counterDataType = GetCounterDataType(instrumentor);
  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();

  GtGenProcedure proc;

  profileArray.ComputeRelAddress(coder, proc, tempAddrReg, baseAddrReg, dataOffsetBytes);
  proc += insF.MakeAtomicInc(NullReg(), tempAddrReg, counterDataType, EXEC_MASK_1_0)
              .SetPredicate(predicate);

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure CounterAddProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                              GtReg baseAddrReg, GtReg tempAddrReg, GtReg dataReg,
                              uint64_t dataOffsetBytes, GtReg offserBytesReg,
                              GtPredicate predicate) {
  PTI_ASSERT(dataReg.ElementSize() >= GetCounterSizeBytes(instrumentor) && "Wrong register size");
  PTI_ASSERT(dataReg.SubRegNum() == 0 &&
             "Data register should have subregister number == 0. Try to use "
             "MakeMsgDataScratch VregFactory interface to allocate data register.");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();

  GtReg dataClipped = {dataReg, static_cast<uint32_t>(GetCounterSizeBytes(instrumentor)), 0};

  GtGenProcedure proc;

  profileArray.ComputeRelAddress(coder, proc, tempAddrReg, baseAddrReg, dataOffsetBytes);
  proc += insF.MakeAtomicAdd(NullReg(), tempAddrReg, dataClipped, GetCounterDataType(instrumentor),
                             EXEC_MASK_1_0);

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure ComputeSimdMaskProc(IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                                   bool maskCtrl, uint32_t execMask, GtPredicate pred) {
  GtGenProcedure proc;

  instrumentor.Coder().ComputeSimdMask(proc, simdMaskReg, maskCtrl, execMask, pred);

  ANNOTATION(proc)
  return proc;
}

/**
 * Calcultaion goes in 16 bits due to no need in all 64 bits. The only need in
 * lower 6 bits Result in tempData1Reg reg. 0 - for not CL aligned, !=0 for CL
 * aligned and channel active
 */
GtGenProcedure IsCacheLineAlignedProc(IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                      size_t channelOffset, GtReg simdMaskReg, GtReg tempData1Reg,
                                      GtReg tempData2Reg, size_t mathWidthBytes) {
  PTI_ASSERT(tempData1Reg.IsValid() && (tempData1Reg.ElementSize() >= mathWidthBytes) &&
             "Wrong register size");
  PTI_ASSERT(tempData2Reg.IsValid() && (tempData2Reg.ElementSize() >= mathWidthBytes) &&
             "Wrong register size");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();
  IGtVregFactory& vregs = coder.VregFactory();
  const GED_DATA_TYPE SIMD_DATA_TYPE = GetGedIntDataTypeBytes(MAX_SIMD_WIDTH_BYTES);
  const GED_DATA_TYPE MATH_DATA_TYPE = GetGedIntDataTypeBytes(mathWidthBytes);

  GtReg temp1Reg = {tempData1Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg temp2Reg = {tempData2Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg firstAddrReg = {addrRegCheckReg,
                        static_cast<uint32_t>(mathWidthBytes), /*Lower bits of first address*/
                        0 /*Sub reg num always == 0 for address register*/};

  GtGenProcedure proc;

  proc += insF.MakeMov(tempData1Reg, GtImm(0, GetGedIntDataTypeBytes(tempData1Reg.ElementSize())),
                       EXEC_MASK_1_0);

  /// Next code computes alligment of address.
  /// temp1Reg == 0 in case of not alligned, temp1Reg == 0x1 in case of aligned
  proc += insF.MakeAnd(temp1Reg, firstAddrReg, GtImm(0b111111, MATH_DATA_TYPE), EXEC_MASK_1_0);
  // temp1Reg = (temp1Reg & 0b1111) | (temp1Reg >> 4)
  proc += insF.MakeShr(temp2Reg, temp1Reg, GtImm(4, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeAnd(temp1Reg, temp1Reg, GtImm(0b1111, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeOr(temp1Reg, temp1Reg, temp2Reg, EXEC_MASK_1_0);
  // temp1Reg = (temp1Reg & 0b11) | (temp1Reg >> 2)
  proc += insF.MakeShr(temp2Reg, temp1Reg, GtImm(2, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeAnd(temp1Reg, temp1Reg, GtImm(0b11, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeOr(temp1Reg, temp1Reg, temp2Reg, EXEC_MASK_1_0);
  // temp1Reg = (temp1Reg & 0b1) | (temp1Reg >> 1)
  proc += insF.MakeShr(temp2Reg, temp1Reg, GtImm(1, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeAnd(temp1Reg, temp1Reg, GtImm(0b1, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeOr(temp1Reg, temp1Reg, temp2Reg, EXEC_MASK_1_0);
  // temp1Reg = !temp1Reg[0]
  proc += insF.MakeXor(temp1Reg, temp1Reg, GtImm(1, MATH_DATA_TYPE), EXEC_MASK_1_0);

  /// Next code checks lane active. temp2Reg == 0: Channel enable, temp2Reg ==
  /// 1: channel disabled temp2Reg = ( SIMDmask >> channelOffset ) & 0x1
  proc += insF.MakeShr(temp2Reg, simdMaskReg, GtImm(channelOffset, SIMD_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeAnd(temp2Reg, temp2Reg, GtImm(0x1, MATH_DATA_TYPE), EXEC_MASK_1_0);

  /// Channel active & CL aligned
  proc += insF.MakeAnd(temp1Reg, temp1Reg, temp2Reg, EXEC_MASK_1_0);

  ANNOTATION(proc)
  return proc;
}

/**
 * Faster, but requeres flag register. Result in tempData1Reg.
 * tempData1Reg = 1 in case of active and alligned, 0 otherwise.
 */
GtGenProcedure IsCacheLineAlignedFlagProc(IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                          size_t channelOffset, GtReg simdMaskReg,
                                          GtReg tempData1Reg, GtReg flagReg,
                                          size_t mathWidthBytes) {
  PTI_ASSERT(tempData1Reg.IsValid() && (tempData1Reg.ElementSize() >= mathWidthBytes) &&
             "Wrong register size");
  PTI_ASSERT(flagReg.IsFlagReg() && "Resister is not a flag register");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();
  IGtVregFactory& vregs = coder.VregFactory();
  const GED_DATA_TYPE MATH_DATA_TYPE = GetGedIntDataTypeBytes(mathWidthBytes);

  GtReg temp1Reg = {tempData1Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg firstAddrReg = {addrRegCheckReg,
                        static_cast<uint32_t>(mathWidthBytes), /*Lower bits of first address*/
                        0 /*Sub reg num always == 0 for address register*/};
  GtPredicate predicate(flagReg);

  GtGenProcedure proc;

  proc += insF.MakeMov(tempData1Reg, GtImm(0, GetGedIntDataTypeBytes(tempData1Reg.ElementSize())),
                       EXEC_MASK_1_0);

  /// flagReg == 1 in case of not alligned, flagReg == 0 in case of aligned
  proc += insF.MakeAnd(temp1Reg, firstAddrReg, GtImm(0b111111, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc +=
      insF.MakeCmp(GED_COND_MODIFIER_z, flagReg, temp1Reg, GtImm(0, MATH_DATA_TYPE), EXEC_MASK_1_0);

  /// Check channel active for M0, M16, etc.
  proc += insF.MakeShr(temp1Reg, simdMaskReg, GtImm(channelOffset, MATH_DATA_TYPE), EXEC_MASK_1_0);
  proc += insF.MakeAnd(temp1Reg, temp1Reg, GtImm(0x1, MATH_DATA_TYPE),
                       EXEC_MASK_1_0);  // temp1Reg[0] = is_channel_active, temp1Reg[1:] == 0

  /// Set 0 if not alligned, left is_channel_active if alligned
  proc += insF.MakeMov(tempData1Reg, GtImm(0, GetGedIntDataTypeBytes(tempData1Reg.ElementSize())),
                       EXEC_MASK_1_0)
              .SetPredicate(!predicate);

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure CalcBaseAddrProc(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                                GtReg baseAddrReg, size_t recordIndex, size_t numTiles) {
  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();

  GtGenProcedure proc;

  if (numTiles == 1) {
    profileArray.ComputeAddress(coder, proc, baseAddrReg, recordIndex);
  } else {
    /// tiled data is not supported due to problems in address calculation do
    /// calc of 1 tile instead.
    profileArray.ComputeAddress(coder, proc, baseAddrReg, recordIndex);

    /// Tiled version:
    // GtReg tileIdReg = {data64, sizeof(uint32_t), 0};
    // proc += insF.MakeRegZero(baseAddrReg);
    // coder.LoadTileId(proc, tileIdReg);
    // proc += insF.MakeMul(tileIdReg, tileIdReg, profileArray.ChunkSize());
    // proc += insF.MakeAdd(tileIdReg, tileIdReg, profileArray.RecordSize() *
    // recordIndex); profileArray.ComputeAddress(coder, proc, baseAddrReg,
    // tileIdReg);
  }

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure DistributionIntIncProc(IGtKernelInstrument& instrumentor, GtReg dataReg,
                                      uint64_t overflowLowOffset, uint64_t overflowHighOffset,
                                      uint64_t baseOffset, uint64_t lowerValue,
                                      uint64_t higherValue) {
  GtGenProcedure proc;
  ANNOTATION(proc)
  return proc;
}

/// This instrumentation makes register allocation. it allocs (1 + execSize) *
/// addrWidthBytes Uses address == -1 as no address marker. Result, number of
/// cache lines, is stored in clCounterReg
#define INVALID_ADDRESS -1
GtGenProcedure CacheLinesCountProc(IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                                   GtReg simdMaskReg, uint32_t execSize, GtReg addrRegCheckReg,
                                   size_t channelOffset, const uint32_t addrWidthBytes,
                                   GtReg flagReg) {
  /// check registers for minimal requerenments
  /// Check that clCounter reg is at least DWORD

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();
  uint32_t grfRegSize = insF.GenModel().GrfRegSize();  // bytes
  GtPredicate predicate(flagReg);

  GtRegNum src0RegNum = addrRegCheckReg.RegNum();

  GtGenProcedure proc;

  IGtVregFactory& vregs = coder.VregFactory();

  /// Datatypes, sizes
  const GED_DATA_TYPE addrGedType = GetGedIntDataTypeBytesSigned(addrWidthBytes);
  const VREG_TYPE addrVregType = GetVregDataTypeBytes(addrWidthBytes);

  const uint32_t localCounterWidthBytes = sizeof(uint16_t);  // bytes
  const GED_DATA_TYPE localCounterGedType = GetGedIntDataTypeBytesSigned(localCounterWidthBytes);
  const VREG_TYPE localCounterVregType = GetVregDataTypeBytes(localCounterWidthBytes);

  /// allocate num of clids, counter, temp
  GtReg clLocalCounter = vregs.MakeScratch(localCounterVregType);

  std::vector<GtReg> clIds;  /// storage for cache lines used in send
  clIds.resize(execSize + 1);
  for (uint32_t i = 0; i < execSize; i++) {
    clIds[i] = vregs.MakeScratch(addrVregType);
  }

  /// Register initialization
  proc +=
      insF.MakeRegZero(clCounterReg)
          .AppendAnnotation((std::string(__FUNCTION__) + std::string(":clCounterRegInit")).c_str());

  /// fill Cacheline storage with unique cachelines, calc unique CL number in
  /// clCounterReg
  uint32_t regNum = src0RegNum;
  uint32_t subRegNum = 0;
  for (uint32_t i = 0; i < execSize; i++) {  // Cacheline storage
    /// Calc current address register
    subRegNum = i;
    regNum = src0RegNum;
    while ((subRegNum + 1) * addrWidthBytes > grfRegSize) {
      regNum -= -1;
      subRegNum -= grfRegSize / addrWidthBytes;
    }
    GtReg currAddrReg = GrfReg(regNum, subRegNum, addrWidthBytes);

    /// Add current cache line ID in storage on the last position.
    /// clIds[i] = cacheline ID for current address
    GtReg clIdTemp = clIds[i];  // alias: clIdTemp = CLid with current index
    proc += insF.MakeMov(clIdTemp, currAddrReg);
    proc.back()->AppendAnnotation(
        (std::string(__FUNCTION__) + std::string(":CheckAddr#") + std::to_string(i)).c_str());
    proc += insF.MakeOr(GtReg(clIdTemp, sizeof(uint32_t), 0), GtReg(clIdTemp, sizeof(uint32_t), 0),
                        0b111111);  // set last 6 bits to 1 to get cache line ID from address

    /// Compare with all previous cache line ids in storage. clLocalCounter =
    /// (bool) cache line already used
    proc += insF.MakeMov(clLocalCounter, 0);
    for (int j = 0; j < i; j++) {
      proc += insF.MakeCmp(GED_COND_MODIFIER_z, flagReg, GtReg(clIdTemp, addrWidthBytes, 0),
                           GtReg(clIds[j], addrWidthBytes, 0));
      proc += insF.MakeOr(clLocalCounter, clLocalCounter, 1).SetPredicate(predicate);
    }
    proc += insF.MakeXor(clLocalCounter, clLocalCounter,
                         1);  /// (bool) clLocalCounter = New_Cache_Line (aka
                              /// cache line not found in storage)

    /// Check if channel active based on SIMD mask, channel offset and channel
    /// ID. The result will be in the flag register. flag = is SIMD lane Active
    proc += insF.MakeShl(clLocalCounter, clLocalCounter,
                         (channelOffset + i));  /// shift bool value to current SIMD lane position
    proc += insF.MakeAnd(clLocalCounter, clLocalCounter,
                         simdMaskReg);  /// clLocalCounter = SIMD_lane_Active && New_Cache_Line

    proc += insF.MakeCmp(GED_COND_MODIFIER_nz, flagReg, clLocalCounter,
                         0);  /// flag = New cache line detected
    proc += insF.MakeAdd(clCounterReg, clCounterReg, 1).SetPredicate(predicate);

    // remove cache line from storage if exists or SIMD lane not active
    proc += insF.MakeMov(clIdTemp, GtImmS32(-1))
                .SetPredicate(!predicate);  // remove cache line from storage if
                                            // exists or SIMD lane not active
  }

  ANNOTATION(proc)
  return proc;
}
#undef INVALID_ADDRESS

GtGenProcedure Mov64(IGtKernelInstrument& instrumentor, GtReg dst, GtReg src,
                     GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc +=
      insF.MakeMov(GtReg(dst, sizeof(uint32_t), 0), GtReg(src, sizeof(uint32_t), 0), GtExecMask(2))
          .SetPredicate(predicate)
          .AppendAnnotation("MOV64");
  return proc;
}
GtGenProcedure Mov64(IGtKernelInstrument& instrumentor, GtReg dst, int64_t srci,
                     GtPredicate predicate) {
  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  GtGenProcedure proc;
  proc += insF.MakeMov(GtReg(dst, sizeof(uint64_t), 0), GtImm(srci, GED_DATA_TYPE_q))
              .SetPredicate(predicate)
              .AppendAnnotation("MOVI64");
  return proc;
}

/*******************************************************************************
 * PointOfInterest
 * ****************************************************************************/

PointOfInterest::PointOfInterest(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                                 size_t recordIndex)
    : m_instrumentor(instrumentor),
      m_profileArray(profileArray),
      m_recordIndex(recordIndex),
      m_vregs(m_instrumentor.Coder().VregFactory()),
      m_insF(m_instrumentor.Coder().InstructionFactory()) {
  GetNumTiles();
}

void PointOfInterest::FinishPOI(GtGenProcedure& proc) {
  proc += m_proc;
  m_proc.Clear();
}

void PointOfInterest::AppendProcedure(GtGenProcedure& proc) { m_proc += proc; }

size_t PointOfInterest::GetNumTiles() {
  if (m_numTiles == INVALID_NUM) {
    m_numTiles = static_cast<size_t>(gtpin_prof::GetNumTiles(m_instrumentor));
  }
  return m_numTiles;
}
GtReg PointOfInterest::GetBaseAddrReg() {
  if (!m_baseAddrReg.IsValid()) {
    m_baseAddrReg = m_vregs.MakeMsgAddrScratch();
    CalcBaseAddr();
  }
  return m_baseAddrReg;
}
GtReg PointOfInterest::GetSimdMaskReg(bool maskCtrl, uint32_t execMask, GtPredicate pred) {
  if (!m_simdMaskReg.IsValid()) {
    m_simdMaskReg = m_vregs.MakeScratch(GtVregType::MakeBySize(MAX_SIMD_WIDTH));
    ComputeSimdMask(maskCtrl, execMask, pred);
  }
  return m_simdMaskReg;
}
GtReg PointOfInterest::GetSimdMaskReg(const IGtIns& gtpinIns) {
  return GetSimdMaskReg(!gtpinIns.IsWriteMaskEnabled(), gtpinIns.ExecMask().Bits(),
                        gtpinIns.Predicate());
}
GtReg PointOfInterest::GetTempAddrReg() {
  if (!m_tempAddrReg.IsValid()) {
    m_tempAddrReg = m_vregs.MakeMsgAddrScratch();
  }
  return m_tempAddrReg;
}
GtReg PointOfInterest::GetTempData64Reg() {
  if (!m_tempData64Reg.IsValid()) {
    m_tempData64Reg = m_vregs.MakeScratch(VREG_TYPE_QWORD);
  }
  return m_tempData64Reg;
}
GtReg PointOfInterest::GetMsgData64Reg() {
  if (!m_tempMsgData64Reg.IsValid()) {
    m_tempMsgData64Reg = m_vregs.MakeMsgDataScratch(VREG_TYPE_QWORD);
  }
  return m_tempMsgData64Reg;
}
/// setters
void PointOfInterest::SetTempAddrReg(GtReg tempAddrReg) { m_tempData64Reg = tempAddrReg; }
void PointOfInterest::SetTempData64Reg(GtReg tempData64Reg) { m_tempData64Reg = tempData64Reg; }

/// Utils
void PointOfInterest::CalcBaseAddr() {
  m_proc +=
      CalcBaseAddrProc(m_instrumentor, m_profileArray, m_baseAddrReg, m_recordIndex, GetNumTiles());
}

void PointOfInterest::IsCacheLineAligned(GtReg isCLAlignedReg, const IGtIns& gtpinIns) {
  size_t mathWidthBytes = 2;

  auto tempReg = GetTempData64Reg();
  GtReg tempData1Reg = {tempReg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg tempData2Reg = {tempReg, static_cast<uint32_t>(mathWidthBytes), 1};

  m_proc += IsCacheLineAlignedProc(m_instrumentor, gtpinIns, GetSimdMaskReg(gtpinIns), tempData1Reg,
                                   tempData2Reg, mathWidthBytes);
}

void PointOfInterest::IsCacheLineAligned(GtReg isCLAlignedReg, GtReg addressRegCheckReg,
                                         size_t channelOffset) {
  /// TODO
}
void PointOfInterest::ComputeSimdMask(bool maskCtrl, uint32_t execMask, GtPredicate pred) {
  m_proc += ComputeSimdMaskProc(m_instrumentor, m_simdMaskReg, maskCtrl, execMask, pred);
}
void PointOfInterest::ComputeSimdMask(const IGtIns& gtpinIns) {
  m_proc += ComputeSimdMaskProc(m_instrumentor, m_simdMaskReg, gtpinIns);
}
void PointOfInterest::CounterAddProcedure(GtReg dataReg, uint64_t dataOffsetBytes,
                                          GtReg offserBytesReg, GtPredicate predicate) {
  m_proc += CounterAddProc(m_instrumentor, m_profileArray, GetBaseAddrReg(), GetTempAddrReg(),
                           dataReg, dataOffsetBytes, NullReg(), predicate);
}
void PointOfInterest::CounterIncProcedure(uint64_t dataOffsetBytes, GtReg offserBytesReg,
                                          GtPredicate predicate) {
  m_proc += CounterIncProc(m_instrumentor, m_profileArray, GetBaseAddrReg(), GetTempAddrReg(),
                           dataOffsetBytes, NullReg(), predicate);
}

/// Analyses
void PointOfInterest::InstructionCounterAnalysis(uint64_t dataOffsetBytes) {
  m_proc += CounterIncProc(m_instrumentor, m_profileArray, GetBaseAddrReg(), GetTempAddrReg(),
                           dataOffsetBytes);
}

void PointOfInterest::SimdActiveCounterAnalysis(const IGtIns& gtpinIns, uint64_t dataOffsetBytes) {
  auto tempReg = GetMsgData64Reg();
  m_proc += m_insF.MakeCbit(GetMsgData64Reg(), GetSimdMaskReg(gtpinIns), EXEC_MASK_1_0);
  CounterAddProcedure(GetMsgData64Reg(), dataOffsetBytes);
}

void PointOfInterest::CacheLineAlignedCounterAnalysis(const IGtIns& gtpinIns,
                                                      uint64_t dataOffsetBytes) {
  size_t mathWidthBytes = 2;

  auto tempReg = GetMsgData64Reg();
  /// clear register
  GtReg clearReg = {tempReg, static_cast<uint32_t>(GetCounterSizeBytes(m_instrumentor)), 0};
  m_proc += Mov64(m_instrumentor, clearReg, 0);
  GtReg flagReg = FlagReg(0);
  GtPredicate predicate(flagReg);

  GtReg tempData1Reg = {tempReg, static_cast<uint32_t>(mathWidthBytes), 0};

  m_proc += IsCacheLineAlignedFlagProc(m_instrumentor, gtpinIns, GetSimdMaskReg(gtpinIns),
                                       tempData1Reg, flagReg, mathWidthBytes);

  m_proc += m_insF.MakeCmp(GED_COND_MODIFIER_z, flagReg, tempData1Reg,
                           GtImm(0x0, GetGedIntDataTypeBytes(mathWidthBytes)), EXEC_MASK_1_0);

  CounterIncProcedure(dataOffsetBytes, NullReg(), predicate);
}

void PointOfInterest::CacheLineCounterAnalysis(const IGtIns& gtpinIns, uint64_t dataOffsetBytes) {
  GtReg clCounterReg = GetMsgData64Reg();

  m_proc += CacheLinesCountProc(m_instrumentor, clCounterReg, GetSimdMaskReg(gtpinIns), gtpinIns);

  CounterAddProcedure(clCounterReg, dataOffsetBytes);
}

void StrideDistrCalc(const IGtIns& gtpinIns, uint64_t dataOffsetBytes) {}

}  // namespace gtpin_prof
}  // namespace gtpin
