//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_XPTI_ADAPTER_H_
#define SRC_XPTI_ADAPTER_H_

#if defined(_WIN32)
#define PTI_XPTI_HOOK_VISIBILITY __declspec(dllexport)
#else
#define PTI_XPTI_HOOK_VISIBILITY __attribute__((visibility("default")))
#endif

extern "C" {
PTI_XPTI_HOOK_VISIBILITY void xptiTraceInit(unsigned int major_version, unsigned int minor_version,
                                            const char* version_str, const char* stream_name);

PTI_XPTI_HOOK_VISIBILITY void xptiTraceFinish(const char* stream_name);  // NOLINT
}

#endif  // SRC_XPTI_ADAPTER_H_
