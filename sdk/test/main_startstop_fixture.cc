//==============================================================
// Based on Vector Add example from OneAPI samples
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"

namespace {

enum class TestType {
  kArbStartStop = 1,
  kArbStartStopDupEnables = 2,
  kArbStartStopDupDisables = 3,
  kArbStartStopNoKernelStart = 4,
  kArbStartStopNoKernelStop = 5,
  kArbStartStopSycl = 6,
  kArbStartStopMt = 7,
  kArbStartStopMtDupEnables = 8,
  kArbStartStopMtDupDisables = 9,
  kArbStartStopMtNoKernelStart = 10,
  kArbStartStopMtNoKernelStop = 11,
  kArbStartStopMtSycl = 12,
};

constexpr uint32_t kArbStartStopCounter =
    4;  // keep this a even number if changed -- test requires it.

bool matched_sq_corr_ids = false;
bool matched_add_corr_ids = false;
bool timestamps_nonzero_duration = true;
bool kernel_timestamps_monotonic = false;
constexpr size_t kVectorSize = 5000;
constexpr size_t kThreadCount = 3;
uint64_t sycl_kernel_corr_id[3];
uint64_t sycl_kernel_start_time[3];
uint64_t kernel_corr_id[3];
uint64_t kernel_append_time[3];
uint64_t sycl_idx = 0;
uint64_t kernel_idx = 0;
uint64_t kernel_start_ts = 0;
uint64_t kernel_stop_ts = 0;
uint64_t number_of_kernel_recs = 0;
uint64_t number_of_sycl_recs = 0;
uint64_t expected_sycl_recs = 0;
uint64_t eid = 11;                  // external correlation id base.
constexpr uint64_t kEpsilon = 100;  // min in kernel duration in nanoseconds

// sync variables
std::mutex common_m;
std::condition_variable main_cv;
std::atomic<size_t> shared_thread_count[kArbStartStopCounter] = {0};

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
template <typename T>
void VecSq(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector) {
  ASSERT_EQ(ptiViewPushExternalCorrelationId(
                pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid + 20),
            pti_result::PTI_SUCCESS);
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_write);
    sycl::accessor b(b_buf, h, sycl::read_write);
    h.parallel_for(num_items, [=](auto i) {
      a[i] = a[i] * a[i];
      b[i] = b[i] * b[i];
    });
  });
  q.wait();
  ASSERT_EQ(ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3,
                                            &eid),
            pti_result::PTI_SUCCESS);
}

//************************************
// Vector add in SYCL on device: returns sum in 4th parameter "sq_add".
//************************************
template <typename T>
[[maybe_unused]] void VecAdd(sycl::queue &q, const std::vector<T> &a_vector,
                             const std::vector<T> &b_vector, std::vector<T> &sq_add) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);
  sycl::buffer sum_buf(sq_add.data(), num_items);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_only);
    sycl::accessor b(b_buf, h, sycl::read_only);
    sycl::accessor sum(sum_buf, h, sycl::write_only, sycl::no_init);
    h.parallel_for(num_items, [=](auto i) { sum[i] = a[i] + b[i]; });
  });
  q.wait();
}

