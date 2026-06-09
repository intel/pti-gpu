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

#include <gtest/gtest.h>

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"

constexpr size_t kBufferSize = 64 * 1024;

namespace {

void BufferRequested(unsigned char** buf, size_t* size) {
  *buf = static_cast<unsigned char*>(std::malloc(kBufferSize));
  *size = (*buf == nullptr) ? 0 : kBufferSize;
}

void BufferCompleted(unsigned char* buf, size_t /*size*/, size_t /*used*/) { std::free(buf); }

}  // namespace

//-----------------------------------------------------------------------------
// Test Fixture
//-----------------------------------------------------------------------------

class PcSamplingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create device list to use for configuration
    ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);
    uint32_t driver_count = 0;
    ASSERT_EQ(zeDriverGet(&driver_count, nullptr), ZE_RESULT_SUCCESS);
    if (driver_count == 0) {
      GTEST_SKIP() << "No L0 driver";
    }

    std::vector<ze_driver_handle_t> drivers(driver_count);
    ASSERT_EQ(zeDriverGet(&driver_count, drivers.data()), ZE_RESULT_SUCCESS);
    ASSERT_FALSE(drivers.empty());
    uint32_t device_count = 0;
    ASSERT_EQ(zeDeviceGet(drivers[0], &device_count, nullptr), ZE_RESULT_SUCCESS);
    ASSERT_GT(device_count, 0) << "No L0 GPU device found under driver";

    devices_.resize(device_count);
    ASSERT_EQ(zeDeviceGet(drivers[0], &device_count, devices_.data()), ZE_RESULT_SUCCESS);

    ASSERT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  }

  void TearDown() override {
    // Clear kernel info storage between tests
    pti::pc_sampling::KernelInfoStorage::Instance().Clear();
    // Disable views to ensure no callbacks are active after test completion
    // No need to check the return status here since we don't want to fail skipped tests
    ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
    ptiFlushAllViews();
  }

 public:
  std::vector<ze_device_handle_t> devices_;
};

//-----------------------------------------------------------------------------
// Handle Lifecycle Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, EnableRejectsNullHandle) {
  EXPECT_EQ(ptiPcSamplingEnable(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

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

TEST_F(PcSamplingTest, RejectsForeignHandle) {
  _pti_pc_sampling_handle_t foreign_handle;

  EXPECT_EQ(ptiPcSamplingConfigure(&foreign_handle, nullptr, 0, 0), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingStartCollection(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(&foreign_handle), PTI_ERROR_BAD_ARGUMENT);
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

  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(nullptr, nullptr));
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, nullptr));

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device));

  handle->configured_devices_.push_back(configured_device);
  EXPECT_TRUE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device));

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, RejectsDeviceFilteredConfiguration) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, &configured_device, 1, 0),
            PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);

  pti_device_handle_t configured_devices[] = {
      reinterpret_cast<pti_device_handle_t>(0x1),
      reinterpret_cast<pti_device_handle_t>(0x2),
  };
  EXPECT_EQ(ptiPcSamplingConfigure(handle, configured_devices, 2, 0),
            PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Sampling Period Configuration Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, ConfigureUsesDefaultSamplingPeriodWhenZero) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  auto device =
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, 0), PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns_, pti::pc_sampling::kDefaultSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, ConfigurePreservesExplicitSamplingPeriod) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  constexpr uint32_t kExplicitSamplingPeriodNs = 42'000;
  size_t device_count = devices_.size() > 0 ? 1 : 0;
  auto device =
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, kExplicitSamplingPeriodNs),
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

  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, RejectsInvalidOrdering) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStopCollection(handle), PTI_ERROR_PC_SAMPLING_NOT_STARTED);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  auto device =
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr;

  // passing non-null device list but device count as zero should be rejected as bad argument
  EXPECT_EQ(ptiPcSamplingConfigure(handle, &device, 0, 0), PTI_ERROR_BAD_ARGUMENT);

  ASSERT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, 0), PTI_SUCCESS);

  size_t profiled_device_count = 0;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

  EXPECT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STARTED);
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);
  EXPECT_EQ(ptiPcSamplingStopCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED);
  EXPECT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0),
            PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED);
  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Empty Query Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, LifecycleSupportsEmptyQueries) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  size_t buffer_size = 0;
  EXPECT_EQ(ptiPcSamplingQueryCollectionBufferSize(handle, &buffer_size),
            PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_EQ(ptiPcSamplingSetCollectionBufferSize(handle, 4096), PTI_ERROR_NOT_IMPLEMENTED);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  auto device =
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, 0), PTI_SUCCESS);
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

TEST_F(PcSamplingTest, ReturnsEmptyAllDeviceQueryAfterStoppedCollection) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
  size_t device_count = devices_.size() > 0 ? 1 : 0;
  auto device =
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr;
  ASSERT_EQ(ptiPcSamplingConfigure(handle, &device, device_count, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t profiled_device_count = 1;
  EXPECT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count), PTI_SUCCESS);
  EXPECT_EQ(profiled_device_count, 1u);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Error String Conversion Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, ResultTypeToStringSupportsPcSamplingSpecificErrors) {
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

//-----------------------------------------------------------------------------
// State String Conversion Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, PcSamplingStateToStringReturnsCorrectStrings) {
  using pti::pc_sampling::PcSamplingState;
  using pti::pc_sampling::PcSamplingStateToString;

  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kEnabled), "ENABLED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kConfigured), "CONFIGURED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kStarted), "STARTED");
  EXPECT_STREQ(PcSamplingStateToString(PcSamplingState::kStopped), "STOPPED");
}
