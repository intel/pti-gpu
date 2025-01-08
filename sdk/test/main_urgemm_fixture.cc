//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>
#include <math.h>
#include <string.h>

// Unified Runtime header(s) can be found in either directory.
#if __has_include(<sycl/ur_api.h>)
#include <sycl/ur_api.h>
#else
#include <ur_api.h>
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sycl/kernel_bundle.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"
#include "utils/sycl_config_info.h"
#include "utils/test_helpers.h"

#define ALIGN 64
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f
#define UR_CHECK_SUCCESS(X)                                                                   \
  do {                                                                                        \
    if (X != ur_result_t::UR_RESULT_SUCCESS) {                                                \
      std::cerr << "UR CALL FAILED: " #X << " WITH ERROR " << std::to_string(X) << std::endl; \
      std::exit(EXIT_FAILURE);                                                                \
    }                                                                                         \
  } while (0)

namespace syclex = sycl::ext::oneapi::experimental;

namespace {
bool memory_view_record_created = false;
bool kernel_view_record_created = false;
bool sycl_runtime_record_created = false;
bool sycl_spv_special_rec_seen = false;
bool sycl_spv_kernel_seen = false;
bool sycl_spv_mem_buffer_fill_seen = false;
bool sycl_spv_mem_buffer_read_seen = false;
bool sycl_spv_mem_buffer_write_seen = false;
bool sycl_spv_mem_buffer_copy_seen = false;
uint64_t memory_view_record_count = 0;
bool memory_view_record_with_zero_uuid = false;
uint64_t kernel_view_record_count = 0;
uint64_t sycl_runtime_record_count = 0;

size_t requested_buffer_calls = 0;
size_t rejected_buffer_calls = 0;  // Buffer requests that are called and rejected by the API
size_t completed_buffer_calls = 0;
size_t completed_buffer_used_bytes = 0;
bool buffer_size_atleast_largest_record = false;
bool capture_records = false;
std::vector<pti_view_record_memory_copy> copy_records;
std::vector<pti_view_record_kernel> kernel_records;

float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

float RunAndCheckSycl(sycl::kernel& k, sycl::queue& q, std::vector<float>& a, std::vector<float>& b,
                      std::vector<float>& c, unsigned size, float expected_result) {
  PTI_ASSERT(size > 0);
  sycl::buffer<float, 1> a_buf(a.data(), a.size());
  sycl::buffer<float, 1> b_buf(b.data(), b.size());
  sycl::buffer<float, 1> c_buf(c.data(), c.size());

  sycl::event event = q.submit([&](sycl::handler& cgh) {
    auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
    auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
    auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

    cgh.set_args(a_acc, b_acc, c_acc, size);
    cgh.parallel_for(sycl::range<2>(size, size), k);
  });
  q.wait_and_throw();

  // Test mem_buffer_copy
  sycl::event event_cp = q.submit([&](sycl::handler& cgh) {
    auto a_acc = a_buf.get_access<sycl::access::mode::read_write>(cgh);
    auto b_acc = b_buf.get_access<sycl::access::mode::read_write>(cgh);
    cgh.copy(a_acc, b_acc);
  });
  q.wait_and_throw();

  return Check(c, expected_result);
}

float RunAndCheck(ur_kernel_handle_t kernel, ur_device_handle_t device, ur_context_handle_t context,
                  std::vector<float>& a, std::vector<float>& b, std::vector<float>& c,
                  unsigned size, float expected_result) {
  PTI_ASSERT(kernel != nullptr);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(context != nullptr);

  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  ur_mem_handle_t dA, dB, dC;
  UR_CHECK_SUCCESS(urMemBufferCreate(context, UR_MEM_FLAG_READ_WRITE, size * size * sizeof(float),
                                     nullptr, &dA));
  UR_CHECK_SUCCESS(urMemBufferCreate(context, UR_MEM_FLAG_READ_WRITE, size * size * sizeof(float),
                                     nullptr, &dB));
  UR_CHECK_SUCCESS(urMemBufferCreate(context, UR_MEM_FLAG_READ_WRITE, size * size * sizeof(float),
                                     nullptr, &dC));

  UR_CHECK_SUCCESS(urKernelSetArgMemObj(kernel, 0, nullptr, dA));
  UR_CHECK_SUCCESS(urKernelSetArgMemObj(kernel, 1, nullptr, dB));
  UR_CHECK_SUCCESS(urKernelSetArgMemObj(kernel, 2, nullptr, dC));
  UR_CHECK_SUCCESS(urKernelSetArgValue(kernel, 3, sizeof(size), nullptr, &size));

  ur_queue_handle_t queue;
  UR_CHECK_SUCCESS(urQueueCreate(context, device, nullptr, &queue));

  float zero = 0;
  UR_CHECK_SUCCESS(urEnqueueMemBufferFill(queue, dC, &zero, sizeof(zero), 0,
                                          size * size * sizeof(float), 0, nullptr, nullptr));
  UR_CHECK_SUCCESS(urEnqueueMemBufferWrite(queue, dA, true, 0, size * size * sizeof(float),
                                           a.data(), 0, nullptr, nullptr));
  UR_CHECK_SUCCESS(urEnqueueMemBufferWrite(queue, dB, true, 0, size * size * sizeof(float),
                                           b.data(), 0, nullptr, nullptr));

  const size_t gWorkOffset[] = {0, 0, 0};
  const size_t gWorkSize[] = {size, size, size};
  const size_t lWorkSize[] = {1, 1, 1};

  ur_event_handle_t event;
  UR_CHECK_SUCCESS(urEnqueueKernelLaunch(queue, kernel, 2, gWorkOffset, gWorkSize, lWorkSize, 0,
                                         nullptr, &event));

  UR_CHECK_SUCCESS(urEnqueueMemBufferRead(queue, dC, true, 0, size * size * sizeof(float), c.data(),
                                          1, &event, nullptr));

  UR_CHECK_SUCCESS(urQueueFinish(queue));

  return Check(c, expected_result);
}

ur_result_t GetL0Adapter(std::vector<ur_adapter_handle_t>& adapters, unsigned int& idx) {
  unsigned int index = 0;
  for (auto adapter : adapters) {
    ur_adapter_backend_t backend;
    UR_CHECK_SUCCESS(urAdapterGetInfo(adapter, UR_ADAPTER_INFO_BACKEND,
                                      sizeof(ur_adapter_backend_t), &backend, nullptr));

    if (backend == UR_ADAPTER_BACKEND_LEVEL_ZERO) {
      idx = index;
      return UR_RESULT_SUCCESS;
    }
    index++;
  }
  return UR_RESULT_ERROR_ADAPTER_SPECIFIC;
}

void ComputeUsingUr(std::vector<float>& a, std::vector<float>& b, std::vector<float>& c,
                    unsigned size, unsigned repeat_count, float expected_result) {
  PTI_ASSERT(size > 0 && repeat_count > 0);

  ur_loader_config_handle_t loader_config = nullptr;
  urLoaderInit(UR_DEVICE_INIT_FLAG_GPU, loader_config);

  uint32_t count = 0;
  uint32_t dcount = 0;

  UR_CHECK_SUCCESS(urAdapterGet(0, nullptr, &count));
  std::vector<ur_adapter_handle_t> adapters(count);
  UR_CHECK_SUCCESS(urAdapterGet(count, adapters.data(), nullptr));

  unsigned int idx;
  UR_CHECK_SUCCESS(GetL0Adapter(adapters, idx));

  std::vector<ur_platform_handle_t> platforms(count);
  UR_CHECK_SUCCESS(urPlatformGet(&adapters[idx], 1, 1, platforms.data(), nullptr));

  UR_CHECK_SUCCESS(urDeviceGet(platforms[0], UR_DEVICE_TYPE_GPU, 0, nullptr, &dcount));
  std::vector<ur_device_handle_t> devices(dcount);
  UR_CHECK_SUCCESS(
      urDeviceGet(platforms.front(), UR_DEVICE_TYPE_GPU, dcount, devices.data(), nullptr));

  ur_context_handle_t hContext;
  UR_CHECK_SUCCESS(urContextCreate(1, &devices[0], nullptr, &hContext));

  std::string module_name = "gemm.spv";
  std::vector<uint8_t> spv = utils::LoadBinaryFile(utils::GetExecutablePath() + module_name);
  if (spv.size() == 0) {
    std::cout << "Unable to find module " << module_name << std::endl;
    return;
  }

  ur_program_handle_t hProgram;
  UR_CHECK_SUCCESS(urProgramCreateWithIL(hContext, spv.data(), spv.size(), nullptr, &hProgram));
  UR_CHECK_SUCCESS(urProgramBuild(hContext, hProgram, nullptr));

  ur_kernel_handle_t hKernel;
  UR_CHECK_SUCCESS(urKernelCreate(hProgram, "GEMM", &hKernel));

  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(hKernel, devices[0], hContext, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") << "CORRECT with accuracy: " << eps
              << std::endl;
  }
}

}  // namespace

class MainUrFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {  // Called right after constructor before each test
    try {
      dev_ = sycl::device(sycl::gpu_selector_v);
      if (pti::test::utils::IsIntegratedGraphics(dev_)) {
        is_integrated_graphics = true;
      }
    } catch (const sycl::exception& e) {
      FAIL() << "Unable to select valid device to run tests on. Check your hardware, driver "
                "install, or system configuration.";
    }
    buffer_cb_registered = true;
    requested_buffer_calls = 0;
    rejected_buffer_calls = 0;
    completed_buffer_calls = 0;
    completed_buffer_used_bytes = 0;
    memory_view_record_created = false;
    kernel_view_record_created = false;
    sycl_runtime_record_created = false;
    sycl_spv_special_rec_seen = false;
    sycl_spv_kernel_seen = false;
    sycl_spv_mem_buffer_fill_seen = false;
    sycl_spv_mem_buffer_read_seen = false;
    sycl_spv_mem_buffer_write_seen = false;
    sycl_spv_mem_buffer_copy_seen = false;
    memory_view_record_count = 0;
    memory_view_record_with_zero_uuid = false;
    kernel_view_record_count = 0;
    sycl_runtime_record_count = 0;
    capture_records = false;
    copy_records.clear();
    kernel_records.clear();
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

