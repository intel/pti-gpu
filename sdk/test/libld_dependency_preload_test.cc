//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Test the dependency pre-load path in pti_lib_handler.h, which exists because the
// loader's search order does not always reach the directories where the PTI Core
// library's dependencies are deployed - in a Python wheel they sit next to the interface
// library, one directory up from the Core library's $ORIGIN.
//
// The layout below reproduces that without needing a wheel: this executable stands in
// for the interface library, and the synthetic libraries built by AddPreloadStub() in
// CMakeLists.txt stand in for the Core library and its dependencies.
//
// Each dependency's directory is its test case, one per way it can relate to the two
// searches in play: the loader's own (which follows this executable's RUNPATH) and the
// full path fallback over DependencySearchDirs(). One core library per dependency, so
// no test can leave a dependency loaded that satisfies another one.
//
//   <build>/test/
//     preload_stage/                            (2nd fallback search directory)
//       libld_dependency_preload_test          (executable, sets RUNPATH to $ORIGIN)
//       libpti_preload_dep_bysoname.so         (dependency: found by loader's search)
//       libpti_preload_dep_chain_head.so       (dependency: found by loader's search)
//       libpti_preload_dep_chain_tail.so       (dependency: also here, but chain_head
//                                               has no RUNPATH to reach it - found only
//                                               when pre-loaded from this executable)
//       pti/                                   (1st fallback search directory)
//         libpti_preload_core_bysoname.so      (core test: depends on dep_bysoname)
//         libpti_preload_core_byfullpath.so    (core test: depends on dep_byfullpath)
//         libpti_preload_core_chain.so         (core test: depends on chain_head ->
//                                               chain_tail)
//         libpti_preload_core_unreachable.so   (core test: depends on dep_unreachable)
//         libpti_preload_core_undefined.so     (core test: has unresolved symbol)
//         libpti_preload_dep_byfullpath.so     (dependency: only in fallback search)
//     preload_unreachable/
//       libpti_preload_dep_unreachable.so      (dependency: unreachable by both searches)

#include <dlfcn.h>
#include <gtest/gtest.h>

#include <string>

#include "pti_lib_handler.h"

namespace {

constexpr const char* kCoreBySoname = "libpti_preload_core_bysoname.so";
constexpr const char* kCoreByFullPath = "libpti_preload_core_byfullpath.so";
constexpr const char* kCoreChain = "libpti_preload_core_chain.so";
constexpr const char* kCoreUnreachable = "libpti_preload_core_unreachable.so";
constexpr const char* kCoreUndefinedSymbol = "libpti_preload_core_undefined.so";
constexpr const char* kCoreCleanupFailure = "libpti_preload_core_cleanup_failure.so";

std::string CoreLibPath(const char* file_name) { return pti::GetPathToPtiModule() + file_name; }

// LoadCoreLibrary reports failure by throwing. Returns the message it reported,
// or an empty string if the load succeeded.
std::string LoadFailure(const std::string& core_lib_path) {
  try {
    pti::LoadCoreLibrary(core_lib_path);
  } catch (const std::exception& error) {
    return error.what();
  }
  return std::string{};
}

// The code under test logs which dependency it could not resolve, but the setup that
// sets a level lives in the PtiLibHandler constructor these tests bypass. Verbose by
// default, since ctest shows output only for a failing test. PTILOG_LEVEL overrides.
class LogLevelFromEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    spdlog::set_level(spdlog::level::trace);

    const auto env_string = ::utils::GetEnv("PTILOG_LEVEL");
    if (!env_string.empty()) {
      spdlog::cfg::helpers::load_levels(env_string);
    }
    ::utils::SetGlobalSpdLogPattern();
  }
};

// GoogleTest takes ownership of the environment and deletes it after the run.
[[maybe_unused]] const auto* const kLogLevelFromEnvironment = ::testing::AddGlobalTestEnvironment(
    new LogLevelFromEnvironment);  // NOLINT(cppcoreguidelines-owning-memory)

// Every core library has its own dependency shape, keeping these tests independent of
// the order they run in.
class DependencyPreloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // GetPathToPtiModule() derives everything from dladdr, which for an executable
    // reports argv[0]. Invoked by a bare or relative name, the paths below come out
    // relative to the current directory; say so rather than fail with a dlopen error
    // that looks like the code under test misbehaving.
    ASSERT_TRUE(pti::utils::filesystem::exists(CoreLibPath(kCoreBySoname)))
        << "the synthetic core libraries are not where this test expects them ("
        << pti::GetPathToPtiModule() << "); run the executable by its full path";
  }
};

TEST_F(DependencyPreloadTest, ResolvesADependencyReachableBySoname) {
  // The wheel case: the loader cannot resolve the dependency on behalf of the core
  // library, but this executable's RUNPATH covers it.
  auto core_lib = pti::LoadCoreLibrary(CoreLibPath(kCoreBySoname));
  ASSERT_NE(core_lib, nullptr);
  EXPECT_NE(core_lib->GetSymbol<int (*)()>("PtiPreloadStub_core_bysoname"), nullptr);
}

TEST_F(DependencyPreloadTest, ResolvesADependencyReachableOnlyByFullPath) {
  // Shipped alongside the core library instead: no RUNPATH in the process covers the
  // module subdirectory, so the fallback has to name the file outright.
  auto core_lib = pti::LoadCoreLibrary(CoreLibPath(kCoreByFullPath));
  ASSERT_NE(core_lib, nullptr);
  EXPECT_NE(core_lib->GetSymbol<int (*)()>("PtiPreloadStub_core_byfullpath"), nullptr);
}

