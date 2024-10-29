//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_PTI_ASSERT_H_
#define PTI_UTILS_PTI_ASSERT_H_

#include <cstdio>
#include <cstdlib>

#if defined(__gnu_linux__)
#define PTI_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_WIN32)
#define PTI_FUNCTION_NAME __FUNCSIG__
#else
#define PTI_FUNCTION_NAME __FUNCTION__
#endif

#define PTI_ASSERT(X)                                                            \
  do {                                                                           \
    if (!(X)) {                                                                  \
      std::fprintf(stderr, "Condition " #X " Failed on %s at " __FILE__ ":%d\n", \
                   PTI_FUNCTION_NAME, __LINE__);                                 \
      std::abort();                                                              \
    }                                                                            \
  } while (0)

#endif  // PTI_UTILS_PTI_ASSERT_H_
