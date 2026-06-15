//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Internal-facing functional tests for PC Sampling.
// Tests cover:
//   - Validation that currently relies on internal handle state
//   - Comparisons against internal metric-group helpers
//   - White-box behavior not exposed through the public API alone
//
// =============================================================

#include "pc_sampling/pti_pc_sampling_internal.h"
#include "pc_sampling_test_fixture.h"

namespace {

std::vector<std::pair<std::string, std::string>> GetExpectedStallReasonsFromL0(
    pti_pc_sampling_handle_t handle) {
  EXPECT_NE(handle, nullptr);
  EXPECT_FALSE(handle->supported_devices_.empty());

  constexpr char kPcSamplingMetricGroupName[] = "EuStallSampling";
  pti_device_handle_t device_handle = handle->supported_devices_.front();

  EXPECT_NE(device_handle, nullptr);

  const auto device = reinterpret_cast<ze_device_handle_t>(device_handle);
  uint32_t group_count = 0;
  if (zetMetricGroupGet(device, &group_count, nullptr) != ZE_RESULT_SUCCESS || group_count == 0) {
    return {};
  }

  std::vector<zet_metric_group_handle_t> groups(group_count);
  if (zetMetricGroupGet(device, &group_count, groups.data()) != ZE_RESULT_SUCCESS) {
    return {};
  }

  for (const auto group : groups) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    if (zetMetricGroupGetProperties(group, &group_props) != ZE_RESULT_SUCCESS) {
      continue;
    }

    if (std::string(group_props.name) != kPcSamplingMetricGroupName ||
        (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) == 0) {
      continue;
    }

    uint32_t metric_count = 0;
    if (zetMetricGet(group, &metric_count, nullptr) != ZE_RESULT_SUCCESS || metric_count == 0) {
      return {};
    }

    std::vector<zet_metric_handle_t> metrics(metric_count);
    if (zetMetricGet(group, &metric_count, metrics.data()) != ZE_RESULT_SUCCESS) {
      return {};
    }

    std::vector<std::pair<std::string, std::string>> metric_names;
    metric_names.reserve(metric_count);
    for (const auto metric : metrics) {
      zet_metric_properties_t metric_props{};
      metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
      if (zetMetricGetProperties(metric, &metric_props) != ZE_RESULT_SUCCESS) {
        continue;
      }

      if (metric_props.metricType == ZET_METRIC_TYPE_EVENT) {
        metric_names.emplace_back(metric_props.name, metric_props.description);
      }
    }

    return metric_names;
  }

  return {};
}

TEST_F(PcSamplingTest, RejectsDeviceFilteredConfigurationForUnsupportedDevices) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device[1] = {reinterpret_cast<pti_device_handle_t>(0x1)};
  EXPECT_EQ(ptiPcSamplingConfigure(handle, configured_device, 1, 0),
            PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED);

  handle->configured_devices_.push_back(configured_device[0]);
  EXPECT_TRUE(pti::pc_sampling::IsConfiguredDevice(handle, configured_device[0]));

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Sampling Period Configuration Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, ConfigureUsesDefaultSamplingPeriodWhenZero) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, 0), PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns_, pti::pc_sampling::kDefaultSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, ConfigurePreservesExplicitSamplingPeriod) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  constexpr uint32_t kExplicitSamplingPeriodNs = 42'000;
  size_t device_count = devices_.size() > 0 ? 1 : 0;
  pti_device_handle_t device_handle[1] = {
      (devices_.size() > 0) ? reinterpret_cast<pti_device_handle_t>(devices_[0]) : nullptr};
  ASSERT_EQ(ptiPcSamplingConfigure(handle, device_handle, device_count, kExplicitSamplingPeriodNs),
            PTI_SUCCESS);
  EXPECT_EQ(handle->sampling_period_ns_, kExplicitSamplingPeriodNs);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

//-----------------------------------------------------------------------------
// Kernel Info Query Tests
//-----------------------------------------------------------------------------