[[maybe_unused]] void StartTracingNonL0() {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

[[maybe_unused]] void StopTracingNonL0() {
  ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION);  // TODO: check return
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

[[maybe_unused]] void StartTracingMTL0([[maybe_unused]] TestType type) {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
}

void StartTracingL0([[maybe_unused]] TestType type) {
  if (type != TestType::kArbStartStopNoKernelStart)
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  if (type == TestType::kArbStartStopDupEnables) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
}

[[maybe_unused]] void StopTracingMTL0([[maybe_unused]] TestType type) {
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
}

void StopTracingL0([[maybe_unused]] TestType type) {
  if (type != TestType::kArbStartStopNoKernelStop) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
  if (type == TestType::kArbStartStopDupDisables) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
}

static void BufferRequested(unsigned char **buf, size_t *buf_size) {
  *buf_size = 10000 * sizeof(pti_view_record_kernel);
  void *ptr = ::operator new(*buf_size);
  ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
  *buf = reinterpret_cast<unsigned char *>(ptr);
  if (!*buf) {
    std::abort();
  }
  return;
}

static void BufferCompleted(unsigned char *buf, size_t buf_size, size_t valid_buf_size) {
  if (!buf_size || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (valid_buf_size) {
      ::operator delete(buf);
    }
    return;
  }
  pti_view_record_base *ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
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
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        [[maybe_unused]] pti_view_record_external_correlation *a_ext_rec =
            reinterpret_cast<pti_view_record_external_correlation *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        [[maybe_unused]] pti_view_record_overhead *record =
            reinterpret_cast<pti_view_record_overhead *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        break;
      }
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        number_of_sycl_recs++;
        pti_view_record_api *rec = reinterpret_cast<pti_view_record_api *>(ptr);
        const char *pName = nullptr;
        pti_result status =
            ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, rec->_api_id, &pName);
        PTI_ASSERT(status == PTI_SUCCESS);
        std::string function_name(pName);
        if ((sycl_idx < 2) && (function_name.find("EnqueueKernelLaunch") != std::string::npos)) {
          sycl_kernel_corr_id[sycl_idx] = rec->_correlation_id;
          sycl_kernel_start_time[sycl_idx] = rec->_start_timestamp;
          sycl_idx++;
        };
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        pti_view_record_kernel *a_kernel_rec = reinterpret_cast<pti_view_record_kernel *>(ptr);
        std::string kernel_name = a_kernel_rec->_name;
        number_of_kernel_recs++;
        if ((kernel_idx < 2) && (kernel_name.find("VecSq<") != std::string::npos)) {
          kernel_corr_id[kernel_idx] = a_kernel_rec->_correlation_id;
          kernel_append_time[kernel_idx] = a_kernel_rec->_append_timestamp;
          kernel_idx++;
        }
        if ((kernel_idx < 2) && (kernel_name.find("VecAdd<") != std::string::npos)) {
          kernel_corr_id[kernel_idx] = a_kernel_rec->_correlation_id;
          kernel_append_time[kernel_idx] = a_kernel_rec->_append_timestamp;
          kernel_idx++;
        }
        // std::cout << "KernelTimestamp for VecAdd: " << a_kernel_rec->_append_timestamp << ":"
        //           << a_kernel_rec->_thread_id << "\n";
        timestamps_nonzero_duration =
            timestamps_nonzero_duration &&
            ((a_kernel_rec->_end_timestamp - a_kernel_rec->_start_timestamp) > kEpsilon);

        kernel_timestamps_monotonic = samples_utils::isMonotonic(
            {a_kernel_rec->_sycl_task_begin_timestamp, a_kernel_rec->_sycl_enqk_begin_timestamp,
             a_kernel_rec->_append_timestamp, a_kernel_rec->_submit_timestamp,
             a_kernel_rec->_start_timestamp, a_kernel_rec->_end_timestamp});
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  ::operator delete(buf);
}

template <typename T>
[[maybe_unused]] void InitVectors(std::vector<T> &a, std::vector<T> &b, std::vector<T> &c,
                                  std::vector<T> &d) {
  a.resize(kVectorSize);
  b.resize(kVectorSize);
  c.resize(2 * kVectorSize);
  d.resize(2 * kVectorSize);

  for (size_t i = 0; i < kVectorSize; i++) {
    a[i] = std::sin(i);
    b[i] = std::cos(i);
    c[2 * i] = std::sin(i) * std::sin(i);
    c[2 * i + 1] = std::sin(i);
    d[2 * i] = std::cos(i) * std::cos(i);
    d[2 * i + 1] = std::cos(i);
  }
}

[[maybe_unused]] void ArriveAndWait(unsigned int index) {
  std::unique_lock<std::mutex> lk(common_m);
  shared_thread_count[index]++;
  main_cv.wait(lk, [&] { return (shared_thread_count[index] == kThreadCount); });
  main_cv.notify_all();
}

