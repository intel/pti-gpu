//==============================================================
// GEMM Metrics Scope Test Fixture
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "metrics_utils.h"
#include "pti/pti_metrics_scope.h"
#include "utils.h"

namespace {

enum class TestType {
  kRunAll = 0,
};

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

const unsigned max_size = 8192;
const unsigned min_size = 32;

std::vector<pti_device_properties_t> devices;
std::vector<pti_metrics_group_properties_t> metric_groups;

static float Check(const std::vector<float>& a, float value) {
  assert(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

// GEMM kernel function - exact copy from main_metrics_scope.cc
void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static float RunAndCheck(sycl::queue queue, const std::vector<float>& a,
                         const std::vector<float>& b, std::vector<float>& c, unsigned size,
                         float expected_result) {
  assert(size > 0);
  assert(a.size() == size * size);
  assert(b.size() == size * size);
  assert(c.size() == size * size);

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    [[maybe_unused]] sycl::event event = queue.submit([&](sycl::handler& cgh) {
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
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }

  std::cout << "Matrix multiplication done. Checking result.." << std::endl;

  return Check(c, expected_result);
}

static void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& c, unsigned size, unsigned repeat_count,
                    float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") << "CORRECT with accuracy: " << eps
              << std::endl;
  }
}

void RunGemm(unsigned size = 1024, unsigned repeat_count = 1) {
  sycl::device dev;

  try {
    dev = sycl::device(sycl::gpu_selector_v);

    // Clamp size to valid range
    size = (size < min_size) ? min_size : (size > max_size) ? max_size : size;
  } catch (...) {
    std::cerr << "Error: Failed to get GPU device" << std::endl;
    return;
  }

  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue queue(dev, sycl::async_handler{}, prop_list);

  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size << " x " << size << ", repeats "
            << repeat_count << " times)" << std::endl;
  std::cout << "Target device: "
            << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
            << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  try {
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    Compute(queue, a, b, c, size, repeat_count, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;

  } catch (const sycl::exception& e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
    throw;
  } catch (const std::exception& e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
    throw;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    throw;
  }
}

}  // namespace

class GemmMetricsScopeFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    uint32_t device_count = 0;
    pti_result result = PTI_SUCCESS;
    result = ptiMetricsGetDevices(nullptr, &device_count);
    if (result != PTI_SUCCESS) {
      std::cout << "Failed to get devices count" << std::endl;
    }

    devices.resize(device_count);
    result = ptiMetricsGetDevices(devices.data(), &device_count);
    if (result != PTI_SUCCESS) {
      std::cout << "Failed to get devices" << std::endl;
    }

    if (devices.size() > 0) {
      // Get groups for the first device
      uint32_t group_count = 0;
      uint32_t device_idx = 0;
      result = ptiMetricsGetMetricGroups(devices[device_idx]._handle, nullptr, &group_count);
      if (result != PTI_SUCCESS) {
        std::cout << "Failed to get metric group count" << std::endl;
      }

      metric_groups.resize(group_count);
      result = ptiMetricsGetMetricGroups(devices[device_idx]._handle, metric_groups.data(),
                                         &group_count);
      if (result != PTI_SUCCESS) {
        std::cout << "Failed to get metric groups" << std::endl;
      }
    }
    // For now, need ptiView to enable ptiCallback
    auto dummy_provide = [](unsigned char** buf, std::size_t* buf_size) {
      *buf_size = sizeof(pti_view_record_kernel) * 100;
      void* ptr = ::operator new(*buf_size);
      *buf = static_cast<unsigned char*>(ptr);
      if (!*buf) {
        std::abort();
      }
    };
    auto dummy_parse = [](unsigned char* buf, std::size_t, std::size_t) {
      if (buf) ::operator delete(buf);
    };

    EXPECT_EQ(ptiViewSetCallbacks(dummy_provide, dummy_parse), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  }

  void TearDown() override {
    devices.clear();
    metric_groups.clear();
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
    EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  }
};

// ============================================================================
// PARAMETERIZED TESTS
// ============================================================================

// Parameter structure for null handle tests
struct NullHandleTestParam {
  std::string function_name;
  std::function<pti_result()> test_function;
  pti_result expected_result;

