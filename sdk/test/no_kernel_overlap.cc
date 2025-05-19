#include <gtest/gtest.h>
#include <stdarg.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "utils.h"
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

constexpr size_t kRepetitions = 10;

float TestCore(bool do_immediate) {
  float time_sec = 0.f;
  try {
    // size purposely small - it makes harder requirement to the test
    // constexpr size_t kVectorSize = 10 * 1024 * 1024;
    constexpr size_t kVectorSize = 1024;
    std::cout << "Adding vectors size: " << kVectorSize << ", Repetitions: " << kRepetitions
              << std::endl;
    std::cout << "Evaluating latency of timing call..." << std::endl;
    auto first_stamp = utils::GetTime();
    auto second_stamp = utils::GetTime();
    auto third_stamp = utils::GetTime();
    auto forth_stamp = utils::GetTime();
    std::cout << "Time stamp (ns):" << first_stamp << std::endl;
    std::cout << "Time stamp (ns):" << second_stamp << std::endl;
    std::cout << "Time stamp (ns):" << third_stamp << std::endl;
    std::cout << "Time stamp (ns):" << forth_stamp << std::endl;

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

    // init values are meaningful as result will be checked afterwards
    for (int64_t i = 0; i < static_cast<int64_t>(kVectorSize); ++i) {
      init_data_host[i] = i;
      zero_data_host[i] = 0LL;
    }

    // H2D - once
    q.memcpy(a, init_data_host.get(), kVectorSize * sizeof(int64_t)).wait();
    q.memcpy(b, init_data_host.get(), kVectorSize * sizeof(int64_t)).wait();
    q.memcpy(c, zero_data_host.get(), kVectorSize * sizeof(int64_t)).wait();

    auto start = std::chrono::steady_clock::now();
    for (size_t iter = 0; iter < kRepetitions; iter++) {
      VecAdd(q, a, b, c, kVectorSize);
    }
    q.wait();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    time_sec = time.count();

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
  return time_sec;
}

enum CollectionMode { kModeFull = 0, kModeHybrid = 1, kModeLocal = 2 };

}  // namespace

