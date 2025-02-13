#include <gtest/gtest.h>
#include <stdarg.h>

#include <iostream>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "utils/test_helpers.h"

namespace {

void VecAdd(sycl::queue& q, int64_t* a, int64_t* b, int64_t* res, int count) {
  q.submit([&](sycl::handler& h) {
    h.parallel_for(count, [=](sycl::item<1> item) {
      int idx = item.get_id(0);
      res[idx] = a[idx] + b[idx];
    });
  });
}

void CheckResults(const int64_t* result, size_t size) {
  for (size_t i = 0; i < size; i++) {
    ASSERT_TRUE(result[i] == static_cast<int64_t>(i * 2))
        << "Wrong element at index " << i << ", should be " << i * 2 << ", but got " << result[i];
  }
}

void StartTracing() {
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

void StopTracing() {
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

void TestCore(bool do_immediate) {
  try {
    constexpr size_t kVectorSize = 10 * 1024 * 1024;
    constexpr size_t kRepetitions = 10;
    std::cout << "Adding vectors size: " << kVectorSize << ", Repetitions: " << kRepetitions
              << std::endl;

    auto dev = sycl::device(sycl::gpu_selector_v);
    // Important that queue is in order
    sycl::queue q;
    if (do_immediate) {
      sycl::property_list prop{sycl::property::queue::in_order(),
                               sycl::ext::intel::property::queue::immediate_command_list()};
      q = sycl::queue(sycl::gpu_selector_v, prop);
    } else {
      sycl::property_list prop{sycl::property::queue::in_order(),
                               sycl::ext::intel::property::queue::no_immediate_command_list()};
      q = sycl::queue(sycl::gpu_selector_v, prop);
    }

    int64_t* a = sycl::malloc_device<int64_t>(kVectorSize, q);
    int64_t* b = sycl::malloc_device<int64_t>(kVectorSize, q);
    int64_t* c = sycl::malloc_device<int64_t>(kVectorSize, q);

    auto init_data_host = std::make_unique<int64_t[]>(kVectorSize);
    auto zero_data_host = std::make_unique<int64_t[]>(kVectorSize);
    auto outp_data_host = std::make_unique<int64_t[]>(kVectorSize);

    // init values are meaningfull as result will be checked afterwards
    for (int64_t i = 0; i < static_cast<int64_t>(kVectorSize); ++i) {
      init_data_host[i] = i;
      zero_data_host[i] = 0LL;
    }

    // H2D - once
    q.memcpy(a, init_data_host.get(), kVectorSize * sizeof(int64_t)).wait();
    q.memcpy(b, init_data_host.get(), kVectorSize * sizeof(int64_t)).wait();
    q.memcpy(c, zero_data_host.get(), kVectorSize * sizeof(int64_t)).wait();

    for (size_t iter = 0; iter < kRepetitions; iter++) {
      VecAdd(q, a, b, c, kVectorSize);
    }
    q.wait();
    q.memcpy(outp_data_host.get(), c, kVectorSize * sizeof(int64_t)).wait();

    CheckResults(outp_data_host.get(), kVectorSize);

    sycl::free(c, q);
    sycl::free(b, q);
    sycl::free(a, q);
  } catch (const sycl::exception& e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
  } catch (const std::exception& e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
  }
}
}  // namespace

bool ComparePair(std::pair<uint64_t, uint64_t> a_stamps, std::pair<uint64_t, uint64_t> b_stamps) {
  return a_stamps.first < b_stamps.first;
}

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

  static std::vector<std::pair<uint64_t, uint64_t> > kernel_device_timestamps_pairs;

  void SetUp() override {  // Called right after constructor before each test
    kernel_device_timestamps_pairs.clear();
    kernel_host_timestamps.clear();
    times_buffer_completed = 0;
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

  bool TestForDeviceKernelsOverlap(std::vector<std::pair<uint64_t, uint64_t> >& timestamps) {
    if (timestamps.size() == 0) {
      std::cerr << "--->  ERROR: Empty kernel timestamps array - Not expected " << std::endl;
      return false;
    }
    std::cout << "In " << __FUNCTION__ << " timestamps array size: " << timestamps.size()
              << std::endl;
    // pair is kernel start and kernel end
    // vector comes sorted by the first value in pair, which is kernel_start
    for (uint32_t item = 1; item < timestamps.size(); item++) {
      /*
      std::cout << "Stamps: i-1 start: " << timestamps[item-1].first
                << ", end: " << timestamps[item-1].second << std::endl
                << "        i   start: " << timestamps[item].first
                << ", end: " << timestamps[item].second
                << std::endl;
      */
      if (timestamps[item].first <= timestamps[item - 1].second) {
        std::cerr << "--->  ERROR: Device timestamps overlaps t(i) < t(i-1), at i: " << item
                  << ", t(i): " << timestamps[item].first
                  << ", t(i-1): " << timestamps[item - 1].second << std::endl;
        return false;
      }
    }
    return true;
  }

  bool TestForDeviceKernelDurationNonZero(std::vector<std::pair<uint64_t, uint64_t> >& timestamps) {
    if (timestamps.size() == 0) {
      std::cerr << "--->  ERROR: Empty kernel timestamps array - Not expected " << std::endl;
      return false;
    }
    std::cout << "In " << __FUNCTION__ << " timestamps array size: " << timestamps.size()
              << std::endl;
    // pair is kernel start and kernel end
    for (uint32_t item = 0; item < timestamps.size(); item++) {
      if ((timestamps[item].second - timestamps[item].first) < 100) {
        std::cerr
            << "--->  ERROR: Device kernel duration is less than 100 ns, timestamps at kernel i: "
            << item << ", end: " << timestamps[item].second << ", start: " << timestamps[item].first
            << std::endl;
        return false;
      }
    }
    return true;
  }

  bool TestForAppendSubmitAtImmediate(std::vector<uint64_t>& timestamps) {
    if (timestamps.size() == 0) {
      std::cerr << "--->  ERROR: Empty kernel timestamps array - Not expected " << std::endl;
      return false;
    }
    std::cout << "In " << __FUNCTION__ << " timestamps array size: " << timestamps.size()
              << std::endl;
    for (uint32_t item = 0; item < timestamps.size(); item += 2) {
      if (timestamps[item] != timestamps[item + 1]) {
        std::cerr << "--->  ERROR: Append and Submit timestamps not equal t(i) != t(i+1), at i: "
                  << item << " \t t(i): " << timestamps[item]
                  << ", t(i+1): " << timestamps[item + 1] << std::endl;
        return false;
      }
    }
    return true;
  }

  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    // very big buffer that would be enough for all records
    *buf_size = sizeof(pti_view_record_kernel) * 10000;
    void* ptr = ::operator new(*buf_size);
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
  }

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    uint32_t kernel_records_in_buffer = 0;

    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }

    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        std::cout << "Reached End of buffer: " << times_buffer_completed
                  << " Kernel records in buffer: " << kernel_records_in_buffer << '\n';
        if (kernel_records_in_buffer > 0) {
          times_buffer_completed++;
          //  OK to get more buffers as soon as there no kernel records in them
          //  But kernel records we want safely process from one same buffer
          ASSERT_TRUE(times_buffer_completed == 1)
              << "ERROR: Not expected to enter to " << __FUNCTION__
              << " more then 1 time, entered here : " << times_buffer_completed << " times";
        }
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        std::cerr << "Found Error Parsing Records in buffer: " << times_buffer_completed
                  << " PTI buf_status: " << buf_status << '\n';
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          std::cout << "Found Invalid Record" << '\n';
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          kernel_records_in_buffer++;
          pti_view_record_kernel* p_kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);

          auto found_issues = pti::test::utils::ValidateTimestamps(
              p_kernel_rec->_sycl_task_begin_timestamp, p_kernel_rec->_sycl_enqk_begin_timestamp,
              p_kernel_rec->_append_timestamp, p_kernel_rec->_submit_timestamp,
              p_kernel_rec->_start_timestamp, p_kernel_rec->_end_timestamp);

          if (found_issues > 0) {
            FAIL() << "------------>     ERROR: Not monotonic kernel timestamps";
          }

          if (p_kernel_rec->_sycl_task_begin_timestamp == 0) {
            FAIL() << "------------>     Something wrong: Sycl Task Begin Time is 0";
          }

          if (p_kernel_rec->_sycl_enqk_begin_timestamp == 0) {
            FAIL() << "------------>     Something wrong: Sycl Enq Launch Kernel Time is 0";
          }

          kernel_host_timestamps.push_back(p_kernel_rec->_append_timestamp);
          kernel_host_timestamps.push_back(p_kernel_rec->_submit_timestamp);

          kernel_device_timestamps_pairs.push_back(std::pair<uint64_t, uint64_t>{
              p_kernel_rec->_start_timestamp, p_kernel_rec->_end_timestamp});
          break;
        }
        default: {
          break;
        }
      }
    }
    ::operator delete(buf);
  }

  static void RunTest(bool do_immediate) {
    StartTracing();
    TestCore(do_immediate);
    StopTracing();
    EXPECT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  }
};