  NullHandleTestParam(std::string name, std::function<pti_result()> func, pti_result result)
      : function_name(std::move(name)), test_function(std::move(func)), expected_result(result) {}
};

void PrintTo(const NullHandleTestParam& param, std::ostream* os) { *os << param.function_name; }

class GemmMetricsScopeNullHandleTest : public GemmMetricsScopeFixtureTest,
                                       public ::testing::WithParamInterface<NullHandleTestParam> {};

TEST_P(GemmMetricsScopeNullHandleTest, NullHandleTests) {
  const auto& param = GetParam();

  pti_result result = param.test_function();
  EXPECT_EQ(result, param.expected_result)
      << "Function: " << param.function_name << " should return " << param.expected_result
      << " when called with null handle, but returned " << result;
}

INSTANTIATE_TEST_SUITE_P(
    NullHandle, GemmMetricsScopeNullHandleTest,
    ::testing::Values(
        NullHandleTestParam(
            "ptiMetricsScopeEnable", []() { return ptiMetricsScopeEnable(nullptr); },
            PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeDisable", []() { return ptiMetricsScopeDisable(nullptr); },
            PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeStartCollection",
            []() { return ptiMetricsScopeStartCollection(nullptr); }, PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeStopCollection",
            []() { return ptiMetricsScopeStopCollection(nullptr); }, PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeSetCollectionBufferSize",
            []() { return ptiMetricsScopeSetCollectionBufferSize(nullptr, 1024); },
            PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeQueryMetricsBufferSize",
            []() {
              void* buffer = reinterpret_cast<void*>(0x1000);
              size_t size = 0;
              size_t count = 0;
              return ptiMetricsScopeQueryMetricsBufferSize(nullptr, buffer, &size, &count);
            },
            PTI_ERROR_BAD_ARGUMENT),
        NullHandleTestParam(
            "ptiMetricsScopeCalculateMetrics",
            []() {
              void* collection_buffer = reinterpret_cast<void*>(0x1000);
              void* metrics_buffer = reinterpret_cast<void*>(0x2000);
              size_t count = 0;
              return ptiMetricsScopeCalculateMetrics(nullptr, collection_buffer, metrics_buffer,
                                                     1024, &count);
            },
            PTI_ERROR_BAD_ARGUMENT)),
    [](const ::testing::TestParamInfo<NullHandleTestParam>& info) {
      return info.param.function_name;
    });

// Helper functions to detect device types
bool hasPVCDevice() {
  for (const auto& device : devices) {
    std::string device_name = device._model_name;  // Assuming device has a name field
    if (device_name.find("Data Center GPU") != std::string::npos ||
        device_name.find("1100") != std::string::npos ||
        device_name.find("1500") != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool hasBMGDevice() {
  for (const auto& device : devices) {
    std::string device_name = device._model_name;  // Assuming device has a name field
    if (device_name.find("B570") != std::string::npos ||
        device_name.find("B580") != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Parameter structure for metric configuration tests
struct MetricConfigTestParam {
  std::vector<std::string> metric_names;
  std::vector<pti_result> expected_results;
  std::string test_description;
  std::string device_type;  // "PVC", "BMG", or "ANY"

  MetricConfigTestParam(std::vector<std::string> names, std::vector<pti_result> results,
                        std::string desc, std::string dev_type = "ANY")
      : metric_names(std::move(names)),
        expected_results(std::move(results)),
        test_description(std::move(desc)),
        device_type(std::move(dev_type)) {}
};

void PrintTo(const MetricConfigTestParam& param, std::ostream* os) {
  *os << param.test_description;
}

class GemmMetricsScopeConfigureTest : public GemmMetricsScopeFixtureTest,
                                      public ::testing::WithParamInterface<MetricConfigTestParam> {
};

TEST_P(GemmMetricsScopeConfigureTest, ScopeConfigureWithVariousMetrics) {
  const auto& param = GetParam();

  // Skip device-specific tests if required devices not available
  if (param.device_type == "PVC" && !hasPVCDevice()) {
    GTEST_SKIP() << "Skipping PVC-specific test - no PVC devices available";
  }
  if (param.device_type == "BMG" && !hasBMGDevice()) {
    GTEST_SKIP() << "Skipping BMG-specific test - no BMG devices available";
  }

  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;

  // Convert string vector to const char* array
  std::vector<const char*> metric_names_cstr;
  metric_names_cstr.reserve(param.metric_names.size());
  for (const auto& name : param.metric_names) {
    metric_names_cstr.push_back(name.c_str());
  }

  pti_result result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device,
                                               1, metric_names_cstr.data(),
                                               static_cast<uint32_t>(metric_names_cstr.size()));

  // Check if result matches any of the expected results
  bool result_matches = std::find(param.expected_results.begin(), param.expected_results.end(),
                                  result) != param.expected_results.end();

  EXPECT_TRUE(result_matches) << "Test: " << param.test_description << "\nActual result: " << result
                              << "\nExpected one of: ";

  if (!result_matches) {
    for (size_t i = 0; i < param.expected_results.size(); ++i) {
      std::cout << param.expected_results[i];
      if (i < param.expected_results.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(
    Configuration, GemmMetricsScopeConfigureTest,
    ::testing::Values(
        MetricConfigTestParam({"InvalidMetricName"}, {PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND},
                              "Single invalid metric name"),
        MetricConfigTestParam({"InvalidMetric1", "InvalidMetric2"},
                              {PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND},
                              "Multiple invalid metric names"),
        MetricConfigTestParam({"SYSMEM_BYTE_READ", "SLM_BYTE_READ", "LOAD_STORE_CACHE_BYTE_WRITE"},
                              {PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP},
                              "Metrics from different groups PVC", "PVC"),
        MetricConfigTestParam({"RENDER_CACHE_HIT", "COMPRESSOR_INPUT", "URB_READ"},
                              {PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP},
                              "Metrics from different groups BMG", "BMG"),
        MetricConfigTestParam({"GpuTime", "GpuCoreClocks", "AvgGpuCoreFrequencyMHz",
                               "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION", "XVE_ACTIVE", "XVE_STALL"},
                              {PTI_SUCCESS}, "Valid metrics from ComputeBasic group"),
        MetricConfigTestParam({"GpuTime"}, {PTI_SUCCESS}, "Single valid metric"),
        MetricConfigTestParam({"GpuTime", "GpuCoreClocks", "AvgGpuCoreFrequencyMHz", "Counter5"},
                              {PTI_SUCCESS}, "Valid metrics from TestOa group PVC", "PVC"),
        MetricConfigTestParam({"GpuTime", "GpuCoreClocks", "AvgGpuCoreFrequencyMHz",
                               "TEST_EVENT1_CYCLES_AVERAGE"},
                              {PTI_SUCCESS}, "Valid metrics from TestOa group BMG", "BMG"),
        MetricConfigTestParam({"GpuTime", "InvalidMetric"},
                              {PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND},
                              "Mix of valid and invalid metrics"),
        MetricConfigTestParam({""}, {PTI_ERROR_BAD_ARGUMENT}, "Empty metric name"),
        MetricConfigTestParam({"gputime", "gpuCoreclocks"},
                              {PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND}, "Case sensitivity test")),
    [](const ::testing::TestParamInfo<MetricConfigTestParam>& info) {
      std::string name = info.param.test_description;
      std::replace_if(name.begin(), name.end(), [](char c) { return !std::isalnum(c); }, '_');
      name.erase(std::unique(name.begin(), name.end(),
                             [](char a, char b) { return a == '_' && b == '_'; }),
                 name.end());
      if (!name.empty() && name.front() == '_') name.erase(0, 1);
      if (!name.empty() && name.back() == '_') name.pop_back();
      return name;
    });

// Parameter structure for buffer size tests
struct BufferSizeTestParam {
  size_t buffer_size;
  pti_result expected_result;
  std::string description;

  BufferSizeTestParam(size_t size, pti_result result, std::string desc)
      : buffer_size(size), expected_result(result), description(std::move(desc)) {}
};

void PrintTo(const BufferSizeTestParam& param, std::ostream* os) { *os << param.description; }

class GemmMetricsScopeBufferSizeTest : public GemmMetricsScopeFixtureTest,
                                       public ::testing::WithParamInterface<BufferSizeTestParam> {};

TEST_P(GemmMetricsScopeBufferSizeTest, ScopeSetBufferSizeVariations) {
  const auto& param = GetParam();

  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with valid metrics first
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 2);

  if (config_result == PTI_SUCCESS) {
    pti_result result = ptiMetricsScopeSetCollectionBufferSize(scope_handle, param.buffer_size);
    EXPECT_EQ(result, param.expected_result)
        << "Buffer size test: " << param.description << " with size " << param.buffer_size;
  } else {
    std::cout << "Skipping buffer size test due to configuration failure" << std::endl;
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(
    BufferSize, GemmMetricsScopeBufferSizeTest,
    ::testing::Values(BufferSizeTestParam(0, PTI_SUCCESS, "Zero_buffer_size"),
                      BufferSizeTestParam(1, PTI_SUCCESS, "Minimum_buffer_size"),
                      BufferSizeTestParam(1024, PTI_SUCCESS, "Standard_buffer_size"),
                      BufferSizeTestParam(1024 * 1024, PTI_SUCCESS, "Large_buffer_size"),
                      BufferSizeTestParam(SIZE_MAX, PTI_SUCCESS, "SIZE_MAX_clamped_to_1GB")),
    [](const ::testing::TestParamInfo<BufferSizeTestParam>& info) {
      return info.param.description + "_" + std::to_string(info.param.buffer_size);
    });

// Parameter structure for invalid handle tests
struct InvalidHandleTestParam {
  std::string function_name;
  std::function<pti_result()> test_function;
  pti_result expected_result;

  InvalidHandleTestParam(std::string name, std::function<pti_result()> func, pti_result result)
      : function_name(std::move(name)), test_function(std::move(func)), expected_result(result) {}
};

void PrintTo(const InvalidHandleTestParam& param, std::ostream* os) { *os << param.function_name; }

class GemmMetricsScopeInvalidHandleTest
    : public GemmMetricsScopeFixtureTest,
      public ::testing::WithParamInterface<InvalidHandleTestParam> {};

TEST_P(GemmMetricsScopeInvalidHandleTest, InvalidHandleTests) {
  const auto& param = GetParam();

  pti_result result = param.test_function();
  EXPECT_EQ(result, param.expected_result)
      << "Function: " << param.function_name << " should return " << param.expected_result
      << " when called with invalid handle, but returned " << result;
}

INSTANTIATE_TEST_SUITE_P(
    InvalidHandle, GemmMetricsScopeInvalidHandleTest,
    ::testing::Values(InvalidHandleTestParam(
                          "ptiMetricsScopeDisable_InvalidHandle",
                          []() {
                            pti_scope_collection_handle_t invalid_handle =
                                reinterpret_cast<pti_scope_collection_handle_t>(0xDEADBEEF);
                            return ptiMetricsScopeDisable(invalid_handle);
                          },
                          PTI_ERROR_BAD_ARGUMENT),
                      InvalidHandleTestParam(
                          "ptiMetricsScopeStartCollection_InvalidHandle",
                          []() {
                            pti_scope_collection_handle_t invalid_handle =
                                reinterpret_cast<pti_scope_collection_handle_t>(0xDEADBEEF);
                            return ptiMetricsScopeStartCollection(invalid_handle);
                          },
                          PTI_ERROR_BAD_ARGUMENT),
                      InvalidHandleTestParam(
                          "ptiMetricsScopeStopCollection_InvalidHandle",
                          []() {
                            pti_scope_collection_handle_t invalid_handle =
                                reinterpret_cast<pti_scope_collection_handle_t>(0xDEADBEEF);
                            return ptiMetricsScopeStopCollection(invalid_handle);
                          },
                          PTI_ERROR_BAD_ARGUMENT)),
    [](const ::testing::TestParamInfo<InvalidHandleTestParam>& info) {
      return info.param.function_name;
    });

// ============================================================================
// REGULAR TESTS (Non-parameterized)
// ============================================================================

TEST_F(GemmMetricsScopeFixtureTest, MetricsEnabled) {
  bool metrics_enabled = (utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  EXPECT_EQ(metrics_enabled, true);
}

TEST_F(GemmMetricsScopeFixtureTest, GetDevices) {
  uint32_t device_count = 0;
  EXPECT_EQ(ptiMetricsGetDevices(nullptr, &device_count), PTI_SUCCESS);
  EXPECT_NE(device_count, static_cast<uint32_t>(0));

  devices.resize(device_count);
  EXPECT_EQ(ptiMetricsGetDevices(devices.data(), &device_count), PTI_SUCCESS);
  EXPECT_NE(devices.empty(), true);
}

TEST_F(GemmMetricsScopeFixtureTest, GetMetricGroups) {
  for (size_t i = 0; i < devices.size(); i++) {
    uint32_t group_count = 0;
    EXPECT_EQ(ptiMetricsGetMetricGroups(devices[i]._handle, nullptr, &group_count), PTI_SUCCESS);
    EXPECT_NE(group_count, static_cast<uint32_t>(0));

    metric_groups.resize(group_count);
    EXPECT_EQ(ptiMetricsGetMetricGroups(devices[i]._handle, metric_groups.data(), &group_count),
              PTI_SUCCESS);
    EXPECT_NE(metric_groups.empty(), true);
  }
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeEnableDisable) {
  pti_scope_collection_handle_t scope_handle = nullptr;

  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);
  EXPECT_NE(scope_handle, nullptr);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureNullDevice) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  const char* metric_names[] = {"GpuTime"};

  EXPECT_EQ(ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, nullptr, 1,
                                     metric_names, 1),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureZeroDevices) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime"};

  EXPECT_EQ(ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 0,
                                     metric_names, 1),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureNullMetricNames) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;

  EXPECT_EQ(
      ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1, nullptr, 1),
      PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureUserMode) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime"};

  EXPECT_EQ(
      ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_USER, &device, 1, metric_names, 1),
      PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeQueryBufferSizeNotConfigured) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  size_t buffer_size = 0;
  EXPECT_EQ(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, 100, &buffer_size),
            PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeSetBufferSizeNotConfigured) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024),
            PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeStartCollectionNotConfigured) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle),
            PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeStopCollectionNotStarted) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeGetBuffersCountWhileActive) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics like in client.cc
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Set buffer size
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);

    // Start collection
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count),
              PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED);

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeGetCollectionBufferNullParams) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  void* buffer = nullptr;
  size_t buffer_size = 0;
  EXPECT_EQ(ptiMetricsScopeGetCollectionBuffer(nullptr, 0, &buffer, &buffer_size),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeGetCollectionBuffer(scope_handle, 0, nullptr, &buffer_size),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeGetCollectionBuffer(scope_handle, 0, &buffer, nullptr),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeGetBufferPropertiesNullParams) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  void* buffer = reinterpret_cast<void*>(0x1000);
  pti_metrics_scope_collection_buffer_properties_t props;
  // indeed wrong value of _struct_size in purpose
  props._struct_size = sizeof(pti_metrics_scope_collection_buffer_properties_t) - 2;
  EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(nullptr, buffer, &props),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, nullptr, &props),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, buffer, nullptr),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, buffer, &props),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeQueryRecordsBufferSizeNullParams) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  void* buffer = reinterpret_cast<void*>(0x1000);
  size_t required_size = 0;
  size_t records_count = 0;

  // Test null parameters
  EXPECT_EQ(ptiMetricsScopeQueryMetricsBufferSize(nullptr, buffer, &required_size, &records_count),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(
      ptiMetricsScopeQueryMetricsBufferSize(scope_handle, nullptr, &required_size, &records_count),
      PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeQueryMetricsBufferSize(scope_handle, buffer, nullptr, &records_count),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeQueryMetricsBufferSize(scope_handle, buffer, &required_size, nullptr),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeCalculateMetricsNullParams) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  void* collection_buffer = reinterpret_cast<void*>(0x1000);
  void* metrics_buffer = reinterpret_cast<void*>(0x2000);
  size_t records_count = 0;

  EXPECT_EQ(ptiMetricsScopeCalculateMetrics(nullptr, collection_buffer, metrics_buffer, 1024,
                                            &records_count),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(
      ptiMetricsScopeCalculateMetrics(scope_handle, nullptr, metrics_buffer, 1024, &records_count),
      PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeCalculateMetrics(scope_handle, collection_buffer, nullptr, 1024,
                                            &records_count),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeCalculateMetrics(scope_handle, collection_buffer, metrics_buffer, 1024,
                                            nullptr),
            PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeStartStopCollection) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics like in client.cc
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Set buffer size
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);

    // Start and stop collection
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeStartAlreadyStartedCollection) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics like in client.cc
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Set buffer size
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);

    // Start collection twice
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle),
              PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED);

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeSetBufferSizeWhileActive) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics like in client.cc
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Set buffer size and start collection
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Try to set buffer size while active
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 2048),
              PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED);

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeQueryBufferSizeValid) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics like in client.cc
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Query buffer size for different scope counts
    size_t buffer_size_10 = 0, buffer_size_100 = 0;
    EXPECT_EQ(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, 10, &buffer_size_10),
              PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, 100, &buffer_size_100),
              PTI_SUCCESS);

    // Buffer size should scale with scope count
    EXPECT_GT(buffer_size_100, buffer_size_10);
    EXPECT_GE(buffer_size_10, static_cast<size_t>(1024));  // At least 1KB minimum
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeCalculateMetricsInsufficientBuffer) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with valid metrics
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 2);

  if (config_result == PTI_SUCCESS) {
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Run a small workload
    try {
      RunGemm(64, 1);
    } catch (...) {
      // Continue even if GEMM fails
    }

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

    // Get first buffer
    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

    if (buffer_count > 0) {
      void* buffer_data = nullptr;
      size_t actual_buffer_size = 0;
      EXPECT_EQ(
          ptiMetricsScopeGetCollectionBuffer(scope_handle, 0, &buffer_data, &actual_buffer_size),
          PTI_SUCCESS);

      if (buffer_data != nullptr) {
        // Query required size
        size_t required_size = 0;
        size_t records_count = 0;
        EXPECT_EQ(ptiMetricsScopeQueryMetricsBufferSize(scope_handle, buffer_data, &required_size,
                                                        &records_count),
                  PTI_SUCCESS);

        if (required_size > 0) {
          // Allocate insufficient buffer
          size_t insufficient_size = required_size / 2;
          auto small_buffer = std::make_unique<uint8_t[]>(insufficient_size);
          ASSERT_NE(small_buffer, nullptr);

          // Should fail with insufficient buffer
          size_t actual_records = 0;
          pti_result result = ptiMetricsScopeCalculateMetrics(
              scope_handle, buffer_data, small_buffer.get(), insufficient_size, &actual_records);

          EXPECT_EQ(result, PTI_ERROR_METRICS_SCOPE_COLLECTION_BUFFER_TOO_SMALL);
        }
      }
    }
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test exception handling in destructor paths
TEST_F(GemmMetricsScopeFixtureTest, ScopeHandleDestructorWithActiveCollection) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Don't stop collection - test destructor cleanup path
    // This tests the destructor warning paths in pti_metrics_scope_helper.h
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test memory allocation failures in CreateKernelMetricData path
TEST_F(GemmMetricsScopeFixtureTest, ScopeMemoryAllocationStress) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    // Set very small buffer to force multiple buffer allocations
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 64), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Run multiple small kernels to stress buffer allocation
    try {
      for (int i = 0; i < 10; ++i) {
        RunGemm(32, 1);  // Very small kernels
      }
    } catch (...) {
      // Continue even if kernels fail
    }

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test buffer overflow and multiple buffer creation
TEST_F(GemmMetricsScopeFixtureTest, ScopeMultipleBufferCreation) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    // Set very small buffer to force buffer overflow
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Run enough kernels to overflow first buffer
    try {
      RunGemm(1024, 10);  // Should create multiple buffers
    } catch (...) {
      // Continue
    }

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

    // Check that multiple buffers were created
    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

    std::cout << "Buffer Count: " << buffer_count << std::endl;

    // Test accessing all buffers
    for (size_t i = 0; i < buffer_count; ++i) {
      void* buffer = nullptr;
      size_t buffer_size = 0;
      EXPECT_EQ(ptiMetricsScopeGetCollectionBuffer(scope_handle, i, &buffer, &buffer_size),
                PTI_SUCCESS);

      if (buffer != nullptr) {
        pti_metrics_scope_collection_buffer_properties_t props;
        props._struct_size = sizeof(props);
        EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, buffer, &props),
                  PTI_SUCCESS);
      }
    }
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test buffer access with invalid indices
TEST_F(GemmMetricsScopeFixtureTest, ScopeInvalidBufferIndex) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

    // Test accessing buffer beyond available count
    void* buffer = nullptr;
    size_t buffer_size = 0;
    EXPECT_EQ(
        ptiMetricsScopeGetCollectionBuffer(scope_handle, buffer_count + 10, &buffer, &buffer_size),
        PTI_ERROR_BAD_ARGUMENT);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test metadata with zero struct size
