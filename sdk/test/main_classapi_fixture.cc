//==============================================================
// Based on Vector Add example from OneAPI samples
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

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
bool buffer_size_atleast_largest_record = false;
bool event_host_synch_rec_present = false;
bool ze_kernel_rec_present = false;
bool ur_event_wait_rec_present = false;
bool ur_mem_write_rec_present = false;
bool ur_mem_read_rec_present = false;
bool ur_kernel_rec_present = false;
bool zecall_present = false;
uint64_t zecall_count = 0;
bool urcall_present = false;
uint64_t urcall_count = 0;
uint8_t device_uuid_test[PTI_MAX_DEVICE_UUID_SIZE] = {};
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
        auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
        GEMM(a_acc_ptr.get(), b_acc_ptr.get(), c_acc_ptr.get(), size, id);
      });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception& e) {
    std::cerr << "[ERROR] " << e.what() << std::endl;
  }

  return Check(c, expected_result);
}

void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
             std::vector<float>& c, unsigned size, unsigned repeat_count, float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    [[maybe_unused]] volatile float eps = RunAndCheck(queue, a, b, c, size, expected_result);
  }
}
}  // namespace

class ClassApiFixtureTest : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {
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
    zecall_present = false;
    zecall_count = 0;
    urcall_present = false;
    urcall_count = 0;
    event_host_synch_rec_present = false;
    ze_kernel_rec_present = false;
    ur_event_wait_rec_present = false;
    ur_mem_write_rec_present = false;
    ur_mem_read_rec_present = false;
    ur_kernel_rec_present = false;
    queue_test = nullptr;
  }

  void TearDown() override {
    // Called right before destructor after each test
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
          break;
        }
        case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          break;
        }
        case pti_view_kind::PTI_VIEW_DRIVER_API: {
          pti_view_record_api* rec = reinterpret_cast<pti_view_record_api*>(ptr);
          if (rec->_api_group == pti_api_group_id::PTI_API_GROUP_LEVELZERO) {
            zecall_present = true;
            zecall_count++;
          }
          const char* api_name = nullptr;
          pti_result status = ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO,
                                                  rec->_api_id, &api_name);
          PTI_ASSERT(status == PTI_SUCCESS);
          // std::cout << "ZECALL: " << api_name << "\n";
          status = ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, -1, &api_name);
          std::string function_name(api_name);
          if ((function_name.find("zeEventHostSynchronize") != std::string::npos)) {
            event_host_synch_rec_present = true;
          }
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
          if ((function_name.find("urEventWait") != std::string::npos)) {
            ur_event_wait_rec_present = true;
          }
          if ((function_name.find("urEnqueueMemBufferWrite") != std::string::npos)) {
            ur_mem_write_rec_present = true;
          }
          if ((function_name.find("urEnqueueKernelLaunch") != std::string::npos)) {
            ur_kernel_rec_present = true;
          }
          if ((function_name.find("urEnqueueMemBufferRead") != std::string::npos)) {
            ur_mem_read_rec_present = true;
          }
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          ze_kernel_rec_present = true;
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

    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size_;
    Compute(std::move(queue), a, b, c, size_, repeat_count_, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
  }

  // Class members commonly used by all tests in the test suite for ClassApiFixtureTest
  sycl::device dev_;
  int expected_mem_transfers_per_mult_ = 4;
  unsigned size_ = 1024;
  unsigned repeat_count_ = 1;
  bool buffer_cb_registered_ = false;
};

void EnableIndividualApis(bool is_for_driver, pti_api_group_id pti_group) {
  if (is_for_driver) {
    PTI_CHECK_SUCCESS(ptiViewEnableDriverApi(
        1, pti_group, pti_api_id_driver_levelzero::zeCommandListAppendLaunchKernel_id));
  } else {
    PTI_CHECK_SUCCESS(
        ptiViewEnableRuntimeApi(1, pti_group, pti_api_id_runtime_sycl::urEnqueueKernelLaunch_id));
  }
}

