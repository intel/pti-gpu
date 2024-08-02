//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file capsule.cpp
 * @brief Contains the implementation of helper functions used in the instrumentation process.
 *
 * This file provides helper functions that are used in the instrumentation process. These functions
 * are used for tasks such as determining the support for 64-bit counters, calculating counter
 * sizes, retrieving counter data types, and more. The functions in this file are used to facilitate
 * the instrumentation of kernels.
 */

#include "capsule.hpp"

#include <cmath>

#include "api/gtpin_api.h"
#include "def_gpu.hpp"

using namespace gtpin;
using namespace gtpin_prof;

/**
 * Macro defines helper functions which is used in instrumentation process.
 */
bool Macro::Is64BitCountersSupport(const IGtKernelInstrument& instrumentor) {
  return (instrumentor.Coder().InstructionFactory().CanAccessAtomically(GED_DATA_TYPE_uq));
}
size_t Macro::GetCounterSizeBytes(const IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? sizeof(uint64_t) : sizeof(uint32_t));
}
GED_DATA_TYPE Macro::GetCounterDataType(const IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_uq : GED_DATA_TYPE_ud);
}
GED_DATA_TYPE Macro::GetCounterDataTypeSigned(const IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GED_DATA_TYPE_q : GED_DATA_TYPE_d);
}
GtVregType Macro::GetCounterVregType(const IGtKernelInstrument& instrumentor) {
  return ((Is64BitCountersSupport(instrumentor)) ? GtVregType(VREG_TYPE_QWORD)
                                                 : GtVregType(VREG_TYPE_DWORD));
}
uint32_t Macro::GetNumTiles(const IGtKernelInstrument& instrumentor) {
  return ((instrumentor.Coder().IsTileIdSupported())
              ? GTPin_GetCore()->GenArch().MaxTiles(instrumentor.Kernel().GpuPlatform())
              : 1);
}
GtVregType Macro::GetVregType(size_t sizeBits) {
  PTI_ASSERT((64 >= sizeBits) && "Incorrect size");
  return GtVregType::MakeBySize((sizeBits / 3));
}
GtVregType Macro::GetVregTypeBytes(size_t sizeBytes) { return GtVregType::MakeBySize(sizeBytes); }
GED_DATA_TYPE Macro::GetGedIntDataType(size_t sizeBits) {
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
GED_DATA_TYPE Macro::GetGedIntDataTypeBytes(size_t sizeBytes) {
  return GetGedIntDataType(sizeBytes * 8);
}
GED_DATA_TYPE Macro::GetGedIntDataTypeSigned(size_t sizeBits) {
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
GED_DATA_TYPE Macro::GetGedIntDataTypeBytesSigned(size_t sizeBytes) {
  return GetGedIntDataTypeSigned(sizeBytes * 8);
}

uint64_t Macro::GetMaskBySize(size_t sizeBits) {
  PTI_ASSERT(sizeBits <= 64 && "Incorrect size");
  if (sizeBits == 64) {
    return 0xFFFF'FFFF'FFFF'FFFF;
  }
  return (1ull << sizeBits) - 1;
}

uint64_t Macro::GetMaskBySizeBytes(size_t sizeBytes) { return Macro::GetMaskBySize(sizeBytes * 8); }

GtReg Macro::GetSubReg(const IGtKernelInstrument& instrumentor, size_t regNum, size_t subRegIdx,
                       size_t subRegSizeBytes) {
  return GetSubReg(regNum, subRegIdx, subRegSizeBytes,
                   instrumentor.Coder().InstructionFactory().GenModel().GrfRegSize());
}
GtReg Macro::GetSubReg(size_t regNum, size_t subRegIdx, size_t subRegSizeBytes, size_t grfRegSize) {
  while ((subRegIdx + 1) * subRegSizeBytes > grfRegSize) {
    regNum -= -1;
    subRegIdx -= grfRegSize / subRegSizeBytes;
  }
  return GrfReg(regNum, subRegIdx, (uint32_t)subRegSizeBytes);
}

/**
 * Procedure defines helper functions which is used in instrumentation process.
 */
GtGenProcedure Procedure::CounterInc(const IGtKernelInstrument& instrumentor,
                                     GtProfileArray& profileArray, GtReg baseAddrReg,
                                     GtReg tempAddrReg, uint64_t dataOffsetBytes,
                                     GED_DATA_TYPE counterDataType, GtExecMask execMask,
                                     GtReg offsetBytesReg, GtPredicate predicate) {
  PTI_ASSERT(instrumentor.Coder().InstructionFactory().CanAccessAtomically(counterDataType) &&
             "Atomic accesses with such size is not available on this HW");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();

  GtGenProcedure proc;

  profileArray.ComputeRelAddress(coder, proc, tempAddrReg, baseAddrReg, dataOffsetBytes);
  if (offsetBytesReg != NullReg()) {
    coder.ComputeRelAddress(proc, tempAddrReg, tempAddrReg, GtReg(offsetBytesReg, 4, 0), predicate);
  }
  proc += insF.MakeAtomicInc(NullReg(), tempAddrReg, GtDataType(counterDataType), execMask)
              .SetPredicate(predicate);

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure Procedure::CounterAdd(const IGtKernelInstrument& instrumentor,
                                     GtProfileArray& profileArray, GtReg baseAddrReg,
                                     GtReg tempAddrReg, GtReg dataReg, uint64_t dataOffsetBytes,
                                     GtExecMask execMask, GtReg offsetBytesReg,
                                     GtPredicate predicate) {
  auto dataGedType = Macro::GetGedIntDataTypeBytes(dataReg.ElementSize());
  PTI_ASSERT(instrumentor.Coder().InstructionFactory().CanAccessAtomically(dataGedType) &&
             "Atomic accesses with such size is not available on this HW");

  PTI_ASSERT(dataReg.SubRegNum() == 0 &&
             "Data register should have sub-register number == 0. Try to use "
             "MakeMsgDataScratch VregFactory interface to allocate data register.");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();

  GtGenProcedure proc;

  profileArray.ComputeRelAddress(coder, proc, tempAddrReg, baseAddrReg, dataOffsetBytes);
  if (offsetBytesReg != NullReg()) {
    coder.ComputeRelAddress(proc, tempAddrReg, tempAddrReg, GtReg(offsetBytesReg, 4, 0), predicate);
  }
  proc += insF.MakeAtomicAdd(NullReg(), tempAddrReg, dataReg, GtDataType(dataGedType), execMask)
              .SetPredicate(predicate);

  ANNOTATION(proc)
  return proc;
}

// baseAddrReg register with already calculated value of base addr
GtGenProcedure Procedure::AtomicStore(const IGtKernelInstrument& instrumentor,
                                      GtProfileArray& profileArray, GtReg baseAddrReg,
                                      GtReg tempAddrReg, GtReg dataReg, uint64_t dataOffsetBytes,
                                      GtExecMask execMask, GtReg offsetBytesReg,
                                      GtPredicate predicate) {
  const IGtGenCoder& coder = instrumentor.Coder();

  GtGenProcedure proc;

  profileArray.ComputeRelAddress(coder, proc, tempAddrReg, baseAddrReg, dataOffsetBytes);
  if (offsetBytesReg != NullReg()) {
    coder.ComputeRelAddress(proc, tempAddrReg, tempAddrReg, GtReg(offsetBytesReg, 4, 0), predicate);
  }
  proc += Macro::AtomicStore(instrumentor, tempAddrReg, dataReg, execMask, predicate);

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure Procedure::ComputeSimdMask(const IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                                          bool maskCtrl, uint32_t execMask, GtPredicate pred) {
  GtGenProcedure proc;

  instrumentor.Coder().ComputeSimdMask(proc, simdMaskReg, maskCtrl, execMask, pred);

  ANNOTATION(proc)
  return proc;
}
GtGenProcedure Procedure::ComputeSimdMask(const IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                                          const IGtIns& gtpinIns) {
  return Procedure::ComputeSimdMask(instrumentor, simdMaskReg, !gtpinIns.IsWriteMaskEnabled(),
                                    gtpinIns.ExecMask().Bits(), gtpinIns.Predicate());
}

GtGenProcedure Procedure::IsCacheLineAligned(const IGtKernelInstrument& instrumentor,
                                             GtReg addrRegCheckReg, size_t channelOffset,
                                             GtReg simdMaskReg, GtReg tempData1Reg,
                                             GtReg tempData2Reg, size_t mathWidthBytes) {
  PTI_ASSERT(tempData1Reg.IsValid() && (tempData1Reg.ElementSize() >= mathWidthBytes) &&
             "Wrong register size");
  PTI_ASSERT(tempData2Reg.IsValid() && (tempData2Reg.ElementSize() >= mathWidthBytes) &&
             "Wrong register size");

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();
  const GED_DATA_TYPE SIMD_DATA_TYPE = Macro::GetGedIntDataTypeBytes(MAX_SIMD_WIDTH_BYTES);
  const GED_DATA_TYPE MATH_DATA_TYPE = Macro::GetGedIntDataTypeBytes(mathWidthBytes);

  GtReg temp1Reg = {tempData1Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg temp2Reg = {tempData2Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg firstAddrReg = {addrRegCheckReg,
                        static_cast<uint32_t>(mathWidthBytes), /*Lower bits of first address*/
                        0 /*Sub reg num always == 0 for address register*/};

  GtGenProcedure proc;

  proc += insF.MakeMov(tempData1Reg,
                       GtImm(0, Macro::GetGedIntDataTypeBytes(tempData1Reg.ElementSize())),
                       EXEC_MASK_1_0);

  /// Next code computes alignment of address.
  /// temp1Reg == 0 in case of not aligned, temp1Reg == 0x1 in case of aligned
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

GtGenProcedure Procedure::IsCacheLineAligned(const IGtKernelInstrument& instrumentor,
                                             const IGtIns& gtpinIns, GtReg simdMaskReg,
                                             GtReg tempData1Reg, GtReg tempData2Reg,
                                             size_t mathWidthBytes) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, mathWidthBytes);

  return Procedure::IsCacheLineAligned(instrumentor, addrRegCheckReg, msg.ChannelOffset(),
                                       simdMaskReg, tempData1Reg, tempData2Reg, mathWidthBytes);
}

GtGenProcedure Procedure::IsCacheLineAlignedFlag(const IGtKernelInstrument& instrumentor,
                                                 GtReg addrRegCheckReg, size_t channelOffset,
                                                 GtReg simdMaskReg, GtReg tempData1Reg,
                                                 GtReg flagReg) {
  PTI_ASSERT(addrRegCheckReg.IsValid() && addrRegCheckReg.ElementSize() > 0 && "Invalid register");
  PTI_ASSERT(simdMaskReg.IsValid() && simdMaskReg.ElementSize() > 0 && "Invalid register");
  PTI_ASSERT(tempData1Reg.IsValid() && tempData1Reg.ElementSize() > 0 && "Invalid register");
  PTI_ASSERT(flagReg.IsFlagReg() && "Resister is not a flag register");

  size_t mathWidthBytes = DEFAULT_MATH_WIDTH_BYTES;
  mathWidthBytes = (addrRegCheckReg.ElementSize() >= mathWidthBytes)
                       ? mathWidthBytes
                       : addrRegCheckReg.ElementSize();
  mathWidthBytes =
      (simdMaskReg.ElementSize() >= mathWidthBytes) ? mathWidthBytes : simdMaskReg.ElementSize();
  mathWidthBytes =
      (tempData1Reg.ElementSize() >= mathWidthBytes) ? mathWidthBytes : tempData1Reg.ElementSize();

  IGtInsFactory& insF = instrumentor.Coder().InstructionFactory();
  const GED_DATA_TYPE MATH_DATA_TYPE = Macro::GetGedIntDataTypeBytes(mathWidthBytes);

  GtReg tempRegCut = {tempData1Reg, static_cast<uint32_t>(mathWidthBytes), 0};
  GtReg firstAddrRegCut = {addrRegCheckReg,
                           static_cast<uint32_t>(mathWidthBytes), /*Lower bits of first address*/
                           0 /*Sub reg num always == 0 for address register*/};
  GtReg simdRegCut = {simdMaskReg, static_cast<uint32_t>(mathWidthBytes), 0};

  GtGenProcedure proc;

  proc += Macro::Mov(instrumentor, tempData1Reg, GtImm(0));

  /// flagReg == 1 in case of not aligned, flagReg == 0 in case of aligned
  proc += Macro::And(instrumentor, tempRegCut, firstAddrRegCut, GtImm(0b111111));
  proc += insF.MakeCmp(GED_COND_MODIFIER_z, flagReg, tempRegCut, GtImm(0));

  /// Check channel active for M0, M16, etc.
  proc += Macro::Shr(instrumentor, tempRegCut, simdRegCut, GtImm(channelOffset));
  proc += Macro::And(instrumentor, tempRegCut, tempRegCut, GtImm(0x1));

  /// Set 0 if not aligned, 1 if channel 0 is active and aligned
  proc += Macro::Mov(instrumentor, tempData1Reg,
                     GtImm(0, Macro::GetGedIntDataTypeBytes(tempData1Reg.ElementSize())),
                     EXEC_MASK_1_0, !GtPredicate(flagReg));

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure Procedure::IsCacheLineAlignedFlag(const IGtKernelInstrument& instrumentor,
                                                 const IGtIns& gtpinIns, GtReg simdMaskReg,
                                                 GtReg tempData1Reg, GtReg flagReg) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, msg.ElementSize());

  return Procedure::IsCacheLineAlignedFlag(instrumentor, addrRegCheckReg, msg.ChannelOffset(),
                                           simdMaskReg, tempData1Reg, flagReg);
}

GtGenProcedure Procedure::CalcBaseAddr(const IGtKernelInstrument& instrumentor,
                                       GtProfileArray& profileArray, GtReg baseAddrReg,
                                       size_t recordIndex, size_t numTiles) {
  const IGtGenCoder& coder = instrumentor.Coder();

  GtGenProcedure proc;

  if (numTiles == 1) {
    profileArray.ComputeAddress(coder, proc, baseAddrReg, recordIndex);
  } else {
    IGtVregFactory& vregs = coder.VregFactory();
    GtReg tileIdReg = {baseAddrReg, sizeof(uint32_t), 0};
    GtReg addrOffsetReg = vregs.MakeScratch(VREG_TYPE_DWORD);

    coder.LoadTileId(proc, tileIdReg);

    proc +=
        Macro::Mov(instrumentor, addrOffsetReg, GtImm((recordIndex * numTiles), GED_DATA_TYPE_ud));
    proc += Macro::Add(instrumentor, addrOffsetReg, addrOffsetReg, tileIdReg);
    proc += Macro::Mul(instrumentor, addrOffsetReg, addrOffsetReg,
                       GtImm(profileArray.RecordSize(), GED_DATA_TYPE_ud));

    profileArray.ComputeAddress(coder, proc, baseAddrReg, addrOffsetReg);
  }

  ANNOTATION(proc)
  return proc;
}

GtGenProcedure Procedure::AdjustDistributionWithinBounds(const IGtKernelInstrument& instrumentor,
                                                         GtReg valueReg, size_t bucketsNum,
                                                         GtReg flagReg) {
  GED_DATA_TYPE signedValueType = Macro::GetGedIntDataTypeBytesSigned(valueReg.ElementSize());
  GtPredicate predicate = GtPredicate(flagReg);

  GtGenProcedure proc;
  // Check that the valueReg lover than 0 -> place -2 in it (which will be 0 at the end)
  proc += Macro::Cmp(
      instrumentor, GED_COND_MODIFIER_l, flagReg,
      GtRegRegion(
          valueReg, ScalarStride(),
          signedValueType),  // Should be signed region. Otherwise comparison will be unsigned
      GtImm(0, GED_DATA_TYPE_d));
  proc += Macro::Mov(instrumentor, GtReg(valueReg), GtImm(-2, signedValueType), EXEC_MASK_1_0,
                     predicate);

  // check that the valueReg higher than bucketsNum -> place -1 in it (which will be 1 at the end)
  uint64_t mask = Macro::GetMaskBySizeBytes(4);
  PTI_ASSERT(bucketsNum <= mask && "Buckets number should be less than dword");
  proc += Macro::Cmp(
      instrumentor, GED_COND_MODIFIER_ge, flagReg,
      GtRegRegion(
          valueReg, ScalarStride(),
          signedValueType),  // Should be signed region. Otherwise comparison will be unsigned
      GtImm(bucketsNum, GED_DATA_TYPE_d));
  proc += Macro::Mov(instrumentor, valueReg, GtImm(-1, signedValueType), EXEC_MASK_1_0, predicate);

  // increment valueReg by 2 without condition modifiers
  proc += Macro::Add(instrumentor, valueReg, valueReg, GtImm(2, signedValueType), EXEC_MASK_1_0);

  ANNOTATION(proc)
  return proc;
}

/// This instrumentation makes register allocation. it allocs (1 + execSize) *
/// addrWidthBytes Uses address == -1 as no address marker. Result, number of
/// cache lines, is stored in clCounterReg
#define INVALID_ADDRESS -1
GtGenProcedure Procedure::CacheLinesCount(const IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                                          GtReg simdMaskReg, uint32_t execSize,
                                          GtReg addrRegCheckReg, size_t channelOffset,
                                          const uint32_t addrWidthBytes, GtReg flagReg) {
  /// check registers for minimal requirements
  /// Check that clCounter reg is at least DWORD

  const IGtGenCoder& coder = instrumentor.Coder();
  IGtInsFactory& insF = coder.InstructionFactory();
  uint32_t grfRegSize = insF.GenModel().GrfRegSize();  // bytes
  GtPredicate predicate(flagReg);

  GtRegNum src0RegNum = addrRegCheckReg.RegNum();

  GtGenProcedure proc;

  IGtVregFactory& vregs = coder.VregFactory();

  /// Datatypes, sizes
  const GED_DATA_TYPE addrGedType = Macro::GetGedIntDataTypeBytesSigned(addrWidthBytes);
  const VREG_TYPE addrVregType = Macro::GetVregTypeBytes(addrWidthBytes);

  const uint32_t localCounterWidthBytes = MAX_SIMD_WIDTH_BYTES;  // it is used in SIMD mask calc
  const GED_DATA_TYPE localCounterGedType =
      Macro::GetGedIntDataTypeBytesSigned(localCounterWidthBytes);
  const VREG_TYPE localCounterVregType = Macro::GetVregTypeBytes(localCounterWidthBytes);

  /// allocate num of cl ids, counter, temp
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
  /// TODO: do it only in case of size of clCounterReg32 more than 32bits
  GtReg clCounterReg32 = {clCounterReg, sizeof(uint32_t), 0};

  /// fill Cacheline storage with unique cache lines, calc unique CL number in
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
    proc += insF.MakeAdd(clCounterReg32, clCounterReg32, 1).SetPredicate(predicate);

    // remove cache line from storage if exists or SIMD lane not active
    proc += insF.MakeMov(clIdTemp, GtImmS32(-1))
                .SetPredicate(!predicate);  // remove cache line from storage if
                                            // exists or SIMD lane not active
  }

  ANNOTATION(proc)
  return proc;
}
#undef INVALID_ADDRESS

GtGenProcedure Procedure::CacheLinesCount(const IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                                          GtReg simdMaskReg, const IGtIns& gtpinIns,
                                          GtReg flagReg) {
  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, msg.AddrSize());

  return Procedure::CacheLinesCount(instrumentor, clCounterReg, simdMaskReg, msg.ExecSize(),
                                    addrRegCheckReg, msg.ChannelOffset(), msg.AddrSize(), flagReg);
}

/**
 * Analysis
 */
void Analysis::InstructionCounter(Capsule& capsule, uint64_t dataOffsetBytes) {
  GtGenProcedure proc;
  proc +=
      Procedure::CounterInc(capsule.GetInstrumentor(), capsule.GetProfileArray(),
                            capsule.GetBaseAddrReg(), capsule.GetTempAddrReg(), dataOffsetBytes,
                            Macro::GetCounterDataType(capsule.GetInstrumentor()), EXEC_MASK_1_0);
  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}
void Analysis::SimdActiveCounter(Capsule& capsule, const IGtIns& gtpinIns,
                                 uint64_t dataOffsetBytes) {
  auto dataReg = capsule.GetMsgData64Reg();

  GtGenProcedure proc;
  proc += Macro::Cbit(capsule.GetInstrumentor(), dataReg, capsule.GetSimdMaskReg(gtpinIns));
  proc += Procedure::CounterAdd(capsule.GetInstrumentor(), capsule.GetProfileArray(),
                                capsule.GetBaseAddrReg(), capsule.GetTempAddrReg(), dataReg,
                                dataOffsetBytes);
  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}
void Analysis::CacheLineAlignedCounter(Capsule& capsule, const IGtIns& gtpinIns,
                                       uint64_t dataOffsetBytes) {
  auto& instrumentor = capsule.GetInstrumentor();
  size_t mathWidthBytes = 2;

  auto tempReg = capsule.GetMsgData64Reg();
  /// clear register
  GtReg clearReg = {tempReg, static_cast<uint32_t>(Macro::GetCounterSizeBytes(instrumentor)), 0};

  GtGenProcedure proc;
  proc += capsule.GetInsFactory().MakeRegZero(clearReg);
  GtReg flagReg = FlagReg(0);

  GtReg tempData1Reg = {tempReg, static_cast<uint32_t>(mathWidthBytes), 0};

  proc += Procedure::IsCacheLineAlignedFlag(
      instrumentor, gtpinIns, capsule.GetSimdMaskReg(gtpinIns), tempData1Reg, flagReg);

  proc += capsule.GetInsFactory().MakeCmp(GED_COND_MODIFIER_z, flagReg, tempData1Reg,
                                          GtImm(0x0, Macro::GetGedIntDataTypeBytes(mathWidthBytes)),
                                          EXEC_MASK_1_0);

  proc += Procedure::CounterInc(capsule.GetInstrumentor(), capsule.GetProfileArray(),
                                capsule.GetBaseAddrReg(), capsule.GetTempAddrReg(), dataOffsetBytes,
                                Macro::GetCounterDataType(capsule.GetInstrumentor()), EXEC_MASK_1_0,
                                NullReg(), flagReg);
  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}

void Analysis::CacheLineCounter(Capsule& capsule, const IGtIns& gtpinIns,
                                uint64_t dataOffsetBytes) {
  GtReg clCounterReg = capsule.GetMsgData64Reg();
  GtGenProcedure proc;
  proc += Procedure::CacheLinesCount(capsule.GetInstrumentor(), clCounterReg,
                                     capsule.GetSimdMaskReg(gtpinIns), gtpinIns);
  proc += Procedure::CounterAdd(capsule.GetInstrumentor(), capsule.GetProfileArray(),
                                capsule.GetBaseAddrReg(), capsule.GetTempAddrReg(), clCounterReg,
                                dataOffsetBytes);
  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}

void Analysis::StrideDistrCalc(Capsule& capsule, const IGtIns& gtpinIns, int32_t strideMin,
                               int32_t strideNum, int32_t strideStep, uint64_t dataOffsetBytes,
                               size_t counterSizeBytes) {
  DcSendMsg msg(gtpinIns.GetGedIns());

  const size_t execSize = msg.ExecSize();
  const GtRegNum src0RegNum = msg.Src0();
  const size_t addrWidth = msg.AddrSize();                                    // bytes
  const size_t grfRegSize = capsule.GetInsFactory().GenModel().GrfRegSize();  // bytes

  GtReg strideReg = {capsule.GetTempData64Reg(), (uint32_t)addrWidth, 0};

  GtGenProcedure proc;

  for (size_t i = 1; i < execSize; i++) {
    GtReg prevAddrReg = Macro::GetSubReg(src0RegNum, i - 1, addrWidth, grfRegSize);
    GtReg currAddrReg = Macro::GetSubReg(src0RegNum, i, addrWidth, grfRegSize);

    /// Calc stride and address offset. strideReg = real stride between two addresses
    proc +=
        Macro::Sub(capsule.GetInstrumentor(), strideReg, currAddrReg, prevAddrReg, EXEC_MASK_1_0);

    proc +=
        Macro::Sub(capsule.GetInstrumentor(), strideReg, strideReg,
                   GtImm(strideMin, Macro::GetGedIntDataTypeBytesSigned(strideReg.ElementSize())),
                   EXEC_MASK_1_0);  // shift min stride to 0

    /// stride is stepped by m_strideStep
    uint32_t logStep = static_cast<uint32_t>(std::log2(strideStep));
    proc += Macro::Shr(capsule.GetInstrumentor(), strideReg, strideReg, logStep);

    proc += Procedure::AdjustDistributionWithinBounds(capsule.GetInstrumentor(), strideReg,
                                                      strideNum, FlagReg(0));

    // Address calculation
    proc += Macro::Mul(capsule.GetInstrumentor(), strideReg, strideReg, counterSizeBytes);

    proc += Procedure::CounterInc(
        capsule.GetInstrumentor(), capsule.GetProfileArray(), capsule.GetBaseAddrReg(),
        capsule.GetTempAddrReg(), dataOffsetBytes,
        Macro::GetCounterDataType(capsule.GetInstrumentor()), EXEC_MASK_1_0, strideReg);
  }

  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}

void Analysis::DumpFirstAddresses(Capsule& capsule, const IGtIns& gtpinIns,
                                  uint64_t addrArrayOffsetBytes, uint64_t alreadyDumpedFlag) {
  // TODO: dump only first SIMD access
  // TODO: size of dumped addr should be passed into capsule (addrArrayOffsetBytes + 8 * i)

  GtReg data64Reg = capsule.GetMsgData64Reg();

  DcSendMsg msg(gtpinIns.GetGedIns());
  GtReg addrRegCheckReg = GrfReg(msg.Src0(), 0, msg.AddrSize());
  uint32_t grfRegSize = capsule.GetInsFactory().GenModel().GrfRegSize();  // bytes

  GtGenProcedure proc;

  GtRegNum src0RegNum = addrRegCheckReg.RegNum();
  for (size_t i = 0; i < msg.SimdWidth(); i++) {
    GtReg currAddrReg = Macro::GetSubReg(src0RegNum, i, msg.AddrSize(), grfRegSize);

    proc += Macro::Mov(capsule.GetInstrumentor(), data64Reg, currAddrReg);
    proc += Procedure::AtomicStore(capsule.GetInstrumentor(), capsule.GetProfileArray(),
                                   capsule.GetBaseAddrReg(), capsule.GetTempAddrReg(), data64Reg,
                                   addrArrayOffsetBytes + 8 * i);
  }

  ANNOTATION(proc)
  capsule.AppendProcedure(proc);
}

/**
 * Capsule class
 */
Capsule::Capsule(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                 size_t recordIndex)
    : m_instrumentor(instrumentor),
      m_profileArray(profileArray),
      m_recordIndex(recordIndex),
      m_vregs(m_instrumentor.Coder().VregFactory()),
      m_insF(m_instrumentor.Coder().InstructionFactory()) {
  GetNumTiles();
}

void Capsule::AppendProcedure(GtGenProcedure& proc) { m_proc += proc; }

GtGenProcedure Capsule::GetProcedure() const { return m_proc; }

/// Getters
size_t Capsule::GetNumTiles() {
  if (m_numTiles == INVALID_NUM) {
    m_numTiles = static_cast<size_t>(Macro::GetNumTiles(m_instrumentor));
  }
  return m_numTiles;
}
GtReg Capsule::GetBaseAddrReg() {
  if (!m_baseAddrReg.IsValid()) {
    m_baseAddrReg = m_vregs.MakeMsgAddrScratch();
    m_proc += Procedure::CalcBaseAddr(m_instrumentor, m_profileArray, m_baseAddrReg, m_recordIndex,
                                      GetNumTiles());
  }
  return m_baseAddrReg;
}
GtReg Capsule::GetSimdMaskReg(bool maskCtrl, uint32_t execMask, GtPredicate pred) {
  if (!m_simdMaskReg.IsValid()) {
    m_simdMaskReg = m_vregs.MakeScratch(GtVregType::MakeBySize(MAX_SIMD_WIDTH_BYTES));
    m_proc += Procedure::ComputeSimdMask(m_instrumentor, m_simdMaskReg, maskCtrl, execMask, pred);
  }
  return m_simdMaskReg;
}
GtReg Capsule::GetSimdMaskReg(const IGtIns& gtpinIns) {
  return GetSimdMaskReg(!gtpinIns.IsWriteMaskEnabled(), gtpinIns.ExecMask().Bits(),
                        gtpinIns.Predicate());
}
GtReg Capsule::GetTempAddrReg() {
  if (!m_tempAddrReg.IsValid()) {
    m_tempAddrReg = m_vregs.MakeMsgAddrScratch();
  }
  return m_tempAddrReg;
}
GtReg Capsule::GetTempData64Reg() {
  if (!m_tempData64Reg.IsValid()) {
    m_tempData64Reg = m_vregs.MakeScratch(VREG_TYPE_QWORD);
  }
  return m_tempData64Reg;
}
GtReg Capsule::GetMsgData64Reg() {
  if (!m_tempMsgData64Reg.IsValid()) {
    m_tempMsgData64Reg = m_vregs.MakeMsgDataScratch(VREG_TYPE_QWORD);
  }
  return m_tempMsgData64Reg;
}

/// Setters
void Capsule::SetTempAddrReg(GtReg tempAddrReg) { m_tempAddrReg = tempAddrReg; }
void Capsule::SetTempData64Reg(GtReg tempData64Reg) { m_tempData64Reg = tempData64Reg; }
