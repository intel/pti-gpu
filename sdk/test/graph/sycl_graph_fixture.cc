#include <fmt/format.h>
#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <string>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "sycl_graph_test_kernels.h"
#include "utils/test_helpers.h"

template <typename T>
using SyclUsmVector = std::unique_ptr<T, std::function<void(T*)>>;

namespace {

template <typename T>
std::string FormatRecord(T* record) {
  return fmt::format("Name: {} \n Duration (ns): {}\nStart Time (ns): {}\nEnd Time (ns): {}\n",
                     record->_name, record->_end_timestamp - record->_start_timestamp,
                     record->_start_timestamp, record->_end_timestamp);
}

template <typename T>
void ValidateViewTimestamps(std::vector<T*> records) {
  for (const auto* record : records) {
    EXPECT_NE(record->_start_timestamp,
              (std::numeric_limits<decltype(record->_start_timestamp)>::min)())
        << "Failing record: " << FormatRecord(record);
    EXPECT_NE(record->_start_timestamp,
              (std::numeric_limits<decltype(record->_start_timestamp)>::max)())
        << "Failing record: " << FormatRecord(record);
    EXPECT_NE(record->_end_timestamp,
              (std::numeric_limits<decltype(record->_end_timestamp)>::min)())
        << "Failing record: " << FormatRecord(record);
    EXPECT_NE(record->_end_timestamp,
              (std::numeric_limits<decltype(record->_end_timestamp)>::max)())
        << "Failing record: " << FormatRecord(record);
    EXPECT_LE(record->_start_timestamp, record->_end_timestamp)
        << "Failing record: " << FormatRecord(record);
  }
}

}  // namespace

class SyclGraphTestSuite : public ::testing::Test {
 protected:
  struct TestData {
    void Reset() {
      kernel_records.clear();
      memcpy_records.clear();
      const std::lock_guard<std::mutex> lock(buffers_mtx);
      buffers.clear();
    }
    mutable std::mutex buffers_mtx;
    std::unordered_map<unsigned char*, pti::test::utils::PtiViewBuffer> buffers;
    std::vector<pti_view_record_kernel*> kernel_records;
    std::vector<pti_view_record_memory_copy*> memcpy_records;
  };

  SyclGraphTestSuite() { test_data_.Reset(); }

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

  template <typename T>
  constexpr T CalculateHostDotProduct() {
    T result = 0;
    for (std::size_t i = 0; i < SyclGraphTestSuite::kDefaultN; ++i) {
      constexpr auto kResultX = (static_cast<T>(kDefaultAlpha) * static_cast<T>(kDefaultAlpha)) +
                                (static_cast<T>(kDefaultBeta) * static_cast<T>(kDefaultBeta));
      constexpr auto kResultZ = (static_cast<T>(kDefaultGamma) * static_cast<T>(kDefaultGamma)) +
                                (static_cast<T>(kDefaultBeta) * static_cast<T>(kDefaultBeta));
      result += kResultX * kResultZ;
    }
    return result;
  }

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    auto buffer = pti::test::utils::PtiViewBuffer(kRequestedBufferSize);
    if (!buffer.Valid()) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf = buffer.data();
    *buf_size = buffer.size();

    const std::lock_guard<std::mutex> lock(test_data_.buffers_mtx);
    test_data_.buffers[*buf] = std::move(buffer);
  }

  static void MarkBuffer(unsigned char* buf, size_t /*buf_size*/, size_t used_bytes) {
    const std::lock_guard<std::mutex> lock(test_data_.buffers_mtx);
    if (auto buffer = test_data_.buffers.find(buf); buffer != test_data_.buffers.end()) {
      buffer->second.SetUsedBytes(used_bytes);
    }
  }

  static void HandleView(pti_view_record_base* view) {
    switch (view->_view_kind) {
      case PTI_VIEW_DEVICE_GPU_KERNEL: {
        test_data_.kernel_records.push_back(reinterpret_cast<pti_view_record_kernel*>(view));
        break;
      }
      case PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        test_data_.memcpy_records.push_back(reinterpret_cast<pti_view_record_memory_copy*>(view));
        break;
      }
      default: {
        FAIL() << "View found but not handled: " << view->_view_kind;
        break;
      }
    }
  }

  static void ParseBuffer(unsigned char* buf, size_t used_bytes) {
    pti_view_record_base* record = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &record);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        FAIL() << "Found Error Parsing Records from PTI";
        break;
      }
      HandleView(record);
    }
  }

  static void ParseAllBuffers() {
    const std::lock_guard<std::mutex> lock(test_data_.buffers_mtx);
    for (auto& [_, buffer] : test_data_.buffers) {
      ParseBuffer(buffer.data(), buffer.UsedBytes());
    }
  }

  inline static TestData test_data_{};
  sycl::queue queue_;
  constexpr static std::size_t kDefaultN = 10;
  constexpr static std::size_t kMaxRecordsInBuffer = 10;
  constexpr static std::size_t kRequestedBufferSize =
      kMaxRecordsInBuffer * sizeof(pti_view_record_kernel);
};

