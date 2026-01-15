//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Multi-thread tests for PTI Metrics API

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "metrics_utils.h"
#include "pti/pti_metrics.h"
#include "utils.h"

namespace {

constexpr int kNumThreads = 8;
constexpr int kNumIterations = 100;

std::vector<pti_device_properties_t> g_devices;
std::vector<std::vector<pti_metrics_group_properties_t>> g_metric_groups;
std::mutex g_setup_mutex;
std::atomic<bool> g_setup_complete{false};

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

// Helper function to get metrics sample count
static uint32_t GetMetricsSampleCount(pti_device_handle_t device_handle,
                                      pti_metrics_group_handle_t metrics_group_handle) {
  uint32_t values_count = 0;
  pti_result result =
      ptiMetricsGetCalculatedData(device_handle, metrics_group_handle, nullptr, &values_count);
  if (result != PTI_SUCCESS) {
    if (result == PTI_ERROR_METRICS_NO_DATA_COLLECTED) {
      return 0;
    }
    std::cerr << "Warning: Failed to get metrics data count, result: " << result << std::endl;
    return 0;
  }

  uint32_t metric_count = 0;
  for (size_t device_idx = 0; device_idx < g_metric_groups.size(); ++device_idx) {
    for (const auto& group : g_metric_groups[device_idx]) {
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

// Helper function to submit minimal GPU compute work
void SubmitMinimalGpuWork(const sycl::device& device) {
  try {
    sycl::queue queue(device);
    constexpr size_t work_size = 1024;
    std::vector<int> host_data(work_size, 1);

    {
      sycl::buffer<int, 1> buffer(host_data.data(), sycl::range<1>(work_size));

      queue.submit([&](sycl::handler& h) {
        auto accessor = buffer.get_access<sycl::access::mode::read_write>(h);
        h.parallel_for(sycl::range<1>(work_size),
                       [=](sycl::id<1> idx) { accessor[idx] = accessor[idx] * 2 + 1; });
      });
    }

    queue.wait();

  } catch (const std::exception& e) {
    std::cerr << "Warning: Failed to submit GPU work: " << e.what() << std::endl;
  }
}

}  // namespace

class MetricsMultiThreadingTest : public ::testing::Test {
 protected:
  std::vector<sycl::device> sycl_devices;

  void SetUp() override {
    bool metrics_enabled = (utils::GetEnv("ZET_ENABLE_METRICS") == "1");
    ASSERT_TRUE(metrics_enabled) << "ZET_ENABLE_METRICS must be set to 1";

    std::lock_guard<std::mutex> lock(g_setup_mutex);
    if (!g_setup_complete.load()) {
      ASSERT_TRUE(DiscoverDevicesThreadSafe(g_devices)) << "Failed to discover devices";

      if (!g_devices.empty()) {
        g_metric_groups.resize(g_devices.size());
        for (size_t i = 0; i < g_devices.size(); ++i) {
          bool success = DiscoverMetricGroupsThreadSafe(g_devices[i]._handle, g_metric_groups[i]);
          ASSERT_TRUE(success) << "Failed to discover metric groups for device " << i;
        }
      }
      g_setup_complete.store(true);
    }

    sycl_devices = sycl::device::get_devices(sycl::info::device_type::gpu);
    ASSERT_FALSE(sycl_devices.empty()) << "No GPU devices available for testing";
  }

  const sycl::device& GetDevice(size_t thread_id) const {
    return sycl_devices[thread_id % sycl_devices.size()];
  }

  void TearDown() override {
    // Cleanup is handled by global destructors
  }
};

// Test Concurrent Device and Metric Group Discovery
TEST_F(MetricsMultiThreadingTest, ConcurrentDiscoveryOperations) {
  std::vector<std::future<bool>> futures;
  std::atomic<int> device_success{0};
  std::atomic<int> device_failure{0};
  std::atomic<int> group_success{0};
  std::atomic<int> group_failure{0};

  for (int i = 0; i < kNumThreads; ++i) {
    futures.push_back(std::async(std::launch::async, [&]() {
      for (int j = 0; j < kNumIterations; ++j) {
        // Test device discovery
        std::vector<pti_device_properties_t> local_devices;
        if (DiscoverDevicesThreadSafe(local_devices)) {
          device_success++;
          EXPECT_FALSE(local_devices.empty());
          for (const auto& device : local_devices) {
            EXPECT_NE(device._handle, nullptr);
            EXPECT_NE(device._model_name, nullptr);
          }
        } else {
          device_failure++;
        }

        // Test metric group discovery (if devices available)
        if (!g_devices.empty()) {
          std::vector<pti_metrics_group_properties_t> local_groups;
          if (DiscoverMetricGroupsThreadSafe(g_devices[0]._handle, local_groups)) {
            group_success++;
            EXPECT_FALSE(local_groups.empty());
            for (const auto& group : local_groups) {
              EXPECT_NE(group._handle, nullptr);
              EXPECT_NE(group._name, nullptr);
              EXPECT_GT(group._metric_count, 0u);
            }
          } else {
            group_failure++;
          }
        }
      }
      return true;
    }));
  }

  for (auto& future : futures) {
    EXPECT_TRUE(future.get());
  }

  std::cout << "Device discovery - Success: " << device_success.load()
            << ", Failures: " << device_failure.load() << std::endl;
  std::cout << "Group discovery - Success: " << group_success.load()
            << ", Failures: " << group_failure.load() << std::endl;

  EXPECT_GT(device_success.load(), 0);
  EXPECT_GT(group_success.load(), 0);
}

// Test concurrent metric properties retrieval
TEST_F(MetricsMultiThreadingTest, ConcurrentMetricPropertiesRetrieval) {
  ASSERT_FALSE(g_metric_groups.empty()) << "No metric groups available for testing";

  std::vector<std::future<bool>> futures;
  std::atomic<int> success_count{0};
  std::atomic<int> failure_count{0};
  pti_metrics_group_handle_t group_handle = g_metric_groups[0][0]._handle;
  uint32_t metric_count = g_metric_groups[0][0]._metric_count;

  // Launch multiple threads to get metric properties concurrently
  for (int i = 0; i < kNumThreads; ++i) {
    futures.push_back(std::async(std::launch::async, [&]() {
      for (int j = 0; j < kNumIterations; ++j) {
        std::vector<pti_metric_properties_t> metric_properties(metric_count);
        pti_result result = ptiMetricsGetMetricsProperties(group_handle, metric_properties.data());

        if (result == PTI_SUCCESS) {
          success_count++;
          // Verify metric properties are valid
          for (const auto& metric : metric_properties) {
            EXPECT_NE(metric._handle, nullptr);
            EXPECT_NE(metric._name, nullptr);
          }
        } else {
          failure_count++;
        }
      }
      return true;
    }));
  }

  // Wait for all threads to complete
  for (auto& future : futures) {
    EXPECT_TRUE(future.get());
  }

  std::cout << "Metric properties retrieval - Success: " << success_count.load()
            << ", Failures: " << failure_count.load() << std::endl;

  EXPECT_GT(success_count.load(), 0);
}

// Test concurrent ptiMetricsStartCollection calls on different devices
TEST_F(MetricsMultiThreadingTest, ConcurrentStartCollectionDifferentDevices) {
  if (g_devices.size() < 2) {
    GTEST_SKIP() << "Need at least 2 devices for this test";
  }

  ASSERT_FALSE(g_metric_groups.empty()) << "No metric groups available for testing";

  int num_devices_to_test = (std::min)(static_cast<int>(g_devices.size()), kNumThreads);
  std::vector<std::future<bool>> futures;
  std::atomic<int> success_count{0};
  std::atomic<int> start_stop_errors{0};
  std::atomic<int> no_samples_count{0};

  std::vector<pti_metrics_group_handle_t> device_metric_groups(num_devices_to_test);

  for (int i = 0; i < num_devices_to_test; ++i) {
    pti_device_handle_t device_handle = g_devices[i]._handle;

    uint32_t group_count = 0;
    pti_result result = ptiMetricsGetMetricGroups(device_handle, nullptr, &group_count);
    ASSERT_EQ(result, PTI_SUCCESS) << "Failed to get metric group count for device " << i;
    ASSERT_GT(group_count, 0u) << "No metric groups found for device " << i;

    std::vector<pti_metrics_group_properties_t> device_groups(group_count);
    result = ptiMetricsGetMetricGroups(device_handle, device_groups.data(), &group_count);
    ASSERT_EQ(result, PTI_SUCCESS) << "Failed to get metric groups for device " << i;

    pti_metrics_group_handle_t selected_group = nullptr;
    for (uint32_t j = 0; j < group_count; ++j) {
      if (device_groups[j]._type == PTI_METRIC_GROUP_TYPE_TIME_BASED) {
        selected_group = device_groups[j]._handle;
        break;
      }
    }

    if (selected_group == nullptr) {
      selected_group = device_groups[0]._handle;
    }

    device_metric_groups[i] = selected_group;

    std::cout << "Device " << i << " will use metric group: " << device_groups[0]._name
              << std::endl;
  }

  // Configure collection for each device with its own metric group
  for (int i = 0; i < num_devices_to_test; ++i) {
    pti_device_handle_t device_handle = g_devices[i]._handle;
    pti_metrics_group_handle_t group_handle = device_metric_groups[i];

    pti_metrics_group_collection_params_t config_params;
    config_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
    config_params._group_handle = group_handle;
    config_params._sampling_interval = 100000;   // 100μs
    config_params._time_aggr_window = 10000000;  // 10ms

    pti_result config_result = ptiMetricsConfigureCollection(device_handle, &config_params, 1);
    ASSERT_EQ(config_result, PTI_SUCCESS)
        << "Failed to configure device " << i << " with its own metric group";
  }

  // Launch threads to start collection on different devices simultaneously
  for (int i = 0; i < num_devices_to_test; ++i) {
    futures.push_back(std::async(std::launch::async, [&, i]() -> bool {
      const sycl::device& device = GetDevice(i);

      pti_device_handle_t device_handle = g_devices[i]._handle;
      pti_metrics_group_handle_t group_handle = device_metric_groups[i];

      pti_result start_result = ptiMetricsStartCollection(device_handle);
      if (start_result != PTI_SUCCESS) {
        start_stop_errors++;
        std::cout << "Device " << i << " start failed: " << start_result << std::endl;
        return false;
      }

      SubmitMinimalGpuWork(device);

      pti_result stop_result = ptiMetricsStopCollection(device_handle);
      if (stop_result != PTI_SUCCESS) {
        start_stop_errors++;
        std::cout << "Device " << i << " stop failed: " << stop_result << std::endl;
        return false;
      }

      uint32_t sample_count = GetMetricsSampleCount(device_handle, group_handle);
      std::cout << "Device " << i << " collected " << sample_count << " samples" << std::endl;

      if (sample_count == 0) {
        no_samples_count++;
        std::cout << "Device " << i << " collected no samples" << std::endl;
        return false;
      }

      success_count++;
      return true;
    }));
  }

  for (auto& future : futures) {
    future.get();
  }

  std::cout << "Results - Success (with samples): " << success_count.load()
            << ", Start/Stop errors: " << start_stop_errors.load()
            << ", No samples: " << no_samples_count.load() << std::endl;

  EXPECT_GT(success_count.load(), 0) << "At least some devices should collect samples successfully";
  EXPECT_EQ(no_samples_count.load(), 0) << "All devices should collect at least one sample";
}

// Test collection state management across multiple threads
TEST_F(MetricsMultiThreadingTest, ConcurrentCollectionManagement) {
  ASSERT_FALSE(g_devices.empty()) << "No devices available for testing";
  ASSERT_FALSE(g_metric_groups.empty()) << "No metric groups available for testing";

  pti_device_handle_t device_handle = g_devices[0]._handle;
  pti_metrics_group_handle_t group_handle = g_metric_groups[0][0]._handle;

  // Phase 1: Test concurrent configuration
  std::vector<std::future<pti_result>> config_futures;
  for (int i = 0; i < kNumThreads; ++i) {
    config_futures.push_back(std::async(std::launch::async, [&]() {
      pti_metrics_group_collection_params_t config_params;
      config_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
      config_params._group_handle = group_handle;
      config_params._sampling_interval = 100000;
      config_params._time_aggr_window = 10000000;
      return ptiMetricsConfigureCollection(device_handle, &config_params, 1);
    }));
  }

  int config_success = 0;
  for (auto& future : config_futures) {
    if (future.get() == PTI_SUCCESS) config_success++;
  }
  EXPECT_GE(config_success, 1);

  // Phase 2: Test concurrent start collection
  std::atomic<int> start_success{0};
  std::atomic<int> already_enabled{0};
  std::atomic<int> other_errors{0};
  std::atomic<int> collections_with_samples{0};
  std::atomic<int> attempted_count{0};
  std::vector<std::future<pti_result>> start_futures;

  for (int i = 0; i < kNumThreads; ++i) {
    start_futures.push_back(std::async(std::launch::async, [&, i]() {
      const sycl::device& device = GetDevice(i);
      pti_result result = ptiMetricsStartCollection(device_handle);
      attempted_count++;

      if (result == PTI_SUCCESS) {
        start_success++;

        SubmitMinimalGpuWork(device);

        while (attempted_count.load() < kNumThreads) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        pti_result stop_result = ptiMetricsStopCollection(device_handle);
        if (stop_result == PTI_SUCCESS) {
          uint32_t sample_count = GetMetricsSampleCount(device_handle, group_handle);
          std::cout << "Collected samples: " << sample_count << std::endl;
          if (sample_count > 0) {
            collections_with_samples++;
          }
        }
      } else if (result == PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED) {
        already_enabled++;
      } else {
        other_errors++;
      }

      return result;
    }));
  }

  for (auto& future : start_futures) {
    future.get();
  }
  // Phase 3: Test state management (pause/resume)
  std::atomic<bool> pause_success{false};
  std::atomic<bool> resume_success{false};
  std::atomic<int> pause_resume_samples{0};
  std::vector<std::future<void>> state_futures;

  state_futures.push_back(std::async(std::launch::async, [&]() {
    const sycl::device& device = GetDevice(0);
    pti_result start_result = ptiMetricsStartCollection(device_handle);
    if (start_result != PTI_SUCCESS) {
      return;
    }
    SubmitMinimalGpuWork(device);

    if (ptiMetricsPauseCollection(device_handle) == PTI_SUCCESS) {
      pause_success.store(true);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      if (ptiMetricsResumeCollection(device_handle) == PTI_SUCCESS) {
        resume_success.store(true);

        SubmitMinimalGpuWork(device);

        pti_result stop_result = ptiMetricsStopCollection(device_handle);
        if (stop_result == PTI_SUCCESS) {
          uint32_t sample_count = GetMetricsSampleCount(device_handle, group_handle);
          std::cout << "Pause/Resume collected samples: " << sample_count << std::endl;
          if (sample_count > 0) {
            pause_resume_samples++;
          }
        }
      }
    }
  }));

  for (auto& future : state_futures) {
    future.wait();
  }

  // Results
  std::cout << "Config successes: " << config_success << std::endl;
  std::cout << "Start - Success: " << start_success.load()
            << ", Already enabled: " << already_enabled.load()
            << ", Other errors: " << other_errors.load() << std::endl;
  std::cout << "Collections with samples: " << collections_with_samples.load() << std::endl;
  std::cout << "Pause/Resume - Pause success: " << pause_success.load()
            << ", Resume success: " << resume_success.load()
            << ", Samples after pause/resume: " << pause_resume_samples.load() << std::endl;

  EXPECT_EQ(start_success.load(), 1) << "Exactly one thread should start collection successfully";
  EXPECT_GT(collections_with_samples.load(), 0) << "At least one collection should gather samples";

  // Pause/resume assertions (only if pause/resume succeeded)
  if (pause_success.load() && resume_success.load()) {
    EXPECT_GT(pause_resume_samples.load(), 0) << "Pause/resume cycle should collect samples";
  }
}

// Test thread safety with invalid parameters
TEST_F(MetricsMultiThreadingTest, ConcurrentErrorHandling) {
  std::vector<std::future<void>> futures;
  std::atomic<int> expected_failures{0};
  std::atomic<int> total_operations{0};

  constexpr int iterations_per_thread = 50;

  for (int i = 0; i < kNumThreads; ++i) {
    futures.push_back(std::async(std::launch::async, [&]() {
      for (int iter = 0; iter < iterations_per_thread; ++iter) {
        int operation = iter % 6;

        switch (operation) {
          case 0:
          case 1: {
            std::vector<pti_device_properties_t> devices;
            DiscoverDevicesThreadSafe(devices);
            if (!g_devices.empty()) {
              std::vector<pti_metrics_group_properties_t> groups;
              DiscoverMetricGroupsThreadSafe(g_devices[0]._handle, groups);
            }
            break;
          }
          case 2: {
            pti_result r1 = ptiMetricsGetDevices(nullptr, nullptr);
            pti_result r2 = ptiMetricsGetMetricGroups(nullptr, nullptr, nullptr);
            if (r1 == PTI_ERROR_BAD_ARGUMENT && r2 == PTI_ERROR_BAD_ARGUMENT) {
              expected_failures += 2;
            }
            break;
          }
          case 3: {
            pti_result r1 = ptiMetricsGetMetricsProperties(nullptr, nullptr);
            pti_result r2 = ptiMetricsConfigureCollection(nullptr, nullptr, 0);
            if (r1 == PTI_ERROR_BAD_ARGUMENT && r2 == PTI_ERROR_BAD_ARGUMENT) {
              expected_failures += 2;
            }
            break;
          }
          case 4: {
            pti_result r = ptiMetricsGetCalculatedData(nullptr, nullptr, nullptr, nullptr);
            if (r == PTI_ERROR_BAD_ARGUMENT) {
              expected_failures++;
            }
            break;
          }
          case 5: {
            pti_result r1 = ptiMetricsGetDevices(nullptr, nullptr);
            pti_result r2 = ptiMetricsGetMetricGroups(nullptr, nullptr, nullptr);
            pti_result r3 = ptiMetricsStartCollection(nullptr);
            pti_result r4 = ptiMetricsStopCollection(nullptr);

            if (r1 == PTI_ERROR_BAD_ARGUMENT && r2 == PTI_ERROR_BAD_ARGUMENT &&
                r3 == PTI_ERROR_BAD_ARGUMENT && r4 == PTI_ERROR_BAD_ARGUMENT) {
              expected_failures += 4;
            }
            break;
          }
        }

        total_operations++;
      }
    }));
  }

  for (auto& future : futures) {
    future.wait();
  }
  int expected_total_ops = kNumThreads * iterations_per_thread;
  int expected_failures_count =
      kNumThreads * ((iterations_per_thread / 6) * 9);  // 9 failures per 6-iteration cycle

  std::cout << "Results:" << std::endl;
  std::cout << "  Total operations: " << total_operations.load()
            << " (expected: " << expected_total_ops << ")" << std::endl;
  std::cout << "  Expected failures: " << expected_failures.load() << " (expected: ~"
            << expected_failures_count << ")" << std::endl;

  EXPECT_EQ(total_operations.load(), expected_total_ops);
  EXPECT_GT(expected_failures.load(), 0);
}