TEST_F(DependencyPreloadTest, ResolvesADependencyOfADependency) {
  // Pre-loading the core library's dependency fails too, because it needs something the
  // loader cannot find either: the deeper one has to be satisfied first.
  auto core_lib = pti::LoadCoreLibrary(CoreLibPath(kCoreChain));
  ASSERT_NE(core_lib, nullptr);
  EXPECT_NE(core_lib->GetSymbol<int (*)()>("PtiPreloadStub_core_chain"), nullptr);

  auto head_dependency = core_lib->GetSymbol<int (*)()>("PtiPreloadStub_dep_chain_head_Dependency");
  ASSERT_NE(head_dependency, nullptr);
  EXPECT_EQ(head_dependency(), 0);
}

TEST_F(DependencyPreloadTest, ReportsADependencyItCannotFindAnywhere) {
  const auto error = LoadFailure(CoreLibPath(kCoreUnreachable));
  ASSERT_FALSE(error.empty()) << "expected the load to fail: the dependency is reachable "
                                 "neither by soname nor from a fallback directory";
  // The reported message is the loader's, so it names what is actually missing.
  EXPECT_NE(error.find("libpti_preload_dep_unreachable.so"), std::string::npos) << error;
}

TEST_F(DependencyPreloadTest, ReleasesAllPreloadedDependenciesWhenRecoveryFails) {
  const auto error = LoadFailure(CoreLibPath(kCoreCleanupFailure));
  ASSERT_FALSE(error.empty()) << "expected the load to fail after pre-loading head and tail";
  EXPECT_NE(error.find("libpti_preload_dep_cleanup_missing.so"), std::string::npos) << error;

  for (const char* dependency : {
           "libpti_preload_dep_cleanup_head.so",
           "libpti_preload_dep_cleanup_tail.so",
       }) {
    dlerror();
    void* handle = dlopen(dependency, RTLD_NOW | RTLD_NOLOAD);
    if (handle != nullptr) {
      EXPECT_EQ(dlclose(handle), 0);
    }
    EXPECT_EQ(handle, nullptr) << dependency << " remained mapped after a failed recovery attempt";
  }
}

TEST_F(DependencyPreloadTest, ReportsAFailureThatIsNotAMissingDependency) {
  const auto error = LoadFailure(CoreLibPath(kCoreUndefinedSymbol));
  ASSERT_FALSE(error.empty()) << "expected the load to fail: the library has an undefined symbol";
  EXPECT_NE(error.find("undefined symbol"), std::string::npos) << error;
}

TEST_F(DependencyPreloadTest, ReportsAMissingCoreLibrary) {
  // The loader names the file we asked for by path, and a path is not a soname,
  // so there is nothing to pre-load and no point retrying.
  const auto missing = CoreLibPath("libpti_preload_core_not_installed.so");
  const auto error = LoadFailure(missing);
  ASSERT_FALSE(error.empty()) << "expected the load to fail: " << missing << " does not exist";
  EXPECT_NE(error.find("libpti_preload_core_not_installed.so"), std::string::npos) << error;
}

TEST(DependencySearchDirs, AreTheModuleDirectoryAndItsParent) {
  const auto dirs = pti::DependencySearchDirs();
  ASSERT_EQ(dirs.size(), 2U);
  // Where the Core library is deployed, then one level up. Neither carries a trailing
  // separator, or the parent of the first would be the first itself.
  EXPECT_EQ(dirs[0].filename().string(), std::string{pti::strings::kModuleSubdir});
  EXPECT_EQ(dirs[1], dirs[0].parent_path());
  EXPECT_NE(dirs[1], dirs[0]);
}

TEST(MissingSonameFromLoaderError, ExtractsTheSonameTheLoaderCouldNotFind) {
  EXPECT_EQ(pti::MissingSonameFromLoaderError(
                "libxptifw.so: cannot open shared object file: No such file or directory"),
            "libxptifw.so");
}

TEST(MissingSonameFromLoaderError, IgnoresAFailureThatIsNotAMissingDependency) {
  EXPECT_TRUE(
      pti::MissingSonameFromLoaderError("/opt/pti/lib/pti/libpti.so: undefined symbol: ptiFake")
          .empty());
  EXPECT_TRUE(
      pti::MissingSonameFromLoaderError("/opt/pti/lib/pti/libpti.so: wrong ELF class: ELFCLASS32")
          .empty());
  EXPECT_TRUE(pti::MissingSonameFromLoaderError("").empty());
}

TEST(MissingSonameFromLoaderError, IgnoresAPathBecauseThereIsNoSonameToPreload) {
  // What the loader reports when the file we named is the one it could not open.
  // Pre-loading a path satisfies no NEEDED entry, and retrying it would only loop.
  EXPECT_TRUE(pti::MissingSonameFromLoaderError("/opt/pti/lib/pti/libpti.so: cannot open shared "
                                                "object file: No such file or directory")
                  .empty());
  EXPECT_TRUE(pti::MissingSonameFromLoaderError(
                  ": cannot open shared object file: No such file or directory")
                  .empty());
}

}  // namespace