TEST_F(SyclGraphTestSuite, TestSyclUsmGraphExecution) {
  const auto sycl_free = [&](auto* ptr) { sycl::free(ptr, queue_); };
  using UnderlyingType = float;
  auto dot_product =
      SyclUsmVector<UnderlyingType>(sycl::malloc_shared<UnderlyingType>(1, queue_), sycl_free);
  auto x_vector = SyclUsmVector<UnderlyingType>(
      sycl::malloc_device<UnderlyingType>(kDefaultN, queue_), sycl_free);
  const auto y_vector = SyclUsmVector<UnderlyingType>(
      sycl::malloc_device<UnderlyingType>(kDefaultN, queue_), sycl_free);
  auto z_vector = SyclUsmVector<UnderlyingType>(
      sycl::malloc_device<UnderlyingType>(kDefaultN, queue_), sycl_free);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  //        node_i
  //       /      \
  //   node_a    node_b
  //       \      /
  //        node_c
  //
  // https://github.com/intel/llvm/blob/sycl/sycl/doc/syclgraph/SYCLGraphUsageGuide.md#code-examples
  sycl::ext::oneapi::experimental::command_graph graph{queue_};

  const auto node_i = graph.add([x_vec = x_vector.get(), y_vec = y_vector.get(),
                                 z_vec = z_vector.get()](sycl::handler& handler) {
    handler.parallel_for(sycl::range<1>{kDefaultN}, InitDotProductVectors(x_vec, y_vec, z_vec));
  });
  const auto node_a = graph.add(
      [x_vec = x_vector.get(), y_vec = y_vector.get()](sycl::handler& handler) {
        handler.parallel_for(sycl::range<1>{kDefaultN},
                             CombineTwoVectors{x_vec, y_vec, kDefaultAlpha, kDefaultBeta});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_i)});

  const auto node_b = graph.add(
      [y_vec = y_vector.get(), z_vec = z_vector.get()](sycl::handler& handler) {
        handler.parallel_for(sycl::range<1>{kDefaultN},
                             CombineTwoVectors{z_vec, y_vec, kDefaultGamma, kDefaultBeta});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_i)});

  [[maybe_unused]] const auto node_c = graph.add(
      [dotp = dot_product.get(), x_vec = x_vector.get(),
       z_vec = z_vector.get()](sycl::handler& handler) {
        handler.single_task(CalculateDotProduct{x_vec, z_vec, dotp, kDefaultN});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_a, node_b)});

  const auto exec = graph.finalize();
  queue_.ext_oneapi_graph(exec).wait_and_throw();
  EXPECT_FLOAT_EQ(*dot_product, CalculateHostDotProduct<UnderlyingType>());
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(test_data_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(test_data_.kernel_records);
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
  EXPECT_EQ(std::size(test_data_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(test_data_.kernel_records);
  EXPECT_EQ(std::size(test_data_.memcpy_records), std::size_t{6});
  ValidateViewTimestamps(test_data_.memcpy_records);
  constexpr std::size_t kExpectedMemCopies = kNumberOfBuffers * 2;
  constexpr std::size_t kExpectedBytesPerCopy = sizeof(UnderlyingType) * kElements;
  for (std::size_t idx = 0; idx < kExpectedMemCopies; ++idx) {
    EXPECT_EQ(test_data_.memcpy_records.at(idx)->_bytes, kExpectedBytesPerCopy);
  }
}
