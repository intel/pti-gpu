//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "pti/pti_version.h"

constexpr auto kPtiVersion = pti_version{PTI_VERSION_MAJOR, PTI_VERSION_MINOR, PTI_VERSION_PATCH};
constexpr const char* const kPtiVersionString = PTI_VERSION_STRING;

const char* ptiVersionString() { return kPtiVersionString; }

pti_version ptiVersion() { return kPtiVersion; }
