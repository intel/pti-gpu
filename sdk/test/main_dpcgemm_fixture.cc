#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <sycl/sycl.hpp>
#include <tuple>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"
#include "utils/sycl_config_info.h"
#include "utils/test_helpers.h"
#include "utils/ze_utils.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

namespace {
size_t requested_buffer_calls = 0;
size_t rejected_buffer_calls = 0;  // Buffer requests that are called and rejected by the API
size_t completed_buffer_calls = 0;
size_t completed_buffer_used_bytes = 0;
uint64_t eid_ = 11;
const uint64_t kEnqueueKernelLaunchId = 17;  // apiid of urEnqueueKernelLaunch
pti_result popNullPtrResult = pti_result::PTI_SUCCESS;
bool special_sycl_rec_present = false;
bool memory_view_record_created = false;
bool kernel_view_record_created = false;
bool kernel_has_sycl_file_info = false;
bool kernel_has_sycl_enqk_info = false;
bool kernel_timestamps_monotonic = false;
bool kernel_has_task_begin0_record = false;
bool kernel_has_enqk_begin0_record = false;
bool demangled_kernel_name = false;
bool kernel_launch_func_name = false;
uint64_t kernel_launch_func_id = 0;
bool zecall_corrids_unique = true;
bool zecall_good_id_name = false;
bool zecall_bad_id_name = false;
bool zecall_present = false;
uint64_t zecall_count = 0;
std::set<uint32_t> zecall_corrids_already_seen;
bool urcall_present = false;
uint64_t urcall_count = 0;
bool sycl_has_all_records = false;
uint64_t memory_bytes_copied = 0;
uint64_t memory_view_record_count = 0;
uint64_t kernel_view_record_count = 0;
bool kernel_uuid_zero = true;
uint64_t kernel_has_sycl_file_count = 0;
uint64_t masked_by_last_id_records = 0;
uint64_t last_pop_eid = 0;
uint64_t last_id_records = 0;
double perf_time_with_tracing = 0;
double perf_time_without_tracing = 0;
double perf_time = 0;
uint64_t num_of_overhead_recs = 0;
bool overhead_kind_stringified = false;
uint64_t num_of_overhead_counts = 0;
bool buffer_size_atleast_largest_record = false;
uint64_t last_kernel_timestamp = 0;
uint64_t user_real_timestamp = 0;

uint8_t device_uuid_test[PTI_MAX_DEVICE_UUID_SIZE] = {};
pti_backend_ctx_t context_test = nullptr;
pti_backend_queue_t queue_test = nullptr;

// TODO - make the enable type param more generic (maybe a bitmap of somesort) so that we can enable
// a mishmash of types
void StartTracing(bool enable_only_zecalls = false) {
  if (!enable_only_zecalls) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_SYNCHRONIZATION), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  } else {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }
}

void StopTracing(bool enable_only_zecalls = false) {
  if (!enable_only_zecalls) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_SYNCHRONIZATION), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD), pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  } else {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }
}

float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

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

void LaunchGemm(sycl::queue queue, const std::vector<float>& a_vector,
                const std::vector<float>& b_vector, std::vector<float>& result, unsigned size) {
  ASSERT_GT(size, 0);
  ASSERT_EQ(a_vector.size(), size * size);
  ASSERT_EQ(b_vector.size(), size * size);
  ASSERT_EQ(result.size(), size * size);
  try {
    sycl::buffer<float, 1> a_buf(a_vector.data(), a_vector.size());
    sycl::buffer<float, 1> b_buf(b_vector.data(), b_vector.size());
    sycl::buffer<float, 1> c_buf(result.data(), result.size());

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
    queue.wait_and_throw();
  } catch (const sycl::exception& e) {
    FAIL() << "[ERROR] Launching GEMM Kernel" << e.what();
  }
}

void ValidateGemm(const std::vector<float>& result, float a_value, float b_value, unsigned size) {
  auto expected_result = a_value * b_value * static_cast<float>(size);
  auto eps = Check(result, expected_result);
  ASSERT_LE(eps, MAX_EPS);
}

float RunAndCheck(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                  std::vector<float>& c, unsigned size, float expected_result) {
  LaunchGemm(queue, a, b, c, size);
  return Check(c, expected_result);
}

