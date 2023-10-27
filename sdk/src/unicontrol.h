
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNICONTROL_H
#define PTI_TOOLS_UNITRACE_UNICONTROL_H

#include <iostream>

#include "utils.h"

class UniController {
 public:
  static bool IsCollectionEnabled(void) {
    if (conditional_collection_) {
      std::string enabled = utils::GetEnv("PTI_ENABLE_COLLECTION");
      if (itt_paused_ || enabled.empty() || enabled == "0") {
        return false;
      }
    }
    return true;
  }
  static void IttPause(void) {
    itt_paused_ = true;
    utils::SetEnv("PTI_ENABLE_COLLECTION", "0");
  }
  static void IttResume(void) {
    itt_paused_ = false;
    utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
  }

 private:
  inline static bool conditional_collection_ =
      (utils::GetEnv("UNITRACE_ConditionalCollection") == "1") ? true : false;
  inline static bool itt_paused_ = false;
};

#endif  // PTI_TOOLS_UNITRACE_UNICONTROL_H
