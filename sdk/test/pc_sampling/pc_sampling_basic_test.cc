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

TEST(PcSamplingBasicTest, ValidateStartableHandleRejectsNullAndForeignHandles) {
  using pti::pc_sampling::PcSamplingState;

  EXPECT_EQ(pti::pc_sampling::ValidateStartableHandle(nullptr), PTI_ERROR_BAD_ARGUMENT);

  _pti_pc_sampling_handle_t foreign_handle;
  EXPECT_EQ(pti::pc_sampling::ValidateStartableHandle(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);

  // A rejected handle must not pick up the default configuration that
  // ptiPcSamplingStartCollection applies when ptiPcSamplingConfigure was skipped.
  EXPECT_EQ(ptiPcSamplingStartCollection(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(foreign_handle.state_, PcSamplingState::kEnabled);
  EXPECT_TRUE(foreign_handle.configured_devices_.empty());
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
  EXPECT_STREQ(ptiResultTypeToString(PTI_ERROR_PC_SAMPLING_CONFIGURATION_FAIL),
               "PTI_ERROR_PC_SAMPLING_CONFIGURATION_FAIL");
  EXPECT_STREQ(ptiResultTypeToString(PTI_WARN_PC_SAMPLING_NO_DATA_COLLECTED),
               "PTI_WARN_PC_SAMPLING_NO_DATA_COLLECTED");
  EXPECT_STREQ(ptiResultTypeToString(PTI_WARN_PC_SAMPLING_BUFFER_SIZE_OVERRIDDEN_BY_ENV),
               "PTI_WARN_PC_SAMPLING_BUFFER_SIZE_OVERRIDDEN_BY_ENV");
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

TEST(PcSamplingBasicTest, TempRawDataFileReadRangeReadsRequestedSlice) {
  pti::pc_sampling::TempRawDataFile raw_data;
  ASSERT_TRUE(raw_data.OpenTemp());

  constexpr std::array<uint8_t, 8> full = {5, 6, 7, 8, 9, 10, 11, 12};
  ASSERT_TRUE(raw_data.Append(full.data(), full.size()));
  ASSERT_TRUE(raw_data.Finalize());

  std::array<uint8_t, 3> slice = {};
  ASSERT_TRUE(raw_data.ReadRange(2, slice.size(), slice.data()));
  EXPECT_EQ(slice, (std::array<uint8_t, 3>{7, 8, 9}));

  EXPECT_TRUE(raw_data.ReadRange(full.size(), 0, nullptr));
  EXPECT_FALSE(raw_data.ReadRange(full.size() - 1, 2, slice.data()));
  EXPECT_FALSE(raw_data.ReadRange(0, 1, nullptr));
}

TEST(PcSamplingBasicTest, ResolveKernelNameReturnsUnknownForNullHandle) {
  EXPECT_STREQ(pti::pc_sampling::ResolveKernelName(nullptr), "<unknown>");
}

TEST(PcSamplingBasicTest, KernelAddressMaskUsesProductConfig) {
  using pti::pc_sampling::GetKernelAddressMask;

  EXPECT_EQ(GetKernelAddressMask(std::nullopt), std::nullopt);
  EXPECT_EQ(GetKernelAddressMask(0u), std::nullopt);
  EXPECT_EQ(GetKernelAddressMask(1u), 0xffffffffull);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kBmgIpVersion), 0xffffffffull);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kNvlPIpVersion - 1), 0xffffffffull);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kNvlPIpVersion), (1ull << 48) - 1);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kNvlPIpVersion + 1), (1ull << 48) - 1);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kCriIpVersion - 1), (1ull << 48) - 1);
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kCriIpVersion), (std::numeric_limits<uint64_t>::max)());
  EXPECT_EQ(GetKernelAddressMask(utils::ze::kCriIpVersion + 1),
            (std::numeric_limits<uint64_t>::max)());
}

TEST(PcSamplingBasicTest, AddressNormalizationHandlesLimits) {
  using pti::pc_sampling::NormalizeInstructionPointer;
  using pti::pc_sampling::NormalizeKernelAddress;

  constexpr uint64_t full_mask = (std::numeric_limits<uint64_t>::max)();
  for (uint64_t address_mask :
       std::array<uint64_t, 3>{0xffffffffull, (1ull << 48) - 1, full_mask}) {
    SCOPED_TRACE(address_mask);
    EXPECT_EQ(NormalizeInstructionPointer(0, address_mask), 0u);
    EXPECT_EQ(NormalizeInstructionPointer(0x1fffffffffffffffull, address_mask),
              address_mask & ~7ull);
    EXPECT_EQ(NormalizeKernelAddress(full_mask, address_mask), address_mask);
  }
  EXPECT_EQ(NormalizeInstructionPointer(0x1fffffffull, 0xffffffffull), 0xfffffff8ull);
}

