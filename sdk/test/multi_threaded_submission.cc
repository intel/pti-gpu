#include <gtest/gtest.h>
#include <string.h>

#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <sycl/sycl.hpp>
#include <thread>
#include <unordered_map>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"
#include "utils/test_helpers.h"

namespace {
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

static bool verbose = true;

static float Check(const std::vector<float>& a, float value) {
  assert(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static void SubmitKernel(sycl::queue queue, const std::vector<float>& a,
                         const std::vector<float>& b, std::vector<float>& c, unsigned size) {
  assert(size > 0);
  assert(a.size() == size * size);
  assert(b.size() == size * size);
  assert(c.size() == size * size);

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    sycl::event event = queue.submit([&](sycl::handler& cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
      auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
      auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

      cgh.parallel_for<class __GEMM>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
        auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
        GEMM(a_acc_ptr.get(), b_acc_ptr.get(), c_acc_ptr.get(), size, id);
      });
    });
  } catch (const sycl::exception& e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& c, unsigned size, unsigned repeat_count) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    SubmitKernel(queue, a, b, c, size);
  }
}

const unsigned thread_count = 2;
const unsigned thread_count_with_main = thread_count + 1;
const unsigned repeat_count = 2;
const unsigned size = 1024;

std::array<uint32_t, thread_count_with_main> records_per_thread = {0};

constexpr auto kRequestedRecordCount =
    repeat_count * 1'000ULL;  // reserving enough space
                              // for all records in one buffer per thread
constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

void StartTracing() {
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS));
}

void StopTracing() {
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS));
}

void ProvideBuffer(unsigned char** buf, std::size_t* buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = kRequestedBufferSize;
}

