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

#include "metrics_utils.h"
#include "pti/pti_metrics.h"
#include "utils.h"

namespace {

enum class TestType {
  kRunAll = 0,
};

constexpr size_t kVectorSize = 5000;

std::vector<pti_device_properties_t> devices;
std::vector<pti_metrics_group_properties_t> metric_groups;

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
template <typename T>
void VecSq(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector) {
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
}

template <typename T>
void VecPassThroughToVecSq(sycl::queue &q, const std::vector<T> &a_vector,
                           const std::vector<T> &b_vector) {
  // This external id(21) is ignored due to overriding push in the VecSq call it preceeds.
  VecSq(q, a_vector, b_vector);
}

//************************************
// Vector add in SYCL on device: returns sum in 4th parameter "sq_add".
//************************************
template <typename T>
void VecAdd(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector,
            std::vector<T> &sq_add) {
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

template <typename T>
void PrintResults(const std::vector<T> &sq_add, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += sq_add[i];
  printf("final result: %f\n", sum / n);
}

template <typename T>
void RunExternalTest(sycl::queue &q, const std::vector<T> &a, const std::vector<T> &b,
                     const std::vector<T> &c, const std::vector<T> &d, std::vector<T> &sq_add,
                     std::vector<T> &sq_add2) {
  VecPassThroughToVecSq(q, a, b);
  VecAdd(q, a, b, sq_add);
  PrintResults(sq_add, kVectorSize);

  VecAdd(q, a, b, sq_add);
  PrintResults(sq_add, kVectorSize);

  VecAdd(q, c, d, sq_add2);
  PrintResults(sq_add2, 2 * kVectorSize);
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

  if (a_test_type == TestType::kRunAll) {
    RunExternalTest(sycl_queue, a, b, c, d, sq_add, sq_add2);
  }
}

void RunVecsqadd(TestType a_test_type) {
  auto dev = sycl::device(sycl::gpu_selector_v);

  auto d_selector{sycl::gpu_selector_v};
  sycl::property_list prop{sycl::property::queue::in_order()};
  sycl::queue q(d_selector, prop);

  if (q.get_device().has(sycl::aspect::fp64)) {
    VecSqAddRouter<double>(q, a_test_type);
  } else {
    VecSqAddRouter<float>(q, a_test_type);
  }
}

}  // namespace

class VecsqaddMetricsFixtureTest : public ::testing::Test {
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
      // just get groups for the first device
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

    // Initialize metrics utils instance
    if (!metrics_utils::MetricsProfiler::MetricsProfilerInstance().InitializeMetricsCollection()) {
      std::cout << "Failed to get initialize metrics collection" << std::endl;
    }

    // Configure collection
    std::string group_name = "ComputeBasic";
    pti_metrics_group_type group_type = PTI_METRIC_GROUP_TYPE_TIME_BASED;
    bool log_data = true;
    std::string filename = group_name + "MetricsFixtureTest_LIB.json";

    // test configure metric groups
    if (!metrics_utils::MetricsProfiler::MetricsProfilerInstance().ConfigureMetricGroups(
            group_name, group_type, log_data, std::move(filename))) {
      std::cout << "Failed to configure metrics collection" << std::endl;
    }
  }

  void TearDown() override {
    // Called right before destructor after each test
    devices.clear();
    metric_groups.clear();
  }
};

TEST_F(VecsqaddMetricsFixtureTest, MetricsEnabled) {
  bool metrics_enabled = (utils::GetEnv("ZET_ENABLE_METRICS") == "1");
  EXPECT_EQ(metrics_enabled, true);
}

TEST_F(VecsqaddMetricsFixtureTest, GetDevices) {
  uint32_t device_count = 0;
  EXPECT_EQ(ptiMetricsGetDevices(nullptr, &device_count), PTI_SUCCESS);
  EXPECT_NE(device_count, static_cast<uint32_t>(0));

  devices.resize(device_count);
  EXPECT_EQ(ptiMetricsGetDevices(devices.data(), &device_count), PTI_SUCCESS);
  EXPECT_NE(devices.empty(), true);
}

