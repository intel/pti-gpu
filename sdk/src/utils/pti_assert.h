//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_PTI_ASSERT_H_
#define PTI_UTILS_PTI_ASSERT_H_

#ifdef NDEBUG
#undef NDEBUG
// Work around for the system (GNU Linux) assert.h defintion
// of __assert_fail (/usr/include/assert.h) not matching the
// sycl builtin
// https://github.com/intel/llvm/blob/sycl/sycl/include/sycl/builtins.hpp#L3145
// TODO: Re-evaluate our need for PTI_ASSERT
#if defined(__gnu_linux__) && defined(SYCL_LANGUAGE_VERSION) && defined(__INTEL_LLVM_COMPILER)
// turn off marking __assert_fail as noexcept.
#undef __THROW
#define __THROW
#endif
#include <assert.h>
#if defined(__gnu_linux__) && defined(SYCL_LANGUAGE_VERSION) && defined(__INTEL_LLVM_COMPILER)
#undef __THROW
// redefine __THROW to mark functions defined elsewhere as
// noexcept (cleanup after ourselves)
#define __THROW noexcept(true)
#endif
#define NDEBUG
#else
#include <assert.h>
#endif

#define PTI_ASSERT(X) assert(X)
#endif  // PTI_UTILS_PTI_ASSERT_H_
