//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_GTPIN_UTILS_H_
#define PTI_SAMPLES_UTILS_GTPIN_UTILS_H_

#include <gtpin.h>
#include <IGA/kv.hpp>

#include "pti_assert.h"

namespace utils {
namespace gtpin {

inline iga_gen_t GetArch(GED_MODEL arch) {
  switch(arch) {
  case GED_MODEL_GEN_8:
    return IGA_GEN8;
  case GED_MODEL_GEN_9:
    return IGA_GEN9;
  case GED_MODEL_GEN_10:
    return IGA_GEN10;
  case GED_MODEL_GEN_11:
    return IGA_GEN11;
  case GED_MODEL_GEN_12_1:
    return IGA_GEN12p1;
  default:
    break;
  }
  return IGA_GEN_INVALID;
}

inline void KnobAddBool(const char* name, bool value) {
  GTPinKnob knob = KNOB_FindArg(name);
  PTI_ASSERT(knob != nullptr);
  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_BOOL;
  KNOB_STATUS status = KNOB_AddValue(knob, &knob_value);
  PTI_ASSERT(status == KNOB_STATUS_SUCCESS);
}

inline void KnobAddInt(const char* name, int value) {
  GTPinKnob knob = KNOB_FindArg(name);
  PTI_ASSERT(knob != nullptr);
  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_INTEGER;
  KNOB_STATUS status = KNOB_AddValue(knob, &knob_value);
  PTI_ASSERT(status == KNOB_STATUS_SUCCESS);
}

} // namespace gtpin
} // namespace utils

#endif // PTI_SAMPLES_UTILS_GTPIN_UTILS_H_