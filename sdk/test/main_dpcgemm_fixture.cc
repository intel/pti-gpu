#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <sycl/sycl.hpp>
#include <tuple>

#include "pti_view.h"
#include "utils.h"
#include "ze_utils.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

namespace {
size_t requested_buffer_calls = 0;
size_t number_of_subdevices = 0;
size_t rejected_buffer_calls = 0;  // Buffer requests that are called and rejected by the API
size_t completed_buffer_calls = 0;
size_t completed_buffer_used_bytes = 0;
uint64_t eid_ = 11;
pti_result popNullPtrResult = pti_result::PTI_SUCCESS;
bool memory_view_record_created = false;
bool kernel_view_record_created = false;
bool kernel_has_sycl_file_info = false;
bool kernel_has_sycl_enqk_info = false;
bool kernel_has_nonmonotonic_record = false;
bool kernel_has_task_begin0_record = false;
bool kernel_has_enqk_begin0_record = false;
bool demangled_kernel_name = false;
bool kernel_launch_func_name = false;
uint64_t memory_view_record_count = 0;
uint64_t kernel_view_record_count = 0;
uint64_t kernel_has_sycl_file_count = 0;
uint64_t masked_by_last_id_records = 0;
uint64_t last_pop_eid = 0;
uint64_t last_id_records = 0;
double perf_time_with_tracing = 0;
double perf_time_without_tracing = 0;
double perf_time = 0;
uint64_t num_of_overhead_recs = 0;
uint64_t num_of_overhead_counts = 0;
bool buffer_size_atleast_largest_record = false;

void StartTracing() {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD), pti_result::PTI_SUCCESS);
}

void StopTracing() {
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS);
  ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION);
  ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD);
}

float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
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

float RunAndCheck(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                  std::vector<float>& c, unsigned size, float expected_result) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    sycl::event event = queue.submit([&](sycl::handler& cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
      auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
      auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

      cgh.parallel_for<class __GEMM>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
        GEMM(a_acc.get_pointer(), b_acc.get_pointer(), c_acc.get_pointer(), size, id);
      });
    });
    queue.wait_and_throw();
  } catch (sycl::exception e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
  }

  return Check(c, expected_result);
}

void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
             std::vector<float>& c, unsigned size, unsigned repeat_count, float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    [[maybe_unused]] volatile float eps = RunAndCheck(queue, a, b, c, size, expected_result);
  }
}

struct GTestData {
  bool buffer_cb_registered = false;
};
}  // namespace

class MainFixtureTest : public ::testing::Test {
 protected:
  MainFixtureTest() {
    // Setup work for each test
  }

  ~MainFixtureTest() override {
    // Cleanup work for each test
  }

  void SetUp() override {  // Called right after constructor before each test
    buffer_cb_registered_ = true;
    number_of_subdevices = 0;
    requested_buffer_calls = 0;
    rejected_buffer_calls = 0;
    completed_buffer_calls = 0;
    completed_buffer_used_bytes = 0;
    eid_ = 11;
    popNullPtrResult = pti_result::PTI_SUCCESS;
    memory_view_record_created = false;
    kernel_view_record_created = false;
    kernel_has_sycl_file_info = false;
    kernel_has_sycl_enqk_info = false;
    kernel_has_nonmonotonic_record = false;
    kernel_has_task_begin0_record = false;
    kernel_has_enqk_begin0_record = false;
    memory_view_record_count = 0;
    kernel_view_record_count = 0;
    kernel_has_sycl_file_count = 0;
    masked_by_last_id_records = 0;
    num_of_overhead_recs = 0;
    num_of_overhead_counts = 0;
    last_pop_eid = 0;
    last_id_records = 0;
    perf_time_with_tracing = 0;
    perf_time_without_tracing = 0;
    perf_time = 0;
  };

  void TearDown() override {
    // Called right before destructor after each test
  }

  // Class members commonly used by all tests in the test suite for MainFixture
  unsigned size_ = 1024;

  unsigned repeat_count_ = 1;

  bool buffer_cb_registered_ = false;