void DisableSyclOpsClassApis(bool useAllGroups = true) {
  if (useAllGroups) {
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApiClass(
        0, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE, pti_api_group_id::PTI_API_GROUP_ALL));
  } else {
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApiClass(
        0, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE, pti_api_group_id::PTI_API_GROUP_SYCL));
  }
}

void EnableClassApis(bool is_for_driver, pti_api_class pti_class, pti_api_group_id pti_group) {
  if (is_for_driver) {
    PTI_CHECK_SUCCESS(ptiViewEnableDriverApiClass(1, pti_class, pti_group));
  } else {
    PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApiClass(1, pti_class, pti_group));
  }
}

TEST_F(ClassApiFixtureTest, EnableRuntimeApisViaClassSpecificGroup) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  EnableClassApis(false, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                  pti_api_group_id::PTI_API_GROUP_SYCL);
  RunGemmNoTrace();
  EXPECT_EQ(urcall_present, true);
}

TEST_F(ClassApiFixtureTest, EnableThenDisableRuntimeApisViaClass) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  EnableClassApis(false, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                  pti_api_group_id::PTI_API_GROUP_SYCL);
  DisableSyclOpsClassApis();
  RunGemmNoTrace();
  EXPECT_EQ(urcall_present, false);
}

// GetParam() - Tuple values correspond to (from left to right) whether we enable the
// use_class_api, use_all_classes, use_all_groups
TEST_P(ClassApiFixtureTest, ClassApiCallsCoarseGranularity) {
  auto [use_class_api, use_all_classes, use_all_groups] = GetParam();
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  if (!use_class_api) {
    if (use_all_groups) {
      // Test individual granular case -- no class but all groups.
      EnableIndividualApis(false, pti_api_group_id::PTI_API_GROUP_ALL);
      ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
      ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
      EnableIndividualApis(true, pti_api_group_id::PTI_API_GROUP_ALL);
      RunGemmNoTrace();
      EXPECT_EQ(zecall_present, true);
      EXPECT_EQ(urcall_present, true);
    } else {
      // Test individual granular case -- no class and specific group.
      EnableIndividualApis(false, pti_api_group_id::PTI_API_GROUP_SYCL);
      ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
      ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
      EnableIndividualApis(true, pti_api_group_id::PTI_API_GROUP_LEVELZERO);
      RunGemmNoTrace();
      EXPECT_EQ(zecall_present, true);
      EXPECT_EQ(zecall_count, 1);
      EXPECT_EQ(urcall_present, true);
      EXPECT_GT(urcall_count, 2);
    }
  } else if (use_all_classes && use_all_groups) {
    // Test with use_all_classes and use_all_groups --- class level apis
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
    EnableClassApis(true, pti_api_class::PTI_API_CLASS_ALL, pti_api_group_id::PTI_API_GROUP_ALL);
    RunGemmNoTrace();
    EXPECT_EQ(zecall_present, true);
    EXPECT_EQ(urcall_present, false);
    EXPECT_GT(zecall_count, 1);
    EXPECT_EQ(urcall_count, 0);
    EXPECT_EQ(event_host_synch_rec_present, true);
    EXPECT_EQ(ze_kernel_rec_present, false);
  } else if (!use_all_classes && use_all_groups) {  // any valid class and use_all_groups
    // Test with specific class but use_all_groups --- class level apis
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
    EnableClassApis(false, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                    pti_api_group_id::PTI_API_GROUP_ALL);
    RunGemmNoTrace();
    EXPECT_EQ(urcall_present, true);
    EXPECT_GT(urcall_count, 2);
    EXPECT_EQ(ur_event_wait_rec_present, false);
    EXPECT_EQ(ur_mem_write_rec_present, true);
    EXPECT_EQ(ur_mem_read_rec_present, true);
    EXPECT_EQ(ur_kernel_rec_present, true);
    EXPECT_EQ(event_host_synch_rec_present, false);
  } else if (use_all_classes && !use_all_groups) {  // use_all_classes and any valid group
    // Test with all classes but use specific groups --- class level apis
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
    EnableClassApis(true, pti_api_class::PTI_API_CLASS_ALL,
                    pti_api_group_id::PTI_API_GROUP_LEVELZERO);
    RunGemmNoTrace();
    EXPECT_EQ(zecall_present, true);
    EXPECT_GT(zecall_count, 1);
    EXPECT_EQ(event_host_synch_rec_present, true);
    EXPECT_EQ(ze_kernel_rec_present, false);
  } else {
    // any specific class with any specific group
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
    EnableClassApis(false, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                    pti_api_group_id::PTI_API_GROUP_SYCL);
    RunGemmNoTrace();
    EXPECT_EQ(urcall_present, true);
    EXPECT_EQ(zecall_present, false);
    EXPECT_GT(urcall_count, 2);
    EXPECT_EQ(zecall_count, 0);
    EXPECT_EQ(ur_event_wait_rec_present, false);
    EXPECT_EQ(ur_mem_write_rec_present, true);
    EXPECT_EQ(ur_mem_read_rec_present, true);
    EXPECT_EQ(ur_kernel_rec_present, true);
    EXPECT_EQ(event_host_synch_rec_present, false);
  }
}