TEST(PcSamplingBasicTest, KernelLookupMatchesSamplingAddressFormats) {
  using pti::pc_sampling::KernelBinaryInfo;
  using pti::pc_sampling::NormalizeInstructionPointer;

  struct AddressCase {
    uint64_t mask;
    uint64_t sampled_ip;
    uint64_t expected_base;
  };
  constexpr std::array<AddressCase, 4> cases = {{
      {0xffffffffull, 0xff601880ull >> 3, 0xff601800ull},
      {(1ull << 48) - 1, 0xffff8000ff601880ull >> 3, 0x8000ff601800ull},
      {(std::numeric_limits<uint64_t>::max)(), 0x8000ff601880ull >> 3, 0x8000ff601800ull},
      {0xffffffffull, 0xff601880ull >> 3, 0xff601800ull},
  }};

  auto& storage = pti::pc_sampling::KernelInfoStorage::Instance();
  storage.Clear();
  KernelBinaryInfo kernel;
  kernel.kernel_base_address_ = 0x8000ff601800ull;
  kernel.kernel_binary_size_ = 0x100;
  storage.Add(kernel);

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.mask);
    storage.SetAddressMask(test_case.mask);
    EXPECT_EQ(storage.GetAddressMask(), test_case.mask);
    EXPECT_EQ(storage.Size(), 1u);

    const uint64_t ip = NormalizeInstructionPointer(test_case.sampled_ip, storage.GetAddressMask());
    EXPECT_EQ(ip, test_case.expected_base + 0x80);
    EXPECT_TRUE(storage.FindByIpAddress(ip).has_value());
    EXPECT_TRUE(storage.FindByIpAddress(test_case.expected_base).has_value());
    EXPECT_TRUE(storage.FindByIpAddress(test_case.expected_base + kernel.kernel_binary_size_ - 1)
                    .has_value());
    EXPECT_FALSE(storage.FindByIpAddress(test_case.expected_base - 1).has_value());
    EXPECT_FALSE(
        storage.FindByIpAddress(test_case.expected_base + kernel.kernel_binary_size_).has_value());
    if (test_case.expected_base != kernel.kernel_base_address_) {
      EXPECT_FALSE(storage.FindByIpAddress(kernel.kernel_base_address_ + 0x80).has_value());
    }
  }

  storage.Clear();
  EXPECT_EQ(storage.Size(), 0u);
  EXPECT_EQ(storage.GetAddressMask(), (std::numeric_limits<uint64_t>::max)());
  EXPECT_FALSE(storage.FindByIpAddress(cases.back().expected_base + 0x80).has_value());
  storage.Add(kernel);
  EXPECT_TRUE(storage.FindByIpAddress(kernel.kernel_base_address_).has_value());
  storage.Clear();
}

TEST(PcSamplingBasicTest, KernelLookupSnapshotSurvivesReplacementAndClear) {
  using pti::pc_sampling::KernelBinaryInfo;
  auto& storage = pti::pc_sampling::KernelInfoStorage::Instance();
  storage.Clear();
  storage.SetAddressMask(0xffffffffull);

  KernelBinaryInfo kernel;
  kernel.kernel_base_address_ = 0x8000ff601800ull;
  kernel.kernel_binary_size_ = 0x100;
  kernel.kernel_name_ = "original";
  storage.Add(kernel);
  const auto original = storage.FindByIpAddress(0xff601880ull);

  kernel.kernel_binary_size_ = 0x200;
  kernel.kernel_name_ = "replacement";
  storage.Add(kernel);
  EXPECT_EQ(storage.Size(), 1u);
  const auto replacement = storage.FindByIpAddress(0xff601980ull);
  storage.Clear();

  ASSERT_TRUE(original.has_value());
  EXPECT_EQ(original->kernel_base_address_, 0xff601800ull);
  EXPECT_EQ(original->kernel_binary_size_, 0x100u);
  EXPECT_STREQ(original->kernel_name_, "original");
  ASSERT_TRUE(replacement.has_value());
  EXPECT_EQ(replacement->kernel_base_address_, 0xff601800ull);
  EXPECT_EQ(replacement->kernel_binary_size_, 0x200u);
  EXPECT_STREQ(replacement->kernel_name_, "replacement");
}
