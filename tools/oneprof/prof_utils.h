//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_PROF_UTILS_H_
#define PTI_TOOLS_ONEPROF_PROF_UTILS_H_

#include "ze_utils.h"

inline ze_driver_handle_t GetDriver(uint32_t device_id) {
  uint32_t id = 0;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      if (id == device_id) {
        return driver;
      }
    }
  }
  return nullptr;
}

inline ze_device_handle_t GetDevice(uint32_t device_id) {
  uint32_t id = 0;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      if (id == device_id) {
        return device;
      }
    }
  }
  return nullptr;
}

inline void PrintDeviceList() {
  ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No devices found" << std::endl;
    return;
  }

  for (size_t i = 0; i < device_list.size(); ++i) {
    ze_device_properties_t device_properties{
        ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    status = zeDeviceGetProperties(device_list[i], &device_properties);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << "Device #" << i << ": " <<
      device_properties.name << std::endl;
  }
}

inline void PrintMetricList(uint32_t device_id) {
  ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No devices found" << std::endl;
    return;
  }

  PTI_ASSERT(device_id < device_list.size());
  ze_device_handle_t device = device_list[device_id];

  uint32_t group_count = 0;
  status = zetMetricGroupGet(device, &group_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || group_count == 0) {
    std::cout << "[WARNING] No metrics found" << std::endl;
    return;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint32_t group_id = 0;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    bool is_ebs = (group_props.samplingType &
                   ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    bool is_tbs = (group_props.samplingType &
                   ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    PTI_ASSERT(is_ebs || is_tbs);
    if (is_ebs) {
      continue;
    }

    std::cout << "Group " << group_id << ": " << group_props.name << " (" <<
      group_props.description << ")" << std::endl;
    ++group_id;

    uint32_t metric_count = group_props.metricCount;
    std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
    status = zetMetricGet(group_list[i], &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == group_props.metricCount);

    for (uint32_t j = 0; j < metric_count; ++j) {
      zet_metric_properties_t metric_props{};
      metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
      status = zetMetricGetProperties(metric_list[j], &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::cout << "\tMetric " << j << ": "  << metric_props.name <<
        " (" << metric_props.description << ") [" <<
        utils::ze::GetResultType(metric_props.resultType) << ", " <<
        utils::ze::GetMetricType(metric_props.metricType) << ", " <<
        group_props.name << "]" << std::endl;
    }
  }
}

#endif //PTI_TOOLS_ONEPROF_PROF_UTILS_H_