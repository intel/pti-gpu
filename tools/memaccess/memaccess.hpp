//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_MEMACCESS
#define PTI_TOOLS_MEMACCESS

#define STRIDE_MIN_DEFAULT -32
#define STRIDE_NUM_DEFAULT 1024
#define STRIDE_STEP_DEFAULT 1  // [-1024; 3072] with step of 4 bytes

#include <map>
#include <memory>

#include "gen_send_decoder.h"
#include "profiler.hpp"

namespace gtpin_prof {

/**
 * Structure representing a raw record for memory access analysis.
 * It extends the base RawRecord structure.
 * Contains counters for the number of instructions, active SIMD lanes, cache lines counter, and
 * other
 */
struct MemAccessRawRecord : public RawRecord {
  uint64_t memAccessCounter = 0;
  uint64_t simdLanesActiveCounter = 0;
  uint64_t cacheLinesCounter = 0;
  uint64_t clNotAlignedCounter = 0;  // cache line not aligned counter
  uint64_t
      addresses[GTPIN_UTILS_MAX_SIMD_WIDTH];  ///>! List of first addresses as an example of access

  // size of distribution counters should be power of 2
  // Stride distribution array with flexible size
  // Distribution data format should be followed:
  uint64_t strideOverflowLowerCounter = 0;
  uint64_t strideOverflowHigherCounter = 0;
  uint64_t strideDistr[];  // flexible size
};

/**
 * Class representing the MemAccessResultDataCommon.
 * It extends the ResultDataCommon class.
 * Provides additional data specific to the MemAccessGTPinTool.
 * Includes the instruction offset and basic block ID.
 */
class MemAccessResultDataCommon : public ResultDataCommon {
 public:
  /**
   * Constructs an MemAccessResultDataCommon object with the given offset and basic block ID.
   * @param offset The instruction offset.
   */
  MemAccessResultDataCommon(InstructionOffset offset, const gtpin::IGtIns& instruction,
                            const int32_t strideMin, const int32_t strideNum,
                            const int32_t strideStep)
      : offset(offset), strideMin(strideMin), strideNum(strideNum), strideStep(strideStep) {
    DcSendMsg dcSendMsg(instruction.GetGedIns());
    auto elementSize_nc = const_cast<uint32_t*>(&elementSize);
    *elementSize_nc = dcSendMsg.ElementSize();
    auto numOfElements_nc = const_cast<uint32_t*>(&numOfElements);
    *numOfElements_nc = dcSendMsg.NumElements();
    auto isWrite_nc = const_cast<uint32_t*>(&isWrite);
    *isWrite_nc = dcSendMsg.IsWrite();
    auto isScatter_nc = const_cast<uint32_t*>(&isScatter);
    *isScatter_nc = dcSendMsg.IsScatter();
    auto isBts_nc = const_cast<uint32_t*>(&isBts);
    *isBts_nc = dcSendMsg.IsBts();
    auto isSlm_nc = const_cast<uint32_t*>(&isSlm);
    *isSlm_nc = dcSendMsg.IsSlm();
    auto isScratch_nc = const_cast<uint32_t*>(&isScratch);
    *isScratch_nc = dcSendMsg.IsScratch();
    auto isAtomic_nc = const_cast<uint32_t*>(&isAtomic);
    *isAtomic_nc = dcSendMsg.IsAtomic();
    auto addrWidth_nc = const_cast<uint32_t*>(&addrWidth);
    *addrWidth_nc = dcSendMsg.AddrSize();
    auto simdWidth_nc = const_cast<uint32_t*>(&simdWidth);
    *simdWidth_nc = dcSendMsg.SimdWidth();
    auto bti_nc = const_cast<uint32_t*>(&bti);
    *bti_nc = dcSendMsg.Bti();
    auto addrPayload_nc = const_cast<uint32_t*>(&addrPayload);
    *addrPayload_nc = dcSendMsg.AddrPayloadLength();
    auto isEot_nc = const_cast<uint32_t*>(&isEot);
    *isEot_nc = dcSendMsg.IsEot();
    auto isMedia_nc = const_cast<uint32_t*>(&isMedia);
    *isMedia_nc = dcSendMsg.IsMedia();
    auto execSize_nc = const_cast<uint32_t*>(&execSize);
    *execSize_nc = dcSendMsg.ExecSize();
    auto channelOffset_nc = const_cast<uint32_t*>(&channelOffset);
    *channelOffset_nc = dcSendMsg.ChannelOffset();
  }