void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
             std::vector<float>& c, unsigned size, unsigned repeat_count, float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    [[maybe_unused]] volatile float eps = RunAndCheck(queue, a, b, c, size, expected_result);
  }
}
}  // namespace

class MainFixtureTest : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {  // Called right after constructor before each test
    try {
      dev_ = sycl::device(sycl::gpu_selector_v);
      if (pti::test::utils::IsIntegratedGraphics(dev_)) {
        expected_mem_transfers_per_mult_ = 1;
      }
      auto device_l0_test = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(dev_);
      if (device_l0_test) {
        ASSERT_TRUE(utils::ze::GetDeviceUUID(device_l0_test, device_uuid_test));
        samples_utils::print_uuid(device_uuid_test, "Test Device UUID: ");
      } else {
        FAIL() << "PTI doesn't support this backend yet. Backend is not Level Zero";
      }

    } catch (const sycl::exception& e) {
      FAIL() << "Unable to select valid device to run tests on. Check your hardware, driver "
                "install, or system configuration.";
    }
    buffer_cb_registered_ = true;
    requested_buffer_calls = 0;
    rejected_buffer_calls = 0;
    completed_buffer_calls = 0;
    completed_buffer_used_bytes = 0;
    eid_ = 11;
    popNullPtrResult = pti_result::PTI_SUCCESS;
    special_sycl_rec_present = false;
    memory_view_record_created = false;
    kernel_view_record_created = false;
    kernel_has_sycl_file_info = false;
    kernel_has_sycl_enqk_info = false;
    kernel_timestamps_monotonic = false;
    kernel_has_task_begin0_record = false;
    kernel_has_enqk_begin0_record = false;
    zecall_corrids_unique = true;
    zecall_good_id_name = false;
    zecall_bad_id_name = false;
    zecall_present = false;
    zecall_count = 0;
    zecall_corrids_already_seen.clear();
    urcall_present = false;
    urcall_count = 0;
    memory_bytes_copied = 0;
    memory_view_record_count = 0;
    kernel_view_record_count = 0;
    kernel_has_sycl_file_count = 0;
    masked_by_last_id_records = 0;
    num_of_overhead_recs = 0;
    overhead_kind_stringified = false;
    num_of_overhead_counts = 0;
    last_pop_eid = 0;
    last_id_records = 0;
    perf_time_with_tracing = 0;
    perf_time_without_tracing = 0;
    perf_time = 0;
    last_kernel_timestamp = 0;
    user_real_timestamp = 0;
    sycl_has_all_records = false;
    context_test = nullptr;
    queue_test = nullptr;
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

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
              if (aExtRec->_external_id == eid_) {
                masked_by_last_id_records += 1;
              }
              if (aExtRec->_external_id == eid_ + 50) {
                last_id_records += 1;
              }
              break;
            }
            default:
              break;
          }
          // PTI_VIEW_EXTERNAL_KIND_CUSTOM_0 = 4,
          break;
        }
        case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
          pti_view_record_overhead* record = reinterpret_cast<pti_view_record_overhead*>(ptr);
          num_of_overhead_counts += record->_overhead_count;
          num_of_overhead_recs++;
          overhead_kind_stringified =
              (std::strcmp(ptiViewOverheadKindToString(record->_overhead_kind), "BUFFER_TIME") ==
               0);
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          memory_bytes_copied = reinterpret_cast<pti_view_record_memory_copy*>(ptr)->_bytes;
          memory_view_record_created = true;
          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_DRIVER_API: {
          pti_view_record_api* rec = reinterpret_cast<pti_view_record_api*>(ptr);
          uint32_t this_corrid = rec->_correlation_id;
          if (rec->_api_group == pti_api_group_id::PTI_API_GROUP_LEVELZERO) {
            zecall_present = true;
            zecall_count++;
          }
          if (zecall_corrids_unique &&
              zecall_corrids_already_seen.find(this_corrid) != zecall_corrids_already_seen.end()) {
            zecall_corrids_unique = false;

            std::cout << this_corrid << " is not unique since already seen in zecalls before. \n";
          }
          zecall_corrids_already_seen.insert(this_corrid);
          const char* api_name = nullptr;
          pti_result status = ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO,
                                                  rec->_api_id, &api_name);
          if (pti_result::PTI_SUCCESS == status) zecall_good_id_name = true;
          std::cout << "ZECALL: " << api_name << "\n";
          status = ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, -1, &api_name);
          if (status != pti_result::PTI_SUCCESS) zecall_bad_id_name = true;
          break;
        }
        case pti_view_kind::PTI_VIEW_RUNTIME_API: {
          pti_view_record_api* rec = reinterpret_cast<pti_view_record_api*>(ptr);
          urcall_present = true;
          urcall_count++;
          const char* api_name = nullptr;
          pti_result status =
              ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, rec->_api_id, &api_name);
          PTI_ASSERT(status == PTI_SUCCESS);
          std::string function_name(api_name);
          if ((function_name.find("zeCommandListAppendLaunchKernel") != std::string::npos)) {
            special_sycl_rec_present = true;
          }
          if ((function_name.find("EnqueueKernelLaunch") != std::string::npos)) {
            kernel_launch_func_name = true;
            kernel_launch_func_id = rec->_api_id;
          } else if ((function_name.find("piEventsWait") != std::string::npos) ||
                     (function_name.find("urEventWait") != std::string::npos)) {
            sycl_has_all_records = true;
          }
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          pti_view_record_kernel* rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
          std::string kernel_name = reinterpret_cast<pti_view_record_kernel*>(ptr)->_name;
          if (kernel_name.find("LaunchGemm(") != std::string::npos) {
            demangled_kernel_name = true;
          }
          std::string kernel_source_filename =
              reinterpret_cast<pti_view_record_kernel*>(ptr)->_source_file_name;
          uint64_t kernel_enqueue_ts =
              reinterpret_cast<pti_view_record_kernel*>(ptr)->_sycl_enqk_begin_timestamp;
          if (kernel_source_filename != "") {
            kernel_has_sycl_file_count += 1;
            kernel_has_sycl_file_info = true;
          }
          if (kernel_enqueue_ts > 0) {
            kernel_has_sycl_enqk_info = true;
          }
          kernel_view_record_created = true;
          kernel_view_record_count += 1;
          kernel_timestamps_monotonic = samples_utils::isMonotonic(
              {rec->_sycl_task_begin_timestamp, rec->_sycl_enqk_begin_timestamp,
               rec->_append_timestamp, rec->_submit_timestamp, rec->_start_timestamp,
               rec->_end_timestamp});
          if (rec->_sycl_task_begin_timestamp == 0) {
            kernel_has_task_begin0_record = true;
          }
          if (rec->_sycl_enqk_begin_timestamp == 0) {
            kernel_has_enqk_begin0_record = true;
          }
          last_kernel_timestamp = rec->_end_timestamp;
          ASSERT_EQ(rec->_context_handle, context_test);
          ASSERT_EQ(rec->_queue_handle, queue_test);
          std::cout << " == Queue reported by PTI: " << rec->_queue_handle << std::endl;

          if (samples_utils::stringify_uuid(rec->_device_uuid, "") !=
              "00000000-0000-0000-0000-000000000000") {
            kernel_uuid_zero = false;
            samples_utils::print_uuid(rec->_device_uuid, "Kernel Device UUID: ");
            ASSERT_EQ(std::memcmp(rec->_device_uuid, device_uuid_test, PTI_MAX_DEVICE_UUID_SIZE),
                      0);
          }
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

  void RunGemm(bool do_immediate = true) {
    StartTracing();
    RunGemmNoTrace(do_immediate);
    StopTracing();
    ptiFlushAllViews();
  }

  void RunGemmNoTrace(bool do_immediate = true) {
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

    sycl::property_list prop_list;

    if (do_immediate) {
      sycl::property_list prop{sycl::property::queue::in_order(),
                               sycl::property::queue::enable_profiling(),
                               sycl::ext::intel::property::queue::immediate_command_list()};
      prop_list = prop;
    } else {
      sycl::property_list prop{sycl::property::queue::in_order(),
                               sycl::property::queue::enable_profiling(),
                               sycl::ext::intel::property::queue::no_immediate_command_list()};
      prop_list = prop;
    }

    sycl::queue queue(dev_, sycl::async_handler{}, prop_list);

    auto sycl_context = queue.get_context();
    context_test = static_cast<pti_backend_ctx_t>(
        sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_context));

    std::cout << "DPC++ Matrix Multiplication (matrix size: " << size_ << " x " << size_
              << ", repeats " << repeat_count_ << " times)" << std::endl;
    std::cout << "Target device: "
              << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
              << std::endl;

    queue_test = samples_utils::GetLevelZeroBackendQueue(queue);
    if (queue_test == nullptr) {
      FAIL() << "Underlying level zero queue handle could not be obtained.";
    }
    std::cout << " == Native Queue reported by Sycl: " << queue_test << std::endl;

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

  // Class members commonly used by all tests in the test suite for MainFixture
  sycl::device dev_;
  int expected_mem_transfers_per_mult_ = 4;
  unsigned size_ = 1024;
  unsigned repeat_count_ = 1;
  bool buffer_cb_registered_ = false;
};

