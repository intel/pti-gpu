//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_LOADER_LOADER_H_
#define PTI_SAMPLES_LOADER_LOADER_H_

#if defined(_WIN32)
#include <windows.h>
extern "C" DWORD Init(void*);
#endif

extern "C" void Usage();
extern "C" int ParseArgs(int argc, char* argv[]);
extern "C" void SetToolEnv();

#endif  // PTI_SAMPLES_LOADER_LOADER_H_