//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_TOOL_FACTORY_H
#define PTI_GTPIN_TOOL_FACTORY_H

#include <api/gtpin_api.h>

#include "def_gpu.hpp"

/**
 * @file tool_factory.hpp
 * @brief This file contains the declaration of the ToolFactory class, which is responsible for
 * creating GTPin tools.
 */

namespace gtpin_prof {

class ApplicationData;
class ControlBase;
class GTPinTool;
class InvocationData;
class KernelData;
class ResultData;
class ResultDataCommon;

/**
 * @class ToolFactory
 * @brief A factory class for creating GTPin tools.
 */
class ToolFactory {
 public:
  ToolFactory(const ControlBaseSPtr control);
  virtual ~ToolFactory() = default;

  /**
   * @brief Creates a GTPin tool.
   * @param factory A shared pointer to the ToolFactory object.
   * @param control A shared pointer to the ControlBase object.
   * @return A shared pointer to the created GTPinTool object.
   */
  virtual GTPinToolSPtr MakeGTPinTool() const = 0;

  /**
   * @brief Gets the record size in bytes.
   * @return The record size.
   */
  virtual uint32_t GetRecordSize() const = 0;

  /**
   * @brief Creates an ApplicationData object.
   * @return A shared pointer to the created ApplicationData object.
   */
  virtual ApplicationDataSPtr MakeApplicationData() const = 0;

  /**
   * @brief Creates a KernelData object.
   * @param instrumentor A reference to the IGtKernelInstrument object.
   * @return A shared pointer to the created KernelData object.
   */
  virtual KernelDataSPtr MakeKernelData(const gtpin::IGtKernelInstrument& instrumentor) const = 0;

  /**
   * @brief Creates an InvocationData object.
   * @param execDescr The KernelExecDescriptor object.
   * @return A shared pointer to the created InvocationData object.
   */
  virtual InvocationDataSPtr MakeInvocationData(const KernelExecDescriptor& execDescr) const = 0;

  /**
   * @brief Creates a ResultData object.
   * @param resultDataCommon A shared pointer to the ResultDataCommon object.
   * @return A shared pointer to the created ResultData object.
   */
  virtual ResultDataSPtr MakeResultData(ResultDataCommonSPtr resultDataCommon,
                                        size_t tileId) const = 0;

  /**
   * @brief Get the Control object
   * @return const ControlBaseSPtr
   */
  const ControlBaseSPtr GetControl();

 protected:
  const ControlBaseSPtr m_control;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_TOOL_FACTORY_H