  const InstructionOffset offset;

  const int32_t strideMin;
  const int32_t strideNum;
  const int32_t strideStep;

  const uint32_t elementSize = 0;
  const uint32_t numOfElements = 0;
  const uint32_t isWrite = 0;
  const uint32_t isScatter = 0;
  const uint32_t isBts = 0;
  const uint32_t isSlm = 0;
  const uint32_t isScratch = 0;
  const uint32_t isAtomic = 0;
  const uint32_t addrWidth = 0;
  const uint32_t simdWidth = 0;
  const uint32_t bti = 0;
  const uint32_t addrPayload = 0;
  const uint32_t isEot = 0;
  const uint32_t isMedia = 0;
  const uint32_t execSize = 0;
  const uint32_t channelOffset = 0;
};

/**
 * Class representing the MemAccessSiteOfInstrument.
 * It extends the base SiteOfInstrument class.
 * Contains a reference to the instruction and the type of instrument.
 */
class MemAccessSiteOfInstrument : public SiteOfInstrument {
 public:
  using SiteOfInstrument::SiteOfInstrument;

  /**
   * Constructs an MemAccessSiteOfInstrument object with the given instruction and type.
   * @param ins The reference to the instruction.
   */
  MemAccessSiteOfInstrument(const gtpin::IGtIns& ins,
                            std::shared_ptr<MemAccessResultDataCommon> resultDataCommon)
      : instruction(ins),
        addrPayload(resultDataCommon->addrPayload),
        isScatter(resultDataCommon->isScatter),
        isSlm(resultDataCommon->isSlm),
        strideMin(resultDataCommon->strideMin),
        strideNum(resultDataCommon->strideNum),
        strideStep(resultDataCommon->strideStep) {}

  const gtpin::IGtIns& instruction;
  const uint32_t addrPayload;
  const uint32_t isScatter;
  const uint32_t isSlm;

  const int32_t strideMin;
  const int32_t strideNum;
  const int32_t strideStep;
};

/**
 * Class representing the MemAccessGTPinTool.
 * It extends the GTPinTool class.
 * Provides implementation for analyzing, instrumenting, and accumulating the memory access data.
 */
class MemAccessGTPinTool : public GTPinTool {
 public:
  using GTPinTool::GTPinTool;
  ~MemAccessGTPinTool() override = default;

  /**
   * Returns the name of the MemAccessGTPinTool.
   * @return The name of the tool.
   */
  const char* Name() const final { return "MemAccessProfiler"; };

  /**
   * Analyzes the kernel data
   * @param kernelData The shared pointer to the kernel data.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The status of the analysis.
   */
  PROF_STATUS AnalyzeKernel(std::shared_ptr<KernelData> kernelData,
                            const gtpin::IGtKernelInstrument& instrumentor) final;

  /**
   * Instruments the kernel data based on the results of AnalyzeKernel.
   * @param kernelData The shared pointer to the kernel data.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The status of the instrumentation.
   */
  PROF_STATUS Instrument(std::shared_ptr<KernelData> kernelData,
                         gtpin::IGtKernelInstrument& instrumentor) final;

