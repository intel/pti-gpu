//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sycl/ext/oneapi/experimental/enqueue_functions.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "utils/pti_record_collection_fixture.h"
#include "utils/sycl_usm_helper.h"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
template <typename T>
inline void SyclSubmitTestKernel(sycl::queue* queue, T* buf, std::size_t number_of_elements) {
  namespace syclex = sycl::ext::oneapi::experimental;
  constexpr int kComputeIterations = 5324;
  constexpr T kScale = 1.000001f;
  constexpr T kOffset = 1e-6f;
  queue->memset(buf, 0, number_of_elements * sizeof(T));
  queue->wait();
  syclex::submit(*queue, [&](sycl::handler& handler) {  // submitted WITHOUT an event
    handler.parallel_for(number_of_elements, [=](sycl::id<1> idx) {
      T val = buf[idx];
      for (int iter = 0; iter < kComputeIterations; ++iter) {
        val = (val * kScale) + kOffset;
      }
      buf[idx] = val;
    });
  });
}

template <typename T>
inline void SyclNdLaunchAndCopyTestKernels(sycl::queue* queue, T* dst, T* tmp,
                                           std::size_t number_of_elements, uint32_t seed) {
  namespace syclex = sycl::ext::oneapi::experimental;
  constexpr std::size_t kWorkGroupSize = 256;
  constexpr uint32_t kHashMultiplier = 0x9E3779B9u;
  constexpr uint32_t kHashShift = 8;
  syclex::nd_launch(
      *queue, sycl::nd_range<1>{number_of_elements, kWorkGroupSize}, [=](sycl::nd_item<1> item) {
        const std::size_t idx = item.get_global_id(0);
        const uint32_t hash = ((static_cast<uint32_t>(idx) ^ seed) * kHashMultiplier) >> kHashShift;
        tmp[idx] = sycl::sqrt(static_cast<T>(hash));
      });
  queue->parallel_for(sycl::range<1>(number_of_elements),
                      [=](sycl::id<1> idx) { dst[idx] = tmp[idx]; });
  queue->wait();
}
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

template <typename T>
inline void ExpectNonEmptyTimestamps(const std::vector<const T*>& records) {
  for (std::size_t idx = 0; idx < std::size(records); ++idx) {
    const auto* record = records.at(idx);
    EXPECT_NE(record->_start_timestamp, 0ULL) << "record " << idx;
    EXPECT_NE(record->_end_timestamp, 0ULL) << "record " << idx;
    EXPECT_GT(record->_end_timestamp, record->_start_timestamp)
        << "record " << idx << " has zero duration";
  }
}

class SyclExperimentalFeaturesTest : public ::pti::test::utils::RecordCollectionFixture {
 protected:
  void SetUp() override {
    try {
      const sycl::property_list prop_list{sycl::property::queue::in_order()};
      queue_ = std::make_unique<sycl::queue>(sycl::gpu_selector_v, prop_list);
    } catch (const sycl::exception& e) {
      GTEST_SKIP() << "No SYCL device or queue available: " << e.what();
    } catch (...) {
      FAIL() << "Unknown exception during SYCL device or queue creation.";
    }
  }

  std::unique_ptr<sycl::queue> queue_ =
      nullptr;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
};

TEST_F(SyclExperimentalFeaturesTest, EventlessSubmission) {
  InitCollection();
  EnableViews(PTI_VIEW_DEVICE_GPU_KERNEL);

  constexpr std::size_t kNumberElements = 1 << 20;
  using BufType = float;

  auto device_buffer = pti::test::utils::CreateDeviceUsmVector<BufType>(*queue_, kNumberElements);
  SyclSubmitTestKernel(queue_.get(), device_buffer.get(), kNumberElements);
  queue_->wait();

  FinalizeCollection();
  ASSERT_EQ(std::size(record_storage_.kernel_records), std::size_t{1});
  ExpectNonEmptyTimestamps(record_storage_.kernel_records);
}

TEST_F(SyclExperimentalFeaturesTest, MultiEventlessSubmission) {
  InitCollection();
  EnableViews(PTI_VIEW_DEVICE_GPU_KERNEL);

  constexpr std::size_t kNumberElements = 1 << 20;
  using BufType = float;
  constexpr std::size_t kNumberOfSubmissions = 8;

  auto device_buffer = pti::test::utils::CreateDeviceUsmVector<BufType>(*queue_, kNumberElements);
  for (std::size_t submission_number = 0; submission_number < kNumberOfSubmissions;
       ++submission_number) {
    SyclSubmitTestKernel(queue_.get(), device_buffer.get(), kNumberElements);
  }
  queue_->wait();

  FinalizeCollection();
  ASSERT_EQ(std::size(record_storage_.kernel_records), std::size_t{kNumberOfSubmissions});
  ExpectNonEmptyTimestamps(record_storage_.kernel_records);
}

TEST_F(SyclExperimentalFeaturesTest, EventlessNdLaunchFollowedByCopyKernel) {
  constexpr std::size_t kNumberElements = 4 * 128 * 1024;
  using BufType = float;
  constexpr uint32_t kNumberOfWarmupSteps = 3;
  constexpr uint32_t kNumberOfSteps = 300;
  constexpr std::size_t kKernelsPerStep = 2;
  constexpr uint32_t kSeedStart = 100;

  auto device_buffer = pti::test::utils::CreateDeviceUsmVector<BufType>(*queue_, kNumberElements);
  auto tmp_buffer = pti::test::utils::CreateDeviceUsmVector<BufType>(*queue_, kNumberElements);
  for (uint32_t warmup_step = 0; warmup_step < kNumberOfWarmupSteps; ++warmup_step) {
    SyclNdLaunchAndCopyTestKernels(queue_.get(), device_buffer.get(), tmp_buffer.get(),
                                   kNumberElements, warmup_step);
  }

  InitCollection();
  EnableViews(PTI_VIEW_DEVICE_GPU_KERNEL);

  for (uint32_t step = 0; step < kNumberOfSteps; ++step) {
    SyclNdLaunchAndCopyTestKernels(queue_.get(), device_buffer.get(), tmp_buffer.get(),
                                   kNumberElements, kSeedStart + step);
  }

  FinalizeCollection();
  ASSERT_EQ(std::size(record_storage_.kernel_records), kNumberOfSteps * kKernelsPerStep);
  ExpectNonEmptyTimestamps(record_storage_.kernel_records);
}