TEST_F(MainFixtureTest, ErrorCodeReturnedOnCallbacksNotSet) {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_ERROR_NO_CALLBACKS_SET);
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
  EXPECT_GT(completed_buffer_used_bytes, static_cast<std::size_t>(0));
}

TEST_F(MainFixtureTest, MemoryViewRecordCreated) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(memory_view_record_created, true);
  EXPECT_EQ(memory_bytes_copied, 4194304ULL);
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
  EXPECT_EQ(kernel_timestamps_monotonic, true);
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
  EXPECT_EQ(memory_view_record_count, expected_mem_transfers_per_mult_ * repeat_count_);
}

TEST_F(MainFixtureTest, NumberOfExpectedMemoryRecordsAfterStopTracing) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemmNoTrace();
  RunGemmNoTrace();
  StartTracing();
  RunGemmNoTrace();
  StopTracing();
  RunGemmNoTrace();
  EXPECT_EQ(memory_view_record_count, expected_mem_transfers_per_mult_ * repeat_count_);
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
  int32_t env_value = utils::IsSetEnv("PTI_VIEW_RUNTIME_API");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  if (env_value == 0) {
    EXPECT_EQ(kernel_launch_func_name, false);
    EXPECT_EQ(sycl_has_all_records, false);  // user has requested no records in buffer via env var.
  } else {
    EXPECT_EQ(kernel_launch_func_name, true);
    EXPECT_EQ(kernel_launch_func_id, kEnqueueKernelLaunchId);  // EnqueueKernelLaunch
    EXPECT_EQ(sycl_has_all_records, true);
  }
}

