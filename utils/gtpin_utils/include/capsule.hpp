//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_CAPSULE_H
#define PTI_GTPIN_CAPSULE_H

#define ANNOTATION(proc) \
  for (const auto& ins : proc) ins->AppendAnnotation(__FUNCTION__);
#define EXEC_MASK_1_0 gtpin::GtExecMask(gtpin::GtExecSize(1), gtpin::GtChannelOffset(0))
#define INVALID_NUM static_cast<size_t>(-1)
#define DEFAULT_MATH_WIDTH_BYTES 4

#include <api/gtpin_api.h>

#include <type_traits>

#include "gen_send_decoder.h"

/**
 * @file capsule.hpp
 * @brief This file contains the declaration of the Capsule class and related functions.
 *
 * The Capsule class is responsible for encapsulating GTPin instrumentation procedures and providing
 * methods for manipulating and accessing the procedures. It also provides methods for analyzing
 * instructions and performing various counters and calculations related to GTPin instrumentation.
 *
 * The file also includes several utility macros and functions for working with GTPin.
 *
 * This file is part of the GTPin Utils library.
 */

using gtpin::FlagReg;
using gtpin::GtCondModifier;
using gtpin::GtDataType;
using gtpin::GtDstRegion;
using gtpin::GtExecMask;
using gtpin::GtGenProcedure;
using gtpin::GtImm;
using gtpin::GtPredicate;
using gtpin::GtProfileArray;
using gtpin::GtReg;
using gtpin::GtRegRegion;
using gtpin::GtVregType;
using gtpin::IGtIns;
using gtpin::IGtInsFactory;
using gtpin::IGtKernelInstrument;
using gtpin::IGtVregFactory;
using gtpin::NullReg;