void ParseBuffer(unsigned char* buf, std::size_t buf_size, std::size_t valid_buf_size) {
  static std::atomic<uint32_t> thread_index = 0;
  static std::atomic<uint32_t> times_buffer_completed = 0;
  uint32_t record_count = 0;
  std::unordered_map<uint32_t, uint32_t> corr_id_map;

  times_buffer_completed++;
  /*
    ASSERT_TRUE(times_buffer_completed <= thread_count_with_main)
        << "ERROR: Not expected to enter to " << __FUNCTION__ << " more then " << thread_count
        << ", entered here : " << times_buffer_completed << " times";
  */

  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    samples_utils::AlignedDealloc(buf);
    return;
  }
  pti_view_record_base* ptr = nullptr;

  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      records_per_thread[thread_index] = record_count;
      thread_index++;
      std::cout << "Reached End of buffer, Record count: " << record_count << '\n';
      break;
    }
    record_count++;
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Sycl Runtime Record" << '\n';
        pti_view_record_sycl_runtime* p_runtime_rec =
            reinterpret_cast<pti_view_record_sycl_runtime*>(ptr);
        samples_utils::dump_record(p_runtime_rec);

        ASSERT_TRUE(corr_id_map.find(p_runtime_rec->_correlation_id) == corr_id_map.end())
            << "ERROR: Found duplicate correlation id: " << p_runtime_rec->_correlation_id;
        corr_id_map[p_runtime_rec->_correlation_id]++;
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        // samples_utils::dump_record(reinterpret_cast<pti_view_record_overhead*>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        // samples_utils::dump_record(reinterpret_cast<pti_view_record_external_correlation*>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';
        pti_view_record_memory_copy* p_memory_rec =
            reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        // samples_utils::dump_record(p_memory_rec);

        ASSERT_TRUE(corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found emply correlation id: " << p_memory_rec->_correlation_id;

        if (corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_memory_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpcted correlation id: "
              << p_memory_rec->_correlation_id << "corr_id_map"
              << corr_id_map[p_memory_rec->_correlation_id];
        }

        auto found_issues = pti::test::utils::ValidateTimestamps(
            p_memory_rec->_append_timestamp, p_memory_rec->_submit_timestamp,
            p_memory_rec->_start_timestamp, p_memory_rec->_end_timestamp);
        if (found_issues > 0) {
          FAIL() << "------------>     ERROR: Not monotonic kernel timestamps";
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';

        pti_view_record_memory_fill* p_memory_rec =
            reinterpret_cast<pti_view_record_memory_fill*>(ptr);
        // samples_utils::dump_record(p_memory_rec);
        ASSERT_TRUE(corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found emply correlation id: " << p_memory_rec->_correlation_id;

        if (corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_memory_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpcted correlation id: "
              << p_memory_rec->_correlation_id << "corr_id_map"
              << corr_id_map[p_memory_rec->_correlation_id];
        }

        auto found_issues = pti::test::utils::ValidateTimestamps(
            p_memory_rec->_append_timestamp, p_memory_rec->_submit_timestamp,
            p_memory_rec->_start_timestamp, p_memory_rec->_end_timestamp);
        if (found_issues > 0) {
          FAIL() << "------------>     ERROR: Not monotonic kernel timestamps";
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Kernel Record" << '\n';

        pti_view_record_kernel* p_kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
        samples_utils::dump_record(p_kernel_rec);
        ASSERT_TRUE(corr_id_map.find(p_kernel_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found emply correlation id: " << p_kernel_rec->_correlation_id;

        if (corr_id_map.find(p_kernel_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_kernel_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpcted correlation id: "
              << p_kernel_rec->_correlation_id << "corr_id_map"
              << corr_id_map[p_kernel_rec->_correlation_id];
        }

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
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  samples_utils::AlignedDealloc(buf);
}

std::mutex mutex;
std::condition_variable cv;  // no barrier in c++17 - so using condition variable
bool ready = false;

int SymmetricMultithreadedWithMain() {
  int exit_code = EXIT_SUCCESS;

  try {
    StartTracing();
    sycl::device dev;
    dev = sycl::device(sycl::gpu_selector_v);
    sycl::property_list prop_list{sycl::property::queue::in_order(),
                                  sycl::property::queue::enable_profiling()};
    sycl::queue queue(dev, sycl::async_handler{}, prop_list);
    float expected_result = A_VALUE * B_VALUE * size;

    auto threadFunction = [&queue](unsigned _size, unsigned _repeat_count, float expected_result) {
      std::vector<float> a(_size * _size, A_VALUE);
      std::vector<float> b(_size * _size, B_VALUE);
      std::vector<float> c(_size * _size, 0.0f);

      [[maybe_unused]] auto start = std::chrono::steady_clock::now();
      Compute(queue, a, b, c, _size, _repeat_count);

      // wait until main thread synchronize queue
      {
        std::unique_lock lk(mutex);
        cv.wait(lk, [] { return ready; });
        [[maybe_unused]] auto end = std::chrono::steady_clock::now();
        [[maybe_unused]] std::chrono::duration<float> time = end - start;

        if (verbose) {
          std::cout << "\t-- Execution Time: " << time.count() << " sec" << std::endl;
        }
        ASSERT_TRUE(Check(c, expected_result) <= MAX_EPS);
      }
    };

    if (verbose) {
      std::cout << "DPC++ Matrix Multiplication (CPU threads: " << thread_count
                << ", matrix size: " << size << " x " << size << ", repeats: " << repeat_count
                << " times)" << std::endl;
      std::cout << "Target device: "
                << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
                << std::endl
                << std::flush;
    }

    std::vector<std::thread> the_threads;
    {
      std::lock_guard lk(mutex);

      for (unsigned i = 0; i < thread_count; i++) {
        std::thread t = std::thread(threadFunction, size, repeat_count, expected_result);
        the_threads.push_back(std::move(t));
      }

      queue.wait_and_throw();
    }
    ready = true;
    cv.notify_all();

    for (auto& th : the_threads) {
      if (th.joinable()) {
        th.join();
      }
    }
    StopTracing();
    PTI_THROW(ptiFlushAllViews());
  } catch (const sycl::exception& e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    exit_code = EXIT_FAILURE;
  }
  return exit_code;
}

}  // namespace

class MulthiThreadedSubmissionFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override { std::fill(records_per_thread.begin(), records_per_thread.end(), 0); }

  void TearDown() override {
    // Called right before destructor after each test
  }
};

// Test verifies that GPU ops reported in the thread buffers where ops were submitted
// in this test all working threads did the same work - so the number of records should be the same
// in all buffers, except for the empty buffer of the main thread
TEST_F(MulthiThreadedSubmissionFixtureTest, MulthiThreadedSubmissionTest) {
  ASSERT_TRUE(thread_count > 1);
  EXPECT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);
  EXPECT_EQ(SymmetricMultithreadedWithMain(), EXIT_SUCCESS);
  auto non_zero_record_count =
      records_per_thread[0] != 0 ? records_per_thread[0] : records_per_thread[1];
  for (unsigned i = 0; i < thread_count_with_main; i++) {
    if (records_per_thread[i] != 0) {
      EXPECT_EQ(records_per_thread[i], non_zero_record_count);
    }
  }
}
