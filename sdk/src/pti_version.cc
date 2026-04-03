//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "pti/pti_version.h"

#include "platform_config.h"

#if defined(_MSC_VER) && !defined(__clang__)
#pragma comment(linker, "/include:kPtiIdentifierString")
#endif

extern "C" {
PTI_USED extern const char kPtiIdentifierString[] =  // NOLINT
    "Intel(R) Profiling Tools Interfaces (PTI) Version " PTI_VERSION_STRING
    " (" PTI_COMMIT_HASH_STRING ")";
}  // extern "C"

namespace {
constexpr auto kPtiVersion = pti_version{PTI_VERSION_MAJOR, PTI_VERSION_MINOR, PTI_VERSION_PATCH};

constexpr const char* const kPtiVersionString = PTI_VERSION_STRING;
}  // namespace

const char* ptiVersionString() { return kPtiVersionString; }

pti_version ptiVersion() { return kPtiVersion; }
