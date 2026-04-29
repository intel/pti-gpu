//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_XPTI_ADAPTER_H_
#define SRC_XPTI_ADAPTER_H_

#include <cstdint>

#if defined(_WIN32)
#define PTI_XPTI_HOOK_VISIBILITY __declspec(dllexport)
#else
#define PTI_XPTI_HOOK_VISIBILITY __attribute__((visibility("default")))
#endif

// Forward declare xpti namespace and enum to avoid requiring xpti headers
namespace xpti {
enum class stream_detail_level_t : uint8_t;
}  // namespace xpti

extern "C" {
PTI_XPTI_HOOK_VISIBILITY void xptiTraceInit(unsigned int major_version, unsigned int minor_version,
                                            const char* version_str, const char* stream_name);

PTI_XPTI_HOOK_VISIBILITY void xptiTraceFinish(const char* stream_name);  // NOLINT

PTI_XPTI_HOOK_VISIBILITY bool xptiQuerySubscriberStreamDetailLevel(
    const char* stream_name, xpti::stream_detail_level_t* level);
}

std::pair<bool, bool> IsForeignXPTISubscriber();

#endif  // SRC_XPTI_ADAPTER_H_
