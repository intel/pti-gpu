//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_INSTCOUNT_H
#define PTI_TOOLS_INSTCOUNT_H

#include <memory>

#include "profiler.hpp"

/**
 * @file instcount.hpp
 * @brief Contains classes and structures related to instruction counting using GTPin.
 */

namespace gtpin_prof {

/**
 * @struct InstCountRawRecord
 * @brief Structure representing a raw record for instruction counting.
 *        It extends the base RawRecord structure.
 *        Contains a count field to store the number of instructions.
 */
struct InstCountRawRecord : public RawRecord {
  uint64_t count;
};

/**
 * @class InstCountSiteOfInstrument
 * @brief Class representing a site of instrument for instruction counting.
 *        It extends the base SiteOfInstrument class.
 *        Contains a reference to the instruction and the type of instrument.
 */
class InstCountSiteOfInstrument : public SiteOfInstrument {
 public:
  using SiteOfInstrument::SiteOfInstrument;

  /**
   * @enum InstCountSiteOfInstrumentType
   * @brief Enumeration representing the type of instrument for instruction counting.
   *        It includes Count, Simd, and TypeInvalid.
   */
  enum class Type { Count, Simd, TypeInvalid };

  /**
   * @brief Constructs an InstCountSiteOfInstrument object with the given instruction and type.
   * @param ins The reference to the instruction.
   * @param type The type of instrument.
   */
  InstCountSiteOfInstrument(const gtpin::IGtIns& ins, const Type type = Type::TypeInvalid)
      : instruction(ins), type(type) {}

  const gtpin::IGtIns& instruction;  // The instruction reference.
  const Type type;                   // The type of instrument.
};

/**
 * @class InstCountGTPinTool
 * @brief Class representing the InstCountGTPinTool.
 *        It extends the GTPinTool class.
 *        Provides implementation for analyzing, instrumenting, and accumulating kernel data for
 * instruction counting.
 */
class InstCountGTPinTool : public GTPinTool {
 public:
  using GTPinTool::GTPinTool;
  ~InstCountGTPinTool() override = default;

  /**
   * @brief Returns the name of the InstCountGTPinTool.
   * @return The name of the tool.
   */
  const char* Name() const final { return "InstCountProfiler"; };

  /**
   * @brief Analyzes the kernel data and performs instruction counting.
   * @param kernelData The shared pointer to the kernel data.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The status of the analysis.
   */
  PROF_STATUS AnalyzeKernel(KernelDataSPtr kernelData,
                            const gtpin::IGtKernelInstrument& instrumentor) final;

  /**
   * @brief Instruments the kernel data for instruction counting.
   * @param kernelData The shared pointer to the kernel data.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The status of the instrumentation.
   */
  PROF_STATUS Instrument(KernelDataSPtr kernelData, gtpin::IGtKernelInstrument& instrumentor) final;

  /**
   * @brief Accumulates the profiling result for instruction counting.
   * @param kernelData The shared pointer to the kernel data.
   * @param profilingResult The shared pointer to the profiling result data.
   * @param siteOfInstrument The shared pointer to the site of instrument.
   * @param record The raw record for instruction counting.
   * @return The status of the accumulation.
   */
  PROF_STATUS Accumulate(KernelDataSPtr kernelData, ResultDataSPtr profilingResult,
                         SiteOfInstrumentSPtr siteOfInstrument, RawRecord* record) final;
};

/**
 * Class representing the InstCountApplicationData.
 * It extends the ApplicationData class.
 * Provides additional data specific to the InstCountGTPinTool.
 */
class InstCountApplicationData : public ApplicationData {
 public:
  using ApplicationData::ApplicationData;
};

/**
 * Class representing the InstCountKernelData.
 * It extends the KernelData class.
 * Provides additional data specific to the InstCountGTPinTool.
 */
class InstCountKernelData : public KernelData {
 public:
  using KernelData::KernelData;
};

/**
 * Class representing the InstCountInvocationData.
 * It extends the InvocationData class.
 * Provides additional data specific to the InstCountGTPinTool.
 */
class InstCountInvocationData : public InvocationData {
 public:
  using InvocationData::InvocationData;
};

/**
 * Class representing the InstCountResultDataCommon.
 * It extends the ResultDataCommon class.
 * Provides additional data specific to the InstCountGTPinTool.
 * Includes the instruction offset and basic block ID.
 */
class InstCountResultDataCommon : public ResultDataCommon {
 public:
  /**
   * Constructs an InstCountResultDataCommon object with the given offset and basic block ID.
   * @param offset The instruction offset.
   * @param bblId The basic block ID.
   */
  InstCountResultDataCommon(InstructionOffset offset, gtpin::BblId bblId)
      : offset(offset), bblId(bblId) {}

