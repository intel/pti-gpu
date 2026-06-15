//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pc_sampling/pti_pc_sampling_raw_data_file.h"
#include "pti/pti_pc_sampling.h"
#include "utils/pti_filesystem.h"

TEST(PcSamplingBasicTest, EnableRejectsNullHandle) {
  EXPECT_EQ(ptiPcSamplingEnable(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST(PcSamplingBasicTest, IsConfiguredDeviceRejectsNullHandle) {
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(nullptr, nullptr));
}

TEST(PcSamplingBasicTest, IsConfiguredDeviceRejectsNullDevice) {
  _pti_pc_sampling_handle_t handle_storage;
  pti_pc_sampling_handle_t handle = &handle_storage;

  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, nullptr));
}

TEST(PcSamplingBasicTest, RejectsForeignHandle) {
  _pti_pc_sampling_handle_t foreign_handle;

  EXPECT_EQ(ptiPcSamplingConfigure(&foreign_handle, nullptr, 0, 0), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingStartCollection(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
}

TEST(PcSamplingBasicTest, ResultTypeToStringSupportsPcSamplingSpecificErrors) {
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED),
               "PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED),
               "PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED),
               "PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_ALREADY_STARTED),
               "PTI_ERROR_PC_SAMPLING_ALREADY_STARTED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_NOT_STARTED),
               "PTI_ERROR_PC_SAMPLING_NOT_STARTED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED),
               "PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_NOT_STOPPED),
               "PTI_ERROR_PC_SAMPLING_NOT_STOPPED");
}

TEST(PcSamplingBasicTest, PcSamplingStateToStringReturnsCorrectStrings) {
  using pti::pc_sampling::PcSamplingState;
  using pti::pc_sampling::PcSamplingStateToString;

  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kEnabled), "ENABLED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kConfigured), "CONFIGURED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kStarted), "STARTED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kStopped), "STOPPED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kForceUint32), "FORCE_UINT32");
}

TEST(PcSamplingBasicTest, TempRawDataFileRoundTripsAndRemovesTempFile) {
  pti::pc_sampling::TempRawDataFile raw_data;

  ASSERT_TRUE(raw_data.OpenTemp());
  ASSERT_TRUE(raw_data.IsOpen());
  ASSERT_TRUE(raw_data.HasPath());

  const std::string path = raw_data.path();
  EXPECT_TRUE(pti::utils::filesystem::exists(path));

  constexpr std::array<uint8_t, 4> expected_data = {1, 2, 3, 4};
  ASSERT_TRUE(raw_data.Append(expected_data.data(), expected_data.size()));
  EXPECT_EQ(raw_data.size(), expected_data.size());

  ASSERT_TRUE(raw_data.Finalize());
  EXPECT_FALSE(raw_data.IsOpen());
  EXPECT_TRUE(pti::utils::filesystem::exists(path));

  std::vector<uint8_t> actual_data;
  ASSERT_TRUE(raw_data.ReadAll(&actual_data));
  EXPECT_EQ(actual_data.size(), expected_data.size());
  EXPECT_TRUE(std::equal(actual_data.begin(), actual_data.end(), expected_data.begin()));

  raw_data.Reset();
  EXPECT_FALSE(raw_data.IsOpen());
  EXPECT_FALSE(raw_data.HasPath());
  EXPECT_EQ(raw_data.size(), 0U);
  EXPECT_FALSE(pti::utils::filesystem::exists(path));
}