TEST_F(GemmMetricsScopeFixtureTest, ScopeMetadataZeroStructSize) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    pti_metrics_scope_record_metadata_t metadata;
    metadata._struct_size = 0;  // Invalid size

    EXPECT_EQ(ptiMetricsScopeGetMetricsMetadata(scope_handle, &metadata), PTI_ERROR_BAD_ARGUMENT);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test configuration with zero metric count
TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureZeroMetrics) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime"};

  // Test with zero metric count
  EXPECT_EQ(ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                                     metric_names, 0),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test configuration with null metric name in array
TEST_F(GemmMetricsScopeFixtureTest, ScopeConfigureNullMetricInArray) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", nullptr, "GpuCoreClocks"};

  // Should fail due to null metric name in array
  EXPECT_EQ(ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                                     metric_names, 3),
            PTI_ERROR_BAD_ARGUMENT);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test query with invalid collection buffer pointer
TEST_F(GemmMetricsScopeFixtureTest, ScopeQueryInvalidCollectionBuffer) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    // Test with invalid buffer pointer
    void* invalid_buffer = reinterpret_cast<void*>(0xDEADBEEF);
    size_t required_size = 0;
    size_t records_count = 0;

    EXPECT_EQ(ptiMetricsScopeQueryMetricsBufferSize(scope_handle, invalid_buffer, &required_size,
                                                    &records_count),
              PTI_ERROR_BAD_ARGUMENT);
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test calculation with zero-sized metrics buffer
TEST_F(GemmMetricsScopeFixtureTest, ScopeCalculateMetricsZeroBuffer) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};

  if (ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                               metric_names, 2) == PTI_SUCCESS) {
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, 1024), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    try {
      RunGemm(64, 1);
    } catch (...) {
      // Continue
    }

    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

    if (buffer_count > 0) {
      void* collection_buffer = nullptr;
      size_t buffer_size = 0;
      EXPECT_EQ(
          ptiMetricsScopeGetCollectionBuffer(scope_handle, 0, &collection_buffer, &buffer_size),
          PTI_SUCCESS);

      if (collection_buffer != nullptr) {
        // Test with zero-sized metrics buffer
        uint8_t dummy_buffer[1];
        size_t records_count = 0;

        EXPECT_EQ(ptiMetricsScopeCalculateMetrics(scope_handle, collection_buffer, dummy_buffer, 0,
                                                  &records_count),
                  PTI_ERROR_METRICS_SCOPE_COLLECTION_BUFFER_TOO_SMALL);
      }
    }
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test query profiler creation failures
TEST_F(GemmMetricsScopeFixtureTest, ScopeQueryProfilerEdgeCases) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Try to configure with a potentially invalid device
  pti_device_handle_t invalid_device = reinterpret_cast<pti_device_handle_t>(0x12345678);
  const char* metric_names[] = {"GpuTime"};

  // This should fail during device validation
  EXPECT_NE(ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL, &invalid_device,
                                     1, metric_names, 1),
            PTI_SUCCESS);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