TEST_F(ClassApiFixtureTest, ValidateClassApiSyclOpsEnableSuccessful) {
  PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApiClass(1, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                                                 pti_api_group_id::PTI_API_GROUP_SYCL));
  PTI_CHECK_SUCCESS(ptiViewEnableRuntimeApiClass(1, pti_api_class::PTI_API_CLASS_ALL,
                                                 pti_api_group_id::PTI_API_GROUP_SYCL));
}

// Validate that after regeneration ApiIds have not changed.
TEST_F(ClassApiFixtureTest, ValidateApiIdsNotChanged) {
  const char* api_name = nullptr;
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 1, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 2, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 3, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 4, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 5, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 6, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 7, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urContextSetExtendedDeleter") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 8, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceGet") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 9, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 10, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 11, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 12, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDevicePartition") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 13, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceSelectBinary") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 14, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 15, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 16, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceGetGlobalTimestamps") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 17, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueKernelLaunch") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 18, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueEventsWait") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 19, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueEventsWaitWithBarrier") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 20, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferRead") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 21, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferWrite") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 22, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferReadRect") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 23, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferWriteRect") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 24, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferCopy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 25, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferCopyRect") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 26, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferFill") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 27, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemImageRead") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 28, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemImageWrite") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 29, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemImageCopy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 30, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemBufferMap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 31, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueMemUnmap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 32, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMFill") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 33, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMMemcpy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 34, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMPrefetch") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 35, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMAdvise") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 38, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueDeviceGlobalVariableWrite") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 39, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueDeviceGlobalVariableRead") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 40, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 41, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventGetProfilingInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 42, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventWait") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 43, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 44, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 45, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 46, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 47, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEventSetCallback") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 48, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 49, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetArgValue") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 50, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetArgLocal") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 51, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 52, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelGetGroupInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 53, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelGetSubGroupInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 54, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 55, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 56, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetArgPointer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 57, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetExecInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 58, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetArgSampler") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 59, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetArgMemObj") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 60, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSetSpecializationConstants") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 61, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 62, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 63, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemImageCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 64, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemBufferCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 65, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 66, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 67, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemBufferPartition") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 68, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 69, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueReadHostPipe") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 70, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 71, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemImageGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 72, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformGet") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 73, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 74, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformGetApiVersion") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 75, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 76, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 78, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramCreateWithIL") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 79, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramCreateWithBinary") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 80, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramBuild") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 81, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramCompile") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 82, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramLink") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 83, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 84, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 85, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramGetFunctionPointer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 86, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 87, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramGetBuildInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 88, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramSetSpecializationConstants") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 89, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 90, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 91, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 92, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 93, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 94, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 95, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 96, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 97, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueFinish") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 98, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urQueueFlush") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 101, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 102, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 103, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 104, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 105, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerGetNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 106, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urSamplerCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 107, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMHostAlloc") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 108, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMDeviceAlloc") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 109, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMSharedAlloc") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 110, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMFree") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 111, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMGetMemAllocInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 112, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMPoolCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 113, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 114, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPlatformGetBackendOption") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 115, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemBufferCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 116, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urMemImageCreateWithNativeHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 117, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueWriteHostPipe") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 118, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMPoolRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 119, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMPoolRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 120, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMPoolGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 121, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferRetainExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 122, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferReleaseExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 123, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferFinalizeExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 125, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendKernelLaunchExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 128, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferEnqueueExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 132, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMPitchedAllocExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 133, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesUnsampledImageHandleDestroyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 134, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesSampledImageHandleDestroyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 135, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImageAllocateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 136, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImageFreeExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 137, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesUnsampledImageCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 138, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesSampledImageCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 139, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImageCopyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 140, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImageGetInfoExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 141, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesMipmapGetLevelExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 142, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesMipmapFreeExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 144, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesMapExternalArrayExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 147, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesReleaseExternalSemaphoreExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 148, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesWaitExternalSemaphoreExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 149, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesSignalExternalSemaphoreExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 151, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMFill2D") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 152, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueUSMMemcpy2D") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 153, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemGranularityGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 154, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemReserve") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 155, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemFree") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 156, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemMap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 157, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemUnmap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 158, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemSetAccess") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 159, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urVirtualMemGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 160, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPhysicalMemCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 161, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPhysicalMemRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 162, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urPhysicalMemRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 163, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMImportExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 164, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUSMReleaseExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 165, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUsmP2PEnablePeerAccessExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 166, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUsmP2PDisablePeerAccessExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 167, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urUsmP2PPeerAccessGetInfoExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 172, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 173, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 174, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 175, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 176, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigEnableLayer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 177, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urAdapterRelease") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 178, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urAdapterGet") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 179, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urAdapterRetain") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 180, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urAdapterGetLastError") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 181, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urAdapterGetInfo") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 197, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramBuildExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 198, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramCompileExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 199, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramLinkExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 200, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigSetCodeLocationCallback") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 201, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderInit") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 202, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderTearDown") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 203, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendUSMMemcpyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 204, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendUSMFillExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 205, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferCopyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 206, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferWriteExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 207, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferReadExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 208, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferCopyRectExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 209, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferWriteRectExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 210, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferReadRectExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 211, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendMemBufferFillExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 212, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendUSMPrefetchExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 213, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferAppendUSMAdviseExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 214, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueCooperativeKernelLaunchExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 215, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelSuggestMaxCooperativeGroupCountExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 216, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urProgramGetGlobalVariablePointer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 217, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urDeviceGetSelected") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 218, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferRetainCommandExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 219, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferReleaseCommandExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 220, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferUpdateKernelLaunchExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 221, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferGetInfoExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 222, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urCommandBufferCommandGetInfoExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 223, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueTimestampRecordingExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 224, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueKernelLaunchCustomExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 225, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urKernelGetSuggestedLocalWorkSize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 226, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImportExternalMemoryExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 227, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesImportExternalSemaphoreExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 228, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urEnqueueNativeCommandExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 229, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urLoaderConfigSetMockingEnabled") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 230, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesReleaseExternalMemoryExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, 231, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "urBindlessImagesMapExternalLinearMemoryExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 1, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeInit") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 2, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGet") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 3, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetApiVersion") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 4, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 5, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetIpcProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 6, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetExtensionProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 7, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGet") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 8, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetSubDevices") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 9, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 10, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetComputeProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 11, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetModuleProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 12, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetCommandQueueGroupProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 13, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetMemoryProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 14, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetMemoryAccessProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 15, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetCacheProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 16, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetImageProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 17, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetExternalMemoryProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 18, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetP2PProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 19, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceCanAccessPeer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 20, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetStatus") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 21, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 22, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 23, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextGetStatus") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 24, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextSystemBarrier") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 25, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextMakeMemoryResident") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 26, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextEvictMemory") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 27, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextMakeImageResident") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 28, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextEvictImage") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 29, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 30, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 31, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueExecuteCommandLists") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 32, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueSynchronize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 33, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 34, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListCreateImmediate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 35, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 36, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListClose") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 37, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListReset") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 38, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendWriteGlobalTimestamp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 39, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendBarrier") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 40, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryRangesBarrier") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 41, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryCopy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 42, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryFill") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 43, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryCopyRegion") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 44, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryCopyFromContext") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 45, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 46, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopyRegion") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 47, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopyToMemory") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 48, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopyFromMemory") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 49, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemoryPrefetch") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 50, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendMemAdvise") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 51, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendSignalEvent") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 52, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendWaitOnEvents") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 53, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendEventReset") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 54, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendQueryKernelTimestamps") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 55, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendLaunchKernel") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 56, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendLaunchCooperativeKernel") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 57, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendLaunchKernelIndirect") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 58, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendLaunchMultipleKernelsIndirect") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 59, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageGetProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 60, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 61, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 62, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemAllocShared") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 63, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemAllocDevice") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 64, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemAllocHost") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 65, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemFree") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 66, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetAllocProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 67, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetAddressRange") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 68, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 69, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemOpenIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 70, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemCloseIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 71, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFenceCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 72, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFenceDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 73, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFenceHostSynchronize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 74, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFenceQueryStatus") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 75, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFenceReset") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 76, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 77, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 78, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolGetIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 79, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolOpenIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 80, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolCloseIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 81, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 82, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 83, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventHostSignal") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 84, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventHostSynchronize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 85, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventQueryStatus") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 86, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventHostReset") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 87, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventQueryKernelTimestamp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 88, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 89, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 90, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleDynamicLink") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 91, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleGetNativeBinary") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 92, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleGetGlobalPointer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 93, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleGetKernelNames") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 94, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleGetProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 95, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleGetFunctionPointer") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 96, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleBuildLogDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 97, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleBuildLogGetString") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 98, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 99, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 100, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSetCacheConfig") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 101, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSetGroupSize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 102, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSuggestGroupSize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 103, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSuggestMaxCooperativeGroupCount") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 104, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSetArgumentValue") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 105, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSetIndirectAccess") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 106, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelGetIndirectAccess") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 107, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelGetSourceAttributes") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 108, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelGetProperties") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 109, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelGetName") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 110, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeSamplerCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 111, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeSamplerDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 112, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zePhysicalMemCreate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 113, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zePhysicalMemDestroy") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 114, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemReserve") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 115, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemFree") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 116, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemQueryPageSize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 117, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemMap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 118, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemUnmap") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 119, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemSetAccessAttribute") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 120, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeVirtualMemGetAccessAttribute") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 121, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeInitDrivers") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 122, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASBuilderCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 123, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASBuilderGetBuildPropertiesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 124, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASBuilderBuildExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 125, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASBuilderDestroyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 126, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASParallelOperationCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 127, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASParallelOperationGetPropertiesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 128, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASParallelOperationJoinExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 129, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeRTASParallelOperationDestroyExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 130, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetExtensionFunctionAddress") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 131, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverGetLastErrorDescription") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 132, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDriverRTASFormatCompatibilityCheckExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 133, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetGlobalTimestamps") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 134, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceImportExternalSemaphoreExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 135, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceReleaseExternalSemaphoreExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 136, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceReserveCacheExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 137, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceSetCacheAdviceExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 138, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDevicePciGetPropertiesExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 139, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetFabricVertexExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 140, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeDeviceGetRootDevice") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 141, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeContextCreateEx") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 142, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueGetOrdinal") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 143, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandQueueGetIndex") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 144, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListGetNextCommandIdWithKernelsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 145, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListUpdateMutableCommandKernelsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 146, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendSignalExternalSemaphoreExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 147, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendWaitExternalSemaphoreExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 148, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopyToMemoryExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 149, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListAppendImageCopyFromMemoryExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 150, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListHostSynchronize") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 151, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListCreateCloneExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 152, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListGetDeviceHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 153, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListGetContextHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 154, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListGetOrdinal") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 155, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListImmediateGetIndex") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 156, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListIsImmediate") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 157, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListImmediateAppendCommandListsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 158, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListGetNextCommandIdExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 159, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListUpdateMutableCommandsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 160, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListUpdateMutableCommandSignalEventExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 161, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeCommandListUpdateMutableCommandWaitEventsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 162, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventQueryTimestampsExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 163, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventQueryKernelTimestampsExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 164, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventGetEventPool") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 165, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventGetSignalScope") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 166, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventGetWaitScope") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 167, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolPutIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 168, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolGetContextHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 169, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeEventPoolGetFlags") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 170, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageGetMemoryPropertiesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 171, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageViewCreateExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 172, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageGetAllocPropertiesExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 173, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageViewCreateExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 174, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeImageGetDeviceOffsetExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 175, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSetGlobalOffsetExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 176, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelGetBinaryExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 177, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeKernelSchedulingHintExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 178, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemFreeExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 179, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetIpcHandleFromFileDescriptorExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 180, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetFileDescriptorFromIpcHandleExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 181, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemPutIpcHandle") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 182, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemSetAtomicAccessAttributeExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 183, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetAtomicAccessAttributeExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 184, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeMemGetPitchFor2dImage") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 185, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeModuleInspectLinkageExt") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 186, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricEdgeGetExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 187, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricEdgeGetVerticesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 188, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricEdgeGetPropertiesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 189, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricVertexGetExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 190, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricVertexGetSubVerticesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 191, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricVertexGetPropertiesExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 192, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zeFabricVertexGetDeviceExp") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 193, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zelTracerSetEnabled") == 0), true);
  PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, 194, &api_name));
  EXPECT_EQ((std::strcmp(api_name, "zelTracerCreate") == 0), true);
}