  static void InadequateBufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel) - 1;
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    rejected_buffer_calls += 1;
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_kernel);
  }

  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel);
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_kernel);
  }

  static void NullBufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_memory_copy) - sizeof(pti_view_record_memory_copy);
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    rejected_buffer_calls += 1;
    *buf = static_cast<unsigned char*>(ptr);
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_memory_copy);
  }

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }

    completed_buffer_calls += 1;
    completed_buffer_used_bytes = used_bytes;
    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
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
          pti_view_record_external_correlation* aExtRec =
              reinterpret_cast<pti_view_record_external_correlation*>(ptr);
          switch (aExtRec->_external_kind) {
            case pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3: {
              if (aExtRec->_external_id == eid_) masked_by_last_id_records += 1;
              if (aExtRec->_external_id == eid_ + 50) last_id_records += 1;
              break;
            }
            default:
              break;
          };
          // PTI_VIEW_EXTERNAL_KIND_CUSTOM_0 = 4,
          break;
        }
        case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
          pti_view_record_overhead* record = reinterpret_cast<pti_view_record_overhead*>(ptr);
          num_of_overhead_counts += record->_overhead_count;
          num_of_overhead_recs++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
          std::string function_name = reinterpret_cast<pti_view_record_sycl_runtime*>(ptr)->_name;
          if (function_name.find("piEnqueueKernelLaunch") != std::string::npos)
            kernel_launch_func_name = true;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          std::string kernel_name = reinterpret_cast<pti_view_record_kernel*>(ptr)->_name;
          if (kernel_name.find("RunAndCheck(") != std::string::npos) demangled_kernel_name = true;
          std::string kernel_source_filename =
              reinterpret_cast<pti_view_record_kernel*>(ptr)->_source_file_name;
          uint64_t kernel_enqueue_ts =
              reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_enqk_begin_timestamp;
          if (kernel_source_filename != "") {
            kernel_has_sycl_file_count += 1;
            kernel_has_sycl_file_info = true;
          };
          if (kernel_enqueue_ts > 0) kernel_has_sycl_enqk_info = true;
          kernel_view_record_created = true;
          kernel_view_record_count += 1;
          ze_device_handle_t dev_handle =
              (reinterpret_cast<pti_view_record_kernel*>(ptr))->_device_handle;
          std::vector<ze_device_handle_t> sub_device_list = utils::ze::GetSubDeviceList(dev_handle);
          number_of_subdevices = sub_device_list.size();
          if (not(reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_task_begin_timestamp <
                  reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_enqk_begin_timestamp <
                  reinterpret_cast<pti_view_record_kernel*>(ptr)->_append_timestamp <
                  reinterpret_cast<pti_view_record_kernel*>(ptr)->_submit_timestamp <
                  reinterpret_cast<pti_view_record_kernel*>(ptr)->_start_timestamp <
                  reinterpret_cast<pti_view_record_kernel*>(ptr)->_end_timestamp))
            kernel_has_nonmonotonic_record = true;
          if (reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_task_begin_timestamp == 0)
            kernel_has_task_begin0_record = true;
          if (reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_enqk_begin_timestamp == 0)
            kernel_has_enqk_begin0_record = true;
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

  void RunGemm() {
    StartTracing();
    RunGemmNoTrace();
    StopTracing();
    ptiFlushAllViews();
  }

  void RunGemmNoTrace() {
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid_);
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0,
                                     eid_ + 10);
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_1,
                                     eid_ + 20);
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3,
                                     eid_ + 50);
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0,
                                     eid_ + 30);
    ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_2,
                                     eid_ + 40);

    auto dev = sycl::device(sycl::gpu_selector_v);
    sycl::property_list prop_list{sycl::property::queue::enable_profiling()};
    sycl::queue queue(dev, sycl::async_handler{}, prop_list);

    std::cout << "DPC++ Matrix Multiplication (matrix size: " << size_ << " x " << size_
              << ", repeats " << repeat_count_ << " times)" << std::endl;
    std::cout << "Target device: "
              << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
              << std::endl;

    std::vector<float> a(size_ * size_, A_VALUE);
    std::vector<float> b(size_ * size_, B_VALUE);
    std::vector<float> c(size_ * size_, 0.0f);

    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid_);
    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &eid_);
    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_1, &eid_);
    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid_);
    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &eid_);
    ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_2, &eid_);
    popNullPtrResult = ptiViewPopExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_2, nullptr);
    last_pop_eid = eid_;
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size_;
    Compute(std::move(queue), a, b, c, size_, repeat_count_, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    perf_time = time.count();
    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
  }
};

TEST_F(MainFixtureTest, ErrorCodeReturnedOnCallbacksNotSet) {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_LEVEL_ZERO_CALLS), pti_result::PTI_ERROR_NO_CALLBACKS_SET);
}