template <typename T>
void RunArbStartStopTestMultiThreaded(sycl::queue &q, const std::vector<T> &a,
                                      const std::vector<T> &b, TestType a_test_type) {
  auto thread_function = [&q](const auto &a, const auto &b, TestType a_test_type) {
    auto start = std::chrono::steady_clock::now();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    for (unsigned int i = 0; i < kArbStartStopCounter; i++) {
      if (i % 2) {
        if (a_test_type != TestType::kArbStartStopMtSycl)
          StartTracingL0(a_test_type);
        else
          StartTracingNonL0();
      }
      VecSq(q, a, b);
      ArriveAndWait(i);
      if (i % 2) {
        if (a_test_type != TestType::kArbStartStopMtSycl) {
          StopTracingL0(a_test_type);
        } else {
          StopTracingNonL0();
        }
        ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
      }
    }
    std::cout << "\t-- Total execution time: " << time.count() << " sec" << std::endl;
  };

  std::vector<std::thread> the_threads;
  for (unsigned i = 0; i < kThreadCount; i++) {
    std::thread t = std::thread(thread_function, a, b, a_test_type);
    the_threads.push_back(std::move(t));
  }

  for (auto &th : the_threads) {
    if (th.joinable()) {
      th.join();
    }
  }
}

template <typename T>
void RunArbStartStopTest(sycl::queue &q, [[maybe_unused]] const std::vector<T> &a,
                         [[maybe_unused]] const std::vector<T> &b, TestType a_test_type) {
  for (unsigned int i = 1; i <= kArbStartStopCounter; i++) {
    if (i % 2) {
      if (a_test_type != TestType::kArbStartStopSycl)
        StartTracingL0(a_test_type);
      else
        StartTracingNonL0();
    }

    VecSq(q, a, b);

    if (i % 2) {
      if (a_test_type != TestType::kArbStartStopSycl) {
        StopTracingL0(a_test_type);
      } else {
        StopTracingNonL0();
      }
      ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
    }
  }
}

template <typename T>
void VecSqAddRouter(sycl::queue &sycl_queue, TestType a_test_type) {
  std::vector<T> a(kVectorSize);
  std::vector<T> b(kVectorSize);
  std::vector<T> c(2 * kVectorSize);
  std::vector<T> d(2 * kVectorSize);
  std::vector<T> sq_add(kVectorSize);
  std::vector<T> sq_add2(2 * kVectorSize);

  for (size_t i = 0; i < kVectorSize; i++) {
    a[i] = std::sin(i);
    b[i] = std::cos(i);
    c[2 * i] = std::sin(i) * std::sin(i);
    c[2 * i + 1] = std::sin(i);
    d[2 * i] = std::cos(i) * std::cos(i);
    d[2 * i + 1] = std::cos(i);
  }

  // Start Tests by Type
  if (a_test_type == TestType::kArbStartStop) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopDupEnables) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopDupDisables) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopNoKernelStart) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopNoKernelStop) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopSycl) {
    RunArbStartStopTest(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopMt) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, a_test_type);
  }

  if (a_test_type == TestType::kArbStartStopMtDupEnables) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, TestType::kArbStartStopDupEnables);
  }

  if (a_test_type == TestType::kArbStartStopMtDupDisables) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, TestType::kArbStartStopDupDisables);
  }

  if (a_test_type == TestType::kArbStartStopMtNoKernelStart) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, TestType::kArbStartStopNoKernelStart);
  }

  if (a_test_type == TestType::kArbStartStopMtNoKernelStop) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, TestType::kArbStartStopNoKernelStop);
  }

  if (a_test_type == TestType::kArbStartStopMtSycl) {
    RunArbStartStopTestMultiThreaded(sycl_queue, a, b, TestType::kArbStartStopMtSycl);
  }
}

void RunVecsqadd(TestType a_test_type) {
  ASSERT_EQ(ptiViewPushExternalCorrelationId(
                pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid),
            pti_result::PTI_SUCCESS);

  auto dev = sycl::device(sycl::gpu_selector_v);

  auto d_selector{sycl::gpu_selector_v};
  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue q(d_selector, sycl::async_handler{}, prop_list);

  if (q.get_device().has(sycl::aspect::fp64)) {
    VecSqAddRouter<double>(q, a_test_type);
  } else {
    VecSqAddRouter<float>(q, a_test_type);
  }

  ASSERT_EQ(ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3,
                                            &eid),
            pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
}

}  // namespace

class StartStopFixtureTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    matched_sq_corr_ids = false;
    matched_add_corr_ids = false;
    timestamps_nonzero_duration = true;
    kernel_timestamps_monotonic = false;
    kernel_start_ts = 0;
    kernel_stop_ts = 0;
    number_of_kernel_recs = 0;
    number_of_sycl_recs = 0;
    expected_sycl_recs = 0;
    for (unsigned int i = 0; i < kArbStartStopCounter; i++) {
      shared_thread_count[i] = 0;
    }
  }

  void TearDown() override {
    // Called right before destructor after each test
  }
};

// MT - StartTracing / StopTracing have matching enable/disable of gpu_kernels every other
// iteration.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedBalanced) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMt);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter * kThreadCount / 2);
  EXPECT_EQ(timestamps_nonzero_duration, true);
  EXPECT_EQ(kernel_timestamps_monotonic, true);
}

// MT - Enable gpu_kernels multiple times when we start / stop disables it once.   Should have no
// effect on expected kernels.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedKernelsDuplicatedEnables) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMtDupEnables);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter * kThreadCount / 2);
}

// MT - Enable gpu_kernels once when we start / stop disables it multiple times.   Should have no
// effect on expected kernels.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedKernelsDuplicatedDisables) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMtDupDisables);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter * kThreadCount / 2);
}

// MT - sycl only tracing -- no l0.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedSycls) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMtSycl);
  EXPECT_GT(number_of_sycl_recs, 0ULL);
  EXPECT_EQ(number_of_kernel_recs, 0ULL);
}

// MT - StartTracing does *not* enable gpu_kernel view kind,  StopTracing has disable for it -- so
// expect 0 kernel recs.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedNoStartKernelWithStopKernel) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMtNoKernelStart);
  EXPECT_EQ(number_of_kernel_recs, 0ULL);
}

// MT - StartTracing enable gpu_kernel view kind,  StopTracing does not disable it -- so span of
// tracing is 1st start to end of prog.
TEST_P(StartStopFixtureTest, ArbStartStopCountMultiThreadedNoStopKernelWithStartKernel) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopMtNoKernelStop);
  if (do_immediate)
    EXPECT_EQ(number_of_kernel_recs, (kArbStartStopCounter - 1) * kThreadCount);
  else
    EXPECT_EQ(number_of_kernel_recs, (kArbStartStopCounter - 1) * kThreadCount);
}

// Enable gpu_kernels once when we start / stop disables it multiple times.   Should have no effect
// on expected kernels.
TEST_P(StartStopFixtureTest, ArbStartStopCountKernelsDuplicatedDisables) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopDupDisables);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter / 2);
}

// Enable gpu_kernels multiple times when we start / stop disables it once.   Should have no effect
// on expected kernels.
TEST_P(StartStopFixtureTest, ArbStartStopCountKernelsDuplicatedEnables) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopDupEnables);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter / 2);
}

// StartTracing / StopTracing have matching enable/disable of gpu_kernels every other iteration.
TEST_P(StartStopFixtureTest, ArbStartStopCountKernelsBalanced) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStop);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter / 2);
  EXPECT_EQ(timestamps_nonzero_duration, true);
  EXPECT_EQ(kernel_timestamps_monotonic, true);
}

// sycl only tracing -- no l0.
TEST_P(StartStopFixtureTest, ArbStartStopCountSycls) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopSycl);
  EXPECT_GT(number_of_sycl_recs, 0ULL);
  EXPECT_EQ(number_of_kernel_recs, 0ULL);
}

// StartTracing does *not* enable gpu_kernel view kind,  StopTracing has disable it -- so expect 0
// kernel recs.
TEST_P(StartStopFixtureTest, ArbStartStopCountNoStartKernelWithStopKernel) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopNoKernelStart);
  EXPECT_EQ(number_of_kernel_recs, 0ULL);
}

// StartTracing enable gpu_kernel view kind,  StopTracing does not disable it -- so span of tracing
// is 1st start to end of prog.
TEST_P(StartStopFixtureTest, ArbStartStopCountNoStopKernelWithStartKernel) {
  bool do_immediate = GetParam();
  // use SetEnv until
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::kArbStartStopNoKernelStop);
  EXPECT_EQ(number_of_kernel_recs, kArbStartStopCounter);
}

INSTANTIATE_TEST_SUITE_P(StartStopTests, StartStopFixtureTest, ::testing::Values(false, true));
