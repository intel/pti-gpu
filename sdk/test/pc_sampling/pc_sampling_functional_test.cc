//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Unit tests for PC Sampling feature.
// Tests cover:
//   - Handle lifecycle management
//   - State machine transitions
//   - Configuration validation
//   - Kernel info storage
//
// =============================================================

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pc_sampling_test_fixture.h"
#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"

namespace {

//-----------------------------------------------------------------------------
// Handle Lifecycle Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, EnableRejectsSecondLiveHandle) {
  pti_pc_sampling_handle_t first_handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&first_handle), PTI_SUCCESS);

  pti_pc_sampling_handle_t second_handle = nullptr;
  EXPECT_EQ(ptiPcSamplingEnable(&second_handle), PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED);
  EXPECT_EQ(second_handle, nullptr);

  EXPECT_EQ(ptiPcSamplingDisable(first_handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingEnable(&second_handle), PTI_SUCCESS);
  EXPECT_NE(second_handle, nullptr);
  EXPECT_EQ(ptiPcSamplingDisable(second_handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, RejectsDisabledHandleAfterRegistryRemoval) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_ERROR_BAD_ARGUMENT);
}

//-----------------------------------------------------------------------------
// Device Configuration Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, IsConfiguredDeviceMatchesOnlyConfiguredDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(PcSamplingTest, RejectsDeviceFilteredConfigurationForUnsupportedDevices) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device[1] = {reinterpret_cast<pti_device_handle_t>(0x1)};
  EXPECT_EQ(ptiPcSamplingConfigure(handle, configured_device, 1, 0),
            PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);

  handle->configured_devices_.push_back(configured_device[0]);
  EXPECT_TRUE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device[0]));

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Sampling Period Configuration Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, ConfigureUsesDefaultSamplingPeriodWhenZero) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, 0), PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns_, pti::pc_sampling::kDefaultSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, ConfigurePreservesExplicitSamplingPeriod) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  constexpr uint32_t kExplicitSamplingPeriodNs = 42'000;
  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, kExplicitSamplingPeriodNs),
            PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns_, kExplicitSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Kernel Info Query Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, GetObservedKernelInfoReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;
  handle->configured_devices_.push_back(configured_device);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);

  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, configured_device, 1, &kernel_info),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetSamplesPerInstructionReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;
  handle->configured_devices_.push_back(configured_device);

  EXPECT_EQ(
      ptiPcSamplingGetSamplesPerInstruction(handle, configured_device, 1, nullptr, 0, nullptr, 0),
      PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// State Machine / Lifecycle Error Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, QueryApisReturnSpecificLifecycleErrors) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  // if device list is empty or null, configuration should succeed and all supported devices will be
  // profiled;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  size_t reason_count = 0;
  EXPECT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  size_t kernel_count = 0;
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, nullptr, nullptr, &kernel_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, nullptr, 1, &kernel_info),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetSamplesPerInstruction(handle, nullptr, 1, nullptr, 0, nullptr, 0),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, nullptr, nullptr, &kernel_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, nullptr, 1, &kernel_info),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetSamplesPerInstruction(handle, nullptr, 1, nullptr, 0, nullptr, 0),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Empty Query Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, LifecycleSupportsEmptyQueries) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  size_t buffer_size = 0;
  EXPECT_EQ(ptiPcSamplingQueryCollectionBufferSize(handle, &buffer_size),
            PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_EQ(ptiPcSamplingSetCollectionBufferSize(handle, 4096), PTI_ERROR_NOT_IMPLEMENTED);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, 0), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t reason_count = 1;
  EXPECT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  EXPECT_EQ(reason_count, 0u);

  size_t profiled_device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count), PTI_SUCCESS);
  ASSERT_EQ(profiled_device_count, 1u);

  size_t kernel_count = 1;
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, nullptr, nullptr, &kernel_count),
            PTI_ERROR_BAD_ARGUMENT);

  pti_pc_sampling_device_status_t device_status{};
  device_status._struct_size = sizeof(device_status);
  EXPECT_EQ(ptiPcSamplingGetDeviceStatus(handle, nullptr, &device_status), PTI_ERROR_BAD_ARGUMENT);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, nullptr, 1, &kernel_info),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingGetSamplesPerInstruction(handle, nullptr, 1, nullptr, 0, nullptr, 0),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetProfiledDevicesReturnsConfiguredSingleDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->configured_devices_.push_back(configured_device);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;

  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);

  pti_device_handle_t profiled_device = nullptr;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, &profiled_device, &device_count), PTI_SUCCESS);
  EXPECT_EQ(device_count, 1u);
  EXPECT_EQ(profiled_device, configured_device);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, RejectsInvalidOrdering) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStopCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_STARTED);

  pti_device_handle_t device_handle[1] = {reinterpret_cast<pti_device_handle_t>(devices_.front())};
  EXPECT_EQ(ptiPcSamplingConfigure(handle, device_handle, 0, 0), PTI_ERROR_BAD_ARGUMENT);

  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, 1, 0), PTI_SUCCESS);

  size_t device_count = 0;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  EXPECT_EQ(ptiPcSamplingConfigure(handle, device_handle, 1, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STARTED);
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);
  EXPECT_EQ(ptiPcSamplingStopCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, ReturnsEmptyAllDeviceQueryAfterStoppedCollection) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t profiled_device_count = 1;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count), PTI_SUCCESS);
  EXPECT_EQ(profiled_device_count, 1u);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, ReturnsConfiguredDeviceAfterStoppedCollection) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t device_handle[1] = {reinterpret_cast<pti_device_handle_t>(devices_.front())};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, 1, 0), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t profiled_device_count = 1;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count), PTI_SUCCESS);
  EXPECT_EQ(profiled_device_count, 1u);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

}  // namespace
