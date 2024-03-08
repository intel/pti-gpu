//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_ZE_UTILS_H_
#define PTI_UTILS_ZE_UTILS_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "demangle.h"
#include "overhead_kinds.h"
#include "pti_assert.h"
#include "utils.h"

namespace utils {
namespace ze {

inline std::vector<ze_driver_handle_t> GetDriverList() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  overhead::Init();
  status = zeDriverGet(&driver_count, nullptr);
  {
    std::string o_api_string = "zeDriverGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (driver_count == 0) {
    return std::vector<ze_driver_handle_t>();
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  overhead::Init();
  status = zeDriverGet(&driver_count, driver_list.data());
  {
    std::string o_api_string = "zeDriverGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t device_count = 0;
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, nullptr);
  {
    std::string o_api_string = "zeDeviceGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> device_list(device_count);
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, device_list.data());
  {
    std::string o_api_string = "zeDeviceGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
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

inline std::vector<ze_device_handle_t> GetSubDeviceList(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t sub_device_count = 0;
  overhead::Init();
  status = zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
  {
    std::string o_api_string = "zeDeviceGetSubDevices";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sub_device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  overhead::Init();
  status = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_list.data());
  {
    std::string o_api_string = "zeDeviceGetSubDevices";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return sub_device_list;
}

inline ze_driver_handle_t GetGpuDriver(std::size_t pti_device_id) {
  std::vector<ze_driver_handle_t> driver_list;

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      {
        std::string o_api_string = "zeDeviceGetProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
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

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      {
        std::string o_api_string = "zeDeviceGetProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
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
  auto device_handle = GetGpuDevice(pti_device_id);

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
  {
    std::string o_api_string = "zeContextCreate";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return context;
}

inline std::string GetDeviceName(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  props.pNext = nullptr;
  overhead::Init();
  status = zeDeviceGetProperties(device, &props);
  {
    std::string o_api_string = "zeDeviceGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.name;
}

inline int GetMetricId(zet_metric_group_handle_t group, std::string name) {
  PTI_ASSERT(group != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t metric_count = 0;
  overhead::Init();
  status = zetMetricGet(group, &metric_count, nullptr);
  {
    std::string o_api_string = "zetMetricGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (metric_count == 0) {
    return -1;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  overhead::Init();
  status = zetMetricGet(group, &metric_count, metric_list.data());
  {
    std::string o_api_string = "zetMetricGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  int target = -1;
  for (uint32_t i = 0; i < metric_count; ++i) {
    zet_metric_properties_t metric_props{};
    overhead::Init();
    status = zetMetricGetProperties(metric_list[i], &metric_props);
    {
      std::string o_api_string = "zetMetricGetProperties";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (name == metric_props.name) {
      target = i;
      break;
    }
  }

  return target;
}

inline zet_metric_group_handle_t FindMetricGroup(ze_device_handle_t device, std::string name,
                                                 zet_metric_group_sampling_type_flag_t type) {
  PTI_ASSERT(device != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  overhead::Init();
  status = zetMetricGroupGet(device, &group_count, nullptr);
  {
    std::string o_api_string = "zetMetricGroupGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  if (group_count == 0) {
    return nullptr;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  overhead::Init();
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  {
    std::string o_api_string = "zetMetricGroupGet";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zet_metric_group_handle_t target = nullptr;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    overhead::Init();
    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    {
      std::string o_api_string = "zetMetricGroupGetProperties";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
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
  overhead::Init();
  ze_result_t status = zeKernelGetProperties(kernel, &props);
  {
    std::string o_api_string = "zeKernelGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.maxSubgroupSize;
}

inline std::string GetKernelName(ze_kernel_handle_t kernel, bool demangle = false) {
  PTI_ASSERT(kernel != nullptr);

  size_t size = 0;
  overhead::Init();
  ze_result_t status = zeKernelGetName(kernel, &size, nullptr);
  {
    std::string o_api_string = "zeKernelGetName";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(size > 0);

  std::vector<char> name(size);
  overhead::Init();
  status = zeKernelGetName(kernel, &size, name.data());
  {
    std::string o_api_string = "zeKernelGetName";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
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
  {
    std::string o_api_string = "zeDeviceGetGlobalTimestamps";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

// TODO: use zeMetricGetGlobalTimestamps
inline void GetMetricTimestamps(ze_device_handle_t device, uint64_t* host_timestamp,
                                uint64_t* metric_timestamp) {
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(host_timestamp != nullptr);
  PTI_ASSERT(metric_timestamp != nullptr);
  overhead::Init();
  ze_result_t status = zeDeviceGetGlobalTimestamps(device, host_timestamp, metric_timestamp);
  {
    std::string o_api_string = "zeDeviceGetGlobalTimestamps";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

inline uint64_t GetDeviceTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  {
    std::string o_api_string = "zeDeviceGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

inline uint64_t GetMetricTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  {
    std::string o_api_string = "zeDeviceGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.timerResolution;
}

inline uint64_t GetDeviceTimestampMask(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  {
    std::string o_api_string = "zeDeviceGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return ((props.kernelTimestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)()
              : ((1ull << props.kernelTimestampValidBits) - 1ull));
}

inline uint64_t GetMetricTimestampMask(ze_device_handle_t device) {
#ifdef PTI_OA_TIMESTAMP_VALID_BITS
  return (1ull << PTI_OA_TIMESTAMP_VALID_BITS) - 1ull;
#else
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  {
    std::string o_api_string = "zeDeviceGetProperties";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  uint32_t devicemask = (props.deviceId & 0xFF00);
  if ((devicemask == 0x5600) || (devicemask == 0x4F00) || (devicemask == 0x0B00)) {
    return (1ull << (props.kernelTimestampValidBits - 1)) - 1ull;
  } else {
    return ((props.kernelTimestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)()
                : ((1ull << props.kernelTimestampValidBits) - 1ull));
  }
#endif
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  overhead::Init();
  ze_result_t status = zeDriverGetApiVersion(driver, &version);
  {
    std::string o_api_string = "zeDriverGetApiVersion";
    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                         o_api_string.c_str());
  };
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

}  // namespace ze
}  // namespace utils

#endif  // PTI_UTILS_ZE_UTILS_H_
