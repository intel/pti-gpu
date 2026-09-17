//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

//
// Tests for the INTEL_LIBITTNOTIFY64 diagnostics in src/itt/itt_lib_env.h.
//
// These exercise the decision logic only. The test binary path stands in for
// libpti_view.so, allowing exact and equivalent path handling to be tested
// without invoking the ITT runtime.
//
// Deliberately a separate binary from itt_test_suite: nothing here needs SYCL,
// pti_view or Level Zero, and no ITT call is ever made. The decision helper
// retains no process-local state, so these cases work both as individually
// discovered CTests and as one standalone GoogleTest process.
// Keeping these tests in their own binary also keeps them independent of
// ittnotify's collector-present latch in itt_test_suite.
//

#include "itt/itt_lib_env.h"

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <string>
#include <system_error>

#include "pti_filesystem.h"
#include "utils.h"

namespace {

class IttLibEnvTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char *current = std::getenv(itt_lib_env::kIttLibEnvVarName);
    had_env_var_ = (current != nullptr);
    if (had_env_var_) {
      saved_env_var_ = current;
    }
    self_path_ = utils::GetPathToSharedObject(&IttLibEnvTest::SelfMarker);
    if (!self_path_.empty()) {
      std::error_code err;
      auto abs = pti::utils::filesystem::absolute(self_path_, err);
      if (!err) self_path_ = abs.string();
    }
  }

  void TearDown() override {
    // Best effort: leave no trace even if a test body bailed out on an
    // assertion before reaching its own cleanup.
    std::error_code err;
    pti::utils::filesystem::remove(SelfLinkPath(), err);

    if (had_env_var_) {
      utils::SetEnv(itt_lib_env::kIttLibEnvVarName, saved_env_var_.c_str());
    } else {
      ASSERT_EQ(::unsetenv(itt_lib_env::kIttLibEnvVarName), 0);
    }
  }

  static pti::utils::filesystem::path SelfLinkPath() {
    std::error_code err;
    const auto temp_dir = pti::utils::filesystem::temp_directory_path(err);
    if (err) {
      return {};
    }
    return temp_dir / ("itt_lib_env_self_link_" + std::to_string(::getpid()) + ".so");
  }

  // Address inside this test binary used to resolve self_path_.
  static void SelfMarker() {}

  std::string self_path_;
  std::string saved_env_var_;
  bool had_env_var_ = false;
};

TEST_F(IttLibEnvTest, SelfRegistersWhenVariableUnset) {
  ASSERT_EQ(::unsetenv(itt_lib_env::kIttLibEnvVarName), 0);
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector(self_path_),
            itt_lib_env::IttCollectorConfig::kSetByPti);
  EXPECT_FALSE(::utils::GetEnv(itt_lib_env::kIttLibEnvVarName).empty());
}

TEST_F(IttLibEnvTest, EmptyModulePathReportsConfigurationFailure) {
  ASSERT_EQ(::unsetenv(itt_lib_env::kIttLibEnvVarName), 0);
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector({}),
            itt_lib_env::IttCollectorConfig::kConfigurationFailed);
  EXPECT_TRUE(::utils::GetEnv(itt_lib_env::kIttLibEnvVarName).empty());
}

TEST_F(IttLibEnvTest, DoesNotOverwriteWhenVariableAlreadySet) {
  ASSERT_FALSE(self_path_.empty()) << "could not resolve this binary's own path";
  utils::SetEnv(itt_lib_env::kIttLibEnvVarName, self_path_.c_str());
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector(self_path_),
            itt_lib_env::IttCollectorConfig::kAlreadyPti);
  EXPECT_EQ(::utils::GetEnv(itt_lib_env::kIttLibEnvVarName), self_path_);
}

TEST_F(IttLibEnvTest, NonExistentPathReportsNotFound) {
  utils::SetEnv(itt_lib_env::kIttLibEnvVarName, "/nonexistent/libpti_view.so");
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector(self_path_),
            itt_lib_env::IttCollectorConfig::kExternalNotFound);
}

TEST_F(IttLibEnvTest, ForeignLibraryReportsNotThisModule) {
  // The stale/foreign-collector case: an existing library that is not us.
  // ctest points the variable at libpti_view.so, which serves here as a real
  // shared object that differs from this test binary.
  if (!had_env_var_ || saved_env_var_.empty()) {
    GTEST_SKIP() << itt_lib_env::kIttLibEnvVarName
                 << " is not set; no foreign ITT library available to test with";
  }
  ASSERT_NE(saved_env_var_, self_path_);
  utils::SetEnv(itt_lib_env::kIttLibEnvVarName, saved_env_var_.c_str());
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector(self_path_),
            itt_lib_env::IttCollectorConfig::kExternalLibrary);
}

TEST_F(IttLibEnvTest, SymlinkToThisModuleComparesEqual) {
  // The property that motivates filesystem::equivalent over string comparison:
  // vars.sh resolves through readlink, users type whatever they like, and the
  // loader reports its own spelling (libpti_view.so vs .so.1 vs .so.1.0.1).
  ASSERT_FALSE(self_path_.empty());
  const auto link_path = SelfLinkPath();
  ASSERT_FALSE(link_path.empty()) << "no usable temporary directory";

  std::error_code err;
  // dladdr reports the path as spelled on the command line, often relative. The
  // link lives elsewhere, so its target has to be absolute or it would dangle.
  const auto self_absolute = pti::utils::filesystem::absolute(self_path_, err);
  ASSERT_FALSE(err) << "could not absolutize " << self_path_ << ": " << err.message();
  pti::utils::filesystem::remove(link_path, err);
  pti::utils::filesystem::create_symlink(self_absolute, link_path, err);
  ASSERT_FALSE(err) << "could not create symlink: " << err.message();

  utils::SetEnv(itt_lib_env::kIttLibEnvVarName, link_path.c_str());
  EXPECT_EQ(itt_lib_env::ConfigureIttCollector(self_path_),
            itt_lib_env::IttCollectorConfig::kAlreadyPti)
      << "a symlink to this module must not be reported as a different module";
}

}  // namespace
