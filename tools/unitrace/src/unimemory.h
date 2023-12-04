
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNIMEMORY_H
#define PTI_TOOLS_UNITRACE_UNIMEMORY_H

#include <iostream>
#include <cstdlib>

namespace UniMemory {
  void
  AbortIfOutOfMemory(void *ptr) {
    if (ptr == nullptr) {
      std::cerr << "Out of memory" << std::endl;
      std::abort();
    }
  }

  void
  ExitIfOutOfMemory(void *ptr) {
    if (ptr == nullptr) {
      std::cerr << "Out of memory" << std::endl;
      std::_Exit(-1);
    }
  }
}

#endif // PTI_TOOLS_UNITRACE_UNIMEMORY_H
