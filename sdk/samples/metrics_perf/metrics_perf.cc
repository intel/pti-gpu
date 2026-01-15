//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// PTI Metrics API Overhead Test

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <iostream>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "pti/pti_metrics.h"
#include "samples_utils.h"

namespace {

// Test configuration
constexpr int kNumIterations = 500;
constexpr int kWarmupIterations = 10;
constexpr unsigned kDefaultMatrixSize = 512;
constexpr unsigned kMaxSize = 8192;
constexpr unsigned kMinSize = 32;
constexpr float kAValue = 0.128f;
constexpr float kBValue = 0.256f;
constexpr float kMaxEPS = 1.0e-4f;

// Global test state
std::vector<pti_device_properties_t> g_devices;
std::vector<std::vector<pti_metrics_group_properties_t>> g_device_metric_groups;

static float Check(const std::vector<float>& a, float value) {
  assert(value > kMaxEPS);

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

static float RunAndCheck(sycl::queue queue, const std::vector<float>& a,
                         const std::vector<float>& b, std::vector<float>& c, unsigned size,
                         float expected_result, bool verify_results = true) {
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
    queue.wait_and_throw();
  } catch (const sycl::exception& e) {
    std::cerr << "[ERROR] " << e.what() << std::endl;
    throw;
  }
  if (verify_results) {
    return Check(c, expected_result);
  }
  return 0.0f;
}

static void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& c, unsigned size, unsigned repeat_count,
                    float expected_result, bool check_results = true) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    bool verify_itr = check_results && (i == 0 || i == repeat_count - 1);
    float eps = RunAndCheck(queue, a, b, c, size, expected_result, verify_itr);
    if (verify_itr && eps > kMaxEPS) {
      std::cerr << "[ERROR] Results are INCORRECT with accuracy: " << eps
                << " while expected less than " << kMaxEPS << std::endl;
    }
  }
}

// Thread-safe device discovery
bool DiscoverDevicesThreadSafe(std::vector<pti_device_properties_t>& devices) {
  uint32_t device_count = 0;
  pti_result result = ptiMetricsGetDevices(nullptr, &device_count);
  if (result != PTI_SUCCESS || device_count == 0) {
    return false;
  }

  devices.resize(device_count);
  result = ptiMetricsGetDevices(devices.data(), &device_count);
  return result == PTI_SUCCESS;
}

// Thread-safe metric groups discovery
bool DiscoverMetricGroupsThreadSafe(pti_device_handle_t device_handle,
                                    std::vector<pti_metrics_group_properties_t>& metric_groups) {
  uint32_t group_count = 0;
  pti_result result = ptiMetricsGetMetricGroups(device_handle, nullptr, &group_count);
  if (result != PTI_SUCCESS || group_count == 0) {
    return false;
  }

  metric_groups.resize(group_count);
  result = ptiMetricsGetMetricGroups(device_handle, metric_groups.data(), &group_count);
  return result == PTI_SUCCESS;
}

// Find suitable metric group for collection
pti_metrics_group_handle_t FindSuitableMetricGroup(
    const std::vector<pti_metrics_group_properties_t>& groups) {
  for (const auto& group : groups) {
    if (group._type == PTI_METRIC_GROUP_TYPE_TIME_BASED) {
      return group._handle;
    }
  }
  for (const auto& group : groups) {
    if (group._type == PTI_METRIC_GROUP_TYPE_EVENT_BASED) {
      return group._handle;
    }
  }
  return nullptr;
}

static void SetupMatrices(std::vector<float>& a, std::vector<float>& b, std::vector<float>& c,
                          unsigned size) {
  const size_t total_size = size * size;
  a.assign(total_size, kAValue);
  b.assign(total_size, kBValue);
  c.assign(total_size, 0.0f);
}