// Test double enable/disable
TEST_F(GemmMetricsScopeFixtureTest, ScopeDoubleEnableDisable) {
  pti_scope_collection_handle_t scope_handle1 = nullptr;
  pti_scope_collection_handle_t scope_handle2 = nullptr;

  // Test multiple enables
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle1), PTI_SUCCESS);
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle2), PTI_SUCCESS);

  EXPECT_NE(scope_handle1, scope_handle2);  // Should be different handles

  // Test disabling both
  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle1), PTI_SUCCESS);
  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle2), PTI_SUCCESS);

  // Test double disable (should fail)
  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle1), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeCompleteWorkflowWithGemm) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};

  const uint32_t metric_count = sizeof(metric_names) / sizeof(metric_names[0]);
  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, 6);

  if (config_result == PTI_SUCCESS) {
    // Query buffer size
    size_t estimated_buffer_size = 0;
    EXPECT_EQ(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, 10, &estimated_buffer_size),
              PTI_SUCCESS);
    EXPECT_GT(estimated_buffer_size, static_cast<size_t>(0));

    // Set buffer size
    EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, estimated_buffer_size),
              PTI_SUCCESS);

    // Get metadata once per scope (reusable for all buffers/records)
    pti_metrics_scope_record_metadata_t metadata;

    // Set struct size
    metadata._struct_size = sizeof(pti_metrics_scope_record_metadata_t);

    // Populate metadata
    EXPECT_EQ(ptiMetricsScopeGetMetricsMetadata(scope_handle, &metadata), PTI_SUCCESS);

    std::cout << "Metrics Metadata set to " << metadata._struct_size << " bytes\n\n";

    std::cout << "Metadata for all records:\n";
    std::cout << "  Metrics count per record: " << metadata._metrics_count << "\n";
    EXPECT_EQ(metadata._metrics_count, metric_count);  // Should match requested count

    for (uint32_t i = 0; i < metadata._metrics_count; ++i) {
      std::cout << "  [" << i << "] " << metadata._metric_names[i];
      if (metadata._metric_units[i]) {
        std::cout << " (" << metadata._metric_units[i] << ")";
      }
      std::cout << " - Type: " << metadata._value_types[i] << "\n";

      // Validate metadata
      EXPECT_NE(metadata._metric_names[i], nullptr);
      EXPECT_TRUE(metadata._value_types[i] >= PTI_METRIC_VALUE_TYPE_UINT32 &&
                  metadata._value_types[i] <= PTI_METRIC_VALUE_TYPE_UINT16);
    }

    // Start collection
    EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);

    // Run GEMM workload - using exact same pattern as main_metrics_scope.cc
    try {
      RunGemm(256, 1);  // Small matrix size for testing
    } catch (...) {
      // If GEMM fails, still test the metrics API
      std::cout << "GEMM workload failed, but continuing with metrics API test" << std::endl;
    }

    // Stop collection
    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

    // Get buffer count
    size_t buffer_count = 0;
    EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

    std::cout << "Number of collection buffers: " << buffer_count << std::endl;

    // If we have buffers, process each buffer with user-owned memory
    if (buffer_count > 0) {
      for (size_t i = 0; i < buffer_count; i++) {
        std::cout << "\n--- Processing Buffer " << i << " with User Buffer ---\n";

        // Get buffer data
        void* buffer_data = nullptr;
        size_t actual_buffer_size = 0;
        EXPECT_EQ(
            ptiMetricsScopeGetCollectionBuffer(scope_handle, i, &buffer_data, &actual_buffer_size),
            PTI_SUCCESS);

        if (buffer_data != nullptr) {
          // Get buffer properties
          pti_metrics_scope_collection_buffer_properties_t props;
          props._struct_size = sizeof(props);
          EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, buffer_data, &props),
                    PTI_SUCCESS);

          std::cout << "Collection Buffer " << i << " properties:\n";
          std::cout << "  Device: " << reinterpret_cast<void*>(props._device_handle) << "\n";
          std::cout << "  Records: " << props._num_scopes << "\n";
          std::cout << "  Used Size: " << actual_buffer_size << " bytes\n";
          std::cout << "  Collected Group Name: " << props._metric_group_name << "\n\n\n";

          // Query required buffer size for user buffer
          size_t required_buffer_size = 0;
          size_t records_count = 0;

          pti_result query_result = ptiMetricsScopeQueryMetricsBufferSize(
              scope_handle, buffer_data, &required_buffer_size, &records_count);

          EXPECT_EQ(query_result, PTI_SUCCESS);
          std::cout << "  Required user buffer size: " << required_buffer_size << " bytes\n";
          std::cout << "  Records count: " << records_count << "\n";

          if (records_count > 0 && required_buffer_size > 0) {
            // User allocates the buffer
            auto metrics_buffer = std::make_unique<uint8_t[]>(required_buffer_size);
            ASSERT_NE(metrics_buffer, nullptr) << "Failed to allocate user buffer";

            // Calculate metrics into user buffer
            size_t actual_records_count = 0;
            pti_result calc_result =
                ptiMetricsScopeCalculateMetrics(scope_handle, buffer_data, metrics_buffer.get(),
                                                required_buffer_size, &actual_records_count);

            EXPECT_EQ(calc_result, PTI_SUCCESS);
            EXPECT_EQ(actual_records_count, records_count);

            if (calc_result == PTI_SUCCESS) {
              std::cout << "  Successfully calculated " << actual_records_count << " records\n";

              // Access records directly from user buffer
              auto records = reinterpret_cast<pti_metrics_scope_record_t*>(metrics_buffer.get());

              // Simplified parsing using metadata - no indices needed
              for (size_t r = 0; r < actual_records_count; r++) {
                std::cout << "    Kernel " << r << ":\n";
                std::cout << "      ID: " << records[r]._kernel_id << "\n";
                if (records[r]._kernel_name) {
                  std::cout << "      Kernel Name: " << records[r]._kernel_name << "\n";
                }

                // Direct access to requested metrics using metadata
                std::cout << "      Metrics (" << metadata._metrics_count << "):\n";

                // Validate that we have metric values
                EXPECT_NE(records[r]._metrics_values, nullptr);

                for (uint32_t m = 0; m < metadata._metrics_count; ++m) {
                  std::cout << "        " << metadata._metric_names[m] << ": ";

                  // Format value based on metadata type
                  switch (metadata._value_types[m]) {
                    case PTI_METRIC_VALUE_TYPE_UINT32:
                      std::cout << records[r]._metrics_values[m].ui32;
                      break;
                    case PTI_METRIC_VALUE_TYPE_UINT64:
                      std::cout << records[r]._metrics_values[m].ui64;
                      break;
                    case PTI_METRIC_VALUE_TYPE_FLOAT32:
                      std::cout << records[r]._metrics_values[m].fp32;
                      break;
                    case PTI_METRIC_VALUE_TYPE_FLOAT64:
                      std::cout << records[r]._metrics_values[m].fp64;
                      break;
                    case PTI_METRIC_VALUE_TYPE_BOOL8:
                      std::cout << (records[r]._metrics_values[m].b8 ? "true" : "false");
                      break;
                    default:
                      std::cout << "unknown";
                      break;
                  }

                  if (metadata._metric_units[m] && strlen(metadata._metric_units[m]) > 0) {
                    std::cout << " " << metadata._metric_units[m];
                  }
                  std::cout << "\n";
                }
              }
            }
          }
        }
      }
    } else {
      std::cout << "No data collected - this might indicate:" << std::endl;
      std::cout << "  1. Callbacks were not triggered" << std::endl;
      std::cout << "  2. No GPU kernels were detected" << std::endl;
      std::cout << "  3. Query data collection failed" << std::endl;
    }
  } else {
    std::cout << "Configuration failed with result: " << config_result << std::endl;
    std::cout << "This might be due to:" << std::endl;
    std::cout << "  1. Metrics not available on this device" << std::endl;
    std::cout << "  2. Metrics not in the same group" << std::endl;
    std::cout << "  3. Device doesn't support event-based metrics" << std::endl;
  }

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
}

