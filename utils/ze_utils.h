//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_ZE_UTILS_H_
#define PTI_UTILS_ZE_UTILS_H_

#include <string.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "demangle.h"
#include "pti_assert.h"
#include "utils.h"

namespace utils {
namespace ze {

inline std::vector<ze_driver_handle_t> GetDriverList() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  status = zeDriverGet(&driver_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (driver_count == 0) {
    return std::vector<ze_driver_handle_t>();
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  status = zeDriverGet(&driver_count, driver_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t device_count = 0;
  status = zeDeviceGet(driver, &device_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> device_list(device_count);
  status = zeDeviceGet(driver, &device_count, device_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return device_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList() {
  std::vector<ze_device_handle_t> device_list;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      device_list.push_back(device);
    }
  }
  return device_list;
}

inline std::vector<ze_device_handle_t> GetSubDeviceList(
    ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t sub_device_count = 0;
  status = zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sub_device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  status = zeDeviceGetSubDevices(
      device, &sub_device_count, sub_device_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return sub_device_list;
}

inline ze_driver_handle_t GetGpuDriver() {
  std::vector<ze_driver_handle_t> driver_list;

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      ze_result_t status = zeDeviceGetProperties(device, &props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        driver_list.push_back(driver);
      }
    }
  }

  if (driver_list.empty()) {
    return nullptr;
  }

  std::string value = utils::GetEnv("PTI_DEVICE_ID");
  uint32_t device_id = value.empty() ? 0 : std::stoul(value);
  PTI_ASSERT(device_id >= 0 && device_id < driver_list.size());
  return driver_list[device_id];
}

inline ze_device_handle_t GetGpuDevice() {
  std::vector<ze_device_handle_t> device_list;

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      ze_result_t status = zeDeviceGetProperties(device, &props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        device_list.push_back(device);
      }
    }
  }

  if (device_list.empty()) {
    return nullptr;
  }

  std::string value = utils::GetEnv("PTI_DEVICE_ID");
  uint32_t device_id = value.empty() ? 0 : std::stoul(value);
  PTI_ASSERT(device_id >= 0 && device_id < device_list.size());

  std::vector<ze_device_handle_t> sub_device_list =
    GetSubDeviceList(device_list[device_id]);
  if (sub_device_list.empty()) {
    return device_list[device_id];
  }

  value = utils::GetEnv("PTI_SUB_DEVICE_ID");
  if (value.empty()) {
    return device_list[device_id];
  }

  uint32_t sub_device_id = value.empty() ? 0 : std::stoul(value);
  PTI_ASSERT(sub_device_id >= 0 && sub_device_id < sub_device_list.size());
  return sub_device_list[sub_device_id];
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
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
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

inline std::string GetResultType(zet_value_type_t type) {
  switch (type) {
    case ZET_VALUE_TYPE_UINT32:
      return "UINT32";
    case ZET_VALUE_TYPE_UINT64:
      return "UINT64";
    case ZET_VALUE_TYPE_FLOAT32:
      return "FLOAT32";
    case ZET_VALUE_TYPE_FLOAT64:
      return "FLOAT64";
    case ZET_VALUE_TYPE_BOOL8:
      return "BOOL8";
    default:
      break;
  }
  return "UNKNOWN";
}

inline std::string GetMetricType(zet_metric_type_t type) {
  switch (type) {
    case ZET_METRIC_TYPE_DURATION:
      return "DURATION";
    case ZET_METRIC_TYPE_EVENT:
      return "EVENT";
    case ZET_METRIC_TYPE_EVENT_WITH_RANGE:
      return "EVENT_WITH_RANGE";
    case ZET_METRIC_TYPE_THROUGHPUT:
      return "THROUGHPUT";
    case ZET_METRIC_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case ZET_METRIC_TYPE_FLAG:
      return "FLAG";
    case ZET_METRIC_TYPE_RATIO:
      return "RATIO";
    case ZET_METRIC_TYPE_RAW:
      return "RAW";
    default:
      break;
  }
  return "UNKNOWN";
}

inline size_t GetKernelMaxSubgroupSize(ze_kernel_handle_t kernel) {
  PTI_ASSERT(kernel != nullptr);
  ze_kernel_properties_t props{};
  ze_result_t status = zeKernelGetProperties(kernel, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.maxSubgroupSize;
}

inline std::string GetKernelName(
    ze_kernel_handle_t kernel, bool demangle = false) {
  PTI_ASSERT(kernel != nullptr);

  size_t size = 0;
  ze_result_t status = zeKernelGetName(kernel, &size, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(size > 0);

  std::vector<char> name(size);
  status = zeKernelGetName(kernel, &size, name.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(name[size - 1] == '\0');

  if (demangle) {
    return utils::Demangle(name.data());
  }
  return std::string(name.begin(), name.end() - 1);
}

inline void GetDeviceTimestamps(
    ze_device_handle_t device,
    uint64_t* host_timestamp,
    uint64_t* device_timestamp) {
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(host_timestamp != nullptr);
  PTI_ASSERT(device_timestamp != nullptr);
  ze_result_t status = zeDeviceGetGlobalTimestamps(
      device, host_timestamp, device_timestamp);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

// TODO: use zeMetricGetGlobalTimestamps
inline void GetMetricTimestamps(
    ze_device_handle_t device,
    uint64_t* host_timestamp,
    uint64_t* metric_timestamp) {
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(host_timestamp != nullptr);
  PTI_ASSERT(metric_timestamp != nullptr);
  ze_result_t status = zeDeviceGetGlobalTimestamps(
      device, host_timestamp, metric_timestamp);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

inline uint64_t GetDeviceTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, };
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

inline uint64_t GetMetricTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, };
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

inline uint64_t GetDeviceTimestampMask(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, };
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return ((props.kernelTimestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)()
              : ((1ull << props.kernelTimestampValidBits) - 1ull));
}

inline uint64_t GetMetricTimestampMask(ze_device_handle_t device) {
#ifdef PTI_OA_TIMESTAMP_VALID_BITS
  return (1ull << PTI_OA_TIMESTAMP_VALID_BITS) - 1ull;
#else
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, };
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  uint32_t devicemask = (props.deviceId & 0xFF00);
  if ((devicemask == 0x5600) || (devicemask == 0x4F00) || (devicemask == 0x0B00) ||
      (devicemask == 0x7D00) || (devicemask == 0xE200) || (devicemask == 0x6400) || (devicemask == 0x9A00)) {
      return (1ull << (props.kernelTimestampValidBits - 1)) - 1ull;
  }
  else {
      return ((props.kernelTimestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)()
                  : ((1ull << props.kernelTimestampValidBits) - 1ull));
  }
#endif
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  ze_result_t status = zeDriverGetApiVersion(driver, &version);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return version;
}

inline ze_api_version_t GetVersion() {
  auto driver_list = GetDriverList();
  if (driver_list.empty()) {
    return ZE_API_VERSION_FORCE_UINT32;
  }
  return GetDriverVersion(driver_list.front());
}

} // namespace ze
} // namespace utils

#endif // PTI_UTILS_ZE_UTILS_H_
