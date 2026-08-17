//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
// Tests for on-demand metric enable/disable functionality
//==============================================================

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <vector>

#include "pti/pti_metrics.h"
#include "utils.h"

namespace {

class MetricEnableDisableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Every test here exercises the on-demand enable/disable path, which is bypassed entirely
    // when the loader latched ZET_ENABLE_METRICS=1 at zeInit(). Skipping from SetUp() skips the
    // test body as well, so each test does not need to repeat this check.
    if (utils::GetEnv("ZET_ENABLE_METRICS") == "1") {
      GTEST_SKIP() << "ZET_ENABLE_METRICS=1 is set, skipping on-demand enable/disable test";
    }

    // Initialize Level Zero
    ze_result_t status = zeInit(0);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS) << "Failed to initialize Level Zero";
  }

  void TearDown() override {
    // Best effort cleanup: disable metrics multiple times to reset reference count
    for (int i = 0; i < 10; i++) {
      pti_result status = ptiMetricsDisable(nullptr);
      if (status != PTI_SUCCESS) {
        break;
      }
    }
  }
};

// Skips the calling test when the driver does not implement zetDeviceEnableMetricsExp. A macro
// rather than a helper function because GTEST_SKIP() only returns from the function it appears
// in -- calling it inside a helper would not stop the rest of the test body from running.
#define SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status)                                        \
  do {                                                                                    \
    if ((status) == PTI_ERROR_METRICS_RUNTIME_ENABLE_UNSUPPORTED) {                       \
      GTEST_SKIP() << "zetDeviceEnableMetricsExp not supported by driver, skipping test"; \
    }                                                                                     \
  } while (0)

// Test: Single enable and disable
TEST_F(MetricEnableDisableTest, SingleEnableDisable) {
  // Enable metrics
  pti_result status = ptiMetricsEnable(nullptr);
  SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Disable metrics
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

// Test: Multiple enable calls (reference counting)
TEST_F(MetricEnableDisableTest, MultipleEnableCalls) {
  // Enable metrics 2 times
  pti_result status = ptiMetricsEnable(nullptr);
  SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status);
  EXPECT_EQ(status, PTI_SUCCESS);

  status = ptiMetricsEnable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Disable once - should succeed (ref count = 1)
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Disable again - should succeed (ref count = 0)
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

// Test: 1 enable but 2 disable calls (underflow protection)
TEST_F(MetricEnableDisableTest, OneEnableTwoDisable) {
  // Enable metrics once
  pti_result status = ptiMetricsEnable(nullptr);
  SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Disable first time - should succeed
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Disable second time - should succeed (underflow protection)
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

// Test: Disable without enable (should be safe)
TEST_F(MetricEnableDisableTest, DisableWithoutEnable) {
  // Disable without enable - should be safe
  pti_result status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

// Test: Enable -> ptiMetricsGetDevices -> Disable
TEST_F(MetricEnableDisableTest, EnableGetDevicesDisable) {
  // Enable metrics
  pti_result status = ptiMetricsEnable(nullptr);
  SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Call ptiMetricsGetDevices
  uint32_t device_count = 0;
  pti_result pti_status = ptiMetricsGetDevices(nullptr, &device_count);
  EXPECT_EQ(pti_status, PTI_SUCCESS);
  EXPECT_GT(device_count, 0u);

  std::vector<pti_device_properties_t> devices(device_count);
  pti_status = ptiMetricsGetDevices(devices.data(), &device_count);
  EXPECT_EQ(pti_status, PTI_SUCCESS);
  EXPECT_FALSE(devices.empty());

  // Disable metrics
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

// Test: ptiMetricsGetDevices without metric enable should fail, then succeed after enable
TEST_F(MetricEnableDisableTest, GetDevicesWithoutMetricEnable) {
  // Call ptiMetricsGetDevices without enabling metrics - should fail
  uint32_t device_count = 0;
  pti_result pti_status = ptiMetricsGetDevices(nullptr, &device_count);
  EXPECT_NE(pti_status, PTI_SUCCESS) << "ptiMetricsGetDevices should fail without metrics enabled";

  // Now enable metrics
  pti_result status = ptiMetricsEnable(nullptr);
  SKIP_IF_RUNTIME_ENABLE_UNSUPPORTED(status);
  EXPECT_EQ(status, PTI_SUCCESS);

  // Call ptiMetricsGetDevices again - should now succeed
  device_count = 0;
  pti_status = ptiMetricsGetDevices(nullptr, &device_count);
  EXPECT_EQ(pti_status, PTI_SUCCESS);
  EXPECT_GT(device_count, 0u);

  std::vector<pti_device_properties_t> devices(device_count);
  pti_status = ptiMetricsGetDevices(devices.data(), &device_count);
  EXPECT_EQ(pti_status, PTI_SUCCESS);
  EXPECT_FALSE(devices.empty());

  // Cleanup: disable metrics
  status = ptiMetricsDisable(nullptr);
  EXPECT_EQ(status, PTI_SUCCESS);
}

}  // namespace
