#include <gtest/gtest.h>
#include <string.h>

#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <sycl/sycl.hpp>
#include <thread>
#include <unordered_map>
#include <utility>

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

const unsigned working_thread_count = 2;
const unsigned repeat_count = 2;
const unsigned size = 1024;

constexpr auto kRequestedRecordCount =
    repeat_count * 1'000ULL;  // reserving enough space
                              // for all records in one buffer per thread
constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

/**
 * @internal
 * The structures below is to store aggregated statistic about particular profiling record
 * count collected per a thread.
 * The test then uses this information to verify that each thread collected the same number
 * of records of a specific type.
 *
 * A record type is defined as ExtendedViewKind,
 * which is a pair of a view_kind and so-called a sub_kind
 *
 * view_kind is exactly pti_view_kind casted to uint32_t.
 *
 * But view_kind is too coarse -
 * e.g. any runtime API record has PTI_VIEW_RUNTIME_API view_kind
 * But as we want to do detail comparision - we need more specific info - e.g. ID of the API.
 * Or for a memory transfer a view_kind is PTI_VIEW_DEVICE_GPU_MEM_COPY.
 * But again - for the detail comparision we need to know the type of the transfer it is -
 * e.g. host to device or device to host, read or write, etc.
 *
 * So we define a sub_kind field to hold that additional infomation where we put
 * either the ID of the API or a type of the transfer, or any other enum value converted to
 * uint32_t.
 *
 * Finally we have a map { thread,  map { ExtendedViewKind, count of records} }
 */

struct ExtendedViewKind {
  ExtendedViewKind(pti_view_kind kind, uint32_t sub_kind) : value(kind, sub_kind) {}
  ExtendedViewKind(const ExtendedViewKind& other) : value(other.value) {}
  ExtendedViewKind& operator=(const ExtendedViewKind& other) {
    value = other.value;
    return *this;
  }
  ExtendedViewKind(ExtendedViewKind&& other) : value(other.value) {}
  ExtendedViewKind& operator=(ExtendedViewKind&& other) {
    value = other.value;
    return *this;
  }
  ~ExtendedViewKind() {}
  bool operator==(const ExtendedViewKind& other) const { return value == other.value; }
  std::pair<pti_view_kind, uint32_t> value;
};

struct ExtendedViewKindHash {
  size_t operator()(const ExtendedViewKind& v) const {
    return static_cast<size_t>(v.value.first) << 32 | v.value.second;
  }
};

// Map a specific ExtendedViewKind to a number of records of that kind
using ExtendedViewKindStatMap =
    std::unordered_map<ExtendedViewKind, uint32_t, ExtendedViewKindHash>;

// Map a thread ID to a ExtendedViewKindStatMap
std::unordered_map<uint32_t, ExtendedViewKindStatMap> thread_view_kind_stat_map;
std::mutex thread_view_kind_stat_map_mtx;

void IncrementThreadViewKindStat(uint32_t thread_id, pti_view_kind view_kind,
                                 uint32_t sub_kind = 0) {
  std::lock_guard<std::mutex> lock(thread_view_kind_stat_map_mtx);
  auto it_thread = thread_view_kind_stat_map.find(thread_id);
  if (it_thread == thread_view_kind_stat_map.end()) {
    thread_view_kind_stat_map[thread_id] = {{ExtendedViewKind(view_kind, sub_kind), 1}};
  } else {
    it_thread->second[ExtendedViewKind(view_kind, sub_kind)]++;
  }
}

void StartTracing() {
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_RUNTIME_API));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_SYNCHRONIZATION));
}

void StopTracing() {
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_RUNTIME_API));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_SYNCHRONIZATION));
}

