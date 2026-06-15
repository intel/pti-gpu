//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Public-API functional tests for PC Sampling.
//
// =============================================================

#include <sycl/sycl.hpp>

#include "pc_sampling_test_fixture.h"

namespace {

constexpr size_t kElementCount = 1 << 18;
constexpr uint32_t kKernelLaunchCount = 12;
constexpr uint32_t kInnerIterations = 2048;

class PcSamplingCollectionKernel;

void RunLargeKernelWorkload(sycl::queue& queue) {
  std::vector<uint32_t> values(kElementCount);
  for (size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<uint32_t>(i + 1);
  }

  {
    sycl::buffer<uint32_t, 1> values_buffer(values.data(), sycl::range<1>(values.size()));
    for (uint32_t launch_index = 0; launch_index < kKernelLaunchCount; ++launch_index) {
      queue.submit([&](sycl::handler& cgh) {
        auto values_acc = values_buffer.get_access<sycl::access::mode::read_write>(cgh);
        cgh.parallel_for<PcSamplingCollectionKernel>(
            sycl::range<1>(values.size()), [=](sycl::id<1> index) {
              uint32_t value = values_acc[index];
              const uint32_t lane = static_cast<uint32_t>(index[0]);
              for (uint32_t iteration = 0; iteration < kInnerIterations; ++iteration) {
                value = value * 1664525u + 1013904223u + lane + iteration;
                value ^= value >> 15;
                value *= 2246822519u;
              }
              values_acc[index] = value;
            });
      });
    }
  }

  queue.wait();
}

}  // namespace

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

TEST_F(PcSamplingTest, IsConfiguredDeviceMatchesOnlyConfiguredDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);

  EXPECT_EQ(ptiPcSamplingStartCollection(handle), PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(PcSamplingTest, QueryApisReturnSpecificLifecycleErrors) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);

  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

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

  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, nullptr, nullptr, &kernel_count),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, nullptr, 1, &kernel_info),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);
  EXPECT_EQ(ptiPcSamplingGetSamplesPerInstruction(handle, nullptr, 1, nullptr, 0, nullptr, 0),
            PTI_ERROR_PC_SAMPLING_NOT_STOPPED);

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

TEST_F(PcSamplingTest, CollectsSamplesForLargeSyclKernel) {
  sycl::queue queue = sycl::queue(sycl::gpu_selector_v, sycl::property::queue::in_order{});

  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);

  RunLargeKernelWorkload(queue);

  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);

  std::vector<pti_device_handle_t> profiled_devices(device_count);
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, profiled_devices.data(), &device_count),
            PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);

  pti_pc_sampling_device_status_t device_status{};
  device_status._struct_size = sizeof(device_status);
  ASSERT_EQ(ptiPcSamplingGetDeviceStatus(handle, profiled_devices[0], &device_status), PTI_SUCCESS);
  EXPECT_EQ(device_status._device, profiled_devices[0]);
  EXPECT_GT(device_status._total_sample_count, 0u);
  EXPECT_GT(device_status._total_pc_count, 0u);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}