TEST_F(MainFixtureTest, BufferFirstArgumentNull) {
  EXPECT_EQ(ptiViewSetCallbacks(nullptr, BufferCompleted), pti_result::PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(MainFixtureTest, BufferSecondArgumentNull) {
  EXPECT_EQ(ptiViewSetCallbacks(InadequateBufferRequested, nullptr),
            pti_result::PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(MainFixtureTest, NegTestBufferSizeAtleastLargestRecord) {
  EXPECT_EQ(ptiViewSetCallbacks(InadequateBufferRequested, BufferCompleted),
            pti_result::PTI_ERROR_BAD_ARGUMENT);
  RunGemm();
  ASSERT_EQ(rejected_buffer_calls, 1 * repeat_count_);
}

TEST_F(MainFixtureTest, BufferSizeAtleastLargestRecord) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(buffer_size_atleast_largest_record, true);
}

TEST_F(MainFixtureTest, BufferCallBacksRegistered) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(buffer_cb_registered_, true);
}

TEST_F(MainFixtureTest, SecondCallbackCalled) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_GT(completed_buffer_used_bytes, 0);
}

TEST_F(MainFixtureTest, DeviceHandleValid) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_GT(number_of_subdevices, 0);
}

TEST_F(MainFixtureTest, MemoryViewRecordCreated) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(memory_view_record_created, true);
}

TEST_F(MainFixtureTest, KernelViewRecordCreated) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_view_record_created, true);
}

TEST_F(MainFixtureTest, KernelViewRecordHasSyclFileName) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_has_sycl_file_info, true);
}

TEST_F(MainFixtureTest, KernelViewRecordHasSyclPiEnqLaunchKernel) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_has_sycl_enqk_info, true);
}

TEST_F(MainFixtureTest, KernelViewRecordHasMonotonicRecords) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_has_nonmonotonic_record, false);
}

TEST_F(MainFixtureTest, KernelViewRecordHasNonZeroTaskBeginRecords) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_has_task_begin0_record, false);
}

TEST_F(MainFixtureTest, KernelViewRecordHasNonZeroEnqkBeginRecords) {
  ptiViewSetCallbacks(BufferRequested, BufferCompleted);
  RunGemm();
  EXPECT_EQ(kernel_has_enqk_begin0_record, false);
}

TEST_F(MainFixtureTest, NumberOfExpectedMemoryRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(memory_view_record_count, 4 * repeat_count_);
}

TEST_F(MainFixtureTest, NumberOfExpectedMemoryRecordsAfterStopTracing) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemmNoTrace();
  RunGemmNoTrace();
  StartTracing();
  RunGemmNoTrace();
  StopTracing();
  RunGemmNoTrace();
  EXPECT_EQ(memory_view_record_count, 4 * repeat_count_);
}

TEST_F(MainFixtureTest, NumberOfExpectedKernelRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(kernel_view_record_count, 1 * repeat_count_);
}

TEST_F(MainFixtureTest, NumberOfExpectedKernelRecordsAfterStopTracing) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemmNoTrace();
  StartTracing();
  RunGemmNoTrace();
  StopTracing();
  RunGemmNoTrace();
  RunGemmNoTrace();
  EXPECT_EQ(kernel_view_record_count, 1 * repeat_count_);
}

TEST_F(MainFixtureTest, RequestedAndCompletedBuffers) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(requested_buffer_calls, completed_buffer_calls);
}

TEST_F(MainFixtureTest, SyclRunTimeFunctionCheck) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(kernel_launch_func_name, true);
}

TEST_F(MainFixtureTest, DeMangledKernelNameCheck) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(demangled_kernel_name, true);
}

TEST_F(MainFixtureTest, NoMaskedByLastIdExternalViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(masked_by_last_id_records, 0);
}

// Tests for external_corr_id parameter properly populated on a pop.
TEST_F(MainFixtureTest, LastEidReturnedParameter) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(last_pop_eid, 51);
}