TEST_F(PcSamplingTest, GetObservedKernelInfoReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;
  handle->configured_devices_.push_back(configured_device);

  pti_pc_sampling_kernel_info_t kernel_info{};
  kernel_info._struct_size = sizeof(kernel_info);

  EXPECT_EQ(ptiPcSamplingGetObservedKernelInfo(handle, configured_device, 1, &kernel_info),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetSamplesPerInstructionReturnsNotImplementedAfterValidation) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;
  handle->configured_devices_.push_back(configured_device);

  EXPECT_EQ(
      ptiPcSamplingGetSamplesPerInstruction(handle, configured_device, 1, nullptr, 0, nullptr, 0),
      PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetStallReasons) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  const auto expected_reasons = GetExpectedStallReasonsFromL0(handle);
  size_t reason_count = 0;
  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, nullptr, &reason_count), PTI_SUCCESS);
  ASSERT_EQ(reason_count, expected_reasons.size());
  ASSERT_FALSE(expected_reasons.empty());

  std::vector<pti_pc_sampling_stall_reason_info_t> reasons(reason_count);
  for (auto& reason : reasons) {
    reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }

  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, reasons.data(), &reason_count), PTI_SUCCESS);
  ASSERT_EQ(reason_count, expected_reasons.size());

  for (size_t index = 0; index < expected_reasons.size(); ++index) {
    EXPECT_STREQ(reasons[index]._name, expected_reasons[index].first.c_str());
    EXPECT_STREQ(reasons[index]._description, expected_reasons[index].second.c_str());
  }

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetStallReasonsSupportsPartialCallerBuffer) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  const auto expected_reasons = GetExpectedStallReasonsFromL0(handle);
  ASSERT_FALSE(expected_reasons.empty());
  size_t reason_count = 1;
  pti_pc_sampling_stall_reason_info_t reason = {};
  reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);

  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, &reason, &reason_count), PTI_SUCCESS);
  EXPECT_EQ(reason_count, 1u);
  EXPECT_STREQ(reason._name, expected_reasons[0].first.c_str());
  EXPECT_STREQ(reason._description, expected_reasons[0].second.c_str());

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetStallReasonsNotReturnTooMuchData) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  const auto expected_reasons = GetExpectedStallReasonsFromL0(handle);
  const size_t expected_reason_count = expected_reasons.size();
  ASSERT_GT(expected_reason_count, 0u);

  size_t reason_count = expected_reason_count + 1;
  std::vector<pti_pc_sampling_stall_reason_info_t> reasons(reason_count);
  for (auto& reason : reasons) {
    reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }

  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, reasons.data(), &reason_count), PTI_SUCCESS);
  EXPECT_EQ(reason_count, expected_reason_count);
  EXPECT_EQ(reasons[0]._name, nullptr);
  EXPECT_EQ(reasons[0]._description, nullptr);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetStallReasonsMultipleCallsReturnSamePointers) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  const auto expected_reasons = GetExpectedStallReasonsFromL0(handle);
  ASSERT_FALSE(expected_reasons.empty());

  size_t reason_count = expected_reasons.size();
  std::vector<pti_pc_sampling_stall_reason_info_t> reasons(reason_count);
  for (auto& reason : reasons) {
    reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }

  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, reasons.data(), &reason_count), PTI_SUCCESS);
  EXPECT_EQ(reason_count, expected_reasons.size());

  size_t reason_count2 = expected_reasons.size();
  std::vector<pti_pc_sampling_stall_reason_info_t> reasons2(reason_count2);
  for (auto& reason : reasons2) {
    reason._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }

  ASSERT_EQ(ptiPcSamplingGetStallReasons(handle, reasons2.data(), &reason_count2), PTI_SUCCESS);
  EXPECT_EQ(reason_count2, expected_reasons.size());

  EXPECT_EQ(reasons[0]._name, reasons2[0]._name);
  EXPECT_EQ(reasons[0]._description, reasons2[0]._description);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

TEST_F(PcSamplingTest, GetProfiledDevicesReturnsConfiguredSingleDevice) {
  pti_pc_sampling_handle_t handle = nullptr;
  ASSERT_EQ(ptiPcSamplingEnable(&handle), PTI_SUCCESS);

  pti_device_handle_t configured_device = reinterpret_cast<pti_device_handle_t>(0x1);
  handle->configured_devices_.push_back(configured_device);
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;

  size_t device_count = 0;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, nullptr, &device_count), PTI_SUCCESS);
  ASSERT_EQ(device_count, 1u);

  pti_device_handle_t profiled_device = nullptr;
  ASSERT_EQ(ptiPcSamplingGetProfiledDevices(handle, &profiled_device, &device_count), PTI_SUCCESS);
  EXPECT_EQ(device_count, 1u);
  EXPECT_EQ(profiled_device, configured_device);

  EXPECT_EQ(ptiPcSamplingDisable(handle), PTI_SUCCESS);
}

}  // namespace
