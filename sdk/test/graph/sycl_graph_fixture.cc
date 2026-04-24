//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "sycl_graph_test_kernels.h"
#include "sycl_graph_workloads.h"
#include "utils/pti_record_collection_fixture.h"
#include "utils/sycl_usm_helper.h"

namespace {

template <typename T>
std::string FormatRecord(const T* record) {
  auto result = fmt::format(
      "Name: {} \nDuration (ns): {}\nStart Time (ns): {}\nEnd Time (ns): {}\nThread ID: {}\n",
      record->_name, record->_end_timestamp - record->_start_timestamp, record->_start_timestamp,
      record->_end_timestamp, record->_thread_id);

  if constexpr (std::is_same_v<T, pti_view_record_kernel>) {
    result += fmt::format("Submit Time: {}\n", record->_submit_timestamp);
    result += fmt::format("Append Time: {}\n", record->_append_timestamp);
    result += fmt::format("Correlation ID: {}\n", record->_correlation_id);
    result += fmt::format("SYCL Enqueue Begin Time (ns): {}\n", record->_sycl_enqk_begin_timestamp);
    result += fmt::format("SYCL Task Begin ID (ns): {}\n", record->_sycl_task_begin_timestamp);
    result += fmt::format("SYCL Queue ID: {}\n", record->_sycl_queue_id);
  }

  return result;
}

template <typename T>
void ValidateView(const T* record) {
  EXPECT_NE(record->_start_timestamp,
            (std::numeric_limits<decltype(record->_start_timestamp)>::min)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_start_timestamp,
            (std::numeric_limits<decltype(record->_start_timestamp)>::max)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_end_timestamp, (std::numeric_limits<decltype(record->_end_timestamp)>::min)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_end_timestamp, (std::numeric_limits<decltype(record->_end_timestamp)>::max)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_LE(record->_start_timestamp, record->_end_timestamp)
      << "Failing record: " << FormatRecord(record);
}

template <typename T>
void ValidateViewTimestamps(const std::vector<T*>& records) {
  for (const auto* record : records) {
    ValidateView(record);
  }
}

}  // namespace

class SyclGraphTestSuite : public pti::test::utils::RecordCollectionFixture {
 protected:
  void SetUp() override {
    try {
      auto graph_selector = [](const sycl::device& dev) {
        if (dev.has(sycl::aspect::ext_oneapi_graph)) {
          return 2;
        }
        if (dev.has(sycl::aspect::ext_oneapi_limited_graph)) {
          return 1;
        }

        return -1;
      };
      queue_ = sycl::queue{graph_selector, {sycl::property::queue::in_order()}};
    } catch (const sycl::exception& e) {
      GTEST_SKIP() << "SYCL exception during setup: " << e.what();
    } catch (...) {
      FAIL() << "Unknown exception during SYCL queue setup.";
    }
  }

