//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_PTI_ASSERT_H_
#define PTI_SAMPLES_UTILS_PTI_ASSERT_H_

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#define PTI_ASSERT(X) assert(X)

#endif // PTI_SAMPLES_UTILS_PTI_ASSERT_H_