// Test if sycl records seen or not based on env value
TEST_F(MainFixtureTest, SyclRunTimeTraceEnv) {
  int32_t env_value = utils::IsSetEnv("PTI_VIEW_RUNTIME_API");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  if ((env_value < 0) || (env_value > 0)) {
    EXPECT_EQ(sycl_has_all_records, true);
  } else {
    EXPECT_EQ(sycl_has_all_records, false);
  }
}

TEST_F(MainFixtureTest, DeMangledKernelNameCheck) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(demangled_kernel_name, true);
}

TEST_F(MainFixtureTest, NoMaskedByLastIdExternalViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(masked_by_last_id_records, 0ULL);
}

// Tests for external_corr_id parameter properly populated on a pop.
TEST_F(MainFixtureTest, LastEidReturnedParameter) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(last_pop_eid, 51ULL);
}

// Ensures that ImmediateCommandList on or off by default does not result in
// number of kernel/memory records in buffer.
TEST_F(MainFixtureTest, ZeroDiffICLonOroff) {
  uint64_t memory_view_record_count_on = 0;
  uint64_t memory_view_record_count_off = 0;
  uint64_t kernel_view_record_count_on = 0;
  uint64_t kernel_view_record_count_off = 0;

  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm(true);
  kernel_view_record_count_on = kernel_view_record_count;
  memory_view_record_count_on = memory_view_record_count;
  kernel_view_record_count = 0;
  memory_view_record_count = 0;
  queue_test = nullptr;
  RunGemm(false);
  kernel_view_record_count_off = kernel_view_record_count;
  memory_view_record_count_off = memory_view_record_count;
  ASSERT_EQ(kernel_view_record_count_on - kernel_view_record_count_off, 0ULL);
  ASSERT_EQ(memory_view_record_count_on - memory_view_record_count_off, 0ULL);
}