void EnableIndividualRuntimeApis(bool useClassApi = false, bool useAllGroups = false) {
  if (useClassApi) {
    std::cout << "Using class apis:\n";
    pti_api_class this_class = pti_api_class::PTI_API_CLASS_ALL;
    if (useAllGroups) {
      std::cout << "Using all groups: setting both runtime/driver: all_classes: " << this_class
                << "\n";
      PTI_CHECK_SUCCESS(
          ptiViewEnableRuntimeApiClass(1, this_class, pti_api_group_id::PTI_API_GROUP_ALL));
    }
  } else {
    std::cout << "NOT Using class apis: -- all granular -- runtime only.\n";
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueUSMFill_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueUSMFill2D_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueUSMMemcpy_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueUSMMemcpy2D_id));

    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueMemBufferFill_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueMemBufferRead_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueMemBufferWrite_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueMemBufferCopy_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urUSMHostAlloc_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urUSMSharedAlloc_id));
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urUSMDeviceAlloc_id));

    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              pti_api_id_runtime_sycl::urEnqueueKernelLaunch_id));
    PTI_CHECK_SUCCESS(
        ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                pti_api_id_runtime_sycl::urEnqueueKernelLaunchCustomExp_id));
    PTI_CHECK_SUCCESS(
        ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                pti_api_id_runtime_sycl::urEnqueueCooperativeKernelLaunchExp_id));
  }
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
  std::unordered_map<uint32_t, uint32_t> corr_id_map;

  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    samples_utils::AlignedDealloc(buf);
    return;
  }
  pti_view_record_base* ptr = nullptr;

  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      std::cout << "Reached End of buffer"
                << "\n";
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
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Sycl Runtime Record" << '\n';
        pti_view_record_api* p_runtime_rec = reinterpret_cast<pti_view_record_api*>(ptr);
        IncrementThreadViewKindStat(p_runtime_rec->_thread_id, ptr->_view_kind,
                                    p_runtime_rec->_api_id);
        samples_utils::DumpRecord(p_runtime_rec);

        ASSERT_TRUE(corr_id_map.find(p_runtime_rec->_correlation_id) == corr_id_map.end())
            << "ERROR: Found duplicate correlation id: " << p_runtime_rec->_correlation_id;
        corr_id_map[p_runtime_rec->_correlation_id]++;
        break;
      }
      case pti_view_kind::PTI_VIEW_DRIVER_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Lz Api Record" << '\n';
        pti_view_record_api* p_driver_rec = reinterpret_cast<pti_view_record_api*>(ptr);
        IncrementThreadViewKindStat(p_driver_rec->_thread_id, ptr->_view_kind);
        samples_utils::DumpRecord(p_driver_rec);
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';

        pti_view_record_overhead* p_ovh_rec = reinterpret_cast<pti_view_record_overhead*>(ptr);
        IncrementThreadViewKindStat(p_ovh_rec->_overhead_thread_id, ptr->_view_kind);
        samples_utils::DumpRecord(p_ovh_rec);
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_external_correlation*>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';
        pti_view_record_memory_copy* p_memory_rec =
            reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        IncrementThreadViewKindStat(p_memory_rec->_thread_id, ptr->_view_kind,
                                    p_memory_rec->_memcpy_type);
        samples_utils::DumpRecord(p_memory_rec);

        ASSERT_TRUE(corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found empty correlation id: " << p_memory_rec->_correlation_id;

        if (corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_memory_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpected correlation id: "
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
        IncrementThreadViewKindStat(p_memory_rec->_thread_id, ptr->_view_kind);
        samples_utils::DumpRecord(p_memory_rec);
        ASSERT_TRUE(corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found empty correlation id: " << p_memory_rec->_correlation_id;

        if (corr_id_map.find(p_memory_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_memory_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpected correlation id: "
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
        IncrementThreadViewKindStat(p_kernel_rec->_thread_id, ptr->_view_kind);
        samples_utils::DumpRecord(p_kernel_rec);
        ASSERT_TRUE(corr_id_map.find(p_kernel_rec->_correlation_id) != corr_id_map.end())
            << "ERROR: Found empty correlation id: " << p_kernel_rec->_correlation_id;

        if (corr_id_map.find(p_kernel_rec->_correlation_id) != corr_id_map.end()) {
          ASSERT_TRUE(corr_id_map[p_kernel_rec->_correlation_id] == 1)
              << "ERROR: Found memory copy record with unexpected correlation id: "
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
      case pti_view_kind::PTI_VIEW_DEVICE_SYNCHRONIZATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Device Synchronization Record" << '\n';
        pti_view_record_synchronization* p_rec =
            reinterpret_cast<pti_view_record_synchronization*>(ptr);
        IncrementThreadViewKindStat(p_rec->_thread_id, ptr->_view_kind, p_rec->_synch_type);
        samples_utils::DumpRecord(p_rec);
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

int SymmetricMultithreadedWithMain(bool useClassApis = false, bool useAllGroup = false) {
  int exit_code = EXIT_SUCCESS;

  try {
    StartTracing();
    EnableIndividualRuntimeApis(useClassApis, useAllGroup);
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

      [[maybe_unused]] auto end = std::chrono::steady_clock::now();
      [[maybe_unused]] std::chrono::duration<float> time = end - start;

      if (verbose) {
        std::cout << "\t-- Execution Time: " << time.count() << " sec" << std::endl;
      }
      ASSERT_TRUE(Check(c, expected_result) <= MAX_EPS);
    };

    if (verbose) {
      std::cout << "DPC++ Matrix Multiplication (CPU threads: " << working_thread_count
                << ", matrix size: " << size << " x " << size << ", repeats: " << repeat_count
                << " times)" << std::endl;
      std::cout << "Target device: "
                << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
                << std::endl
                << std::flush;
    }

    std::vector<std::thread> the_threads;
    for (unsigned i = 0; i < working_thread_count; i++) {
      std::thread t = std::thread(threadFunction, size, repeat_count, expected_result);
      the_threads.push_back(std::move(t));
    }

    for (auto& th : the_threads) {
      if (th.joinable()) {
        th.join();
      }
    }
    queue.wait_and_throw();

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

void ValidateViewStats() {
  auto threads_with_records = thread_view_kind_stat_map.size();
  std::cout << "Threads with records: " << threads_with_records << "\n";
  EXPECT_GE(threads_with_records, working_thread_count)
      << "ERROR: Not all working threads have records, expected: " << working_thread_count
      << ", actual: " << threads_with_records;

  uint32_t main_thread_id = 0;  // it will be thread with the least number of records
                                // as all work - data copying, kernel submission done from
                                // working threads

  for (const auto& thread_view_kind_stat : thread_view_kind_stat_map) {
    std::cout << "Thread id: " << thread_view_kind_stat.first << "\n";
    if (threads_with_records > working_thread_count) {
      if (main_thread_id == 0) {
        main_thread_id = thread_view_kind_stat.first;
      }
      if (thread_view_kind_stat.second.size() < thread_view_kind_stat_map[main_thread_id].size()) {
        main_thread_id = thread_view_kind_stat.first;
      }
    }
    for (const auto& view_stat : thread_view_kind_stat.second) {
      std::cout << " View kind: " << static_cast<uint32_t>(view_stat.first.value.first)
                << " sub-kind: " << static_cast<uint32_t>(view_stat.first.value.second)
                << ", Record count: " << view_stat.second << '\n';
    }
  }
  if (main_thread_id == 0) {
    // removing main_thread stats as not interested in comparing its stats with working threads
    std::cout << "Main thread id: " << main_thread_id << "\n";
    thread_view_kind_stat_map.erase(main_thread_id);
  }

  auto it_thread1 = thread_view_kind_stat_map.begin();
  auto it_thread2 = it_thread1++;

  for (const auto& it_view1 : it_thread1->second) {
    std::cout << "Thread id: " << it_thread1->first
              << " View kind: " << static_cast<uint32_t>(it_view1.first.value.first)
              << " sub-kind: " << static_cast<uint32_t>(it_view1.first.value.second)
              << " Record count: " << it_view1.second << '\n';

    auto it_view2 = it_thread2->second.find(it_view1.first);
    if (it_view2 != it_thread2->second.end()) {
      EXPECT_EQ(it_view1.second, it_view2->second);
    } else {
      std::cout << "Thread id: " << it_thread2->first
                << " doesn't have the same view kind as thread id: " << it_thread1->first
                << " View kind: " << static_cast<uint32_t>(it_view1.first.value.first)
                << " Sub-kind: " << static_cast<uint32_t>(it_view1.first.value.second)
                << " Record count: " << it_view1.second << '\n';
    }
  }
}

}  // namespace

class MultiThreadedSubmissionFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override { thread_view_kind_stat_map.clear(); }

  void TearDown() override {
    // Called right before destructor after each test
  }
};

// Test verifies that GPU ops reported in the thread buffers where ops were submitted
// in this test all working threads did the same work - so the number of records should be the same
// in all buffers, except for the empty buffer of the main thread
TEST_F(MultiThreadedSubmissionFixtureTest, MultiThreadedSubmissionTestUsingGranularApis) {
  ASSERT_TRUE(working_thread_count > 1);
  EXPECT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);
  EXPECT_EQ(SymmetricMultithreadedWithMain(), EXIT_SUCCESS);

  ValidateViewStats();
}

// Test verifies that GPU ops reported in the thread buffers where ops were submitted
// in this test all working threads did the same work - so the number of records should be the same
// in all buffers, except for the empty buffer of the main thread
TEST_F(MultiThreadedSubmissionFixtureTest,
       MultiThreadedSubmissionTestUsingSyclOpsAllClassAllGroups) {
  ASSERT_TRUE(working_thread_count > 1);
  EXPECT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);
  EXPECT_EQ(SymmetricMultithreadedWithMain(true, true), EXIT_SUCCESS);

  ValidateViewStats();
}

// Test verifies an api call request for an unsupported class type for the driver api returns
// appropriate bad error code
TEST_F(MultiThreadedSubmissionFixtureTest, ValidateDriverApiClassReturnsAppropriateCode) {
  ASSERT_TRUE(working_thread_count > 1);
  EXPECT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);
  pti_result status = ptiViewEnableDriverApiClass(
      1, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE, pti_api_group_id::PTI_API_GROUP_ALL);
  EXPECT_EQ(status, pti_result::PTI_SUCCESS);
}