  // Class members commonly used by all tests in the test suite for MainFixture
  unsigned size = 1024;

  unsigned repeat_count = 1;

  bool buffer_cb_registered = false;

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }

    completed_buffer_calls += 1;
    completed_buffer_used_bytes = used_bytes;
    uint8_t zero_uuid[PTI_MAX_DEVICE_UUID_SIZE];
    memset(zero_uuid, 0, PTI_MAX_DEVICE_UUID_SIZE);
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
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          pti_view_record_memory_copy* rec = reinterpret_cast<pti_view_record_memory_copy*>(ptr);
          std::cout << " --- Found Memory Copy Record" << '\n';
          samples_utils::dump_record(rec);

          if (memcmp(rec->_device_uuid, zero_uuid, PTI_MAX_DEVICE_UUID_SIZE) == 0) {
            EXPECT_TRUE(false) << "Device UUID is zero, which is not expected";
            memory_view_record_with_zero_uuid = true;
          }
          if (capture_records) {
            copy_records.push_back(*rec);
          }
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          memory_view_record_created = true;
          std::cout << " --- Found Memory Fill Record" << '\n';
          pti_view_record_memory_fill* rec = reinterpret_cast<pti_view_record_memory_fill*>(ptr);
          samples_utils::dump_record(rec);

          if (memcmp(rec->_device_uuid, zero_uuid, PTI_MAX_DEVICE_UUID_SIZE) == 0) {
            EXPECT_TRUE(false) << "Device UUID is zero, which is not expected";
            memory_view_record_with_zero_uuid = true;
          }

          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
          sycl_runtime_record_created = true;
          sycl_runtime_record_count += 1;
          if (capture_records) {
            pti_view_record_sycl_runtime* rec =
                reinterpret_cast<pti_view_record_sycl_runtime*>(ptr);
            std::string function_name = rec->_name;
            std::cout << "Runtime_recs: " << function_name << "\n";
            if ((function_name.find("EnqueueKernelLaunch") != std::string::npos)) {
              sycl_spv_kernel_seen = true;
            } else if ((function_name.find("EnqueueMemBufferFill") != std::string::npos)) {
              sycl_spv_mem_buffer_fill_seen = true;
            } else if ((function_name.find("EnqueueMemBufferRead") != std::string::npos)) {
              sycl_spv_mem_buffer_read_seen = true;
            } else if ((function_name.find("EnqueueMemBufferWrite") != std::string::npos)) {
              sycl_spv_mem_buffer_write_seen = true;
            } else if ((function_name.find("EnqueueMemBufferCopy") != std::string::npos)) {
              sycl_spv_mem_buffer_copy_seen = true;
            } else if ((function_name.find("zeCommandListAppendLaunchKernel") !=
                        std::string::npos)) {
              sycl_spv_special_rec_seen = true;
            }
            break;
          }
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          kernel_view_record_created = true;
          kernel_view_record_count += 1;
          if (capture_records) {
            pti_view_record_kernel* rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
            kernel_records.push_back(*rec);
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

  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel);
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_memory_copy);
  }

  void ComputeUsingSycl(std::vector<float>& a, std::vector<float>& b, std::vector<float>& c,
                        unsigned size, unsigned repeat_count, float expected_result) {
    sycl::queue q;
    bool l0_backend_found = false;
    for (auto platform : sycl::platform::get_platforms()) {
      std::vector<sycl::device> gpu_devices = platform.get_devices();
      if (platform.get_backend() == sycl::backend::ext_oneapi_level_zero) {
        q = sycl::queue(gpu_devices[0]);
        l0_backend_found = true;
      }
    }

    PTI_ASSERT(l0_backend_found);
    std::string module_name = "gemm.spv";
    std::cout << utils::GetExecutablePath() + module_name << std::endl;
    std::ifstream spv_stream(utils::GetExecutablePath() + module_name, std::ios::binary);
    spv_stream.seekg(0, std::ios::end);
    size_t sz = spv_stream.tellg();
    spv_stream.seekg(0);
    std::vector<std::byte> spv(sz);
    spv_stream.read((char*)spv.data(), sz);

    // Create a kernel bundle from the binary SPIR-V.
    sycl::kernel_bundle<sycl::bundle_state::ext_oneapi_source> kb_src =
        syclex::create_kernel_bundle_from_source(q.get_context(), syclex::source_language::spirv,
                                                 spv);

    // Build the SPIR-V module for our device.
    sycl::kernel_bundle<sycl::bundle_state::executable> kb_exe = syclex::build(kb_src);

    // Get a "kernel" object representing the kernel from the SPIR-V module.
    sycl::kernel k = kb_exe.ext_oneapi_get_kernel("GEMM");

    for (unsigned i = 0; i < repeat_count; ++i) {
      float eps = RunAndCheckSycl(k, q, a, b, c, size, expected_result);
      std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN")
                << "CORRECT with accuracy: " << eps << std::endl;
    }
  }

  int RunGemm(bool use_ur = false) {
    // sycl::property_list prop_list{sycl::property::queue::enable_profiling()};
    // sycl::queue queue;

    PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS));
    PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
    PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
    PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));

    std::cout << "Level Zero Matrix Multiplication (matrix size: " << size << " x " << size
              << ", repeats " << repeat_count << " times)" << std::endl;

    std::vector<float> a(size * size, A_VALUE);
    std::vector<float> b(size * size, B_VALUE);
    std::vector<float> c(size * size, 0.0f);

    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    if (use_ur) {
      ComputeUsingUr(a, b, c, size, repeat_count, expected_result);
    } else {
      ComputeUsingSycl(a, b, c, size, repeat_count, expected_result);
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS));
    PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
    PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
    PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));

    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
    auto flush_results = ptiFlushAllViews();
    return flush_results;
  }
  bool is_integrated_graphics = false;
  sycl::device dev_;
};

TEST_F(MainUrFixtureTest, urGemmSpvKernelDetected) {
  capture_records = true;
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm(true);
  EXPECT_EQ(sycl_spv_kernel_seen, true);
  EXPECT_EQ(sycl_spv_special_rec_seen, false);
  EXPECT_EQ(sycl_spv_mem_buffer_fill_seen, true);
  EXPECT_EQ(memory_view_record_with_zero_uuid, false);
}

// TODO -- add tests for USMFill2D and USMMemcpy2D
TEST_F(MainUrFixtureTest, syclGemmSpvRuntimeRecordsDetected) {
  capture_records = true;
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunGemm();
  EXPECT_EQ(sycl_spv_kernel_seen, true);
  EXPECT_EQ(sycl_spv_special_rec_seen, false);
  EXPECT_EQ(sycl_spv_mem_buffer_read_seen, true);
  EXPECT_EQ(sycl_spv_mem_buffer_copy_seen, true);
  EXPECT_EQ(memory_view_record_with_zero_uuid, false);
  if (!is_integrated_graphics) EXPECT_EQ(sycl_spv_mem_buffer_write_seen, true);
}
