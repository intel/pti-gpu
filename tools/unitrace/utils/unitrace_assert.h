//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UNITRACE_UTILS_UNITRACE_ASSERT_H_
#define UNITRACE_UTILS_UNITRACE_ASSERT_H_

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#define UNITRACE_ASSERT(X) assert(X)

#endif // UNITRACE_UTILS_UNITRACE_ASSERT_H_