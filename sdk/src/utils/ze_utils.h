//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_ZE_UTILS_H_
#define PTI_UTILS_ZE_UTILS_H_

#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <limits>
#include <array>
#include <iomanip>
#include <optional>
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
  overhead_fini("zeDriverGet");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (driver_count == 0) {
    return std::vector<ze_driver_handle_t>();
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  overhead::Init();
  status = zeDriverGet(&driver_count, driver_list.data());
  overhead_fini("zeDriverGet");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t device_count = 0;
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, nullptr);
  overhead_fini("zeDriverGet");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> device_list(device_count);
  overhead::Init();
  status = zeDeviceGet(driver, &device_count, device_list.data());
  overhead_fini("zeDriverGet");
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
  overhead_fini("zeDeviceGetSubDevices");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sub_device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  overhead::Init();
  status = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_list.data());
  overhead_fini("zeDeviceGetSubDevices");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return sub_device_list;
}

inline ze_driver_handle_t GetGpuDriver(std::size_t pti_device_id) {
  std::vector<ze_driver_handle_t> driver_list;

  for (auto* driver : GetDriverList()) {
    for (auto* device : GetDeviceList(driver)) {
      ze_device_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      overhead_fini("zeDeviceGetProperties");
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
      props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeDeviceGetProperties(device, &props);
      overhead_fini("zeDeviceGetProperties");
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
  overhead_fini("zeContextCreate");
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
  overhead_fini("zeDeviceGetProperties");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return static_cast<char*>(props.name);
}

inline size_t GetKernelMaxSubgroupSize(ze_kernel_handle_t kernel) {
  PTI_ASSERT(kernel != nullptr);
  ze_kernel_properties_t props{};
  overhead::Init();
  ze_result_t status = zeKernelGetProperties(kernel, &props);
  overhead_fini("zeKernelGetProperties");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.maxSubgroupSize;
}

inline std::string GetKernelName(ze_kernel_handle_t kernel, bool demangle = false) {
  PTI_ASSERT(kernel != nullptr);

  size_t size = 0;
  overhead::Init();
  ze_result_t status = zeKernelGetName(kernel, &size, nullptr);
  overhead_fini("zeKernelGetName");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(size > 0);

  std::vector<char> name(size);
  overhead::Init();
  status = zeKernelGetName(kernel, &size, name.data());
  overhead_fini("zeKernelGetName");
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
  overhead_fini("zeDeviceGetGlobalTimestamps");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

inline uint64_t GetDeviceTimerFrequency(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini("zeDeviceGetProperties");
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
  overhead_fini("zeDeviceGetProperties");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return ((props.kernelTimestampValidBits == 64)
              ? (std::numeric_limits<uint64_t>::max)()
              : ((1ULL << props.kernelTimestampValidBits) - 1ULL));
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  PTI_ASSERT(driver != nullptr);

  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  overhead::Init();
  ze_result_t status = zeDriverGetApiVersion(driver, &version);
  overhead_fini("zeDriverGetApiVersion");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return version;
}

inline bool GetDeviceTimerFrequency_TimestampMask_UUID(ze_device_handle_t device,
                                                       uint64_t& timer_frequency,
                                                       uint64_t& timestamp_mask,
                                                       ze_device_uuid_t& uuid) {
  PTI_ASSERT(device != nullptr);

  ze_device_properties_t props;
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  overhead::Init();
  ze_result_t status = zeDeviceGetProperties(device, &props);
  overhead_fini("zeDeviceGetProperties");
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  timer_frequency = props.timerResolution;
  timestamp_mask = (props.kernelTimestampValidBits == 64)
                       ? (std::numeric_limits<uint64_t>::max)()
                       : ((1ULL << props.kernelTimestampValidBits) - 1ULL);
  std::copy_n(props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE, uuid.id);
  return true;
}

inline bool GetDeviceUUID(ze_device_handle_t device, uint8_t* uuid, bool measure_overhead = false) {
  PTI_ASSERT(device != nullptr);

  ze_device_properties_t props;
  std::memset(&props, 0, sizeof(props));
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  props.pNext = nullptr;
  if (measure_overhead) {
    overhead::Init();
  }
  ze_result_t status = zeDeviceGetProperties(device, &props);
  if (measure_overhead) {
    overhead_fini("zeDeviceGetProperties");
  }
  if (status != ZE_RESULT_SUCCESS) {
    return false;
  }
  std::copy_n(props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE, uuid);
  return true;
}

inline ze_api_version_t GetVersion(const std::vector<ze_driver_handle_t>& driver_list) {
  if (driver_list.empty()) {
    return ZE_API_VERSION_FORCE_UINT32;
  }
  return GetDriverVersion(driver_list.front());
}

inline ze_api_version_t GetVersion() {
  auto driver_list = GetDriverList();
  return GetVersion(driver_list);
}

inline std::optional<zel_version_t> GetLoaderVersion() {
  constexpr auto* kLoaderComponentName = "loader";
  constexpr auto kLoaderComponentNameLength = std::char_traits<char>::length(kLoaderComponentName);
  size_t number_of_components = 0;
  auto status = zelLoaderGetVersions(&number_of_components, nullptr);
  if (number_of_components == 0 || status != ZE_RESULT_SUCCESS) {
    return std::nullopt;
  }

  std::vector<zel_component_version_t> versions(number_of_components);

  status = zelLoaderGetVersions(&number_of_components, versions.data());
  if (status != ZE_RESULT_SUCCESS) {
    return std::nullopt;
  }

  for (const auto& component_version : versions) {
    if (!std::strncmp(component_version.component_name, kLoaderComponentName,
                      kLoaderComponentNameLength)) {
      return component_version.component_lib_version;
    }
  }

  return std::nullopt;
}

}  // namespace ze
}  // namespace utils

#endif  // PTI_UTILS_ZE_UTILS_H_