// Tests for external_corr_id usage of kind stack --- use only the top/last for
// the kind.
TEST_F(MainFixtureTest, OnlyLastIdExternalViewRecords) {
  if (utils::IsSetEnv("PTI_VIEW_RUNTIME_API") != 1) GTEST_SKIP();
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_GT(last_id_records, 0ULL);
}

// Tests for overhead records present in stream.
TEST_F(MainFixtureTest, OverheadRecordsPresentViewRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_GT(num_of_overhead_recs, 0ULL);
}

// Tests for overhead records have stringified enum types in stream.
TEST_F(MainFixtureTest, OverheadRecordsKindTypeStringified) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(overhead_kind_stringified, true);
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
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_CPU_KERNEL), pti_result::PTI_ERROR_NOT_IMPLEMENTED);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
}

TEST_F(MainFixtureTest, ValidateNullPtrPopExternalId) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(popNullPtrResult, PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY);
}

TEST_F(MainFixtureTest, KerneluuidDeviceNonZero) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_EQ(kernel_uuid_zero, false);
}

// ptisdk default is real clock api_group -- this tests that ptiViewGetTimestamp returns a
// monotonically increasing ts in same api_group.
TEST_F(MainFixtureTest, ValidateRealTimestampToUser) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  ASSERT_GT(last_kernel_timestamp, 0ULL);
  user_real_timestamp = ptiViewGetTimestamp();
  ASSERT_GT(user_real_timestamp, last_kernel_timestamp);
}

// set user ts function in clockmonotonic raw api_group -- test output is in increasing
// timestamps in same api_group.
TEST_F(MainFixtureTest, ValidateRealTimestampFromUser) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewSetTimestampCallback(utils::GetTime), pti_result::PTI_SUCCESS);
  uint64_t before_run = utils::GetTime();
  RunGemm();
  ASSERT_GT(last_kernel_timestamp, 0ULL);
  uint64_t after_run = utils::GetTime();
  ASSERT_LT(before_run, last_kernel_timestamp);
  ASSERT_GT(after_run, last_kernel_timestamp);
}

// set user ts function in real clock api_group and capture last timestamp before switch
// switch to monotonic raw api_group and capture last timestamp
// -- test output is in increasing timestamps in same api_group.
TEST_F(MainFixtureTest, ValidateSwitchedTSCallbackFromUser) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewSetTimestampCallback(utils::GetRealTime),
            pti_result::PTI_SUCCESS);  // kernels will have real api_group
  RunGemm();
  ASSERT_GT(last_kernel_timestamp, 0ULL);
  user_real_timestamp = ptiViewGetTimestamp();  // Real Domain
  uint64_t after_run = utils::GetTime();        // Monotonic raw Domain
  ASSERT_LT(after_run, last_kernel_timestamp);
  ASSERT_LT(after_run, user_real_timestamp);
  ASSERT_LT(last_kernel_timestamp, user_real_timestamp);

  uint64_t before_switch_last_kernel_ts = last_kernel_timestamp;  // real clock
  ASSERT_EQ(ptiViewSetTimestampCallback(utils::GetTime),
            pti_result::PTI_SUCCESS);           // Switch -- kernels will have monotonic raw
  user_real_timestamp = ptiViewGetTimestamp();  // Monotonic raw also
  RunGemm();
  ASSERT_GT(last_kernel_timestamp, 0ULL);
  ASSERT_GT(last_kernel_timestamp, user_real_timestamp);
  after_run = utils::GetTime();

  ASSERT_GT(before_switch_last_kernel_ts,
            last_kernel_timestamp);  // real clock raw value > than monotonic raw
  ASSERT_GT(after_run,
            last_kernel_timestamp);  // in same api_group so monotonically increasing
}

TEST_F(MainFixtureTest, ValidateNullptrTSCallbackFromUser) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_NE(ptiViewSetTimestampCallback(nullptr), pti_result::PTI_SUCCESS);
  EXPECT_GT(ptiViewGetTimestamp(), 0ULL);
  RunGemm();
  EXPECT_GT(ptiViewGetTimestamp(), 0ULL);
}

