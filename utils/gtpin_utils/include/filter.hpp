//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_FILTER_H
#define PTI_GTPIN_FILTER_H

#include <api/gtpin_api.h>

#include "def_gpu_gtpin.hpp"

namespace gtpin {
namespace gtpin_prof {

/**
 * @brief GTPinFilterBase is an interface class for GTPin profiling filtering. It
 * provides functions for profiling filter behaviour.
 */
class GTPinFilterBase {
 public:
  virtual ~GTPinFilterBase() = default;
  virtual bool ShouldInstrument(const GTPinKernelBuildDescriptor& descr) const = 0;
  virtual bool ShouldProfile(const GTPinKernelExecDesriptor& descr) const = 0;
  virtual HWProfileMask GetHWProfileMask() const = 0;
};

/**
 * @brief DefaultGTPinFilter is a filter implementaation that profiles everything. Used
 * as default.
 */
class DefaultGTPinFilter : public GTPinFilterBase {
 public:
  DefaultGTPinFilter() = default;
  virtual ~DefaultGTPinFilter() = default;
  bool ShouldInstrument(const GTPinKernelBuildDescriptor& descr) const final;
  bool ShouldProfile(const GTPinKernelExecDesriptor& descr) const final;
  HWProfileMask GetHWProfileMask() const final;
};

}  // namespace gtpin_prof
}  // namespace gtpin

#endif  // PTI_GTPIN_FILTER_H
