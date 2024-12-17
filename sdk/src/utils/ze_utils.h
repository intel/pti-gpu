//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_ZE_UTILS_H_
#define PTI_UTILS_ZE_UTILS_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <pti/pti_cbids_runtime.h>

#include <sstream>
#include <string>
#include <vector>

#include "demangle.h"
#include "overhead_kinds.h"
#include "pti_assert.h"

namespace utils {
namespace ze {

inline std::vector<ze_driver_handle_t> GetDriverList() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  overhead::Init();
  status = zeDriverGet(&driver_count, nullptr);
  overhead_fini(zeDriverGet_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (driver_count == 0) {
    return std::vector<ze_driver_handle_t>();
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  overhead::Init();
  status = zeDriverGet(&driver_count, driver_list.data());
  overhead_fini(zeDriverGet_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t device_count = 0;
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, nullptr);
  overhead_fini(zeDriverGet_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> device_list(device_count);
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, device_list.data());
  overhead_fini(zeDriverGet_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return device_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList() {
  std::vector<ze_device_handle_t> device_list;
  for (auto* driver : utils::ze::GetDriverList()) {
    for (auto* device : utils::ze::GetDeviceList(driver)) {
      device_list.push_back(device);
    }
  }
  return device_list;
}

inline std::vector<ze_device_handle_t> GetSubDeviceList(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t sub_device_count = 0;
  overhead::Init();
  status = zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
  overhead_fini(zeDeviceGetSubDevices_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sub_device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  overhead::Init();
  status = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_list.data());
  overhead_fini(zeDeviceGetSubDevices_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return sub_device_list;
}

inline ze_driver_handle_t GetGpuDriver(std::size_t pti_device_id) {
  std::vector<ze_driver_handle_t> driver_list;

  for (auto* driver : GetDriverList()) {
    for (auto* device : GetDeviceList(driver)) {
      ze_device_properties_t props;
      std::memset(&props, 0, sizeof(props));
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      overhead_fini(zeDeviceGetProperties_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        driver_list.push_back(driver);
      }
    }
  }

  if (driver_list.empty()) {
    return nullptr;
  }

  if (pti_device_id >= driver_list.size()) {
    return nullptr;
  }

  return driver_list[pti_device_id];
}

inline ze_device_handle_t GetGpuDevice(std::size_t pti_device_id) {
  std::vector<ze_device_handle_t> device_list;

  for (auto* driver : GetDriverList()) {
    for (auto* device : GetDeviceList(driver)) {
      ze_device_properties_t props;
      std::memset(&props, 0, sizeof(props));
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      overhead_fini(zeDeviceGetProperties_id);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        device_list.push_back(device);
      }
    }
  }

  if (device_list.empty()) {
    return nullptr;
  }

  if (pti_device_id >= device_list.size()) {
    return nullptr;
  }

  return device_list[pti_device_id];
}

inline ze_device_handle_t GetGpuDevice(std::size_t pti_device_id, std::size_t pti_sub_device_id) {
  auto* device_handle = GetGpuDevice(pti_device_id);

  if (!device_handle) {
    return nullptr;
  }

  std::vector<ze_device_handle_t> sub_device_list = GetSubDeviceList(device_handle);

  if (sub_device_list.empty()) {
    return device_handle;
  }

  if (pti_sub_device_id >= sub_device_list.size()) {
    return nullptr;
  }

  return sub_device_list[pti_sub_device_id];
}

inline ze_context_handle_t GetContext(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_context_handle_t context = nullptr;
  ze_context_desc_t context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

  overhead::Init();
  status = zeContextCreate(driver, &context_desc, &context);
  overhead_fini(zeContextCreate_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return context;
}

inline std::string GetDeviceName(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  props.pNext = nullptr;
  overhead::Init();
  status = zeDeviceGetProperties(device, &props);
  overhead_fini(zeDeviceGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return static_cast<char*>(props.name);
}

inline std::string GetMetricTypedValue(const zet_typed_value_t& typed_value,
                                       uint8_t precision = 2) {
  std::stringstream stream;
  switch (typed_value.type) {
    case ZET_VALUE_TYPE_UINT32:
      return std::to_string(typed_value.value.ui32);
    case ZET_VALUE_TYPE_UINT64:
      return std::to_string(typed_value.value.ui64);
    case ZET_VALUE_TYPE_FLOAT32:
      stream << std::fixed << std::setprecision(precision) << typed_value.value.fp32;
      return stream.str();
    case ZET_VALUE_TYPE_FLOAT64:
      stream << std::fixed << std::setprecision(precision) << typed_value.value.fp64;
      return stream.str();
    case ZET_VALUE_TYPE_BOOL8:
      return std::to_string(static_cast<uint32_t>(typed_value.value.b8));
    default:
      break;
  }
  return "UNKNOWN";
}

inline uint32_t GetMetricCount(zet_metric_group_handle_t group) {
  PTI_ASSERT(group != nullptr);

  zet_metric_group_properties_t group_props;
  std::memset(&group_props, 0, sizeof(group_props));
  group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
  ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return group_props.metricCount;
}

inline std::string GetMetricUnits(const char* units) {
  PTI_ASSERT(units != nullptr);

  std::string result = units;
  if (result.find("null") != std::string::npos) {
    result = "";
  } else if (result.find("percent") != std::string::npos) {
    result = "%";
  }

  return result;
}

inline std::vector<std::string> GetMetricList(zet_metric_group_handle_t group) {
  PTI_ASSERT(group != nullptr);

  uint32_t metric_count = GetMetricCount(group);
  PTI_ASSERT(metric_count > 0);

  std::vector<zet_metric_handle_t> metric_list(metric_count);
  ze_result_t status = zetMetricGet(group, &metric_count, metric_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(metric_count == metric_list.size());

  std::vector<std::string> name_list;
  for (auto metric : metric_list) {
    zet_metric_properties_t metric_props;
    std::memset(&metric_props, 0, sizeof(metric_props));
    metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;

    status = zetMetricGetProperties(metric, &metric_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::string units = GetMetricUnits(metric_props.resultUnits);
    std::string name = metric_props.name;
    if (!units.empty()) {
      name += "[" + units + "]";
    }
    name_list.push_back(name);
  }

  return name_list;
}

inline uint32_t GetMetricId(const std::vector<std::string>& metric_list,
                            const std::string& metric_name) {
  PTI_ASSERT(!metric_list.empty());
  PTI_ASSERT(!metric_name.empty());

  for (size_t i = 0; i < metric_list.size(); ++i) {
    if (metric_list[i].find(metric_name) == 0) {
      return i;
    }
  }

  return metric_list.size();
}

/* Find all metric groups for given device regardless of type
 *
 * @param[in] device Target device
 * @param[out] metric_groups Vector of metric groups found
 */
inline void FindMetricGroups(ze_device_handle_t device,
                             std::vector<zet_metric_group_handle_t>& metric_groups) {
  PTI_ASSERT(device != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  status = zetMetricGroupGet(device, &group_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  if (group_count == 0) {
    return;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  for (uint32_t i = 0; i < group_count; ++i) {
    metric_groups.push_back(group_list[i]);
  }
}

inline size_t GetKernelMaxSubgroupSize(ze_kernel_handle_t kernel) {
  PTI_ASSERT(kernel != nullptr);
  ze_kernel_properties_t props;
  std::memset(&props, 0, sizeof(props));
  overhead::Init();
  ze_result_t status = zeKernelGetProperties(kernel, &props);
  overhead_fini(zeKernelGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.maxSubgroupSize;
}

inline std::string GetKernelName(ze_kernel_handle_t kernel, bool demangle = false) {
  PTI_ASSERT(kernel != nullptr);

  size_t size = 0;
  overhead::Init();
  ze_result_t status = zeKernelGetName(kernel, &size, nullptr);
  overhead_fini(zeKernelGetName_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(size > 0);

  std::vector<char> name(size);
  overhead::Init();
  status = zeKernelGetName(kernel, &size, name.data());
  overhead_fini(zeKernelGetName_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(name[size - 1] == '\0');

  if (demangle) {
    return utils::Demangle(name.data());
  }
  return std::string(name.begin(), name.end() - 1);
}

inline void GetDeviceTimestamps(ze_device_handle_t device, uint64_t* host_timestamp,
                                uint64_t* device_timestamp) {
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(host_timestamp != nullptr);
  PTI_ASSERT(device_timestamp != nullptr);
  overhead::Init();
  ze_result_t status = zeDeviceGetGlobalTimestamps(device, host_timestamp, device_timestamp);
  overhead_fini(zeDeviceGetGlobalTimestamps_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

inline uint64_t GetDeviceTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini(zeDeviceGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

inline uint64_t GetDeviceTimestampMask(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini(zeDeviceGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return ((props.kernelTimestampValidBits == 64)
              ? (std::numeric_limits<uint64_t>::max)()
              : ((1ULL << props.kernelTimestampValidBits) - 1ULL));
}

inline uint64_t GetMetricTimestampMask(ze_device_handle_t device) {
  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini(zeDeviceGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  uint32_t devicemask = (props.deviceId & 0xFF00);
  if ((devicemask == 0x5600) || (devicemask == 0x4F00) || (devicemask == 0x0B00)) {
    return (1ull << (props.kernelTimestampValidBits - 1)) - 1ull;
  } else {
    return ((props.kernelTimestampValidBits == 64)
                ? (std::numeric_limits<uint64_t>::max)()
                : ((1ull << props.kernelTimestampValidBits) - 1ull));
  }
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  overhead::Init();
  ze_result_t status = zeDriverGetApiVersion(driver, &version);
  overhead_fini(zeDriverGetApiVersion_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return version;
}

inline bool GetDeviceTimerFrequency_TimestampMask_UUID(ze_device_handle_t device,
                                                       uint64_t& timer_frequency,
                                                       uint64_t& timestamp_mask,
                                                       ze_device_uuid_t& uuid) {
  PTI_ASSERT(device != nullptr);

  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini(zeDeviceGetProperties_id);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  timer_frequency = props.timerResolution;
  timestamp_mask = (props.kernelTimestampValidBits == 64)
                       ? (std::numeric_limits<uint64_t>::max)()
                       : ((1ULL << props.kernelTimestampValidBits) - 1ULL);
  std::copy_n(props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE, uuid.id);
  return true;
}

inline ze_api_version_t GetVersion() {
  auto driver_list = GetDriverList();
  if (driver_list.empty()) {
    return ZE_API_VERSION_FORCE_UINT32;
  }
  return GetDriverVersion(driver_list.front());
}

}  // namespace ze
}  // namespace utils

#endif  // PTI_UTILS_ZE_UTILS_H_