  InstructionOffset offset = -1;  // The instruction offset.
  gtpin::BblId bblId = -1;        // The basic block ID.
};

/**
 * Class representing the InstCountResultData.
 * It extends the ResultData class.
 * Provides additional data specific to the InstCountGTPinTool.
 * Includes counters for the number of active SIMD lanes and the total number of instructions.
 */
class InstCountResultData : public ResultData {
 public:
  using ResultData::ResultData;

  size_t simdActiveLaneCounter = 0;  // Counter for the number of active SIMD lanes.
  size_t instructionCounter = 0;     // Counter for the total number of instructions.
};

using InstCountApplicationDataSPtr = std::shared_ptr<InstCountApplicationData>;
using InstCountKernelDataSPtr = std::shared_ptr<InstCountKernelData>;
using InstCountInvocationDataSPtr = std::shared_ptr<InstCountInvocationData>;
using InstCountResultDataSPtr = std::shared_ptr<InstCountResultData>;
using InstCountSiteOfInstrumentSPtr = std::shared_ptr<InstCountSiteOfInstrument>;
using InstCountResultDataCommonSPtr = std::shared_ptr<InstCountResultDataCommon>;

/**
 * Class representing the InstCountFactory.
 * It extends the ToolFactory class.
 * Provides implementation for creating instances of the InstCountGTPinTool and related data
 * structures.
 */
class InstCountFactory : public ToolFactory {
 public:
  using ToolFactory::ToolFactory;
  ~InstCountFactory() override = default;

  /**
   * Creates an instance of the InstCountGTPinTool.
   * @param factory The shared pointer to the tool factory.
   * @param control The shared pointer to the control base.
   * @return The shared pointer to the InstCountGTPinTool.
   */
  GTPinToolSPtr MakeGTPinTool() const final {
    return std::make_shared<InstCountGTPinTool>(std::make_shared<InstCountFactory>(*this));
  }

  /**
   * Returns the size of the raw record for instruction counting.
   * @return The size of the raw record.
   */
  uint32_t GetRecordSize() const final { return sizeof(InstCountRawRecord); }

  /**
   * Creates an instance of the InstCountApplicationData.
   * @return The shared pointer to the InstCountApplicationData.
   */
  ApplicationDataSPtr MakeApplicationData() const final {
    return std::make_shared<InstCountApplicationData>();
  }

  /**
   * Creates an instance of the InstCountKernelData.
   * @param instrumentor The reference to the kernel instrumentor.
   * @return The shared pointer to the InstCountKernelData.
   */
  KernelDataSPtr MakeKernelData(const gtpin::IGtKernelInstrument& instrumentor) const final {
    return std::make_shared<InstCountKernelData>(instrumentor);
  }

  /**
   * Creates an instance of the InstCountInvocationData.
   * @param execDescr The kernel execution descriptor.
   * @return The shared pointer to the InstCountInvocationData.
   */
  InvocationDataSPtr MakeInvocationData(const KernelExecDescriptor& execDescr) const final {
    return std::make_shared<InstCountInvocationData>(execDescr);
  }