TEST_F(MainFixtureTest, UniqueCorrIdsAllZeCalls) {
  bool env_off = (utils::IsSetEnv("PTI_VIEW_DRIVER_API") == 0);
  if (env_off) GTEST_SKIP();
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(zecall_corrids_unique, true);
  EXPECT_EQ(zecall_good_id_name, true);
  EXPECT_EQ(zecall_bad_id_name, true);
}

TEST_F(MainFixtureTest, OnlyZeCallsTraced) {
  bool env_off = (utils::IsSetEnv("PTI_VIEW_DRIVER_API") == 0);
  if (env_off) GTEST_SKIP();
  bool enable_only_zecalls = true;
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  StartTracing(enable_only_zecalls);
  RunGemmNoTrace();
  StopTracing(enable_only_zecalls);
  ptiFlushAllViews();
  EXPECT_EQ(zecall_good_id_name, true);
  EXPECT_EQ(zecall_bad_id_name, true);
}

// GetParam() - Tuple values correspond to (from left to right) whether we enable the viewkinds for
// --- sycl, zecalls, kernel.
TEST_P(MainFixtureTest, ZeCallsGeneration) {
  bool env_off = (utils::IsSetEnv("PTI_VIEW_DRIVER_API") == 0);
  if (env_off) GTEST_SKIP();
  auto [sycl, zecall, kernel] = GetParam();

  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);

  if (kernel) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }

  if (sycl) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  }

  if (zecall) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }

  RunGemmNoTrace();

  if (kernel) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
  if (sycl) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  }
  if (zecall) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }

  if (zecall) {
    EXPECT_EQ(zecall_present, true);
    EXPECT_EQ(special_sycl_rec_present, false);
  } else {
    // special rec requires (no sycl rec+sycl+kernel enabled+zecalls disabled) -- hence false
    // expected.
    EXPECT_EQ(special_sycl_rec_present, false);
    EXPECT_EQ(zecall_present, false);
  }
}

void EnableIndividualApis(bool is_for_driver, pti_api_group_id pti_group) {
  if (is_for_driver) {
    PTI_CHECK_SUCCESS(ptiViewEnableDriverApi(
        1, pti_group, pti_api_id_driver_levelzero::zeCommandListAppendLaunchKernel_id));
  } else {
    PTI_CHECK_SUCCESS(
        ptiViewEnableRuntimeApi(1, pti_group, pti_api_id_runtime_sycl::urEnqueueKernelLaunch_id));
  }
}

// apicalls have the following behaviour depending on env variable:
//     0/off --- no calls should be present
//     1/on  --- all calls should be present irrespective of individual level apis set.
//    unset  --- calls should be present either granularly OR fully depending on how apis used.
TEST_P(MainFixtureTest, ApiCallsGenerationDriver) {
  auto [driver_view_kind, env_off, granular_on] = GetParam();  // Note:  env_off is overridden below
  env_off = (utils::IsSetEnv("PTI_VIEW_DRIVER_API") == 0);
  bool env_on = (utils::IsSetEnv("PTI_VIEW_DRIVER_API") == 1);
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  if (driver_view_kind) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }
  if (granular_on) {
    EnableIndividualApis(true, pti_api_group_id::PTI_API_GROUP_ALL);
  }
  RunGemmNoTrace();
  if (driver_view_kind) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  }
  EXPECT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  if (driver_view_kind && !env_off) {
    // We get here if env variable for DRIVER API is on or unset  ---  L0 calls should be found.
    EXPECT_EQ(zecall_present, true);
    if (env_on) {
      std::cout << "Env set 1 case: " << zecall_count << "\n";
      // If env var is explicitly 1 -- then L0 call count should be more than 1
      //                            -- since the granular apis have no effect
      EXPECT_GE(zecall_count, 2ULL);
    } else {
      std::cout << "Env Unset case-Granular(" << granular_on << "): " << zecall_count << "\n";
      // If env is unset
      if (granular_on) {                // and granular api is individually on.
        EXPECT_EQ(zecall_count, 1ULL);  // call count should be exactly 1
      } else {
        EXPECT_GE(zecall_count, 2ULL);  // no granular hence call count should be all calls (> 1)
      }
    }
  } else {
    std::cout << "Env set 0 case: " << zecall_count << "\n";
    // We get here if env variable for DRIVER API is off --- so no l0 calls should be found.
    EXPECT_EQ(zecall_present, false);
  }
}