namespace gtpin_prof {

namespace Macro {
/// Get HW & instrumentation info
bool Is64BitCountersSupport(const IGtKernelInstrument& instrumentor);
size_t GetCounterSizeBytes(const IGtKernelInstrument& instrumentor);
GED_DATA_TYPE GetCounterDataType(const IGtKernelInstrument& instrumentor);
GED_DATA_TYPE GetCounterDataTypeSigned(const IGtKernelInstrument& instrumentor);
GtVregType GetCounterVregType(const IGtKernelInstrument& instrumentor);
uint32_t GetNumTiles(const IGtKernelInstrument& instrumentor);
GtVregType GetVregType(size_t sizeBits);
GtVregType GetVregTypeBytes(size_t sizeBytes);
GED_DATA_TYPE GetGedIntDataType(size_t sizeBits);
GED_DATA_TYPE GetGedIntDataTypeBytes(size_t sizeBytes);
GED_DATA_TYPE GetGedIntDataTypeSigned(size_t sizeBits);
GED_DATA_TYPE GetGedIntDataTypeBytesSigned(size_t sizeBytes);
uint64_t GetMaskBySize(size_t sizeBits);
uint64_t GetMaskBySizeBytes(size_t sizeBytes);
GtReg GetSubReg(const IGtKernelInstrument& instrumentor, size_t regNum, size_t subRegIdx,
                size_t subRegSizeBytes);
GtReg GetSubReg(size_t regNum, size_t subRegIdx, size_t subRegSizeBytes, size_t grfRegSize);

GtGenProcedure Mov(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Mov(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst, const GtImm& srcI,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Not(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Not(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst, const GtImm& srcI,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Cbit(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                    const GtRegRegion& src, GtExecMask execMask = EXEC_MASK_1_0,
                    GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Cbit(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst, const GtImm& srcI,
                    GtExecMask execMask = EXEC_MASK_1_0,
                    GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Add(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Addc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                    const GtRegRegion& src0, const GtRegRegion& src1,
                    GtExecMask execMask = EXEC_MASK_1_0,
                    GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Addc(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                    const GtRegRegion& src0, const GtImm& srcI1,
                    GtExecMask execMask = EXEC_MASK_1_0,
                    GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure And(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure And(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Or(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                  const GtRegRegion& src0, const GtRegRegion& src1,
                  GtExecMask execMask = EXEC_MASK_1_0,
                  GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Or(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                  const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                  GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Xor(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Xor(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Shr(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Shl(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Mul(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Mul(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Sel(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Sel(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Sub(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Sub(const IGtKernelInstrument& instrumentor, const GtDstRegion& dst,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure Cmp(const IGtKernelInstrument& instrumentor, GtCondModifier cond, GtReg flagReg,
                   const GtRegRegion& src0, const GtRegRegion& src1,
                   GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());
GtGenProcedure Cmp(const IGtKernelInstrument& instrumentor, GtCondModifier cond, GtReg flagReg,
                   const GtRegRegion& src0, const GtImm& srcI1, GtExecMask execMask = EXEC_MASK_1_0,
                   GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure AtomicStore(const IGtKernelInstrument& instrumentor, GtReg addrReg, GtReg dataReg,
                           GtExecMask execMask = EXEC_MASK_1_0,
                           GtPredicate predicate = GtPredicate::MakeNone());

};  // namespace Macro

namespace Procedure {
GtGenProcedure CounterInc(const IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                          GtReg baseAddrReg, GtReg tempAddrReg, uint64_t dataOffsetBytes,
                          GED_DATA_TYPE counterDataType = GED_DATA_TYPE::GED_DATA_TYPE_uq,
                          GtExecMask execMask = {1}, GtReg offsetBytesReg = NullReg(),
                          GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure CounterAdd(const IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                          GtReg baseAddrReg, GtReg tempAddrReg, GtReg dataReg,
                          uint64_t dataOffsetBytes, GtExecMask execMask = {1},
                          GtReg offsetBytesReg = NullReg(),
                          GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure AtomicStore(const IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                           GtReg baseAddrReg, GtReg tempAddrReg, GtReg dataReg,
                           uint64_t dataOffsetBytes, GtExecMask execMask = {1},
                           GtReg offsetBytesReg = NullReg(),
                           GtPredicate predicate = GtPredicate::MakeNone());

GtGenProcedure ComputeSimdMask(const IGtKernelInstrument& instrumentor, GtReg simdMaskReg, bool maskCtrl,
                               uint32_t execMask, GtPredicate pred);
GtGenProcedure ComputeSimdMask(const IGtKernelInstrument& instrumentor, GtReg simdMaskReg,
                               const IGtIns& gtpinIns);
/**
 * Calculation goes in 16 bits due to no need in all 64 bits. The only need in
 * lower 6 bits Result in tempData1Reg reg. 0 - for not CL aligned, !=0 for CL
 * aligned and channel active
 */
GtGenProcedure IsCacheLineAligned(const IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                  size_t channelOffset, GtReg simdMaskReg, GtReg tempData1Reg,
                                  GtReg tempData2Reg, size_t mathWidthBytes = 2);
GtGenProcedure IsCacheLineAligned(const IGtKernelInstrument& instrumentor, const IGtIns& gtpinIns,
                                  GtReg simdMaskReg, GtReg tempData1Reg, GtReg tempData2Reg,
                                  size_t mathWidthBytes = 2);
/**
 * Faster, but requires flag register. Result in tempData1Reg.
 * tempData1Reg = 1 in case of active and aligned, 0 otherwise.
 * Checks that SIMD lane 0 is active and cache line is aligned (lower 6 bits of address are 0).
 */
GtGenProcedure IsCacheLineAlignedFlag(const IGtKernelInstrument& instrumentor, GtReg addrRegCheckReg,
                                      size_t channelOffset, GtReg simdMaskReg, GtReg tempData1Reg,
                                      GtReg flagReg = FlagReg(0));

GtGenProcedure IsCacheLineAlignedFlag(const IGtKernelInstrument& instrumentor, const IGtIns& gtpinIns,
                                      GtReg simdMaskReg, GtReg tempData1Reg,
                                      GtReg flagReg = FlagReg(0));

GtGenProcedure CalcBaseAddr(const IGtKernelInstrument& instrumentor, GtProfileArray& profileArray,
                            GtReg baseAddrReg, size_t recordIndex, size_t numTiles);

GtGenProcedure AdjustDistributionWithinBounds(const IGtKernelInstrument& instrumentor, GtReg valueReg,
                                              size_t bucketsNum, GtReg flagReg = FlagReg(0));

GtGenProcedure CacheLinesCount(const IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                               GtReg simdMaskReg, uint32_t execSize, GtReg addrRegCheckReg,
                               size_t channelOffset, const uint32_t addrWidthBytes,
                               GtReg flagReg = FlagReg(0));
GtGenProcedure CacheLinesCount(const IGtKernelInstrument& instrumentor, GtReg clCounterReg,
                               GtReg simdMaskReg, const IGtIns& gtpinIns,
                               GtReg flagReg = FlagReg(0));
};  // namespace Procedure

class Capsule;

namespace Analysis {
/// Analyses
void InstructionCounter(Capsule& capsule, uint64_t dataOffsetBytes = 0);
void SimdActiveCounter(Capsule& capsule, const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);
void CacheLineAlignedCounter(Capsule& capsule, const IGtIns& gtpinIns,
                             uint64_t dataOffsetBytes = 0);
void CacheLineCounter(Capsule& capsule, const IGtIns& gtpinIns, uint64_t dataOffsetBytes = 0);
void StrideDistrCalc(Capsule& capsule, const IGtIns& gtpinIns, int32_t strideMin, int32_t strideNum,
                     int32_t strideStep, uint64_t dataOffsetBytes = 0, size_t counterSizeBytes = 8);
void DumpFirstAddresses(Capsule& capsule, const IGtIns& gtpinIns, uint64_t addrArrayOffsetBytes = 0,
                        uint64_t alreadyDumpedFlag = 0);
};  // namespace Analysis

class Capsule {
 public:
  Capsule(IGtKernelInstrument& instrumentor, GtProfileArray& profileArray, size_t recordIndex);
  ~Capsule() = default;

  void AppendProcedure(GtGenProcedure& proc);
  /// TODO: operator +
  GtGenProcedure GetProcedure() const;

  /// Getters
  size_t GetNumTiles();
  GtReg GetBaseAddrReg();
  GtReg GetSimdMaskReg(const IGtIns& gtpinIns);
  GtReg GetSimdMaskReg(bool maskCtrl, uint32_t execMask, GtPredicate pred);
  GtReg GetTempAddrReg();
  GtReg GetTempData64Reg();
  GtReg GetMsgData64Reg();
  IGtKernelInstrument& GetInstrumentor() const { return m_instrumentor; }
  GtProfileArray& GetProfileArray() const { return m_profileArray; }
  IGtVregFactory& GetVregFactory() const { return m_vregs; }
  IGtInsFactory& GetInsFactory() const { return m_insF; }
  /// Setters
  void SetTempAddrReg(GtReg tempAddrReg);
  void SetTempData64Reg(GtReg tempAddrReg);

 private:
  IGtKernelInstrument& m_instrumentor;
  GtProfileArray& m_profileArray;
  IGtVregFactory& m_vregs;
  IGtInsFactory& m_insF;

  size_t m_recordIndex = INVALID_NUM;

  size_t m_numTiles = INVALID_NUM;
  GtReg m_baseAddrReg = GtReg();
  GtReg m_simdMaskReg = GtReg();
  GtReg m_tempAddrReg = GtReg();
  GtReg m_tempData64Reg = GtReg();
  GtReg m_tempMsgData64Reg = GtReg();

  GtGenProcedure m_proc;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_CAPSULE_H