  /**
   * Creates an instance of the InstCountResultData.
   * @param resultDataCommon The shared pointer to the common result data.
   * @return The shared pointer to the InstCountResultData.
   */
  ResultDataSPtr MakeResultData(ResultDataCommonSPtr resultDataCommon) const final {
    return std::make_shared<InstCountResultData>(resultDataCommon);
  }
};

/**
 * @class InstCountWriterBase
 * @brief Base class for writers in the InstCountGTPinTool.
 *        It extends the WriterBase class.
 *        Provides a common interface for writing data specific to the InstCountGTPinTool.
 */
class InstCountWriterBase : public virtual WriterBase {
 public:
  using WriterBase::WriterBase;
  virtual ~InstCountWriterBase() = default;

 protected:
  // WriterBase interface implementation. It casts structures to InstCount types and calls InstCount
  // specific functions for writing the data
  bool WriteApplicationData(const ApplicationDataSPtr res) override;
  bool WriteKernelData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData) override;
  bool WriteInvocationData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                           const InvocationDataSPtr invocationData) override;
  bool WriteResultData(const ApplicationDataSPtr res, const KernelDataSPtr kernelData,
                       const InvocationDataSPtr invocationData, const ResultDataSPtr resultData,
                       const ResultDataCommonSPtr resultDataCommon, size_t tileId) override;

  // InstCount specific functions for writing data, which operates InstCount structures
  virtual bool WriteInstCountApplicationData(const InstCountApplicationDataSPtr res) {
    return false;
  }
  virtual bool WriteInstCountKernelData(const InstCountApplicationDataSPtr res,
                                        const InstCountKernelDataSPtr kernelData) {
    return false;
  }
  virtual bool WriteInstCountInvocationData(const InstCountApplicationDataSPtr res,
                                            const InstCountKernelDataSPtr kernelData,
                                            const InstCountInvocationDataSPtr invocationData) {
    return false;
  }
  virtual bool WriteInstCountResultData(const InstCountApplicationDataSPtr res,
                                        const InstCountKernelDataSPtr kernelData,
                                        const InstCountInvocationDataSPtr invocationData,
                                        const InstCountResultDataSPtr resultData,
                                        const InstCountResultDataCommonSPtr resultDataCommon,
                                        size_t tileId) {
    return false;
  }
};

/**
 * Class representing the InstCountControl.
 * It extends the ControlBase class.
 * Provides additional control options specific to the InstCountGTPinTool.
 */
class InstCountControl : public ControlBase {
 public:
  using ControlBase::ControlBase;

  /**
   * @brief Checks if the SIMD width should be collected.
   * @return True if the SIMD width should be collected, false otherwise.
   */
  virtual bool ShouldCollectSimdWidth() const = 0;
};

/**
 * @class InstCountControlDefault
 * @brief Default implementation of the InstCountControl class.
 *        It extends the ControlBase class.
 *        Provides additional control options specific to the InstCountGTPinTool.
 */
class InstCountControlDefault : public InstCountControl {
 public:
  using InstCountControl::InstCountControl;

  bool ShouldCollectSimdWidth() const final { return true; }

  bool ShouldInstrument(const KernelBuildDescriptor& buildDescr) const final { return true; }

  bool EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const final {
    return false;
  }

  bool ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const final { return true; }
};

/**
 * Class representing the InstCountGTPinProfiler.
 * It extends the GTPinProfiler class.
 * Provides implementation for creating the InstCountFactory.
 */
class InstCountGTPinProfiler : public GTPinProfiler {
 public:
  InstCountGTPinProfiler(
      const std::shared_ptr<InstCountWriterBase> writer,
      const std::shared_ptr<InstCountControl> control = std::make_shared<InstCountControlDefault>())
      : GTPinProfiler(writer, control) {}

 private:
  /**
   * Returns the factory for the InstCountGTPinTool.
   * @return The shared pointer to the InstCountFactory.
   */
  ToolFactorySPtr GetFactory(const ControlBaseSPtr control) final {
    return std::make_shared<InstCountFactory>(control);
  };
};

}  // namespace gtpin_prof

#endif  // PTI_TOOLS_INSTCOUNT_H