// GetParam() - Tuple values correspond to (from left to right) whether we enable the apicalls for
// --- runtime, set runtime env to off, set runtime granular api
// apicalls have the following behaviour depending on env variable:
//     0/off --- no calls should be present
//     1/on  --- all calls should be present irrespective of individual level apis set.
//    unset  --- calls should be present either granularly OR fully depending on how apis used.
TEST_P(MainFixtureTest, ApiCallsGenerationRuntime) {
  auto [runtime_view_kind, env_off, granular_on] = GetParam();  // Note: env_off is overridden below
  // RUNTIME_API env is triggered via the CMakeLists.txt by the test type being run -- check there.
  env_off = (utils::IsSetEnv("PTI_VIEW_RUNTIME_API") == 0);
  bool env_on = (utils::IsSetEnv("PTI_VIEW_RUNTIME_API") == 1);
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  if (runtime_view_kind) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  }
  if (granular_on) {
    EnableIndividualApis(false, pti_api_group_id::PTI_API_GROUP_ALL);
  }
  RunGemmNoTrace();
  if (runtime_view_kind) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  }
  if (runtime_view_kind && !env_off) {
    // We get here if env variable for RUNTIME API is on or unset  ---  ur calls should be found.
    EXPECT_EQ(urcall_present, true);
    if (env_on) {
      std::cout << "Env set 1 case: " << urcall_count << "\n";
      // If env var is explicitly 1 -- then ur call count should be more than 1
      //                            -- since the granular apis have no effect
      EXPECT_GE(urcall_count, 2ULL);
    } else {
      // If env is unset
      std::cout << "Env Unset case-Granular(" << granular_on << "): " << urcall_count << "\n";
      if (granular_on) {                // and granular api is individually on.
        EXPECT_EQ(urcall_count, 1ULL);  // call count should be exactly 1
      } else {
        EXPECT_GE(urcall_count, 2ULL);  // no granular hence call count should be all calls (> 1)
      }
    }
  } else {
    // We get here if env variable for RUNTIME API is off --- so no ur calls should be found.
    std::cout << "Env set 0 case: " << urcall_count << "\n";
    EXPECT_EQ(urcall_present, false);
  }
}

INSTANTIATE_TEST_SUITE_P(
    MainTests, MainFixtureTest,
    ::testing::Values(std::make_tuple(true, true, true), std::make_tuple(true, true, false),
                      std::make_tuple(false, true, true), std::make_tuple(false, true, false),
                      std::make_tuple(true, false, true), std::make_tuple(true, false, false),
                      std::make_tuple(false, false, true), std::make_tuple(false, false, false)));

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

// Can be a tad easier to read than boolean.
enum class QueueType { kImmediate, kNonImmediate };