  /**
   * Accumulates the profiling result
   * @param kernelData The shared pointer to the kernel data.
   * @param profilingResult The shared pointer to the profiling result data.
   * @param siteOfInstrument The shared pointer to the site of instrument.
   * @param record The raw record.
   * @return The status of the accumulation.
   */
  PROF_STATUS Accumulate(std::shared_ptr<KernelData> kernelData,
                         std::shared_ptr<ResultData> profilingResult,
                         std::shared_ptr<SiteOfInstrument> siteOfInstrument,
                         RawRecord* record) final;
};

/**
 * Class representing the MemAccessApplicationData.
 * It extends the ApplicationData class.
 * Provides additional data specific to the MemAccessGTPinTool.
 */
class MemAccessApplicationData : public ApplicationData {
 public:
  using ApplicationData::ApplicationData;
};

/**
 * Class representing the MemAccessKernelData.
 * It extends the KernelData class.
 * Provides additional data specific to the MemAccessGTPinTool.
 */
class MemAccessKernelData : public KernelData {
 public:
  using KernelData::KernelData;
};

/**
 * Class representing the MemAccessInvocationData.
 * It extends the InvocationData class.
 * Provides additional data specific to the MemAccessGTPinTool.
 */
class MemAccessInvocationData : public InvocationData {
 public:
  using InvocationData::InvocationData;
};

/**
 * Class representing the MemAccessResultData.
 * It extends the ResultData class.
 * Provides additional data specific to the MemAccessGTPinTool.
 * Includes counters for the number of active SIMD lanes and the total number of instructions.
 */
class MemAccessResultData : public ResultData {
 public:
  MemAccessResultData(std::shared_ptr<ResultDataCommon> resultDataCommon, size_t tileId)
      : ResultData(resultDataCommon, tileId) {
    auto rdc = std::dynamic_pointer_cast<MemAccessResultDataCommon>(resultDataCommon);
    PTI_ASSERT(rdc != nullptr);
    for (size_t idx = 0; idx < rdc->strideNum; idx++) {
      int64_t stride = rdc->strideMin + idx * rdc->strideStep;
      strideDistributionCounter[stride] = 0;
    }
    addresses.resize(rdc->simdWidth);
    for (auto addr : addresses) addr = 0;
  }

  size_t accessInstructionCounter = 0;
  size_t simdLanesActiveCounter = 0;
  size_t cacheLinesCounter = 0;
  size_t clNotAlignedCounter = 0;
  size_t strideOverflowHigherCounter = 0;
  size_t strideOverflowLowerCounter = 0;
  std::vector<uint64_t> addresses;
  std::map<int64_t, size_t> strideDistributionCounter;
};

using MemAccessApplicationDataSPtr = std::shared_ptr<MemAccessApplicationData>;
using MemAccessKernelDataSPtr = std::shared_ptr<MemAccessKernelData>;
using MemAccessInvocationDataSPtr = std::shared_ptr<MemAccessInvocationData>;
using MemAccessResultDataSPtr = std::shared_ptr<MemAccessResultData>;
using MemAccessSiteOfInstrumentSPtr = std::shared_ptr<MemAccessSiteOfInstrument>;
using MemAccessResultDataCommonSPtr = std::shared_ptr<MemAccessResultDataCommon>;

/**
 * Class representing the MemAccessFactory.
 * It extends the ToolFactory class.
 * Provides implementation for creating instances of the MemAccessGTPinTool and related data
 * structures.
 */
class MemAccessFactory final : public ToolFactory {
  mutable int32_t strideNum = -1;

 public:
  using ToolFactory::ToolFactory;
  ~MemAccessFactory() final = default;

  /**
   * Creates an instance of the MemAccessGTPinTool.
   * @param factory The shared pointer to the tool factory.
   * @param control The shared pointer to the control base.
   * @return The shared pointer to the MemAccessGTPinTool.
   */
  std::shared_ptr<GTPinTool> MakeGTPinTool() const final;

  /**
   * Returns the size of the raw record.
   * @return The size of the raw record.
   */
  uint32_t GetRecordSize() const final {
    return (strideNum == -1) ? -1
                             : (sizeof(MemAccessRawRecord) +
                                strideNum * sizeof(MemAccessRawRecord::strideDistr[0]));
  }

