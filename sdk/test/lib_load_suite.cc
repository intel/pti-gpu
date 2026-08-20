//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Real-library preload integration regression test. The executable deliberately does
// not link against either PTI library, so loading begins through the staged interface.

#include <dlfcn.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;

std::uint64_t WallClockNowNanoseconds() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::string FormatLocalTimestamp(std::uint64_t timestamp) {
  const auto seconds = static_cast<std::time_t>(timestamp / kNanosecondsPerSecond);
  const auto nanoseconds = timestamp % kNanosecondsPerSecond;
  std::tm local_time{};
  localtime_r(&seconds, &local_time);

  std::ostringstream output;
  output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
         << std::setw(9) << nanoseconds << ' ' << std::put_time(&local_time, "%Z");
  return output.str();
}

}  // namespace

// The staged interface and XPTI runtime closure share a directory, but Core resides in
// its pti/ child. Core's $ORIGIN cannot reach XPTI, so LoadCoreLibrary must recursively
// pre-load XPTI and its missing runtime dependencies through the interface RUNPATH.
TEST(LibLoadTest, InterfacePreloadsCoreDependenciesWithoutLdLibraryPath) {
  // The staged Core has no route to the interface-directory runtime closure on its own.
  dlerror();
  void *direct_core_handle = dlopen(PTI_PRELOAD_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_EQ(direct_core_handle, nullptr)
      << "staged Core unexpectedly loaded without interface-side pre-loading: " << dlerror();
  void *raw_handle = dlopen(PTI_PRELOAD_VIEW_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(raw_handle, nullptr) << "Failed to dlopen staged libpti_view.so: " << dlerror();
  auto handle = std::unique_ptr<void, decltype(&dlclose)>(raw_handle, dlclose);

  using ptiViewGetTimestamp_t = std::uint64_t (*)();
  dlerror();
  auto get_timestamp =
      reinterpret_cast<ptiViewGetTimestamp_t>(dlsym(handle.get(), "ptiViewGetTimestamp"));
  ASSERT_NE(get_timestamp, nullptr) << "Failed to find ptiViewGetTimestamp: " << dlerror();

  const auto wall_clock_before = WallClockNowNanoseconds();
  const auto timestamp = get_timestamp();
  const auto wall_clock_after = WallClockNowNanoseconds();
  ASSERT_NE(timestamp, 0U);

  std::cout << "ptiViewGetTimestamp = " << FormatLocalTimestamp(timestamp) << " (" << timestamp
            << " ns)" << std::endl;

  constexpr std::uint64_t kWallClockToleranceNanoseconds = 2 * kNanosecondsPerSecond;
  EXPECT_GE(timestamp + kWallClockToleranceNanoseconds, wall_clock_before);
  EXPECT_LE(timestamp, wall_clock_after + kWallClockToleranceNanoseconds);

  // RTLD_NOLOAD verifies that the preceding API call mapped staged Core; it cannot load it.
  dlerror();
  void *core_handle = dlopen(PTI_PRELOAD_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_NOLOAD);
  ASSERT_NE(core_handle, nullptr) << "ptiViewGetTimestamp did not load staged libpti.so: "
                                  << dlerror();
  EXPECT_EQ(dlclose(core_handle), 0);

  // Keep the interface loaded until process exit. Its PtiLibHandler singleton owns Core
  // forwarding pointers and pre-loaded dependencies for the same lifetime.
  (void)handle.release();
}