class NoKernelOverlapParametrizedTestFixture
    : public ::testing::TestWithParam<std::tuple<bool, CollectionMode>> {
 protected:
  NoKernelOverlapParametrizedTestFixture() {
    // Setup work for each test
  }

  ~NoKernelOverlapParametrizedTestFixture() override {
    // Cleanup work for each test
  }

  static bool do_immediate;
  static std::vector<pti_view_record_kernel> kernel_records;

  void SetUp() override {  // Called right after constructor before each test
    kernel_records.clear();

    auto [immediate, collection_mode] = GetParam();
    do_immediate = immediate;
    if (immediate) {
      std::cout << " ** Immediate command list mode" << std::endl;
    } else {
      std::cout << " ** Non-immediate command list mode" << std::endl;
    }

    // After we got more experience with this test - we can pass Gaps
    // as test parameters.
    // At the moment we want to test that different events of kernel life are not far away from
    // each other.
    //
    // while passed ModeLocal local it is expected it will be selected automatically of course
    // in case introspection API is available
    if (collection_mode != CollectionMode::kModeLocal) {
      utils::SetEnv("PTI_COLLECTION_MODE", std::to_string(collection_mode).c_str());
    }
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

  /**
   * @internal
   * Checks that all GPU kernels have the same append and submit timestamps
   * which holds for Immediate command list
   */
  void TestForAppendSubmitAtImmediate() {
    for (uint32_t kidx = 0; kidx < kernel_records.size(); ++kidx) {
      auto kernel = kernel_records[kidx];
      if (kernel._append_timestamp != kernel._submit_timestamp) {
        FAIL() << "--->  ERROR: Append and Submit timestamps not equal at i: " << kidx
               << " \t append: " << kernel._append_timestamp
               << ", submit: " << kernel._submit_timestamp << std::endl;
      }
    }
  }

  std::stringstream PrintKernelTimeStamps(const pti_view_record_kernel& kernel) {
    std::stringstream ss;
    ss << "Sycl Task Begin Time:        " << kernel._sycl_task_begin_timestamp << "\n"
       << "Sycl Enq Launch Kernel Time: " << kernel._sycl_enqk_begin_timestamp << "\n"
       << "Append Time:                 " << kernel._append_timestamp << "\n"
       << "Submit Time:                 " << kernel._submit_timestamp << "\n"
       << "Start Time:                  " << kernel._start_timestamp << "\n"
       << "End Time:                    " << kernel._end_timestamp;
    return ss;
  }

  /**
   * @internal
   * Checks timestamps of GPU kernels for different types of consistency within one kernel
   * and in relations to the neighbour kernels
   *
   * Extensive output helps debug speedup in case of the test failure
   */
  void InspectKernelRecords(uint32_t repetitions, float kernel_loop_duration_sec) {
    // Sort by kernel records by sycl_task_begin_timestamp
    std::sort(kernel_records.begin(), kernel_records.end(),
              [](const pti_view_record_kernel& r1, const pti_view_record_kernel& r2) {
                return r1._sycl_task_begin_timestamp < r2._sycl_task_begin_timestamp;
              });

    // Check that timestamps of neighbour kernels are not overlapping
    // this holds true for kernels submitted to the same in-order queue
    for (uint32_t kidx = 1; kidx < kernel_records.size(); ++kidx) {
      auto k0 = kernel_records[kidx - 1];
      auto k1 = kernel_records[kidx];
      if (k1._start_timestamp <= k0._end_timestamp) {
        FAIL() << "--->  ERROR: Device kernel timestamps overlap end_of_i < start_of_i-1, at i: "
               << kidx << ", end_of_i-1: " << k0._end_timestamp
               << ", start_of_i: " << k1._start_timestamp << "\n"
               << "...Kernel  Details: i-1:" << kidx - 1 << "\n"
               << PrintKernelTimeStamps(k0).str() << "\n"
               << "...                   i:" << kidx << "\n"
               << PrintKernelTimeStamps(k1).str() << "\n";
      }
    }

    uint64_t max_gap_ns = static_cast<uint64_t>(kernel_loop_duration_sec * 1e9);

    std::cout << "Repetitions: " << repetitions
              << ", kernel loop duration: " << static_cast<uint64_t>(kernel_loop_duration_sec * 1e9)
              << " ns" << std::endl;

    // Checks for the monotonicity of different stages timestamps of a kernel
    // as well as the "gap" between these timestamps in not "big" -
    // top bound of the test compute loop duration time
    for (uint32_t kidx = 0; kidx < kernel_records.size(); ++kidx) {
      auto kernel = kernel_records[kidx];
      auto found_issues = pti::test::utils::ValidateTimestamps(
          kernel._sycl_task_begin_timestamp, kernel._sycl_enqk_begin_timestamp,
          kernel._append_timestamp, kernel._submit_timestamp, kernel._start_timestamp,
          kernel._end_timestamp);

      if (found_issues > 0) {
        FAIL() << "------------>     ERROR: Not monotonic kernel timestamps. Here are details:\n"
               << PrintKernelTimeStamps(kernel).str() << "\n";
      }

      if (kernel._sycl_task_begin_timestamp == 0) {
        std::cout << "WARN ------------>     Something wrong: Sycl Enq Launch Kernel Time is 0"
                  << std::endl;
      }

      if (kernel._sycl_enqk_begin_timestamp == 0) {
        FAIL() << "------------>     Something wrong: Sycl Enq Launch Kernel Time is 0"
               << std::endl;
      }
      if (kernel._start_timestamp == kernel._end_timestamp) {
        FAIL() << "------------>     Something wrong: Append Time is 0" << std::endl;
      }
      std::cout << " ** Kernel " << kidx << " Start: " << kernel._sycl_task_begin_timestamp
                << std::setw(8) << std::right << " Full time: " << std::setw(8) << std::right
                << kernel._end_timestamp - kernel._sycl_task_begin_timestamp << "\n"
                << std::setw(8) << std::right
                << "Sycl Task Begin to Sycl Enq Time: " << std::setw(8) << std::right
                << kernel._sycl_enqk_begin_timestamp - kernel._sycl_task_begin_timestamp << "\n"
                << "Sycl Enq to Append Time:          " << std::setw(8) << std::right
                << kernel._append_timestamp - kernel._sycl_enqk_begin_timestamp << "\n"
                << "Append to Submit Time:            " << std::setw(8) << std::right
                << kernel._submit_timestamp - kernel._append_timestamp << "\n"
                << "Submit to Start Time:             " << std::setw(8) << std::right
                << kernel._start_timestamp - kernel._submit_timestamp << "\n"
                << "Start to End Time:                " << std::setw(8) << std::right
                << kernel._end_timestamp - kernel._start_timestamp << "\n";

      if (kernel._sycl_task_begin_timestamp != 0) {
        if (0 !=
            pti::test::utils::ValidateNoBigGapBetweenTimestampsNs(
                max_gap_ns, {kernel._sycl_task_begin_timestamp, kernel._sycl_enqk_begin_timestamp,
                             kernel._append_timestamp, kernel._submit_timestamp,
                             kernel._start_timestamp, kernel._end_timestamp})) {
          FAIL() << "------------>     ERROR: Gap between timestamps more than " << max_gap_ns
                 << " ns\n"
                 << PrintKernelTimeStamps(kernel).str() << "\n";
        }
      } else {
        if (0 != pti::test::utils::ValidateNoBigGapBetweenTimestampsNs(
                     max_gap_ns,
                     {kernel._sycl_enqk_begin_timestamp, kernel._append_timestamp,
                      kernel._submit_timestamp, kernel._start_timestamp, kernel._end_timestamp})) {
          FAIL() << "------------>     ERROR: Gap between timestamps more than " << max_gap_ns
                 << " ns\n"
                 << PrintKernelTimeStamps(kernel).str() << "\n";
        }
      }
    }
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
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }

    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        std::cout << "Reached End of buffer " << '\n';
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        std::cerr << "Error Parsing Records in buffer.  PTI buf_status: " << buf_status << '\n';
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          std::cout << "Found Invalid Record" << '\n';
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          pti_view_record_kernel* p_kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
          kernel_records.push_back(*p_kernel_rec);
          break;
        }
        default: {
          break;
        }
      }
    }
    ::operator delete(buf);
  }

  float RunTest(bool do_immediate) {
    StartTracing();
    auto kernel_loop_duration = TestCore(do_immediate);
    StopTracing();
    EXPECT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
    return kernel_loop_duration;
  }
};