// Force failure of granular api with incorrect api_ids and ensure error code returned.
TEST_F(ClassApiFixtureTest, ValidateSetApiTracingFailure) {
  pti_result status = ptiViewEnableRuntimeApi(1, pti_api_group_id::PTI_API_GROUP_SYCL,
                                              static_cast<pti_api_id_runtime_sycl>(3000));
  EXPECT_EQ(status, pti_result::PTI_ERROR_BAD_API_ID);
  status = ptiViewEnableDriverApi(1, pti_api_group_id::PTI_API_GROUP_LEVELZERO,
                                  static_cast<pti_api_id_driver_levelzero>(3000));
  EXPECT_EQ(status, pti_result::PTI_ERROR_BAD_API_ID);
  status = ptiViewEnableDriverApi(1, pti_api_group_id::PTI_API_GROUP_OPENCL,
                                  static_cast<pti_api_id_driver_levelzero>(3000));
  EXPECT_EQ(status, pti_result::PTI_ERROR_NOT_IMPLEMENTED);
}

INSTANTIATE_TEST_SUITE_P(
    ClassTests, ClassApiFixtureTest,
    ::testing::Values(std::make_tuple(true, true, true), std::make_tuple(true, true, false),
                      std::make_tuple(false, true, true), std::make_tuple(false, true, false),
                      std::make_tuple(true, false, true), std::make_tuple(true, false, false),
                      std::make_tuple(false, false, true), std::make_tuple(false, false, false)));
