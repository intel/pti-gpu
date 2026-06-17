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
class PcSamplingCollectionKernelAlt;

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

void RunAlternateKernelWorkload(sycl::queue& queue) {
  std::vector<uint32_t> values(kElementCount);
  for (size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<uint32_t>((i % 97) + 3);
  }

  {
    sycl::buffer<uint32_t, 1> values_buffer(values.data(), sycl::range<1>(values.size()));
    for (uint32_t launch_index = 0; launch_index < kKernelLaunchCount; ++launch_index) {
      queue.submit([&](sycl::handler& cgh) {
        auto values_acc = values_buffer.get_access<sycl::access::mode::read_write>(cgh);
        cgh.parallel_for<PcSamplingCollectionKernelAlt>(
            sycl::range<1>(values.size()), [=](sycl::id<1> index) {
              uint32_t value = values_acc[index];
              const uint32_t lane = static_cast<uint32_t>(index[0]);
              for (uint32_t iteration = 0; iteration < kInnerIterations; ++iteration) {
                value += (lane ^ iteration) + 0x9e3779b9u;
                value = (value << 7) | (value >> 25);
                value ^= lane * 40503u + iteration;
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
  // No kernels profiled, we expect a valid, empty results.
  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  // After a real collection the stall-reason table is derived from the
  // EuStallSampling metric group and must be non-empty.
  size_t reason_count = 0;
  EXPECT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  EXPECT_GT(reason_count, 0u);

  size_t profiled_device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &profiled_device_count), PTI_SUCCESS);
  ASSERT_EQ(profiled_device_count, 1u);

  size_t kernel_count = 1;
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device_handle[0], nullptr, &kernel_count),
            PTI_SUCCESS);
  EXPECT_EQ(kernel_count, 0u);

  pti_pc_sampling_device_status_t device_status{};
  device_status._struct_size = sizeof(device_status);
  EXPECT_EQ(ptiPcSamplingGetDeviceStatus(handle, device_handle[0], &device_status), PTI_SUCCESS);
  EXPECT_EQ(device_status._device, device_handle[0]);
  EXPECT_EQ(device_status._samples_dropped, 0u);
  EXPECT_EQ(device_status._total_sample_count, 0u);
  EXPECT_EQ(device_status._total_pc_count, 0u);

  kernel_count = 10;
  EXPECT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device_handle[0], nullptr, &kernel_count),
            PTI_SUCCESS);
  EXPECT_EQ(kernel_count, 0u);

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

TEST_F(PcSamplingTest, AggregatesPerKernelAndPerInstructionData) {
  sycl::queue queue = sycl::queue(sycl::gpu_selector_v, sycl::property::queue::in_order{});

  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_NE(handle, nullptr);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  RunLargeKernelWorkload(queue);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  // Profiled device.
  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);
  std::vector<pti_device_handle_t> profiled_devices(device_count);
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, profiled_devices.data(), &device_count),
            PTI_SUCCESS);
  pti_device_handle_t device = profiled_devices[0];

  // Stall reasons (two-call pattern).
  size_t reason_count = 0;
  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  ASSERT_GT(reason_count, 0u);
  std::vector<pti_pc_sampling_stall_reason_info_t> reasons(reason_count);
  for (auto& reason : reasons) {
    reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }
  size_t filled_reason_count = reason_count;
  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, reasons.data(), &filled_reason_count),
            PTI_SUCCESS);
  EXPECT_EQ(filled_reason_count, reason_count);
  for (const auto& reason : reasons) {
    EXPECT_NE(reason._name, nullptr);
    EXPECT_NE(reason._description, nullptr);
  }

  // Device totals.
  pti_pc_sampling_device_status_t device_status{};
  device_status._struct_size = sizeof(device_status);
  ASSERT_EQ(ptiPcSamplingGetDeviceStatus(handle, device, &device_status), PTI_SUCCESS);
  ASSERT_GT(device_status._total_sample_count, 0u);
  ASSERT_GT(device_status._total_pc_count, 0u);

  // Observed kernels (two-call pattern).
  size_t kernel_count = 0;
  ASSERT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device, nullptr, &kernel_count),
            PTI_SUCCESS);
  ASSERT_EQ(kernel_count, 1u);
  std::vector<uint64_t> kernel_handles(kernel_count);
  size_t filled_kernel_count = kernel_count;
  ASSERT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device, kernel_handles.data(),
                                                  &filled_kernel_count),
            PTI_SUCCESS);
  EXPECT_EQ(filled_kernel_count, kernel_count);

  uint64_t device_total_pc_count = 0;
  uint64_t summed_kernel_samples = 0;
  for (uint64_t kernel_handle : kernel_handles) {
    pti_pc_sampling_kernel_info_t kernel_info{};
    kernel_info._struct_size = sizeof(kernel_info);
    std::vector<uint64_t> kernel_aggregated_samples(reason_count, 0);
    kernel_info._aggregated_samples = kernel_aggregated_samples.data();
    ASSERT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, device, kernel_handle, &kernel_info),
              PTI_SUCCESS);
    EXPECT_EQ(kernel_info._device, device);
    EXPECT_EQ(kernel_info._kernel_handle, kernel_handle);
    EXPECT_NE(kernel_info._kernel_name, nullptr);
    EXPECT_EQ(kernel_info._reason_count, reason_count);
    EXPECT_GT(kernel_info._instructions_with_samples_count, 0u);

    const uint64_t kernel_aggregate_total = std::accumulate(
        kernel_aggregated_samples.begin(), kernel_aggregated_samples.end(), uint64_t{0});
    summed_kernel_samples += kernel_aggregate_total;
    device_total_pc_count += kernel_info._instructions_with_samples_count;

    // Per-instruction data.
    const size_t instruction_count = kernel_info._instructions_with_samples_count;
    std::vector<pti_pc_sampling_instruction_t> instructions(instruction_count);
    std::vector<uint64_t> samples(instruction_count * reason_count, 0);
    ASSERT_EQ(
        ptiPcSamplingGetSamplesPerInstruction(handle, device, kernel_handle, instructions.data(),
                                              instructions.size(), samples.data(), samples.size()),
        PTI_SUCCESS);

    // Source correlation is currently not implemented.
    for (const auto& instruction : instructions) {
      EXPECT_EQ(instruction._source_info, nullptr);
    }

    // Per-instruction reason sums must equal the kernel aggregate per reason.
    std::vector<uint64_t> reconstructed(reason_count, 0);
    for (size_t i = 0; i < instruction_count; ++i) {
      bool has_non_zero_sample = false;
      for (size_t j = 0; j < reason_count; ++j) {
        const uint64_t sample_count = samples[i * reason_count + j];
        reconstructed[j] += sample_count;
        has_non_zero_sample = has_non_zero_sample || sample_count != 0;
      }
      EXPECT_TRUE(has_non_zero_sample);
    }
    for (size_t j = 0; j < reason_count; ++j) {
      EXPECT_EQ(reconstructed[j], kernel_aggregated_samples[j]);
    }
  }

  // Sum over kernels must match the device total sample count.
  EXPECT_EQ(summed_kernel_samples, device_status._total_sample_count);
  // Per-kernel instruction counts sum to the device unique-PC count (one bucket
  // per unique mapped PC).
  EXPECT_EQ(device_total_pc_count, device_status._total_pc_count);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, DistinctKernelsEachExposeStallReasonData) {
  sycl::queue queue = sycl::queue(sycl::gpu_selector_v, sycl::property::queue::in_order{});

  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  RunLargeKernelWorkload(queue);
  RunAlternateKernelWorkload(queue);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t reason_count = 0;
  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  ASSERT_GT(reason_count, 0u);

  size_t device_count = 1;
  std::vector<pti_device_handle_t> profiled_devices(1);
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, profiled_devices.data(), &device_count),
            PTI_SUCCESS);
  pti_device_handle_t device = profiled_devices[0];

  size_t kernel_count = 0;
  ASSERT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device, nullptr, &kernel_count),
            PTI_SUCCESS);
  ASSERT_EQ(kernel_count, 2u);

  std::vector<uint64_t> kernel_handles(kernel_count);
  ASSERT_EQ(
      ptiPcSamplingGetObservedKernelHandles(handle, device, kernel_handles.data(), &kernel_count),
      PTI_SUCCESS);

  for (uint64_t kernel_handle : kernel_handles) {
    pti_pc_sampling_kernel_info_t kernel_info{};
    kernel_info._struct_size = sizeof(kernel_info);
    std::vector<uint64_t> kernel_aggregated_samples(reason_count, 0);
    kernel_info._aggregated_samples = kernel_aggregated_samples.data();

    ASSERT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, device, kernel_handle, &kernel_info),
              PTI_SUCCESS);
    ASSERT_EQ(kernel_info._reason_count, reason_count);
    EXPECT_NE(kernel_info._kernel_name, nullptr);
    EXPECT_GT(kernel_info._instructions_with_samples_count, 0u);
    EXPECT_TRUE(std::any_of(kernel_aggregated_samples.begin(), kernel_aggregated_samples.end(),
                            [](uint64_t value) { return value != 0; }));
  }

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, AggregationIsIdempotentAcrossRepeatedQueries) {
  sycl::queue queue = sycl::queue(sycl::gpu_selector_v, sycl::property::queue::in_order{});

  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);
  ASSERT_EQ(ptiPcSamplingConfigure(handle, nullptr, 0, 0), PTI_SUCCESS);

  ASSERT_EQ(ptiPcSamplingStartCollection(handle), PTI_SUCCESS);
  RunLargeKernelWorkload(queue);
  ASSERT_EQ(ptiPcSamplingStopCollection(handle), PTI_SUCCESS);

  size_t device_count = 1;
  std::vector<pti_device_handle_t> profiled_devices(1);
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, profiled_devices.data(), &device_count),
            PTI_SUCCESS);
  pti_device_handle_t device = profiled_devices[0];

  pti_pc_sampling_device_status_t first_status{};
  first_status._struct_size = sizeof(first_status);
  ASSERT_EQ(ptiPcSamplingGetDeviceStatus(handle, device, &first_status), PTI_SUCCESS);

  size_t first_kernel_count = 0;
  ASSERT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device, nullptr, &first_kernel_count),
            PTI_SUCCESS);

  // Repeated queries must return identical cached results (single aggregation).
  for (int repeat = 0; repeat < 3; ++repeat) {
    pti_pc_sampling_device_status_t status{};
    status._struct_size = sizeof(status);
    ASSERT_EQ(ptiPcSamplingGetDeviceStatus(handle, device, &status), PTI_SUCCESS);
    EXPECT_EQ(status._total_sample_count, first_status._total_sample_count);
    EXPECT_EQ(status._total_pc_count, first_status._total_pc_count);

    size_t kernel_count = 0;
    ASSERT_EQ(ptiPcSamplingGetObservedKernelHandles(handle, device, nullptr, &kernel_count),
              PTI_SUCCESS);
    EXPECT_EQ(kernel_count, first_kernel_count);
  }

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}