// static members initialization
std::vector<pti_view_record_kernel> NoKernelOverlapParametrizedTestFixture::kernel_records{};
bool NoKernelOverlapParametrizedTestFixture::do_immediate = true;

TEST_P(NoKernelOverlapParametrizedTestFixture, NoKernelOverlapImmediate) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);

  auto kernel_loop_duration_sec = RunTest(do_immediate);
  EXPECT_GE(kernel_loop_duration_sec, 0.0f);
  std::cout << "Kernel loop duration: " << kernel_loop_duration_sec << " sec, "
            << kernel_loop_duration_sec * 1e9 << "ns" << std::endl;

  EXPECT_EQ(kernel_records.size(), kRepetitions);

  InspectKernelRecords(kRepetitions, kernel_loop_duration_sec);

  if (do_immediate) {
    TestForAppendSubmitAtImmediate();
  }
}

INSTANTIATE_TEST_SUITE_P(NoKernelOverlapAndGapInSubmittingTests,
                         NoKernelOverlapParametrizedTestFixture,
                         ::testing::Values(std::make_tuple(true, CollectionMode::kModeFull),
                                           std::make_tuple(false, CollectionMode::kModeFull),
                                           std::make_tuple(true, CollectionMode::kModeHybrid),
                                           std::make_tuple(false, CollectionMode::kModeHybrid),
                                           std::make_tuple(true, CollectionMode::kModeLocal),
                                           std::make_tuple(false, CollectionMode::kModeLocal)));
