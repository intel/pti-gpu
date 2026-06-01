//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pti/pti_pc_sampling.h"

TEST(PcSamplingTest, EnableRejectsNullHandle) {
  EXPECT_EQ(ptiPcSamplingEnable(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST(PcSamplingTest, EnableRejectsSecondLiveHandle) {
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

TEST(PcSamplingTest, RejectsForeignHandle) {
  _pti_pc_sampling_handle_t foreign_handle;

  EXPECT_EQ(ptiPcSamplingConfigure(&foreign_handle, nullptr, 0, 0), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingStartCollection(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
}

TEST(PcSamplingTest, IsConfiguredDeviceMatchesOnlyConfiguredDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(nullptr, nullptr));
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, nullptr));

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device));

  handle->configured_devices.push_back(configured_device);
  EXPECT_TRUE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device));

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, GetObservedKernelInfoReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED;
  handle->configured_devices.push_back(configured_device);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);

  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, configured_device, 1, &kernel_info),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, GetSamplesPerInstructionReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED;
  handle->configured_devices.push_back(configured_device);

  EXPECT_EQ(
      ptiPcSamplingGetSamplesPerInstruction(handle, configured_device, 1, nullptr, 0, nullptr, 0),
      PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, RejectsDisabledHandleAfterRegistryRemoval) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_ERROR_BAD_ARGUMENT);
}

TEST(PcSamplingTest, ConfigureUsesDefaultSamplingPeriodWhenZero) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns, kDefaultSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, ConfigurePreservesExplicitSamplingPeriod) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  constexpr uint32_t kExplicitSamplingPeriodNs = 42'000;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, kExplicitSamplingPeriodNs), PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns, kExplicitSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, QueryApisReturnSpecificLifecycleErrors) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
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

  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, LifecycleSupportsEmptyQueries) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  size_t buffer_size = 0;
  EXPECT_EQ(ptiPcSamplingQueryCollectionBufferSize(handle, &buffer_size),
            PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_EQ(ptiPcSamplingSetCollectionBufferSize(handle, 4096), PTI_ERROR_NOT_IMPLEMENTED);

  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t reason_count = 1;
  EXPECT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  EXPECT_EQ(reason_count, 0u);

  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 0u);

  size_t kernel_count = 1;
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, nullptr, nullptr, &kernel_count),
            PTI_ERROR_BAD_ARGUMENT);

  pti_pc_sampling_device_status_t device_status{};
  device_status._struct_size = sizeof(device_status);
  EXPECT_EQ(ptiPcSamplingGetDeviceStatus(handle, nullptr, &device_status),
            PTI_ERROR_NOT_IMPLEMENTED);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, nullptr, 1, &kernel_info),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingGetSamplesPerInstruction(handle, nullptr, 1, nullptr, 0, nullptr, 0),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, RejectsInvalidOrdering) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStopCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_STARTED);

  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 1, 0), PTI_ERROR_NOT_IMPLEMENTED);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  size_t device_count = 0;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0),
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

TEST(PcSamplingTest, ResultTypeToStringSupportsPcSamplingSpecificErrors) {
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

TEST(PcSamplingTest, RejectsDeviceFilteredConfiguration) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, &configured_device, 1, 0), PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_TRUE(handle->configured_devices.empty());

  pti_device_handle_t configured_devices[] = {
      reinterpret_cast<pti_device_handle_t>(0x1),
      reinterpret_cast<pti_device_handle_t>(0x2),
  };
  EXPECT_EQ(ptiPcSamplingConfigure(handle, configured_devices, 2, 0), PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_TRUE(handle->configured_devices.empty());

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, GetProfiledDevicesReturnsConfiguredSingleDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->configured_devices.push_back(configured_device);
  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED;

  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);

  pti_device_handle_t profiled_device = nullptr;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, &profiled_device, &device_count), PTI_SUCCESS);
  EXPECT_EQ(device_count, 1u);
  EXPECT_EQ(profiled_device, configured_device);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST(PcSamplingTest, ReturnsEmptyAllDeviceQueryAfterStoppedCollection) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t device_count = 1;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  EXPECT_EQ(device_count, 0u);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}
