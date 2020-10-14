//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_ZE_UTILS_H_
#define PTI_SAMPLES_UTILS_ZE_UTILS_H_

#include <string.h>

#include <string>
#include <vector>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "pti_assert.h"

namespace utils {
namespace ze {

inline void GetIntelDeviceAndDriver(ze_device_type_t type,
                                    ze_device_handle_t& device,
                                    ze_driver_handle_t& driver) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  status = zeDriverGet(&driver_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || driver_count == 0) {
    return;
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count, nullptr);
  status = zeDriverGet(&driver_count, driver_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  for (uint32_t i = 0; i < driver_count; ++i) {
    uint32_t device_count = 0;
    status = zeDeviceGet(driver_list[i], &device_count, nullptr);
    if (status != ZE_RESULT_SUCCESS || device_count == 0) {
        continue;
    }

    std::vector<ze_device_handle_t> device_list(device_count, nullptr);
    status = zeDeviceGet(driver_list[i], &device_count, device_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t j = 0; j < device_count; ++j) {
      ze_device_properties_t props{};
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      status = zeDeviceGetProperties(device_list[j], &props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (props.type == type && strstr(props.name, "Intel") != nullptr) {
        device = device_list[j];
        driver = driver_list[i];
        break;
      }
    }
  }
}

inline ze_context_handle_t GetContext(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_context_handle_t context = nullptr;
  ze_context_desc_t context_desc = {
      ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

  status = zeContextCreate(driver, &context_desc, &context);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return context;
}

inline std::string GetDeviceName(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_device_properties_t props{};
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.name;
}

inline int GetMetricId(zet_metric_group_handle_t group, std::string name) {
  PTI_ASSERT(group != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t metric_count = 0;
  status = zetMetricGet(group, &metric_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (metric_count == 0) {
    return -1;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  status = zetMetricGet(group, &metric_count, metric_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  int target = -1;
  for (uint32_t i = 0; i < metric_count; ++i) {
    zet_metric_properties_t metric_props{};
    status = zetMetricGetProperties(metric_list[i], &metric_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (name == metric_props.name) {
      target = i;
      break;
    }
  }

  return target;
}

inline zet_metric_group_handle_t FindMetricGroup(
    ze_device_handle_t device, std::string name,
    zet_metric_group_sampling_type_flag_t type) {
  PTI_ASSERT(device != nullptr);
  
  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  status = zetMetricGroupGet(device, &group_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  if (group_count == 0) {
    return nullptr;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zet_metric_group_handle_t target = nullptr;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (name == group_props.name && (group_props.samplingType & type)) {
      target = group_list[i];
      break;
    }
  }

  return target;
}

inline uint64_t GetTimerResolution(ze_device_handle_t device) {
  ze_device_properties_t props{};
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  props.pNext = nullptr;
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

} // namespace ze
} // namespace utils

#endif // PTI_SAMPLES_UTILS_ZE_UTILS_H_