// Ensures that ImmediateCommandList on or off by default does not result in
// number of kernel/memory records in buffer.
TEST_F(MainFixtureTest, ZeroDiffICLonOroff) {
  uint64_t memory_view_record_count_on = 0;
  uint64_t memory_view_record_count_off = 0;
  uint64_t kernel_view_record_count_on = 0;
  uint64_t kernel_view_record_count_off = 0;

  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", "1");
  RunGemm();
  kernel_view_record_count_on = kernel_view_record_count;
  memory_view_record_count_on = memory_view_record_count;
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", "0");
  kernel_view_record_count = 0;
  memory_view_record_count = 0;
  RunGemm();
  kernel_view_record_count_off = kernel_view_record_count;
  memory_view_record_count_off = memory_view_record_count;
  ASSERT_EQ(kernel_view_record_count_on - kernel_view_record_count_off, 0);
  ASSERT_EQ(memory_view_record_count_on - memory_view_record_count_off, 0);
}

// Tests for external_corr_id usage of kind stack --- use only the top/last for
// the kind.
TEST_F(MainFixtureTest, OnlyLastIdExternalViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_GT(last_id_records, 0);
}

// Tests for overhead records present in stream.
TEST_F(MainFixtureTest, OverheadRecordsPresentViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_GT(num_of_overhead_recs, 0);
}

// Tests for overhead records present in stream.
TEST_F(MainFixtureTest, OverheadRecordsCountsAllOnesViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(num_of_overhead_counts, num_of_overhead_recs);
}

// TEST_F(MainFixtureTest, PerfDeltaForTracingUnder3For1100Repeats) {
//   EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted),
//             pti_result::PTI_SUCCESS);
//   repeat_count_ = 1100;
//   RunGemm();
//   perf_time_with_tracing = perf_time;
//   RunGemmNoTrace();
//   perf_time_without_tracing = perf_time;
//   repeat_count_ = 1;
//   EXPECT_LT((perf_time_with_tracing / perf_time_without_tracing) * 100 - 100,
//             3);
// }

TEST_F(MainFixtureTest, NegTestNullBufferSize) {
  EXPECT_EQ(ptiViewSetCallbacks(NullBufferRequested, BufferCompleted),
            pti_result::PTI_ERROR_BAD_ARGUMENT);
  RunGemm();
  ASSERT_EQ(rejected_buffer_calls, 1 * repeat_count_);
}

TEST_F(MainFixtureTest, ValidateNotImplementedViewReturn) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_LEVEL_ZERO_CALLS), pti_result::PTI_ERROR_NOT_IMPLEMENTED);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_OPENCL_CALLS), pti_result::PTI_ERROR_NOT_IMPLEMENTED);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_CPU_KERNEL), pti_result::PTI_ERROR_NOT_IMPLEMENTED);
}

TEST_F(MainFixtureTest, ValidateNullPtrPopExternalId) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(popNullPtrResult, PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY);
}

namespace {
constexpr std::size_t kTestParam1 = 0;
constexpr std::size_t kTestParam2 = 1;

constexpr std::size_t kNumOverflowInts = 11;

template <typename T>
constexpr auto OverflowIntValues() {
  return std::array<T, kNumOverflowInts>{static_cast<T>(-1), 0,          0x100,      0x3fffffff,
                                         0x7ffffffe,         0x7fffffff, 0x80000000, 0xfffffffe,
                                         0xffffffff,         0x10000,    0x100000};
}

constexpr std::array<pti_view_external_kind, 1> kExternalKinds = {
    pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3};

constexpr auto kExternalIds = OverflowIntValues<uint64_t>();
}  // namespace

class ExternalCorrelationOverFlowSuite
    : public testing::TestWithParam<std::tuple<pti_view_external_kind, uint64_t>> {};

TEST_P(ExternalCorrelationOverFlowSuite, ValidatePushExternalOverflowValues) {
  const auto result = ptiViewPushExternalCorrelationId(std::get<kTestParam1>(GetParam()),
                                                       std::get<kTestParam2>(GetParam()));
  ASSERT_EQ(result, PTI_SUCCESS);
}

TEST_P(ExternalCorrelationOverFlowSuite, ValidatePopExternalOverflowValues) {
  auto result_id = static_cast<uint64_t>(-1);
  auto result = ptiViewPushExternalCorrelationId(std::get<kTestParam1>(GetParam()),
                                                 std::get<kTestParam2>(GetParam()));
  result = ptiViewPopExternalCorrelationId(std::get<kTestParam1>(GetParam()), &result_id);
  ASSERT_EQ(result, PTI_SUCCESS);
  ASSERT_EQ(result_id, std::get<kTestParam2>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(OverflowGroup, ExternalCorrelationOverFlowSuite,
                         testing::Combine(testing::ValuesIn(kExternalKinds),
                                          testing::ValuesIn(kExternalIds)));
