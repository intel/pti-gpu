//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_PROF_UTILS_H_
#define PTI_TOOLS_ONEPROF_PROF_UTILS_H_

#include <iomanip>
#include <iostream>

#include <level_zero/zes_api.h>
#include <CL/cl_ext_private.h>

#include "ze_utils.h"
#include "cl_utils.h"

inline ze_driver_handle_t GetZeDriver(uint32_t device_id) {
  uint32_t id = 0;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      if (id == device_id) {
        return driver;
      }
      ++id;
    }
  }
  return nullptr;
}

inline ze_device_handle_t GetZeDevice(uint32_t device_id) {
  uint32_t id = 0;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      if (id == device_id) {
        return device;
      }
      ++id;
    }
  }
  return nullptr;
}

inline cl_device_pci_bus_info_khr GetDevicePciInfo(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  if (!utils::cl::CheckExtension(device, "cl_khr_pci_bus_info")) {
    return cl_device_pci_bus_info_khr{0, 0, 0, 0};
  }

  cl_device_pci_bus_info_khr pci_info{};
  cl_int status = clGetDeviceInfo(
      device, CL_DEVICE_PCI_BUS_INFO_KHR,
      sizeof(cl_device_pci_bus_info_khr), &pci_info, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return pci_info;
}

inline cl_device_id GetClDevice(uint32_t device_id) {
  ze_device_handle_t device = GetZeDevice(device_id);
  if (device == nullptr) {
    return nullptr;
  }

  zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
  ze_result_t status = zesDevicePciGetProperties(device, &pci_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  for (auto device : utils::cl::GetDeviceList(CL_DEVICE_TYPE_GPU)) {
    cl_device_pci_bus_info_khr pci_info = GetDevicePciInfo(device);
    if (pci_info.pci_domain == pci_props.address.domain &&
        pci_info.pci_bus == pci_props.address.bus &&
        pci_info.pci_device == pci_props.address.device &&
        pci_info.pci_function == pci_props.address.function) {
      return device;
    }
  }

  return nullptr;
}

inline ze_device_handle_t GetZeDevice(cl_device_id device_id) {
  if (device_id == nullptr) {
    return nullptr;
  }

  cl_device_pci_bus_info_khr pci_info = GetDevicePciInfo(device_id);

  for (auto device : utils::ze::GetDeviceList()) {
    zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
    ze_result_t status = zesDevicePciGetProperties(device, &pci_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (pci_info.pci_domain == pci_props.address.domain &&
        pci_info.pci_bus == pci_props.address.bus &&
        pci_info.pci_device == pci_props.address.device &&
        pci_info.pci_function == pci_props.address.function) {
      return device;
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

  // Save the original format state
  std::ios originalState(nullptr);
  originalState.copyfmt(std::cout);

  for (size_t i = 0; i < device_list.size(); ++i) {
    ze_device_properties_t device_properties{
        ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    status = zeDeviceGetProperties(device_list[i], &device_properties);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
    status = zesDevicePciGetProperties(device_list[i], &pci_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << "Device #" << i << ": [" << std::hex << std::setfill('0') <<
      std::setw(4) << pci_props.address.domain << ":" <<
      std::setw(2) << pci_props.address.bus << ":" <<
      std::setw(2) << pci_props.address.device << "." <<
      std::setw(1) << pci_props.address.function << std::dec << "] " <<
      device_properties.name << std::endl;
  }

  // Restore the original format state
  std::cout.copyfmt(originalState);
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

      std::cout << "\tMetric " << j << ": "  << metric_props.name;
      std::string units = GetMetricUnits(metric_props.resultUnits);
      if (!units.empty()) {
        std::cout << "[" << units << "]";
      }
      std::cout << " (" << metric_props.description << ") [" <<
        utils::ze::GetResultType(metric_props.resultType) << ", " <<
        utils::ze::GetMetricType(metric_props.metricType) << ", " <<
        group_props.name << "]" << std::endl;
    }
  }
}

#endif //PTI_TOOLS_ONEPROF_PROF_UTILS_H_
