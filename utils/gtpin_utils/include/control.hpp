//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_CONTROL_H
#define PTI_GTPIN_CONTROL_H

#include "def_gpu.hpp"
#include "kernel_exec_config.h"

/**
 * @file control.hpp
 * @brief This file contains the declaration of the ControlBase class and its derived classes.
 */

namespace gtpin_prof {

/**
 * @class ControlBase
 * @brief ControlBase is an interface class for GTPin profiling filtering and collection control.
 */
class ControlBase {
 public:
  virtual ~ControlBase() = default;
  virtual bool ShouldInstrument(const KernelBuildDescriptor& buildDescr) const = 0;
  virtual bool EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const = 0;
  virtual bool ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const = 0;
};

/**
 * @class DefaultControl
 * @brief DefaultControl is a control implementation that profiles everything. Used as default.
 */
class DefaultControl : public ControlBase {
 public:
  using ControlBase::ControlBase;
  virtual ~DefaultControl() = default;
  bool ShouldInstrument(const KernelBuildDescriptor& buildDescr) const final;
  bool EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const final;
  bool ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const final;
};

/**
 * @class GTPinKnobControl
 * @brief GTPinKnobControl is a control implementation that filters based on standard GTPin knobs.
 */
class GTPinKnobControl : public ControlBase {
 public:
  using ControlBase::ControlBase;
  virtual ~GTPinKnobControl() = default;
  bool ShouldInstrument(const KernelBuildDescriptor& buildDescr) const final;
  bool EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const final;
  bool ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const final;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_CONTROL_H
