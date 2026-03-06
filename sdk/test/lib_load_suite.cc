//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Standalone library loading regression tests.
//
// IMPORTANT: This executable is intentionally NOT linked against Pti::pti_view
// to ensure clean library loading state. This allows proper testing of dlopen
// scenarios that would be masked if libpti_view.so was already loaded at process startup.
//
// These tests verify that libpti.so has no unresolved dependencies on libpti_view.so
// symbols, which is critical for Python interoperability and modular loading scenarios.

#include <dlfcn.h>
#include <gtest/gtest.h>

#include <memory>

#include "pti/pti.h"

// Tests to ensure libpti.so can be loaded independently of libpti_view.so.
// This is essential for Python environments where libpti_view.so may be loaded
// with RTLD_LOCAL, making its symbols unavailable to subsequently loaded libraries.

// Simulates Python's typical library loading pattern where libpti_view.so is loaded
// with RTLD_LOCAL scope, making its symbols private. Tests that libpti.so can still
// be loaded internally by the PTI library without symbol resolution failures.
TEST(LibLoadTest, CoreModuleLoadsWithLocalScope) {
  // Load with RTLD_LOCAL (Python's default behavior)
  void *raw_handle = dlopen("libpti_view.so", RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(raw_handle, nullptr) << "Failed to dlopen libpti_view.so: " << dlerror();

  // Use RAII to ensure handle is always closed, even on early returns
  auto handle = std::unique_ptr<void, decltype(&dlclose)>(raw_handle, dlclose);

  // Call PTI function through loaded library
  using ptiViewGPULocalAvailable_t = pti_result (*)();

  // Clear any previous dlerror() state before dlsym()
  dlerror();
  auto fn =
      reinterpret_cast<ptiViewGPULocalAvailable_t>(dlsym(handle.get(), "ptiViewGPULocalAvailable"));
  ASSERT_NE(fn, nullptr) << "Failed to find ptiViewGPULocalAvailable: " << dlerror();

  // PTI_ERROR_NOT_IMPLEMENTED means internal dlopen("libpti.so") failed
  pti_result result = fn();

  EXPECT_NE(result, pti_result::PTI_ERROR_NOT_IMPLEMENTED)
      << "Core library (libpti.so) failed to load. "
         "This usually means libpti.so has undefined symbols that are "
         "only defined in libpti_view.so, breaking RTLD_LOCAL loading.";

  EXPECT_TRUE(result == pti_result::PTI_SUCCESS ||
              result == pti_result::PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED)
      << "Unexpected error code: " << result;

  // handle automatically closed by unique_ptr destructor
}

// Tests that libpti.so can be loaded in complete isolation without any prior
// loading of libpti_view.so. This verifies that libpti.so has no unresolved
// cross-library symbol dependencies that would prevent standalone loading.
// Any undefined symbols will cause dlopen() itself to fail immediately.
TEST(LibLoadTest, CoreModuleHasNoUnresolvedCrossLibrarySymbols) {
  // Clear any previous dlerror() state before dlopen()
  dlerror();
  void *raw_handle = dlopen("libpti.so", RTLD_NOW | RTLD_LOCAL);

  EXPECT_NE(raw_handle, nullptr) << "dlopen(\"libpti.so\", RTLD_NOW | RTLD_LOCAL) failed: "
                                 << dlerror()
                                 << "\nThis means libpti.so has undefined symbols that cannot be "
                                    "resolved without libpti_view.so in global scope.";

  // Use RAII to ensure handle is always closed, even if future changes add early returns
  if (raw_handle) {
    auto handle = std::unique_ptr<void, decltype(&dlclose)>(raw_handle, dlclose);
    // handle automatically closed by unique_ptr destructor
  }
}
