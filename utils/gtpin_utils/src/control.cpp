//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file control.cpp
 * @brief Contains the implementation of default control classes for GTPin profiling and
 * instrumentation.
 *
 * This file provides the implementation of control classes used for GTPin profiling and
 * instrumentation. It includes the definition of the DefaultControl and GTPinKnobControl classes,
 * which determine whether a kernel should be instrumented or profiled based on certain conditions.
 */

#include "control.hpp"

#include "knob_parser.h"

using namespace gtpin_prof;

bool DefaultControl::ShouldInstrument(const KernelBuildDescriptor& buildDescr) const {
  return true;
}

bool DefaultControl::EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const {
  return true;
}

bool DefaultControl::ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const {
  return true;
}

bool GTPinKnobControl::ShouldInstrument(const KernelBuildDescriptor& buildDescr) const {
  return true;
}

static gtpin::Knob<bool> knobPerTileCollection("per-tile-collection", false,
                                               "Collect data with tile granularity");
bool GTPinKnobControl::EnablePerTileCollection(const KernelBuildDescriptor& buildDescr) const {
  return knobPerTileCollection;
}

static gtpin::KnobVector<int> knobKernelRun("kernel-run", {}, "Kernel run to profile");
bool GTPinKnobControl::ShouldProfileEnqueue(const KernelExecDescriptor& execDescr) const {
  if (!gtpin::IsKernelExecProfileEnabled(execDescr.gtExecDesc, execDescr.gpuPlatform)) return false;

  if (!knobKernelRun.NumValues()) return true;

  for (uint32_t i = 0; i != knobKernelRun.NumValues(); ++i) {
    if (knobKernelRun.GetValue(i) == execDescr.runIdx) {
      return true;
    }
  }
  return false;
}