static bool StartMetricsCollection(pti_device_handle_t device_handle,
                                   pti_metrics_group_handle_t metrics_group_handle) {
  pti_metrics_group_collection_params_t config_params = {};
  config_params._struct_size = sizeof(config_params);
  config_params._group_handle = metrics_group_handle;
  config_params._sampling_interval = 100000;   // 100μs
  config_params._time_aggr_window = 10000000;  // 10ms

  if (ptiMetricsConfigureCollection(device_handle, &config_params, 1) != PTI_SUCCESS) {
    std::cerr << "Failed to configure metrics collection" << std::endl;
    return false;
  }

  if (ptiMetricsStartCollection(device_handle) != PTI_SUCCESS) {
    std::cerr << "Failed to start metrics collection" << std::endl;
    return false;
  }
  return true;
}

static void StopMetricsCollection(pti_device_handle_t device_handle) {
  if (ptiMetricsStopCollection(device_handle) != PTI_SUCCESS) {
    std::cerr << "Failed to stop metrics collection" << std::endl;
  }
}

// Helper function to get metrics sample count
static uint32_t GetMetricsSampleCount(pti_device_handle_t device_handle,
                                      pti_metrics_group_handle_t metrics_group_handle) {
  uint32_t values_count = 0;
  pti_result result =
      ptiMetricsGetCalculatedData(device_handle, metrics_group_handle, nullptr, &values_count);
  if (result != PTI_SUCCESS) {
    if (result == PTI_ERROR_METRICS_NO_DATA_COLLECTED) {
      return 0;  // No data collected is not an error for our reporting purposes
    }
    std::cerr << "Warning: Failed to get metrics data count, result: " << result << std::endl;
    return 0;
  }

  uint32_t metric_count = 0;
  for (size_t device_idx = 0; device_idx < g_device_metric_groups.size(); ++device_idx) {
    for (const auto& group : g_device_metric_groups[device_idx]) {
      if (group._handle == metrics_group_handle) {
        metric_count = group._metric_count;
        break;
      }
    }
    if (metric_count > 0) break;  // Found it
  }

  if (metric_count == 0) {
    std::cerr << "Warning: Could not find metric group in global list" << std::endl;
    return 0;
  }

  return values_count / metric_count;
}

// Helper function to print metrics collection info
static void PrintMetricsInfo(pti_device_handle_t device_handle,
                             pti_metrics_group_handle_t metrics_group_handle,
                             const std::string& test_name) {
  uint32_t samples_count = GetMetricsSampleCount(device_handle, metrics_group_handle);
  std::cout << test_name << " - Metrics samples collected: " << samples_count << std::endl;
}

// Warmup function for single queue
static void WarmupSingleQueue(sycl::queue& queue, unsigned size, unsigned warmup_iterations) {
  std::vector<float> a, b, c;
  SetupMatrices(a, b, c, size);

  Compute(queue, a, b, c, size, warmup_iterations, kAValue * kBValue * size, false);
}