// static members initialization
uint32_t NoKernelOverlapParametrizedTestFixture::times_buffer_completed = 0;
std::vector<uint64_t> NoKernelOverlapParametrizedTestFixture::kernel_host_timestamps{};
std::vector<std::pair<uint64_t, uint64_t> >
    NoKernelOverlapParametrizedTestFixture::kernel_device_timestamps_pairs{};

TEST_P(NoKernelOverlapParametrizedTestFixture, NoKernelOverlapImmediate) {
  bool do_immediate = GetParam();
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);

  RunTest(do_immediate);

  // order of records is not garantie the order of submission and execution of kernels -
  // so let's  sort kernel stamp pairs by device start stamp
  std::sort(kernel_device_timestamps_pairs.begin(), kernel_device_timestamps_pairs.end(),
            ComparePair);

  EXPECT_EQ(TestForDeviceKernelsOverlap(kernel_device_timestamps_pairs), true);
  EXPECT_EQ(TestForDeviceKernelDurationNonZero(kernel_device_timestamps_pairs), true);
  if (do_immediate) {
    EXPECT_EQ(TestForAppendSubmitAtImmediate(kernel_host_timestamps), true);
  }
}

INSTANTIATE_TEST_SUITE_P(NoKernelOverlapTests, NoKernelOverlapParametrizedTestFixture,
                         ::testing::Values(true, false));