class GemmLaunchTest
    : public ::testing::TestWithParam<std::tuple<std::size_t, unsigned int, QueueType>> {
 protected:
  static constexpr unsigned int kDefaultMatrixSize = 8;
  // This is a reasonable default. We are storing the buffers during the tests, so not super
  // important.
  static constexpr auto kRequestedBufferSize = 1'000 * sizeof(pti_view_record_kernel);

  struct TimestampRange {
    uint64_t start = 0;
    uint64_t end = 0;
  };

  struct GemmLaunchTestData {
    static GemmLaunchTestData& Instance() {
      static GemmLaunchTestData data{};
      return data;
    }

    void Reset() {
      range = std::nullopt;
      kernels = 0;
      buffers.clear();
    }

    std::optional<TimestampRange> range = std::nullopt;  // If set, only consider values in range
    std::size_t kernels = 0;
    mutable std::mutex buffers_mtx;  // buffers requested and stored in different threads.
    std::unordered_map<unsigned char*, pti::test::utils::PtiViewBuffer> buffers;
  };

  GemmLaunchTest() {
    GemmLaunchTestData::Instance().Reset();
    const auto [iterations, mat_size, queue_type] = GetParam();
    const std::size_t vec_size = mat_size * mat_size;

    a_vector_ = std::vector<float>(vec_size, A_VALUE);
    b_vector_ = std::vector<float>(vec_size, B_VALUE);
    result_vector_ = std::vector<float>(vec_size, 0.0f);
  }

  static bool WithinRange(TimestampRange range, uint64_t timestamp) {
    return timestamp >= range.start && timestamp <= range.end;
  }

  static void HandleView(pti_view_record_base* view) {
    switch (view->_view_kind) {
      case PTI_VIEW_DEVICE_GPU_KERNEL: {
        const auto* const kernel = reinterpret_cast<const pti_view_record_kernel*>(view);
        auto range = GemmLaunchTestData::Instance().range;
        if (range.has_value()) {
          auto within_range = WithinRange(*range, kernel->_start_timestamp);
          EXPECT_TRUE(within_range) << "Range Start: " << range->start
                                    << " Kernel Start Timestamp: " << kernel->_start_timestamp
                                    << " Range End: " << range->end << '\n';
          if (within_range) {
            ++GemmLaunchTestData::Instance().kernels;
          }
        }
        break;
      }
      default: {
        FAIL() << "View found but not handled: " << view->_view_kind;
        break;
      }
    }
  }

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    auto buffer = pti::test::utils::PtiViewBuffer(kRequestedBufferSize);
    if (!buffer.Valid()) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf = buffer.data();
    *buf_size = buffer.size();

    const std::lock_guard<std::mutex> lock(GemmLaunchTestData::Instance().buffers_mtx);
    GemmLaunchTestData::Instance().buffers[*buf] = std::move(buffer);
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

  static void MarkBuffer(unsigned char* buf, size_t /*buf_size*/, size_t used_bytes) {
    const std::lock_guard<std::mutex> lock(GemmLaunchTestData::Instance().buffers_mtx);
    if (auto buffer = GemmLaunchTestData::Instance().buffers.find(buf);
        buffer != GemmLaunchTestData::Instance().buffers.end()) {
      buffer->second.SetUsedBytes(used_bytes);
    }
  }

  void SetUp() override {
    try {
      dev_ = sycl::device(sycl::gpu_selector_v);

      const auto [iterations, mat_size, queue_type] = GetParam();

      sycl::property_list prop_list;

      if (queue_type == QueueType::kImmediate) {
        sycl::property_list prop{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::immediate_command_list()};
        prop_list = prop;
      } else {
        sycl::property_list prop{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::no_immediate_command_list()};
        prop_list = prop;
      }

      queue_ = sycl::queue(dev_, prop_list);

    } catch (const std::exception& e) {
      FAIL() << "Unable to select valid device to run tests on. Check your hardware, driver "
                "install, or system configuration.";
    }
  }

  unsigned int mat_size_ = kDefaultMatrixSize;
  std::vector<float> a_vector_;
  std::vector<float> b_vector_;
  std::vector<float> result_vector_;
  sycl::device dev_;
  sycl::queue queue_;
};

TEST_P(GemmLaunchTest, CheckWhetherAllLaunchedKernelDeviceTimestampsFitWithinAGivenTimeRange) {
  const auto [iterations, mat_size, queue_type] = GetParam();
  TimestampRange range;
  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  range.start = ptiViewGetTimestamp();
  for (std::size_t run_idx = 0; run_idx < iterations; ++run_idx) {
    LaunchGemm(queue_, a_vector_, b_vector_, result_vector_, mat_size);
    ValidateGemm(result_vector_, A_VALUE, B_VALUE, mat_size);
  }
  range.end = ptiViewGetTimestamp();
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  GemmLaunchTestData::Instance().range = range;
  ASSERT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  for (auto& [buf, buffer] : GemmLaunchTestData::Instance().buffers) {
    ParseBuffer(buffer.data(), buffer.UsedBytes());
  }

  ASSERT_EQ(GemmLaunchTestData::Instance().kernels, iterations);
}

INSTANTIATE_TEST_SUITE_P(GranularGemmLaunchTest, GemmLaunchTest,
                         testing::Values(std::make_tuple(100, 8, QueueType::kImmediate),
                                         std::make_tuple(100, 8, QueueType::kNonImmediate)));
