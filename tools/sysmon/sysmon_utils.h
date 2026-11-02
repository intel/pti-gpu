//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Helpers used only by sysmon, so that the tool does not depend on the shared
// utils/utils.h and utils/ze_utils.h headers.

#ifndef PTI_TOOLS_SYSMON_SYSMON_UTILS_H_
#define PTI_TOOLS_SYSMON_SYSMON_UTILS_H_

#include <stdlib.h>

#include <vector>

#include <level_zero/ze_api.h>

// Checks stay active in release builds, as with PTI_ASSERT in utils/pti_assert.h.
#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#define SYSMON_ASSERT(X) assert(X)

#define SYSMON_STRINGIFY(x) #x
#define SYSMON_TOSTRING(x) SYSMON_STRINGIFY(x)

namespace sysmon {

inline void SetEnv(const char* name, const char* value) {
  SYSMON_ASSERT(name != nullptr);
  SYSMON_ASSERT(value != nullptr);
  int status = setenv(name, value, 1);
  SYSMON_ASSERT(status == 0);
}

inline std::vector<ze_driver_handle_t> GetDriverList() {
  uint32_t driver_count = 0;
  ze_result_t status = zeDriverGet(&driver_count, nullptr);
  SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  if (driver_count > 0) {
    status = zeDriverGet(&driver_count, driver_list.data());
    SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);
  }
  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  SYSMON_ASSERT(driver != nullptr);
  uint32_t device_count = 0;
  ze_result_t status = zeDeviceGet(driver, &device_count, nullptr);
  SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_device_handle_t> device_list(device_count);
  if (device_count > 0) {
    status = zeDeviceGet(driver, &device_count, device_list.data());
    SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);
  }
  return device_list;
}

inline std::vector<ze_device_handle_t> GetSubDeviceList(ze_device_handle_t device) {
  SYSMON_ASSERT(device != nullptr);
  uint32_t sub_device_count = 0;
  ze_result_t status = zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
  SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  if (sub_device_count > 0) {
    status = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_list.data());
    SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);
  }
  return sub_device_list;
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  SYSMON_ASSERT(driver != nullptr);
  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  ze_result_t status = zeDriverGetApiVersion(driver, &version);
  SYSMON_ASSERT(status == ZE_RESULT_SUCCESS);
  return version;
}

// Structure type for zeDeviceGetProperties: ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES
// is deprecated; drivers implementing API 1.2 or newer take the _1_2 variant.
inline ze_structure_type_t GetDevicePropertiesType(ze_driver_handle_t driver) {
  return GetDriverVersion(driver) >= ZE_API_VERSION_1_2 ?
      ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2 :
      ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
}

}  // namespace sysmon

#endif  // PTI_TOOLS_SYSMON_SYSMON_UTILS_H_