TEST_F(VecsqaddMetricsFixtureTest, GetMetricGroups) {
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

TEST_F(VecsqaddMetricsFixtureTest, GetMetricGroupsNullDeviceHandle) {
  uint32_t group_count = 0;
  EXPECT_EQ(ptiMetricsGetMetricGroups(nullptr, nullptr, &group_count), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, GetMetricGroupsInvalidDeviceHandle) {
  uint32_t group_count = 0;
  pti_device_handle_t device_handle =
      metric_groups.size() > 0 ? metric_groups[0]._handle
                               : reinterpret_cast<pti_device_handle_t>(
                                     0xDEADBEEF);  // set to a metric group handle instead of device
                                                   // handle of random value
  EXPECT_EQ(ptiMetricsGetMetricGroups(device_handle, nullptr, &group_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, GetMetrics) {
  for (size_t i = 0; i < devices.size(); i++) {
    for (size_t j = 0; j < metric_groups.size(); j++) {
      uint32_t metric_count = metric_groups[j]._metric_count;
      EXPECT_NE(metric_count, static_cast<uint32_t>(0));

      std::vector<pti_metric_properties_t> metric_properties(metric_count);
      metric_groups[j]._metric_properties = metric_properties.data();

      EXPECT_EQ(ptiMetricsGetMetricsProperties(metric_groups[j]._handle,
                                               metric_groups[j]._metric_properties),
                PTI_SUCCESS);
      EXPECT_NE(metric_groups[j]._metric_properties, nullptr);
    }
  }
}

TEST_F(VecsqaddMetricsFixtureTest, GetMetricsNullMetricGroupHandle) {
  for (size_t i = 0; i < devices.size(); i++) {
    for (size_t j = 0; j < metric_groups.size(); j++) {
      uint32_t metric_count = metric_groups[j]._metric_count;
      EXPECT_NE(metric_count, static_cast<uint32_t>(0));

      std::vector<pti_metric_properties_t> metric_properties(metric_count);
      metric_groups[j]._metric_properties = metric_properties.data();

      EXPECT_EQ(ptiMetricsGetMetricsProperties(nullptr, metric_groups[j]._metric_properties),
                PTI_ERROR_BAD_ARGUMENT);
    }
  }
}

TEST_F(VecsqaddMetricsFixtureTest, GetMetricsInvalidMetricGroupHandle) {
  // test the first device and the first metric group in that device
  uint32_t device_idx = 0;
  uint32_t metric_group_idx = 0;
  if (devices.size() > 0) {
    if (metric_groups.size() > 0) {
      uint32_t metric_count = metric_groups[metric_group_idx]._metric_count;
      EXPECT_NE(metric_count, static_cast<uint32_t>(0));

      std::vector<pti_metric_properties_t> metric_properties(metric_count);
      metric_groups[metric_group_idx]._metric_properties = metric_properties.data();

      // user device handle instead of metric group handle
      EXPECT_EQ(ptiMetricsGetMetricsProperties(devices[device_idx]._handle,
                                               metric_groups[metric_group_idx]._metric_properties),
                PTI_ERROR_BAD_ARGUMENT);
      EXPECT_NE(metric_groups[metric_group_idx]._metric_properties, nullptr);
    }
  }
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigureNullMetricGroupCollectionParameters) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;

  // test null metric groups collection parameters
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, nullptr, 1), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigureEmptyMetricGroup) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;

  // test empty metric groups collection parameters
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer;
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, config_collection_params_buffer.data(), 1),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigurNullMetricGroupHandle) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;

  // test null metric group handle
  pti_metrics_group_collection_params_t config_collection_params;
  config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
  config_collection_params._group_handle = nullptr;
  config_collection_params._sampling_interval = 100000;   // ns
  config_collection_params._time_aggr_window = 10000000;  // ns
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {
      config_collection_params};
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, config_collection_params_buffer.data(), 1),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigureInvalidMetricGroupHandle) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;

  // test invalid metric group handle
  pti_metrics_group_collection_params_t config_collection_params;
  config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
  config_collection_params._group_handle = device_handle;  // use the device handle;
  config_collection_params._sampling_interval = 100000;    // ns
  config_collection_params._time_aggr_window = 10000000;   // ns
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {
      config_collection_params};
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, config_collection_params_buffer.data(), 1),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, Configure0MetricGroups) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;
  pti_metrics_group_handle_t metric_group_handle =
      metric_groups.size() > 0 ? metric_groups[0]._handle : nullptr;

  // test 0 metric groups
  pti_metrics_group_collection_params_t config_collection_params;
  config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
  config_collection_params._group_handle = metric_group_handle;  // valid metric group
  config_collection_params._sampling_interval = 100000;          // ns
  config_collection_params._time_aggr_window = 10000000;         // ns
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {
      config_collection_params};
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, config_collection_params_buffer.data(), 0),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, Configure2MetricGroups) {
  pti_device_handle_t device_handle = devices.size() > 0 ? devices[0]._handle : nullptr;
  pti_metrics_group_handle_t metric_group_handle =
      metric_groups.size() > 0 ? metric_groups[0]._handle : nullptr;

  // test more than 1 metric groups
  pti_metrics_group_collection_params_t config_collection_params;
  config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
  config_collection_params._group_handle = metric_group_handle;  // valid metric group
  config_collection_params._sampling_interval = 100000;          // ns
  config_collection_params._time_aggr_window = 10000000;         // ns

  // Add first one
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {
      config_collection_params};

  // Add second one
  pti_metrics_group_handle_t metric_group_handle_2 =
      metric_groups.size() > 1 ? metric_groups[1]._handle : nullptr;
  config_collection_params._group_handle = metric_group_handle_2;  // valid metric group 2
  config_collection_params_buffer.push_back(config_collection_params);

  // Test 2 metric groups collection parameters in the buffer
  EXPECT_EQ(ptiMetricsConfigureCollection(device_handle, config_collection_params_buffer.data(), 2),
            PTI_ERROR_NOT_IMPLEMENTED);
}

