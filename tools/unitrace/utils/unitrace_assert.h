//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UNITRACE_UTILS_UNITRACE_ASSERT_H_
#define UNITRACE_UTILS_UNITRACE_ASSERT_H_

#include <cstdio>
#include <cstdlib>

// UNITRACE_ASSERT is always active, also in release (NDEBUG) builds. It does
// not use the standard assert() macro: <cassert> re-reads NDEBUG every time it
// is included, so any later #include <cassert> in the same translation unit
// would turn an assert()-based macro into a no-op.
[[noreturn]] inline void UnitraceAssertFail(const char* expr, const char* file, int line,
                                            const char* func) {
  std::fprintf(stderr, "%s:%d: %s: Assertion `%s' failed.\n", file, line, func, expr);
  std::fflush(stderr);
  std::abort();
}

#define UNITRACE_ASSERT(X) \
  ((X) ? static_cast<void>(0) : UnitraceAssertFail(#X, __FILE__, __LINE__, __func__))

#endif  // UNITRACE_UTILS_UNITRACE_ASSERT_H_
