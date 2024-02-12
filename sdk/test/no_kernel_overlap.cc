#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <stdarg.h>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "pti_view.h"
#include "utils.h"

using namespace sycl;

namespace {

void vecAdd(sycl::queue &q, int64_t* a, int64_t* b, int64_t* res, int count) {
  q.submit([&](sycl::handler &h) {
      h.parallel_for(count, [=](sycl::item<1> item) {
          int idx = item.get_id(0);
          res[idx] = a[idx] + b[idx];
      });
  });
}

void check_results(const int64_t* result, size_t size) {
  for (size_t i = 0; i < size; i++) {
    ASSERT_TRUE(result[i] == static_cast<int64_t>(i * 2) )
      << "Wrong element at index " << i
      << ", should be " << i * 2 << ", but got " << result[i];
  }
}

void StartTracing() {
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS), pti_result::PTI_SUCCESS);
}

void StopTracing() {
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS), pti_result::PTI_SUCCESS);
}

void TestCore() {
  try {
    constexpr size_t vector_size = 10*1024*1024;
    const size_t repetitions = 10;
    std::cout << "Adding vectors size: " << vector_size
              << ", Repetitions: " << repetitions << std::endl;

    auto dev = sycl::device(sycl::gpu_selector_v);
/*  will use it after not supporting compiler 2023
    sycl::propirty_list prop_immediate{sycl::property::queue::in_order(),
                                  sycl::ext::intel::property::queue::immediate_command_list()};

    sycl::property_list prop_no_immediate{sycl::property::queue::in_order(),
                                  sycl::ext::intel::property::queue::no_immediate_command_list()};
*/
    // Important that queue is in order
    sycl::property_list prop{sycl::property::queue::in_order()};

    queue q(sycl::gpu_selector_v, prop);
    int64_t* a = sycl::malloc_device<int64_t>(vector_size, q);
    int64_t* b = sycl::malloc_device<int64_t>(vector_size, q);
    int64_t* c = sycl::malloc_device<int64_t>(vector_size, q);

    int64_t* init_data_host = new int64_t[vector_size];
    int64_t* zero_data_host = new int64_t[vector_size];
    int64_t* outp_data_host = new int64_t[vector_size];

    // init values are meaningfull as result will be checked afterwards
    for (size_t i = 0; i < vector_size; ++i) {
      init_data_host[i] = i;
      zero_data_host[i] = 0;
    }

    // H2D - once
    q.memcpy(a, init_data_host, vector_size * sizeof(int64_t)).wait();
    q.memcpy(b, init_data_host, vector_size * sizeof(int64_t)).wait();
    q.memcpy(c, zero_data_host, vector_size * sizeof(int64_t)).wait();

    for (size_t iter=0; iter < repetitions; iter++) {
      vecAdd(q, a, b, c, vector_size);
    }
    q.wait();
    q.memcpy(outp_data_host, c, vector_size * sizeof(int64_t)).wait();
    check_results(outp_data_host, vector_size);

  } catch (const sycl::exception &e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value()
              << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
  } catch (const std::exception &e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
  }
  return ;
}

uint32_t validate_timestamps(uint64_t count, ...) {
  uint32_t found_issues = 0;
  va_list args;
  va_start(args, count);
  if (1LU == count) return found_issues;
  uint64_t prev_stamp = va_arg(args, uint64_t);
  uint64_t next_stamp = 0LU;
  for (uint64_t i = 1; i < count; ++i)
  {
    next_stamp = va_arg(args, uint64_t);
    if ( ! (prev_stamp<=next_stamp) ) {
      found_issues++;
    }
    prev_stamp = next_stamp;
  }
  va_end(args);
  return found_issues;
}
} // namespace

class NoKernelOverlapParametrizedTestFixture : public ::testing::TestWithParam<bool> {
 protected:
  NoKernelOverlapParametrizedTestFixture() {
    // Setup work for each test
  }

  ~NoKernelOverlapParametrizedTestFixture() override {
    // Cleanup work for each test
  }

  static uint32_t times_buffer_completed;
  static std::vector<uint64_t> kernel_device_timestamps;
  static std::vector<uint64_t> kernel_host_timestamps;