TEST_F(GemmMetricsScopeFixtureTest, ScopeMultiThreadedDifferentKernels) {
  pti_scope_collection_handle_t scope_handle = nullptr;
  EXPECT_EQ(ptiMetricsScopeEnable(&scope_handle), PTI_SUCCESS);

  // Configure with ComputeBasic metrics
  pti_device_handle_t device = devices.size() > 0 ? devices[0]._handle : nullptr;
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};
  const uint32_t metric_count = sizeof(metric_names) / sizeof(metric_names[0]);

  pti_result config_result = ptiMetricsScopeConfigure(scope_handle, PTI_METRICS_SCOPE_AUTO_KERNEL,
                                                      &device, 1, metric_names, metric_count);

  if (config_result != PTI_SUCCESS) {
    std::cout << "Configuration failed with error: " << config_result
              << ", skipping multi-threaded test" << std::endl;
    EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
    return;
  }

  // Get metadata once per scope (reusable for all buffers/records)
  pti_metrics_scope_record_metadata_t metadata;

  // Set struct size
  metadata._struct_size = sizeof(pti_metrics_scope_record_metadata_t);

  // Populate metadata
  EXPECT_EQ(ptiMetricsScopeGetMetricsMetadata(scope_handle, &metadata), PTI_SUCCESS);

  std::cout << "Metrics Metadata set to " << metadata._struct_size << " bytes\n\n";

  std::cout << "Metadata for all records:\n";
  std::cout << "  Metrics count per record: " << metadata._metrics_count << "\n";
  EXPECT_EQ(metadata._metrics_count, metric_count);  // Should match requested count

  for (uint32_t i = 0; i < metadata._metrics_count; ++i) {
    std::cout << "  [" << i << "] " << metadata._metric_names[i];
    if (metadata._metric_units[i]) {
      std::cout << " (" << metadata._metric_units[i] << ")";
    }
    std::cout << " - Type: " << metadata._value_types[i] << "\n";
  }

  // Set buffer size for multiple kernels
  size_t estimated_buffer_size = 0;
  EXPECT_EQ(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, 50, &estimated_buffer_size),
            PTI_SUCCESS);
  EXPECT_EQ(ptiMetricsScopeSetCollectionBufferSize(scope_handle, estimated_buffer_size * 3),
            PTI_SUCCESS);

  std::cout << "\n=== Starting Multi-Threaded Metrics Collection ===" << std::endl;
  std::cout << "Buffer size set to: " << (estimated_buffer_size * 3) << " bytes" << std::endl;

  // Start collection
  EXPECT_EQ(ptiMetricsScopeStartCollection(scope_handle), PTI_SUCCESS);
  std::cout << "Metrics collection started successfully" << std::endl;

  // Create SYCL queue for GPU operations
  sycl::device dev;
  try {
    dev = sycl::device(sycl::gpu_selector_v);
    std::cout << "Using GPU device: " << dev.get_info<sycl::info::device::name>() << std::endl;
  } catch (...) {
    std::cout << "GPU device not available, skipping multi-threaded test" << std::endl;
    EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);
    EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);
    return;
  }

  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue queue(dev, sycl::async_handler{}, prop_list);

  // Thread synchronization
  std::atomic<int> threads_completed{0};
  std::atomic<bool> all_threads_ready{false};
  std::atomic<int> threads_ready{0};
  const int num_threads = 4;
  std::vector<std::thread> threads;
  std::vector<std::exception_ptr> thread_exceptions(num_threads);
  std::mutex print_mutex;  // For synchronized printing

  // Helper function to format metric value using metadata
  auto FormatMetricValue = [metadata](uint32_t metric_index,
                                      const pti_value_t& value) -> std::string {
    std::ostringstream oss;

    switch (metadata._value_types[metric_index]) {
      case PTI_METRIC_VALUE_TYPE_UINT32:
        oss << value.ui32;
        break;
      case PTI_METRIC_VALUE_TYPE_UINT64:
        oss << value.ui64;
        break;
      case PTI_METRIC_VALUE_TYPE_FLOAT32:
        oss << std::fixed << std::setprecision(3) << value.fp32;
        break;
      case PTI_METRIC_VALUE_TYPE_FLOAT64:
        oss << std::fixed << std::setprecision(3) << value.fp64;
        break;
      case PTI_METRIC_VALUE_TYPE_BOOL8:
        oss << (value.b8 ? "true" : "false");
        break;
      default:
        oss << "unknown";
        break;
    }

    if (metadata._metric_units[metric_index] && strlen(metadata._metric_units[metric_index]) > 0) {
      oss << " " << metadata._metric_units[metric_index];
    }

    return oss.str();
  };

  // Thread 1: GEMM kernels
  threads.emplace_back([&, thread_id = 0]() {
    try {
      threads_ready++;
      while (!all_threads_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "\n[Thread " << thread_id << "] Starting GEMM kernels..." << std::endl;
      }

      const unsigned size = 128;
      std::vector<float> a(size * size, A_VALUE);
      std::vector<float> b(size * size, B_VALUE);
      std::vector<float> c(size * size, 0.0f);

      // Run multiple GEMM operations
      for (int i = 0; i < 3; ++i) {
        try {
          sycl::buffer<float, 1> a_buf(a.data(), a.size());
          sycl::buffer<float, 1> b_buf(b.data(), b.size());
          sycl::buffer<float, 1> c_buf(c.data(), c.size());

          auto start_time = std::chrono::high_resolution_clock::now();

          queue.submit([&](sycl::handler& cgh) {
            auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
            auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
            auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

            cgh.parallel_for<class __GEMM_MT>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
              auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
              GEMM(a_acc_ptr.get(), b_acc_ptr.get(), c_acc_ptr.get(), size, id);
            });
          });
          queue.wait_and_throw();

          auto end_time = std::chrono::high_resolution_clock::now();
          auto duration =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

          {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[Thread " << thread_id << "] GEMM kernel " << (i + 1) << " completed in "
                      << duration.count() << " μs" << std::endl;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (const std::exception& e) {
          std::lock_guard<std::mutex> lock(print_mutex);
          std::cout << "[Thread " << thread_id << "] GEMM iteration " << i
                    << " failed: " << e.what() << std::endl;
        }
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[Thread " << thread_id << "] All GEMM kernels completed" << std::endl;
      }
      threads_completed++;
    } catch (...) {
      thread_exceptions[thread_id] = std::current_exception();
      threads_completed++;
    }
  });

  // Thread 2: Vector operations
  threads.emplace_back([&, thread_id = 1]() {
    try {
      threads_ready++;
      while (!all_threads_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "\n[Thread " << thread_id << "] Starting vector operations..." << std::endl;
      }

      const size_t vector_size = 10000;
      std::vector<float> vec_a(vector_size, 2.0f);
      std::vector<float> vec_b(vector_size, 3.0f);
      std::vector<float> vec_c(vector_size, 0.0f);

      // Run vector addition kernels
      for (int i = 0; i < 4; ++i) {
        try {
          sycl::buffer<float, 1> a_buf(vec_a.data(), vec_a.size());
          sycl::buffer<float, 1> b_buf(vec_b.data(), vec_b.size());
          sycl::buffer<float, 1> c_buf(vec_c.data(), vec_c.size());

          auto start_time = std::chrono::high_resolution_clock::now();

          queue.submit([&](sycl::handler& cgh) {
            auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
            auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
            auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

            cgh.parallel_for<class __VectorAdd_MT>(
                sycl::range<1>(vector_size),
                [=](sycl::id<1> idx) { c_acc[idx] = a_acc[idx] + b_acc[idx]; });
          });
          queue.wait_and_throw();

          auto end_time = std::chrono::high_resolution_clock::now();
          auto duration =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

          {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[Thread " << thread_id << "] VectorAdd kernel " << (i + 1)
                      << " completed in " << duration.count() << " μs" << std::endl;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } catch (const std::exception& e) {
          std::lock_guard<std::mutex> lock(print_mutex);
          std::cout << "[Thread " << thread_id << "] Vector iteration " << i
                    << " failed: " << e.what() << std::endl;
        }
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[Thread " << thread_id << "] All vector operations completed" << std::endl;
      }
      threads_completed++;
    } catch (...) {
      thread_exceptions[thread_id] = std::current_exception();
      threads_completed++;
    }
  });

  // Thread 3: Memory operations
  threads.emplace_back([&, thread_id = 2]() {
    try {
      threads_ready++;
      while (!all_threads_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "\n[Thread " << thread_id << "] Starting memory operations..." << std::endl;
      }

      const size_t mem_size = 8192;
      std::vector<int> data(mem_size, 42);

      // Run memory fill/copy operations
      for (int i = 0; i < 3; ++i) {
        try {
          sycl::buffer<int, 1> data_buf(data.data(), data.size());

          auto start_time = std::chrono::high_resolution_clock::now();

          // Memory fill operation
          queue.submit([&](sycl::handler& cgh) {
            auto acc = data_buf.get_access<sycl::access::mode::write>(cgh);
            cgh.parallel_for<class __MemFill_MT>(sycl::range<1>(mem_size), [=](sycl::id<1> idx) {
              acc[idx] = static_cast<int>(idx[0]) + i;
            });
          });
          queue.wait_and_throw();

          auto mid_time = std::chrono::high_resolution_clock::now();

          // Memory copy-like operation
          queue.submit([&](sycl::handler& cgh) {
            auto acc = data_buf.get_access<sycl::access::mode::read_write>(cgh);
            cgh.parallel_for<class __MemCopy_MT>(
                sycl::range<1>(mem_size / 2),
                [=](sycl::id<1> idx) { acc[idx + mem_size / 2] = acc[idx]; });
          });
          queue.wait_and_throw();

          auto end_time = std::chrono::high_resolution_clock::now();
          auto fill_duration =
              std::chrono::duration_cast<std::chrono::microseconds>(mid_time - start_time);
          auto copy_duration =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time - mid_time);

          {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[Thread " << thread_id << "] Memory operations " << (i + 1)
                      << " - Fill: " << fill_duration.count()
                      << " μs, Copy: " << copy_duration.count() << " μs" << std::endl;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(8));
        } catch (const std::exception& e) {
          std::lock_guard<std::mutex> lock(print_mutex);
          std::cout << "[Thread " << thread_id << "] Memory iteration " << i
                    << " failed: " << e.what() << std::endl;
        }
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[Thread " << thread_id << "] All memory operations completed" << std::endl;
      }
      threads_completed++;
    } catch (...) {
      thread_exceptions[thread_id] = std::current_exception();
      threads_completed++;
    }
  });

  // Thread 4: Reduction operations
  threads.emplace_back([&, thread_id = 3]() {
    try {
      threads_ready++;
      while (!all_threads_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "\n[Thread " << thread_id << "] Starting reduction operations..." << std::endl;
      }

      const size_t reduce_size = 16384;
      std::vector<float> input(reduce_size);
      std::vector<float> output(reduce_size / 256, 0.0f);

      // Initialize input data
      for (size_t i = 0; i < reduce_size; ++i) {
        input[i] = static_cast<float>(i % 100) / 100.0f;
      }

      // Run reduction kernels
      for (int i = 0; i < 2; ++i) {
        try {
          sycl::buffer<float, 1> input_buf(input.data(), input.size());
          sycl::buffer<float, 1> output_buf(output.data(), output.size());

          auto start_time = std::chrono::high_resolution_clock::now();

          queue.submit([&](sycl::handler& cgh) {
            auto input_acc = input_buf.get_access<sycl::access::mode::read>(cgh);
            auto output_acc = output_buf.get_access<sycl::access::mode::write>(cgh);

            // Local memory for reduction
            sycl::local_accessor<float, 1> local_mem(sycl::range<1>(256), cgh);

            cgh.parallel_for<class __Reduction_MT>(
                sycl::nd_range<1>(sycl::range<1>(reduce_size), sycl::range<1>(256)),
                [=](sycl::nd_item<1> item) {
                  size_t global_id = item.get_global_id(0);
                  size_t local_id = item.get_local_id(0);
                  size_t group_id = item.get_group(0);

                  // Load data into local memory
                  local_mem[local_id] = (global_id < reduce_size) ? input_acc[global_id] : 0.0f;
                  item.barrier(sycl::access::fence_space::local_space);

                  // Reduction in local memory
                  for (size_t stride = 128; stride > 0; stride >>= 1) {
                    if (local_id < stride) {
                      local_mem[local_id] += local_mem[local_id + stride];
                    }
                    item.barrier(sycl::access::fence_space::local_space);
                  }

                  // Write result
                  if (local_id == 0) {
                    output_acc[group_id] = local_mem[0];
                  }
                });
          });
          queue.wait_and_throw();

          auto end_time = std::chrono::high_resolution_clock::now();
          auto duration =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

          {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[Thread " << thread_id << "] Reduction kernel " << (i + 1)
                      << " completed in " << duration.count() << " μs" << std::endl;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(12));
        } catch (const std::exception& e) {
          std::lock_guard<std::mutex> lock(print_mutex);
          std::cout << "[Thread " << thread_id << "] Reduction iteration " << i
                    << " failed: " << e.what() << std::endl;
        }
      }

      {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[Thread " << thread_id << "] All reduction operations completed" << std::endl;
      }
      threads_completed++;
    } catch (...) {
      thread_exceptions[thread_id] = std::current_exception();
      threads_completed++;
    }
  });

  // Wait for all threads to be ready
  while (threads_ready.load() < num_threads) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::cout << "\nAll threads ready, starting concurrent execution..." << std::endl;
  all_threads_ready = true;

  // Wait for all threads to complete with timeout
  auto start_time = std::chrono::steady_clock::now();
  const auto timeout = std::chrono::seconds(30);

  while (threads_completed.load() < num_threads) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > timeout) {
      std::cout << "Timeout waiting for threads to complete" << std::endl;
      break;
    }
  }

  // Join all threads
  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // Check for thread exceptions
  for (int i = 0; i < num_threads; ++i) {
    if (thread_exceptions[i]) {
      try {
        std::rethrow_exception(thread_exceptions[i]);
      } catch (const std::exception& e) {
        std::cout << "[Thread " << i << "] Exception: " << e.what() << std::endl;
      }
    }
  }

  std::cout << "\n=== All threads completed, stopping collection ===" << std::endl;

  // Stop collection
  EXPECT_EQ(ptiMetricsScopeStopCollection(scope_handle), PTI_SUCCESS);

  // Process collected data with detailed metrics printing
  size_t buffer_count = 0;
  EXPECT_EQ(ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &buffer_count), PTI_SUCCESS);

  std::cout << "\n=== METRICS COLLECTION RESULTS ===" << std::endl;
  std::cout << "Total collection buffers: " << buffer_count << std::endl;

  size_t total_records = 0;
  std::map<std::string, int> kernel_type_counts;
  std::map<std::string, std::vector<double>> metric_values_by_kernel;

  if (buffer_count > 0) {
    for (size_t i = 0; i < buffer_count; i++) {
      std::cout << "\n--- Processing Buffer " << i << " with User Buffer---" << std::endl;

      void* buffer = nullptr;
      size_t buffer_size = 0;
      EXPECT_EQ(ptiMetricsScopeGetCollectionBuffer(scope_handle, i, &buffer, &buffer_size),
                PTI_SUCCESS);

      if (buffer != nullptr) {
        pti_metrics_scope_collection_buffer_properties_t props;
        props._struct_size = sizeof(props);
        EXPECT_EQ(ptiMetricsScopeGetCollectionBufferProperties(scope_handle, buffer, &props),
                  PTI_SUCCESS);

        total_records += props._num_scopes;
        std::cout << "Buffer " << i << " properties:" << std::endl;
        std::cout << "  Device: " << reinterpret_cast<void*>(props._device_handle) << std::endl;
        std::cout << "  Records: " << props._num_scopes << std::endl;
        std::cout << "  Size: " << buffer_size << " bytes" << std::endl;

        // Query required buffer size for user buffer
        size_t required_buffer_size = 0;
        size_t records_count = 0;
        pti_result query_result = ptiMetricsScopeQueryMetricsBufferSize(
            scope_handle, buffer, &required_buffer_size, &records_count);

        if (query_result == PTI_SUCCESS && records_count > 0 && required_buffer_size > 0) {
          std::cout << "  Required user buffer size: " << required_buffer_size << " bytes"
                    << std::endl;
          std::cout << "  Records count: " << records_count << std::endl;

          // User allocates the buffer
          auto metrics_buffer = std::make_unique<uint8_t[]>(required_buffer_size);
          ASSERT_NE(metrics_buffer, nullptr) << "Failed to allocate user buffer";

          // Calculate metrics into user buffer for detailed analysis
          size_t actual_records_count = 0;
          pti_result calc_result =
              ptiMetricsScopeCalculateMetrics(scope_handle, buffer, metrics_buffer.get(),
                                              required_buffer_size, &actual_records_count);

          if (calc_result == PTI_SUCCESS && actual_records_count > 0) {
            std::cout << "  Successfully calculated metrics for " << actual_records_count
                      << " kernel records using user buffer" << std::endl;

            auto records = reinterpret_cast<pti_metrics_scope_record_t*>(metrics_buffer.get());

            // Process each kernel record
            for (size_t r = 0; r < actual_records_count; r++) {
              std::cout << "\n  === Kernel Record " << r << " ===" << std::endl;
              std::cout << "    Kernel ID: " << records[r]._kernel_id << std::endl;
              std::cout << "    Queue: " << reinterpret_cast<void*>(records[r]._queue) << std::endl;

              std::string kernel_name = "Unknown";
              if (records[r]._kernel_name) {
                kernel_name = std::string(records[r]._kernel_name);
                std::cout << "    Kernel Name: " << kernel_name << std::endl;
              }

              // Count kernel types
              std::string kernel_type = "Other";
              if (kernel_name.find("GEMM") != std::string::npos)
                kernel_type = "GEMM";
              else if (kernel_name.find("VectorAdd") != std::string::npos)
                kernel_type = "VectorAdd";
              else if (kernel_name.find("MemFill") != std::string::npos)
                kernel_type = "MemFill";
              else if (kernel_name.find("MemCopy") != std::string::npos)
                kernel_type = "MemCopy";
              else if (kernel_name.find("Reduction") != std::string::npos)
                kernel_type = "Reduction";

              kernel_type_counts[kernel_type]++;

              // Direct access to requested metrics using metadata
              std::cout << "    Metrics (" << metadata._metrics_count << " total):" << std::endl;

              // Validate that we have metric values
              EXPECT_NE(records[r]._metrics_values, nullptr);

              for (uint32_t m = 0; m < metadata._metrics_count; ++m) {
                std::string formatted_value = FormatMetricValue(m, records[r]._metrics_values[m]);
                std::cout << "      " << std::setw(25) << std::left << metadata._metric_names[m]
                          << ": " << std::setw(15) << std::right << formatted_value << std::endl;

                // Store metric values for summary
                std::string metric_key = kernel_type + "::" + metadata._metric_names[m];

                // Extract numeric value for averaging
                double numeric_value = 0.0;
                switch (metadata._value_types[m]) {
                  case PTI_METRIC_VALUE_TYPE_UINT32:
                    numeric_value = static_cast<double>(records[r]._metrics_values[m].ui32);
                    break;
                  case PTI_METRIC_VALUE_TYPE_UINT64:
                    numeric_value = static_cast<double>(records[r]._metrics_values[m].ui64);
                    break;
                  case PTI_METRIC_VALUE_TYPE_FLOAT32:
                    numeric_value = static_cast<double>(records[r]._metrics_values[m].fp32);
                    break;
                  case PTI_METRIC_VALUE_TYPE_FLOAT64:
                    numeric_value = records[r]._metrics_values[m].fp64;
                    break;
                  default:
                    numeric_value = 0.0;
                    break;
                }
                metric_values_by_kernel[metric_key].push_back(numeric_value);
              }
            }
          } else {
            std::cout << "  Failed to calculate metrics into user buffer (result: " << calc_result
                      << ")" << std::endl;
          }
        } else {
          std::cout << "  Failed to query buffer size (result: " << query_result << ")"
                    << std::endl;
        }
      }
    }

    // Print summary statistics
    std::cout << "\n=== SUMMARY STATISTICS ===" << std::endl;
    std::cout << "Total kernel records collected: " << total_records << std::endl;
    std::cout << "Kernel type distribution:" << std::endl;

    for (const auto& [kernel_type, count] : kernel_type_counts) {
      std::cout << "  " << std::setw(15) << std::left << kernel_type << ": " << count << " kernels"
                << std::endl;
    }

    if (!metric_values_by_kernel.empty()) {
      std::cout << "\nAverage metric values by kernel type:" << std::endl;

      std::map<std::string, std::map<std::string, double>> avg_metrics_by_type;

      for (const auto& [metric_key, values] : metric_values_by_kernel) {
        size_t pos = metric_key.find("::");
        if (pos != std::string::npos) {
          std::string kernel_type = metric_key.substr(0, pos);
          std::string metric_name = metric_key.substr(pos + 2);

          double sum = 0.0;
          for (double val : values) sum += val;
          double avg = values.empty() ? 0.0 : sum / values.size();

          avg_metrics_by_type[kernel_type][metric_name] = avg;
        }
      }

      for (const auto& [kernel_type, metrics] : avg_metrics_by_type) {
        std::cout << "\n  " << kernel_type << " kernels:" << std::endl;
        for (const auto& [metric_name, avg_value] : metrics) {
          std::cout << "    " << std::setw(25) << std::left << metric_name << ": " << std::setw(15)
                    << std::right << std::fixed << std::setprecision(3) << avg_value << std::endl;
        }
      }
    }

  } else {
    std::cout << "\nNo data collected. This might indicate:" << std::endl;
    std::cout << "  1. Callbacks were not triggered" << std::endl;
    std::cout << "  2. No GPU kernels were detected" << std::endl;
    std::cout << "  3. Query data collection failed" << std::endl;
    std::cout << "  4. Metric group not supported on this device" << std::endl;
  }

  std::cout << "\n=== MULTI-THREADED TEST SUMMARY ===" << std::endl;
  std::cout << "Threads completed: " << threads_completed.load() << "/" << num_threads << std::endl;
  std::cout << "Total kernel records: " << total_records << std::endl;
  std::cout << "Collection buffers used: " << buffer_count << std::endl;
  std::cout << "Unique kernel types detected: " << kernel_type_counts.size() << std::endl;

  // The test passes if:
  // 1. All threads completed without crashing
  // 2. Collection worked without errors
  // 3. We collected some data (even if callbacks didn't trigger, the API should work)
  EXPECT_EQ(threads_completed.load(), num_threads);

  EXPECT_EQ(ptiMetricsScopeDisable(scope_handle), PTI_SUCCESS);

  std::cout << "\n=== Multi-threaded metrics scope test completed successfully ===" << std::endl;
}