  /**
   * Creates an instance of the MemAccessApplicationData.
   * @return The shared pointer to the MemAccessApplicationData.
   */
  std::shared_ptr<ApplicationData> MakeApplicationData() const final {
    return std::make_shared<MemAccessApplicationData>();
  }

  /**
   * Creates an instance of the MemAccessKernelData.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The shared pointer to the MemAccessKernelData.
   */
  std::shared_ptr<KernelData> MakeKernelData(
      const gtpin::IGtKernelInstrument& instrumentor) const final {
    return std::make_shared<MemAccessKernelData>(instrumentor);
  }

  /**
   * Creates an instance of the MemAccessInvocationData.
   * @param execDescr The kernel execution descriptor.
   * @return The shared pointer to the MemAccessInvocationData.
   */
  std::shared_ptr<InvocationData> MakeInvocationData(
      const KernelExecDescriptor& execDescr) const final {
    return std::make_shared<MemAccessInvocationData>(execDescr);
  }

  /**
   * Creates an instance of the MemAccessResultData.
   * @param resultDataCommon The shared pointer to the common result data.
   * @return The shared pointer to the MemAccessResultData.
   */
  std::shared_ptr<ResultData> MakeResultData(
      std::shared_ptr<ResultDataCommon> resultDataCommon, size_t tileId) const final {
    return std::make_shared<MemAccessResultData>(resultDataCommon, tileId);
  }
};

class MemAccessWriterBase : public virtual WriterBase {
 public:
  using WriterBase::WriterBase;
  virtual ~MemAccessWriterBase() = default;

 protected:
  // WriterBase interface implementation. It casts structures to MemAccess types and calls MemAccess
  // specific functions for writing the data
  bool WriteApplicationData(const ApplicationDataSPtr res) override;
  bool WriteKernelData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData) override;
  bool WriteInvocationData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                           const InvocationDataSPtr invocationData) override;
  bool WriteResultData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                       const InvocationDataSPtr invocationData, const ResultDataSPtr resultData,
                       const ResultDataCommonSPtr resultDataCommon, size_t tileId) override;

  // MemAccess specific functions for writing data, which operates MemAccess structures
  virtual bool WriteMemAccessApplicationData(const MemAccessApplicationDataSPtr res) {
    return false;
  }
  virtual bool WriteMemAccessKernelData(const MemAccessApplicationDataSPtr res,
                                        const MemAccessKernelDataSPtr kernelData) {
    return false;
  }
  virtual bool WriteMemAccessInvocationData(const MemAccessApplicationDataSPtr res,
                                            const MemAccessKernelDataSPtr kernelData,
                                            const MemAccessInvocationDataSPtr invocationData) {
    return false;
  }
  virtual bool WriteMemAccessResultData(const MemAccessApplicationDataSPtr res,
                                        const MemAccessKernelDataSPtr kernelData,
                                        const MemAccessInvocationDataSPtr invocationData,
                                        const MemAccessResultDataSPtr resultData,
                                        const MemAccessResultDataCommonSPtr resultDataCommon,
                                        size_t tileId) {
    return false;
  }
};

/**
 * @class MemAccessControl
 * @brief This class is a base class for controlling memory access profiling.
 *        It provides virtual functions for determining whether to collect access information,
 *        sample addresses, cache lines number, and stride distribution for a specific instruction.
 *        It also provides functions for getting the minimum stride value, number of stride buckets,
 *        and stride step value for profiling.
 */
class MemAccessControl : public ControlBase {
 public:
  using ControlBase::ControlBase;

  /**
   * @brief Determines whether to collect access information for a specific instruction.
   * @param offset The offset of the instruction.
   * @param instruction The instruction object.
   * @return True if access information should be collected, false otherwise.
   */
  virtual bool ShouldCollectAccess(const InstructionOffset offset,
                                   const gtpin::IGtIns& instruction) const = 0;