  sycl::queue queue_;
  constexpr static std::size_t kMaxRecordsInBuffer = 10;
  constexpr static std::size_t kRequestedBufferSize =
      kMaxRecordsInBuffer * sizeof(pti_view_record_kernel);
};

class SyclUsmGraphExecutionTestSuite : public SyclGraphTestSuite,
                                       public testing::WithParamInterface<std::tuple<std::size_t>> {
 protected:
  using UnderlyingType = float;
  using FloatVec = pti::test::utils::SyclUsmVector<UnderlyingType>;
  using FinalizedGraphType = sycl::ext::oneapi::experimental::command_graph<
      sycl::ext::oneapi::experimental::graph_state::executable>;
  static constexpr std::size_t kExpectedKernelsPerExecution = kDefaultUsmKernelNumber;
  // Given that a timestamp conversion is being done after record generation and before buffer
  // insertion, some tolerence is needed due to timestamp drift.
  // This could be mitigated in the environment with PTI_CONV_CLOCK_SYNC_TIME_NS=1000000000.
  // However, that is fragile as well.
  static constexpr auto kTimestampEqualityTolerance = std::chrono::nanoseconds(5);

  SyclUsmGraphExecutionTestSuite() : replays_(std::get<0>(GetParam())) {}

  void InitializeTracing() {
    ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  }

  void EnableTracing() {
    for (const auto view : kEnableViews) {
      ASSERT_EQ(ptiViewEnable(view), pti_result::PTI_SUCCESS);
    }
  }

  void FinalizeTracing() {
    for (const auto view : kEnableViews) {
      ASSERT_EQ(ptiViewDisable(view), pti_result::PTI_SUCCESS);
    }
    ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
    ParseAllBuffers();
  }

  void SetUp() override {
    SyclGraphTestSuite::SetUp();
    std::tie(dot_product_, x_vector_, y_vector_, z_vector_) =
        CreateUsmDotProductVectors<UnderlyingType>(queue_, kDefaultUsmVectorSize);
  }

  void TearDown() override { SyclGraphTestSuite::TearDown(); }

  void ValidateGraphReplayTimestamps() {
    // clang-format off
    // First, sort kernels by submission (unless they're the same, then sort by
    // append time). We replay the graph N + 1 times. We want to validate all
    // kernels in a given submission and compare to subsequent submissions.
    //
    //  Let's say we have three graph submissions. This tests executes them sequentially.
    //  Append Time:         A_1 <  A_2 < A_3 < A_4   A_1 < A_2 < A_3 < A_4  A_1 < A_2 < A_3 < A_4
    //                        /     /    /     /      /    /     /    /      /    /    /     /
    //  Graph Submissions:  (K1 -  K2 - K3 - K4) -> (K1 - K2 - K3 - K4) -> (K1 - K2 - K3 - K4)
    //                       /                     /                      /
    //  Submit Time:       S_1       <           S_2        <           S_3
    //
    //  A_1(_1) == A_1(_2)
    //  Ks_4_1 < Ks_4_2 < Ks_4_3 (Ks = Kernel start time)
    //  Ks_4_1 < Ks_1_2
    // clang-format on
    std::sort(record_storage_.kernel_records.begin(), record_storage_.kernel_records.end(),
              [](const auto* first_record, const auto* second_record) {
                if (first_record->_submit_timestamp != second_record->_submit_timestamp) {
                  return first_record->_submit_timestamp < second_record->_submit_timestamp;
                }
                return first_record->_append_timestamp < second_record->_append_timestamp;
              });

    ValidateViewTimestamps(record_storage_.kernel_records);
    ValidateGraphSubmitTimestamps();
    ValidateGraphAppendTimestamps();
  }

  void ValidateGraphSubmitTimestamps() {
    for (std::size_t i = 0; i < std::size(record_storage_.kernel_records);
         i += kExpectedKernelsPerExecution) {
      const auto* prev_kernel_in_submission = record_storage_.kernel_records.at(i);
      const std::size_t next_submission_idx = i + kExpectedKernelsPerExecution;
      // Validate that all kernels in the same submission have the same submit timestamp.
      for (std::size_t j = i + 1; j < next_submission_idx; ++j) {
        const auto* curr_kernel_record = record_storage_.kernel_records.at(j);
        const uint64_t submit_diff =
            curr_kernel_record->_submit_timestamp > prev_kernel_in_submission->_submit_timestamp
                ? curr_kernel_record->_submit_timestamp -
                      prev_kernel_in_submission->_submit_timestamp
                : prev_kernel_in_submission->_submit_timestamp -
                      curr_kernel_record->_submit_timestamp;
        EXPECT_LE(submit_diff, kTimestampEqualityTolerance.count())
            << "Kernels in the same submission have different submit timestamps. Previous kernel:\n"
            << FormatRecord(prev_kernel_in_submission)
            << "\nCurrent kernel: " << FormatRecord(curr_kernel_record);
        EXPECT_LT(curr_kernel_record->_append_timestamp, curr_kernel_record->_submit_timestamp)
            << "Append timestamp should be before submit timestamp. Current kernel:\n"
            << FormatRecord(curr_kernel_record);
        prev_kernel_in_submission = curr_kernel_record;
      }

      // Validate that each submission happens after the previous one and that the kernel timestamps
      // relect that too. Each graph submission is sequential (perhaps not the kernel execution,
      // given the diamond shape graph). We're comparing _submissions_.
      if (next_submission_idx < std::size(record_storage_.kernel_records)) {
        const auto* next_submission_kernel = record_storage_.kernel_records.at(next_submission_idx);
        EXPECT_LT(prev_kernel_in_submission->_submit_timestamp,
                  next_submission_kernel->_submit_timestamp)
            << "Next submission starts before the previous one. Previous kernel: "
            << FormatRecord(prev_kernel_in_submission)
            << "\nNext kernel: " << FormatRecord(next_submission_kernel);
        EXPECT_LT(prev_kernel_in_submission->_start_timestamp,
                  next_submission_kernel->_start_timestamp);
        EXPECT_LT(prev_kernel_in_submission->_end_timestamp,
                  next_submission_kernel->_end_timestamp);
      }
    }
  }

  void ValidateGraphAppendTimestamps() {
    // Special handling of append timestamps is needed because our graph isn't strictly linear,
    // we initially submit K1 then K2 and K3 can execute in any order, then K4 executes after K2 and
    // K3 are done.
    constexpr std::size_t kLastKernelIndex = kExpectedKernelsPerExecution - 1;
    for (std::size_t i = 0; i < std::size(record_storage_.kernel_records);
         i += kExpectedKernelsPerExecution) {
      const auto* curr_kernel_record = record_storage_.kernel_records.at(i);
      const auto* last_kernel_record = record_storage_.kernel_records.at(i + kLastKernelIndex);
      const std::size_t first_kernel_next_submission_idx = i + kExpectedKernelsPerExecution;
      const std::size_t last_kernel_next_submission_idx =
          i + kLastKernelIndex + kExpectedKernelsPerExecution;
      if (last_kernel_next_submission_idx < std::size(record_storage_.kernel_records)) {
        const auto* first_kernel_next_submission =
            record_storage_.kernel_records.at(first_kernel_next_submission_idx);
        const auto* last_kernel_next_submission =
            record_storage_.kernel_records.at(last_kernel_next_submission_idx);
        EXPECT_STREQ(curr_kernel_record->_name, first_kernel_next_submission->_name)
            << "Kernels being compared should be the same across submissions. Previous kernel:\n"
            << FormatRecord(curr_kernel_record)
            << "\nNext submission's kernel: " << FormatRecord(first_kernel_next_submission);
        uint64_t append_diff =
            first_kernel_next_submission->_append_timestamp > curr_kernel_record->_append_timestamp
                ? first_kernel_next_submission->_append_timestamp -
                      curr_kernel_record->_append_timestamp
                : curr_kernel_record->_append_timestamp -
                      first_kernel_next_submission->_append_timestamp;
        EXPECT_LE(append_diff, kTimestampEqualityTolerance.count())
            << "Kernels in subsequent submissions have different append timestamps. Previous "
               "kernel:\n"
            << FormatRecord(curr_kernel_record)
            << "\nNext submission's same kernel: " << FormatRecord(first_kernel_next_submission);
        EXPECT_STREQ(last_kernel_record->_name, last_kernel_next_submission->_name)
            << "Kernels being compared should be the same across submissions. Previous kernel:\n"
            << FormatRecord(last_kernel_record)
            << "\nNext submission's kernel: " << FormatRecord(last_kernel_next_submission);

        append_diff =
            last_kernel_next_submission->_append_timestamp > last_kernel_record->_append_timestamp
                ? last_kernel_next_submission->_append_timestamp -
                      last_kernel_record->_append_timestamp
                : last_kernel_record->_append_timestamp -
                      last_kernel_next_submission->_append_timestamp;

        EXPECT_LE(append_diff, kTimestampEqualityTolerance.count())
            << "Kernels in subsequent submissions have different append timestamps. Previous "
               "kernel:\n"
            << FormatRecord(last_kernel_record)
            << "\nNext submission's same kernel: " << FormatRecord(last_kernel_next_submission);
      }
    }
  }

  constexpr static std::array kEnableViews = {PTI_VIEW_DEVICE_GPU_KERNEL, PTI_VIEW_RUNTIME_API};
  std::size_t replays_ = 0;
  FloatVec dot_product_ = nullptr;
  FloatVec x_vector_ = nullptr;
  FloatVec y_vector_ = nullptr;
  FloatVec z_vector_ = nullptr;
  std::optional<FinalizedGraphType> graph_;
};

TEST_F(SyclGraphTestSuite, TestSyclUsmGraphExecution) {
  using UnderlyingType = float;
  auto [dot_product, x_vector, y_vector, z_vector] =
      CreateUsmDotProductVectors<UnderlyingType>(queue_, kDefaultUsmVectorSize);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  auto graph = CreateUsmDotProductGraph(queue_, kDefaultUsmVectorSize, dot_product.get(),
                                        x_vector.get(), y_vector.get(), z_vector.get());

  const auto exec = graph.finalize();
  queue_.ext_oneapi_graph(exec).wait_and_throw();
  EXPECT_FLOAT_EQ(*dot_product, CalculateHostDotProduct<UnderlyingType>(kDefaultUsmVectorSize));
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{kDefaultUsmKernelNumber});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_F(SyclGraphTestSuite, TestSyclBuffersGraphExecution) {
  // https://github.com/intel/llvm/blob/sycl/sycl/doc/syclgraph/SYCLGraphUsageGuide.md#code-examples
  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  constexpr std::size_t kNumberOfBuffers = 3;
  constexpr std::size_t kElements = 32;
  using UnderlyingType = int;
  std::vector<UnderlyingType> underlying_vec_a(kElements, 1);
  std::vector<UnderlyingType> underlying_vec_b(kElements, 2);
  std::vector<UnderlyingType> underlying_vec_c(kElements, 3);
  auto range = sycl::range<1>{kElements};
  sycl::buffer<UnderlyingType> sycl_buffer_a{underlying_vec_a.data(), range};
  sycl_buffer_a.set_write_back(false);
  sycl::buffer<UnderlyingType> sycl_buffer_b{underlying_vec_b.data(), range};
  sycl_buffer_b.set_write_back(false);
  sycl::buffer<UnderlyingType> sycl_buffer_c{underlying_vec_c.data(), range};
  sycl_buffer_c.set_write_back(false);

  {
    sycl::ext::oneapi::experimental::command_graph graph{
        queue_, {sycl::ext::oneapi::experimental::property::graph::assume_buffer_outlives_graph{}}};

    queue_.submit([&](sycl::handler& cgh) {
      auto data_ptr = sycl_buffer_a.get_access<sycl::access::mode::read_write>(cgh);
      cgh.parallel_for(range, IncrementBufferKernel{data_ptr});
    });

    queue_.submit([&](sycl::handler& cgh) {
      auto data_ptr1 = sycl_buffer_a.get_access<sycl::access::mode::read>(cgh);
      auto data_ptr2 = sycl_buffer_b.get_access<sycl::access::mode::read_write>(cgh);
      cgh.parallel_for(range, AddBuffersKernel{data_ptr1, data_ptr2});
    });

    queue_.submit([&](sycl::handler& cgh) {
      auto data_ptr1 = sycl_buffer_a.get_access<sycl::access::mode::read>(cgh);
      auto data_ptr2 = sycl_buffer_c.get_access<sycl::access::mode::read_write>(cgh);
      cgh.parallel_for(range, SubtractBuffersKernel{data_ptr1, data_ptr2});
    });

    queue_.submit([&](sycl::handler& cgh) {
      auto data_ptr1 = sycl_buffer_b.get_access<sycl::access::mode::read_write>(cgh);
      auto data_ptr2 = sycl_buffer_c.get_access<sycl::access::mode::read_write>(cgh);
      cgh.parallel_for(range, DecrementBuffersKernel{data_ptr1, data_ptr2});
    });

    graph.end_recording();

    auto exec_graph = graph.finalize();

    queue_.submit([&](sycl::handler& cgh) { cgh.ext_oneapi_graph(exec_graph); }).wait();
  }

  sycl::host_accessor data_access_a(sycl_buffer_a);
  sycl::host_accessor data_access_b(sycl_buffer_b);
  sycl::host_accessor data_access_c(sycl_buffer_c);

  for (std::size_t i = 0; i < kElements; ++i) {
    EXPECT_EQ(data_access_a[i], 2);
    EXPECT_EQ(data_access_b[i], 3);
    EXPECT_EQ(data_access_c[i], 0);
  }

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(record_storage_.kernel_records);
  EXPECT_EQ(std::size(record_storage_.memcpy_records), std::size_t{6});
  ValidateViewTimestamps(record_storage_.memcpy_records);
  constexpr std::size_t kExpectedMemCopies = kNumberOfBuffers * 2;
  constexpr std::size_t kExpectedBytesPerCopy = sizeof(UnderlyingType) * kElements;
  for (std::size_t idx = 0; idx < kExpectedMemCopies; ++idx) {
    EXPECT_EQ(record_storage_.memcpy_records.at(idx)->_bytes, kExpectedBytesPerCopy);
  }
}

TEST_F(SyclGraphTestSuite, TestSyclUsmTwoGraphsReplay) {
  using UnderlyingType = float;
  constexpr auto kNumberOfReplaysPerGraph = 2;
  constexpr auto kNumberOfGraphs = 2;

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  auto [dot_product_a, x_vector_a, y_vector_a, z_vector_a] =
      CreateUsmDotProductVectors<UnderlyingType>(queue_, kDefaultUsmVectorSize);
  auto graph_a = CreateUsmDotProductGraph(queue_, kDefaultUsmVectorSize, dot_product_a.get(),
                                          x_vector_a.get(), y_vector_a.get(), z_vector_a.get());
  const auto exec_a = graph_a.finalize();

  auto [dot_product_b, x_vector_b, y_vector_b, z_vector_b] =
      CreateUsmDotProductVectors<UnderlyingType>(queue_, kDefaultUsmVectorSize);
  auto graph_b = CreateUsmDotProductGraph(queue_, kDefaultUsmVectorSize, dot_product_b.get(),
                                          x_vector_b.get(), y_vector_b.get(), z_vector_b.get());
  const auto exec_b = graph_b.finalize();

  queue_.ext_oneapi_graph(exec_a).wait_and_throw();
  queue_.ext_oneapi_graph(exec_b).wait_and_throw();
  queue_.ext_oneapi_graph(exec_a).wait_and_throw();
  queue_.ext_oneapi_graph(exec_b).wait_and_throw();

  const auto expected_per_graph =
      CalculateHostDotProduct<UnderlyingType>(kDefaultUsmVectorSize) * kNumberOfReplaysPerGraph;
  EXPECT_FLOAT_EQ(*dot_product_a, expected_per_graph);
  EXPECT_FLOAT_EQ(*dot_product_b, expected_per_graph);

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();

  EXPECT_EQ(std::size(record_storage_.kernel_records),
            std::size_t{kDefaultUsmKernelNumber * kNumberOfGraphs * kNumberOfReplaysPerGraph});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_P(SyclUsmGraphExecutionTestSuite, TestArbitraryReplays) {
  InitializeTracing();
  EnableTracing();
  auto graph = CreateUsmDotProductGraph(queue_, kDefaultUsmVectorSize, dot_product_.get(),
                                        x_vector_.get(), y_vector_.get(), z_vector_.get());
  graph_.emplace(graph.finalize());

  queue_.ext_oneapi_graph(*graph_).wait_and_throw();
  for (std::size_t i = 0; i < replays_; ++i) {
    queue_.ext_oneapi_graph(*graph_).wait_and_throw();
  }
  FinalizeTracing();
  const auto expected_result = (CalculateHostDotProduct<UnderlyingType>(kDefaultUsmVectorSize) *
                                static_cast<UnderlyingType>(replays_)) +
                               CalculateHostDotProduct<UnderlyingType>(kDefaultUsmVectorSize);
  EXPECT_FLOAT_EQ(*dot_product_, expected_result);
  EXPECT_EQ(std::size(record_storage_.kernel_records),
            (replays_ + 1) * kExpectedKernelsPerExecution);
  ValidateGraphReplayTimestamps();
}

INSTANTIATE_TEST_SUITE_P(SyclUsmGraphExecutionReplayTests, SyclUsmGraphExecutionTestSuite,
                         ::testing::Values(0, 1, 5, 10), [](const auto& info) {
                           return fmt::format("{}_Replays", std::get<0>(info.param));
                         });