TEST_F(VecsqaddMetricsFixtureTest, InitializeMetricsCollection) {
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().InitializeMetricsCollection(),
            true);
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigureGoodMetricGroup) {
  std::string group_name = "ComputeBasic";
  pti_metrics_group_type group_type = PTI_METRIC_GROUP_TYPE_TIME_BASED;
  bool log_data = true;
  std::string filename = group_name + "MetricsFixtureTest_LIB.json";

  // test configure metric groups
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().ConfigureMetricGroups(
                group_name, group_type, log_data, filename),
            true);

  // test multiple configure metric groups calls
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().ConfigureMetricGroups(
                group_name, group_type, log_data, std::move(filename)),
            true);
}

TEST_F(VecsqaddMetricsFixtureTest, ConfigureAlreadyConfiguredMetricGroup) {
  std::string group_name = "ComputeBasic";
  pti_metrics_group_type group_type = PTI_METRIC_GROUP_TYPE_TIME_BASED;
  bool log_data = true;
  std::string filename = group_name + "MetricsFixtureTest_LIB.json";

  // test multiple configure metric groups calls
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().ConfigureMetricGroups(
                group_name, group_type, log_data, std::move(filename)),
            true);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionAndStop) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionNullDevice) {
  // start collection with null device
  EXPECT_EQ(ptiMetricsStartCollection(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionInvalidDevice) {
  // use metric group handle instead of device handle
  pti_device_handle_t device = static_cast<pti_device_handle_t>(
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle());

  // start collection with invalid device
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StartAlreadyStartedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // test start already started collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StartPausedAlreadyStartedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // test start paused already running collection
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionPausedAndStop) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionPausedNullDevice) {
  // start collection paused with null device
  EXPECT_EQ(ptiMetricsStartCollectionPaused(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionPausedInvalidDevice) {
  // use metric group handle instead of device handle
  pti_device_handle_t device = static_cast<pti_device_handle_t>(
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle());

  // start collection paused with invalid device
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StartAlreadySartedInPauseModeCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // test start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StartInPausedModeAlreadySartedInPausedModeCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // test start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, ResumeAlreadyRunningCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // test resume already running collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, ResumeNotStartedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // test resume not started collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED);
}

TEST_F(VecsqaddMetricsFixtureTest, ResumeCollectionNullDevice) {
  // Resume collection with null device
  EXPECT_EQ(ptiMetricsResumeCollection(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, ResumeCollectionInvalidDevice) {
  // use metric group handle instead of device handle
  pti_device_handle_t device = static_cast<pti_device_handle_t>(
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle());

  // resume collection with invalid device
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, ResumePausedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // test resume already started collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_SUCCESS);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, StopNotStartedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // test stop not started collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED);
}

TEST_F(VecsqaddMetricsFixtureTest, StopCollectionNullDevice) {
  // stop collection with null device
  EXPECT_EQ(ptiMetricsStopCollection(nullptr), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StopCollectionInvalidDevice) {
  // use metric group handle instead of device handle
  pti_device_handle_t device = static_cast<pti_device_handle_t>(
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle());

  // stop collection with invalid device
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, StopAlreadyStoppedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);

  // stop already stopped collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED);
}

TEST_F(VecsqaddMetricsFixtureTest, StopPausedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // stop paused collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateNotStartedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();

  // test calculate on not started collection
  uint32_t metrics_values_count;

  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, nullptr, &metrics_values_count),
            PTI_ERROR_METRICS_NO_DATA_COLLECTED);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateWith0BufferSize) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();

  // test calculate with 0 buffer size
  uint32_t metrics_values_count = 0;
  pti_value_t dummy = pti_value_t();
  std::vector<pti_value_t> metrics_values_buffer = {dummy};

  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, metrics_values_buffer.data(),
                                       &metrics_values_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateWithNullDevice) {
  // test calculate with null device
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();

  EXPECT_EQ(ptiMetricGetCalculatedData(nullptr, group, nullptr, &metrics_values_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateWithInvalidDevice) {
  // test calculate with invalid device, use group handle instead of device handle
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();

  EXPECT_EQ(ptiMetricGetCalculatedData(static_cast<pti_device_handle_t>(group), group, nullptr,
                                       &metrics_values_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateWithNullMetricGroup) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // test calculate on not started collection
  uint32_t metrics_values_count;
  EXPECT_EQ(ptiMetricGetCalculatedData(device, nullptr, nullptr, &metrics_values_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateWithInvalidMetricGroup) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // test calculate with invalid metric group, use device handle instad of metric group handle
  uint32_t metrics_values_count;
  EXPECT_EQ(ptiMetricGetCalculatedData(device, static_cast<pti_metrics_group_handle_t>(device),
                                       nullptr, &metrics_values_count),
            PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateEnabledCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection
  EXPECT_EQ(ptiMetricsStartCollection(device), PTI_SUCCESS);

  // Test calculate on enabled collection
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();
  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, nullptr, &metrics_values_count),
            PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculatePausedCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection paused
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // Test calculate on paused collection
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();
  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, nullptr, &metrics_values_count),
            PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED);
}

TEST_F(VecsqaddMetricsFixtureTest, CalculateEmptyCollection) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // stop paused collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);

  // calculate on stopped collection before any data is collected
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();
  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, nullptr, &metrics_values_count),
            PTI_ERROR_METRICS_NO_DATA_COLLECTED);

  // test empty calculate on no data collected
  EXPECT_EQ(metrics_values_count, static_cast<uint32_t>(0));
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionPausedAndResumeAndCalculateBeforeStop) {
  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // test resume collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_SUCCESS);

  // test calculate with no stop
  uint32_t metrics_values_count;
  pti_metrics_group_handle_t group =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredMetricGroupHandle();
  EXPECT_EQ(ptiMetricGetCalculatedData(device, group, nullptr, &metrics_values_count),
            PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED);
}

TEST_F(VecsqaddMetricsFixtureTest, StartCollectionPausedAndResumeAndStopAndCalculate) {
  std::string group_name = "ComputeBasic";
  pti_metrics_group_type group_type = PTI_METRIC_GROUP_TYPE_TIME_BASED;
  bool log_data = true;
  std::string filename = group_name + "MetricsFixtureTest_LIB.json";

  // test configure metric groups
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().ConfigureMetricGroups(
                group_name, group_type, log_data, std::move(filename)),
            true);

  pti_device_handle_t device =
      metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetConfiguredDeviceHandle();

  // start collection in paused mode
  EXPECT_EQ(ptiMetricsStartCollectionPaused(device), PTI_SUCCESS);

  // test start in paused mode already started collection
  EXPECT_EQ(ptiMetricsResumeCollection(device), PTI_SUCCESS);

  // run workload
  RunVecsqadd(TestType::kRunAll);

  // stop collection
  EXPECT_EQ(ptiMetricsStopCollection(device), PTI_SUCCESS);

  std::string sample_filename = group_name + "MetricsFixtureTest_TEST.json";
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().GetCalculatedData(
                log_data, std::move(sample_filename)),
            true);

  // Add small sleep to wait for the output files to be written
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Compare LIB generated file vs TEST generated file
  std::string file1 = group_name + "MetricsFixtureTest_LIB.json";
  std::string file2 = group_name + "MetricsFixtureTest_TEST.json";

  EXPECT_EQ(metrics_utils::CompareFiles(file1, file2), true);

  metrics_utils::DeleteFile(file1);
  metrics_utils::DeleteFile(file2);

  // Validate the data
  EXPECT_EQ(metrics_utils::MetricsProfiler::MetricsProfilerInstance().ValidateData(), true);
}