  /**
   * @brief Determines whether to sample addresses for a specific instruction.
   * @param offset The offset of the instruction.
   * @param instruction The instruction object.
   * @return True if addresses should be sampled, false otherwise.
   */
  virtual bool ShouldSampleAddresses(const InstructionOffset offset,
                                     const gtpin::IGtIns& instruction) const = 0;

  /**
   * @brief Determines whether to collect cache lines number for a specific instruction.
   * @param offset The offset of the instruction.
   * @param instruction The instruction object.
   * @return True if cache lines number should be collected, false otherwise.
   */
  virtual bool ShouldCollectCacheLinesNumber(const InstructionOffset offset,
                                             const gtpin::IGtIns& instruction) const = 0;

  /**
   * @brief Determines whether to collect stride distribution for a specific instruction.
   * @param offset The offset of the instruction.
   * @param instruction The instruction object.
   * @return True if stride distribution should be collected, false otherwise.
   */
  virtual bool ShouldCollectStrideDistribution(const InstructionOffset offset,
                                               const gtpin::IGtIns& instruction) const = 0;

  /**
   * @brief Gets the minimum stride value that should be profiled. The distribution will have
   * GetStrideNum() buckets, starting from GetStrideMin() with GetStrideStep() step in bytes.
   * @return The minimum stride value in bytes.
   */
  virtual int32_t GetStrideMin() const = 0;

  /**
   * @brief Gets the number of stride buckets.
   * @return The number of stride buckets.
   */
  virtual int32_t GetStrideNum() const = 0;

  /**
   * @brief Gets the stride step value which will be used in profiling.
   * @return The stride step value in bytes.
   */
  virtual int32_t GetStrideStep() const = 0;
};

/**
 * Class representing the MemAccessControlDefault.
 * It implements the MemAccessControl class.
 */
class MemAccessControlDefault : public MemAccessControl {
 public:
  using MemAccessControl::MemAccessControl;

  bool ShouldInstrument(const KernelBuildDescriptor& buildDescr) const override { return true; };
  bool EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const override {
    return false;
  };
  bool ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const override { return true; };

  bool ShouldCollectAccess(const InstructionOffset offset,
                           const gtpin::IGtIns& instruction) const override {
    return true;
  }

  bool ShouldSampleAddresses(const InstructionOffset offset,
                             const gtpin::IGtIns& instruction) const override {
    return true;
  }

  bool ShouldCollectCacheLinesNumber(const InstructionOffset offset,
                                     const gtpin::IGtIns& instruction) const override {
    return true;
  }

  bool ShouldCollectStrideDistribution(const InstructionOffset offset,
                                       const gtpin::IGtIns& instruction) const override {
    return true;
  }

  int32_t GetStrideMin() const override { return STRIDE_MIN_DEFAULT; }

  int32_t GetStrideNum() const override { return STRIDE_NUM_DEFAULT; }

  int32_t GetStrideStep() const override { return STRIDE_STEP_DEFAULT; }
};

/**
 * Class representing the MemAccessGTPinProfiler.
 * It extends the GTPinProfiler class.
 * Provides implementation for creating the MemAccessFactory.
 */
class MemAccessGTPinProfiler : public GTPinProfiler {
 public:
  MemAccessGTPinProfiler(
      const std::shared_ptr<MemAccessWriterBase> writer,
      const std::shared_ptr<MemAccessControl> control = std::make_shared<MemAccessControlDefault>())
      : GTPinProfiler(writer, control) {}

 private:
  /**
   * Returns the factory for the MemAccessGTPinTool.
   * @return The shared pointer to the MemAccessFactory.
   */
  std::shared_ptr<ToolFactory> GetFactory(const ControlBaseSPtr control) final {
    return std::make_shared<MemAccessFactory>(control);
  };
};

}  // namespace gtpin_prof

#endif  // PTI_TOOLS_MEMACCESS