  void SetUp() override {  // Called right after constructor before each test
    kernel_device_timestamps.clear();
    kernel_host_timestamps.clear();
    times_buffer_completed = 0;
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

  // This function is for workaround to have every test to run in a separate process
  void ExitFunction() {
    _exit(0);
  }

  bool TestForDeviceKernelsOverlap(std::vector<uint64_t>& timestamps) {
    if (timestamps.size() == 0) {
      std::cerr << "--->  ERROR: Empty kernel timestamps array - Not expected "<< std::endl;
      return false;
    }
    std::cout << "In " << __FUNCTION__ << " timestamps array size: " << timestamps.size() << std::endl;
    for (uint32_t item = 1; item < timestamps.size(); item++ ) {
      if (timestamps[item] <= timestamps[item-1]) {
        std::cerr << "--->  ERROR: Device timestamps overlaps t(i) < t(i-1), at i: " << item
          << ", t(i): " << timestamps[item]
          << ", t(i-1): " << timestamps[item-1]
          << std::endl;
          return false;
      }
    }
    return true;
  }

  bool TestForAppendSubmitAtImmediate(std::vector<uint64_t>& timestamps) {
    if (timestamps.size() == 0) {
      std::cerr << "--->  ERROR: Empty kernel timestamps array - Not expected "<< std::endl;
      return false;
    }
    std::cout << "In " << __FUNCTION__ << " timestamps array size: " << timestamps.size() << std::endl;
    for (uint32_t item = 0; item < timestamps.size(); item +=2 ) {
      if (timestamps[item] != timestamps[item+1]) {
        std::cerr << "--->  ERROR: Append and Submit timestamps not equal t(i) != t(i+1), at i: "
          << item
          << " \t t(i): " << timestamps[item] << ", t(i+1): " << timestamps[item+1] << std::endl;
          return false;
      }
    }
    return true;
  }

  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    // very big buffer that would be enough for all records
    *buf_size = sizeof(pti_view_record_kernel)*10000;
    void* ptr = ::operator new(*buf_size);
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
  }

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    times_buffer_completed++;
    ASSERT_TRUE(times_buffer_completed == 1) << "ERROR: Not expected to enter to "
        << __FUNCTION__ << " more then 1 time, entered here : " << times_buffer_completed
        << " times";

    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }
    pti_view_record_base *ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        std::cout << "Reached End of buffer" << '\n';
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        std::cerr << "Found Error Parsing Records from PTI" << '\n';
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          std::cout << "Found Invalid Record" << '\n';
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          pti_view_record_kernel *p_kernel_rec =
              reinterpret_cast<pti_view_record_kernel *>(ptr);

          uint32_t found_issues = validate_timestamps(6, p_kernel_rec->_sycl_task_begin_timestamp,
                                p_kernel_rec->_sycl_enqk_begin_timestamp,
                                p_kernel_rec->_append_timestamp, p_kernel_rec->_submit_timestamp,
                                p_kernel_rec->_start_timestamp, p_kernel_rec->_end_timestamp);
          if (found_issues > 0) {
            std::cerr << "------------>     ERROR: Not monotonic kernel timestamps"
                      << std::endl;
            exit(1);
          }
          if (p_kernel_rec->_sycl_task_begin_timestamp == 0) {
            std::cerr << "------------>     Something wrong: Sycl Task Begin Time is 0"
                      << std::endl;
            exit(1);
          }
          if (p_kernel_rec->_sycl_enqk_begin_timestamp == 0) {
            std::cerr << "------------>     Something wrong: Sycl Enq Launch Kernel Time is 0"
                      << std::endl;
            exit(1);
          }
          kernel_host_timestamps.push_back(p_kernel_rec->_append_timestamp);
          kernel_host_timestamps.push_back(p_kernel_rec->_submit_timestamp);

          kernel_device_timestamps.push_back(p_kernel_rec->_start_timestamp);
          kernel_device_timestamps.push_back(p_kernel_rec->_end_timestamp);
          break;
        }
        default: {
          break;
        }
      }
    }
    ::operator delete(buf);
  }

  static void RunTest() {
    StartTracing();
    TestCore();
    StopTracing();
    ptiFlushAllViews(); // having 2 ptiFlushAllViews is a workaround until there is a fix
  }

};

// static members initialization
uint32_t NoKernelOverlapParametrizedTestFixture::times_buffer_completed = 0;
std::vector<uint64_t> NoKernelOverlapParametrizedTestFixture::kernel_device_timestamps{};
std::vector<uint64_t> NoKernelOverlapParametrizedTestFixture::kernel_host_timestamps{};

TEST_P(NoKernelOverlapParametrizedTestFixture, NoKernelOverlapImmediate) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);

  RunTest();

  EXPECT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  EXPECT_EQ(TestForDeviceKernelsOverlap(kernel_device_timestamps), true);
  if (do_immediate) {
    EXPECT_EQ(TestForAppendSubmitAtImmediate(kernel_host_timestamps), true);
  }
  EXPECT_EXIT(ExitFunction(), testing::ExitedWithCode(0),"");
}

INSTANTIATE_TEST_SUITE_P(
        NoKernelOverlapTests,
        NoKernelOverlapParametrizedTestFixture,
        ::testing::Values(
                true, false
        ));


