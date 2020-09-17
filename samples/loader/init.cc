//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "tool.h"
#include "utils.h"

#if defined(_WIN32)

void AtExit() {
  DisableProfiling();
}

extern "C" __declspec(dllexport)
DWORD Init(void*) {
  atexit(AtExit);
  EnableProfiling();
  return 0;
}

#else

static bool IsEnabled() {
  std::string value = utils::GetEnv("PTI_ENABLE");
  if (value == "1") {
    return true;
  }
  return false;
}

void __attribute__((constructor)) Load() {
  if (IsEnabled()) {
    EnableProfiling();
  }
}

void __attribute__((destructor)) Unload() {
  DisableProfiling();
}

#endif