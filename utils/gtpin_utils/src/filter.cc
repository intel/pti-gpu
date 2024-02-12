//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "filter.hpp"

#include <api/gtpin_api.h>

/**
 * @file Implementaion of DefaultGTPinFilter
 */

namespace gtpin {
namespace gtpin_prof {

KnobVector<int> knobKernelRun("kernel-run", {}, "Kernel run to profile");

bool DefaultGTPinFilter::ShouldInstrument(const GTPinKernelBuildDescriptor& descr) const {
  return true;
}

bool DefaultGTPinFilter::ShouldProfile(const GTPinKernelExecDesriptor& descr) const {
  bool active = !knobKernelRun.NumValues();
  for (uint32_t i = 0; i != knobKernelRun.NumValues(); ++i) {
    if (knobKernelRun.GetValue(i) == descr.runIdx) {
      active = true;
      break;
    }
  }

  return active;
}

HWProfileMask DefaultGTPinFilter::GetHWProfileMask() const { return HWProfileMask(); }

}  // namespace gtpin_prof
}  // namespace gtpin
