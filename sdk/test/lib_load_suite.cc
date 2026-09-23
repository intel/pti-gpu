//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Real-library preload integration regression test. The executable deliberately does
// not link against either PTI library, so loading begins through the staged interface.

#include <dlfcn.h>
#include <gtest/gtest.h>

#include <memory>

#include "pti/pti_view.h"

// The staged interface and XPTI runtime closure share a directory, but Core resides in
// its pti/ child. Core's $ORIGIN cannot reach XPTI, so LoadCoreLibrary must recursively
// pre-load XPTI and its missing runtime dependencies through the interface RUNPATH.
TEST(LibLoadTest, InterfacePreloadsCoreDependenciesWithoutLdLibraryPath) {
  // The staged Core has no route to the interface-directory runtime closure on its own.
  dlerror();
  void *direct_core_handle = dlopen(PTI_PRELOAD_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_EQ(direct_core_handle, nullptr)
      << "staged Core unexpectedly loaded without interface-side pre-loading: " << dlerror();
  void *raw_handle = dlopen(PTI_PRELOAD_VIEW_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
  ASSERT_NE(raw_handle, nullptr) << "Failed to dlopen staged libpti_view.so: " << dlerror();
  auto handle = std::unique_ptr<void, decltype(&dlclose)>(raw_handle, dlclose);

  using ptiViewOverheadKindToString_t = const char *(*)(pti_view_overhead_kind);
  dlerror();
  auto overhead_kind_to_string = reinterpret_cast<ptiViewOverheadKindToString_t>(
      dlsym(handle.get(), "ptiViewOverheadKindToString"));
  ASSERT_NE(overhead_kind_to_string, nullptr)
      << "Failed to find ptiViewOverheadKindToString: " << dlerror();
  // The interface fallback is "INVALID"; use a distinct value to prove Core dispatch.
  EXPECT_STREQ(overhead_kind_to_string(PTI_VIEW_OVERHEAD_KIND_RESOURCE), "RESOURCE");

  // RTLD_NOLOAD verifies that the preceding API call mapped staged Core; it cannot load it.
  dlerror();
  void *core_handle = dlopen(PTI_PRELOAD_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_NOLOAD);
  ASSERT_NE(core_handle, nullptr) << "ptiViewOverheadKindToString did not load staged libpti.so: "
                                  << dlerror();
  EXPECT_EQ(dlclose(core_handle), 0);
}