// Warmup function for multiple queues
static void WarmupMultiQueues(const std::vector<sycl::queue>& queues, unsigned size,
                              unsigned warmup_iterations) {
  std::vector<std::thread> warmup_threads;

  for (const auto& queue : queues) {
    warmup_threads.emplace_back([&queue, size, warmup_iterations]() {
      std::vector<float> a, b, c;
      SetupMatrices(a, b, c, size);
      Compute(queue, a, b, c, size, warmup_iterations, kAValue * kBValue * size, false);
    });
  }

  for (auto& thread : warmup_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

static void WorkerThread(sycl::queue queue, pti_device_handle_t device_handle = nullptr,
                         pti_metrics_group_handle_t metrics_group_handle = nullptr,
                         unsigned iterations = kNumIterations, unsigned size = kDefaultMatrixSize,
                         const std::string& thread_name = "Worker") {
  std::vector<float> a, b, c;
  SetupMatrices(a, b, c, size);

  std::cout << thread_name << " - Starting computation" << std::endl;

  // Start metrics if handles provided
  bool metrics_started = false;
  if (device_handle && metrics_group_handle) {
    metrics_started = StartMetricsCollection(device_handle, metrics_group_handle);
    if (!metrics_started) {
      std::cerr << thread_name << " Thread failed to start metrics collection" << std::endl;
      return;
    }
  }

  Compute(queue, a, b, c, size, iterations, kAValue * kBValue * size, false);

  if (metrics_started) {
    StopMetricsCollection(device_handle);
    PrintMetricsInfo(device_handle, metrics_group_handle, thread_name);
  }
}

// Test 1: Baseline without profiling
std::chrono::duration<double> RunBaselineTest(sycl::queue& queue, unsigned iterations,
                                              unsigned size) {
  WarmupSingleQueue(queue, size, kWarmupIterations);

  std::vector<float> a, b, c;
  SetupMatrices(a, b, c, size);

  auto start_time = std::chrono::high_resolution_clock::now();
  Compute(queue, a, b, c, size, iterations, kAValue * kBValue * size, false);
  auto end_time = std::chrono::high_resolution_clock::now();

  return end_time - start_time;
}

// Test 2: Single-threaded with profiling
std::chrono::duration<double> RunSingleThreadedProfilingTest(
    pti_device_handle_t device_handle, pti_metrics_group_handle_t metrics_group_handle,
    sycl::queue& queue, unsigned iterations, unsigned size) {
  WarmupSingleQueue(queue, size, kWarmupIterations);

  std::vector<float> a, b, c;
  SetupMatrices(a, b, c, size);

  auto start_time = std::chrono::high_resolution_clock::now();

  if (!StartMetricsCollection(device_handle, metrics_group_handle)) {
    return std::chrono::duration<double>(0.0);
  }

  Compute(queue, a, b, c, size, iterations, kAValue * kBValue * size, false);
  StopMetricsCollection(device_handle);

  auto end_time = std::chrono::high_resolution_clock::now();

  PrintMetricsInfo(device_handle, metrics_group_handle, "Single-threaded profiling test");

  return end_time - start_time;
}

// Test 3: Multi-threaded without profiling
std::chrono::duration<double> RunMultiThreadedTest(const std::vector<sycl::queue>& queues,
                                                   unsigned iterations, unsigned size) {
  WarmupMultiQueues(queues, size, kWarmupIterations);

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  size_t thread_count = (std::min)(queues.size(), size_t(2));

  for (size_t i = 0; i < thread_count; ++i) {
    std::string thread_name = "NoProfile-Thread-" + std::to_string(i);
    threads.emplace_back(WorkerThread, queues[i], nullptr, nullptr, iterations, size, thread_name);
  }

  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  return end_time - start_time;
}

// Test 4: Multi-threaded with profiling
std::chrono::duration<double> RunMultiThreadedProfilingTest(
    const std::vector<pti_device_handle_t>& device_handles,
    const std::vector<pti_metrics_group_handle_t>& metrics_group_handles,
    const std::vector<sycl::queue>& queues, unsigned iterations, unsigned size) {
  WarmupMultiQueues(queues, size, kWarmupIterations);

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  size_t thread_count = (std::min)({device_handles.size(), queues.size(), size_t(2)});

  for (size_t i = 0; i < thread_count; ++i) {
    std::string thread_name = "Thread-" + std::to_string(i);
    threads.emplace_back(WorkerThread, queues[i], device_handles[i], metrics_group_handles[i],
                         iterations, size, thread_name);
  }

  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  return end_time - start_time;
}

bool InitializeTest() {
  bool metrics_enabled = (samples_utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  if (!metrics_enabled) {
    std::cerr << "ERROR: ZET_ENABLE_METRICS must be set to 1" << std::endl;
    return false;
  }

  if (!DiscoverDevicesThreadSafe(g_devices)) {
    std::cerr << "ERROR: Failed to discover devices" << std::endl;
    return false;
  }

  if (g_devices.empty()) {
    std::cerr << "ERROR: No devices available for testing" << std::endl;
    return false;
  }

  g_device_metric_groups.resize(g_devices.size());
  for (size_t i = 0; i < g_devices.size(); ++i) {
    if (!DiscoverMetricGroupsThreadSafe(g_devices[i]._handle, g_device_metric_groups[i])) {
      std::cerr << "ERROR: Failed to discover metric groups for device " << i << std::endl;
      return false;
    }
    if (g_device_metric_groups[i].empty()) {
      std::cerr << "ERROR: No metric groups for device " << i << std::endl;
      return false;
    }
  }

  return true;
}

}  // namespace

void Usage(const char* name) {
  std::cout << " Calculating floating point matrix multiply on gpu, compare the performance "
               "with and without PTI Metrics API (Single and multi-thread)\n"
            << "  Usage " << name << "  [ options ]" << std::endl;
  std::cout << "--size [-s]     integer        "
            << "Matrix size, default: " << kDefaultMatrixSize << std::endl;
  std::cout << "--repeat [-r]   integer        "
            << "Number of iterations, default: " << kNumIterations << std::endl;
}

int main(int argc, char* argv[]) {
  int exit_code = EXIT_SUCCESS;
  unsigned repeat_count = kNumIterations;
  unsigned size = kDefaultMatrixSize;

  try {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        size = (temp < kMinSize) ? kMinSize : (temp > kMaxSize) ? kMaxSize : temp;
      } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repeat") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        repeat_count = (temp < 1) ? 1 : temp;
      } else {
        Usage(argv[0]);
        return EXIT_SUCCESS;
      }
    }
  } catch (...) {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  std::cout << "=== PTI Metrics API Overhead Test ===" << std::endl;
  std::cout << "Matrix size: " << size << "x" << size << std::endl;
  std::cout << "Iterations: " << repeat_count << std::endl;
  std::cout << std::endl;

  if (!InitializeTest()) {
    return EXIT_FAILURE;
  }

  try {
    std::vector<sycl::device> sycl_devices;
    std::vector<sycl::queue> queues;

    auto all_devices = sycl::device::get_devices(sycl::info::device_type::gpu);
    if (all_devices.empty()) {
      std::cerr << "ERROR: No GPU devices found" << std::endl;
      return EXIT_FAILURE;
    }

    size_t num_queues = (std::min)({all_devices.size(), g_devices.size(), size_t(2)});
    sycl::property_list prop_list{sycl::property::queue::in_order()};

    for (size_t i = 0; i < num_queues; ++i) {
      sycl_devices.push_back(all_devices[i]);
      queues.emplace_back(all_devices[i], sycl::async_handler{}, prop_list);
      std::cout << "Queue " << i
                << " created on device: " << all_devices[i].get_info<sycl::info::device::name>()
                << std::endl;
    }

    if (queues.size() < 2) {
      std::cout << "Warning: Only " << queues.size() << " device(s) available for testing"
                << std::endl;
    }

    std::cout << "Device verification:" << std::endl;
    for (size_t i = 0; i < queues.size(); ++i) {
      auto device = queues[i].get_device();

      try {
        auto ze_device = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);

        std::cout << "Queue[" << i << "] native device handle: " << ze_device << std::endl;
      } catch (...) {
        std::cout << "Queue[" << i << "] - Could not get native handle" << std::endl;
      }
    }

    for (size_t i = 0; i < g_devices.size(); ++i) {
      std::cout << "g_devices[" << i << "]._handle: " << g_devices[i]._handle << std::endl;
      std::cout << "g_devices[" << i << "] UUID: ";
      for (size_t j = 0; j < PTI_MAX_DEVICE_UUID_SIZE; ++j) {
        printf("%02x", g_devices[i]._uuid[j]);
      }
      std::cout << std::endl;
    }

    std::cout << std::endl;

    pti_device_handle_t device_handle = g_devices[0]._handle;
    pti_metrics_group_handle_t metrics_group_handle =
        FindSuitableMetricGroup(g_device_metric_groups[0]);

    if (metrics_group_handle == nullptr) {
      std::cerr << "ERROR: No suitable metric group found" << std::endl;
      return EXIT_FAILURE;
    }

    std::cout << "Running tests..." << std::endl;

    sycl::queue& primary_queue = queues[0];

    // 1. Single thread baseline (no profiling)
    std::cout << "=== Running Single thread baseline (no profiling) ===" << std::endl;
    auto single_baseline_time = RunBaselineTest(primary_queue, repeat_count, size);
    double single_baseline_throughput =
        static_cast<double>(repeat_count) / single_baseline_time.count();

    // 2. Single thread with profiling
    std::cout << "=== Running Single thread with PTI Metrics profiling ===" << std::endl;
    auto single_profiling_time = RunSingleThreadedProfilingTest(device_handle, metrics_group_handle,
                                                                primary_queue, repeat_count, size);
    double single_profiling_throughput =
        static_cast<double>(repeat_count) / single_profiling_time.count();

    // Calculate overheads
    double single_overhead_pct = ((single_profiling_time.count() - single_baseline_time.count()) /
                                  single_baseline_time.count()) *
                                 100.0;

    double multi_baseline_throughput = 0.0;
    double multi_profiling_throughput = 0.0;
    double multi_overhead_pct = 0.0;
    std::chrono::duration<double> multi_baseline_time, multi_profiling_time;

    // Only run multi-threaded tests if we have multiple devices/queues
    if (queues.size() >= 2 && g_devices.size() >= 2) {
      // 3. Multi-threaded baseline (no profiling)
      std::cout << "=== Running Multiple thread baseline (no profiling) ===" << std::endl;
      multi_baseline_time = RunMultiThreadedTest(queues, repeat_count, size);
      multi_baseline_throughput = (2.0 * repeat_count) / multi_baseline_time.count();

      // 4. Multi-threaded with profiling
      std::vector<pti_device_handle_t> device_handles;
      std::vector<pti_metrics_group_handle_t> group_handles;

      device_handles = {g_devices[0]._handle, g_devices[1]._handle};
      group_handles = {FindSuitableMetricGroup(g_device_metric_groups[0]),
                       FindSuitableMetricGroup(g_device_metric_groups[1])};

      std::cout << "=== Running Multiple thread with PTI Metrics profiling ===" << std::endl;
      multi_profiling_time =
          RunMultiThreadedProfilingTest(device_handles, group_handles, queues, repeat_count, size);
      multi_profiling_throughput = (2.0 * repeat_count) / multi_profiling_time.count();

      multi_overhead_pct = ((multi_profiling_time.count() - multi_baseline_time.count()) /
                            multi_baseline_time.count()) *
                           100.0;

    } else {
      std::cout << "Less than 2 devices, skipping multiple device tests..." << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== RESULTS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "1 thread baseline (no profiling):  " << single_baseline_throughput
              << " kernels/sec" << std::endl;
    std::cout << "1 thread with profiling:           " << single_profiling_throughput
              << " kernels/sec" << std::endl;
    if (g_devices.size() >= 2) {
      std::cout << "2 threads baseline (no profiling): " << multi_baseline_throughput
                << " kernels/sec" << std::endl;
      std::cout << "2 threads with profiling:          " << multi_profiling_throughput
                << " kernels/sec" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== OVERHEAD ===" << std::endl;
    std::cout << "Single-threaded profiling overhead: " << single_overhead_pct << "%" << std::endl;
    if (g_devices.size() >= 2) {
      std::cout << "Multi-threaded profiling overhead:  " << multi_overhead_pct << "%" << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return exit_code;
}
