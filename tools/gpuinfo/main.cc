//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>
#include <string>

#include <metrics_discovery_api.h>

#include "metric_device.h"

enum Mode {
  GPU_LIST = 0,
  GPU_INFO = 1,
  GPU_METRICS = 2
};

static void Usage() {
  std::cout << "Usage: ./gpuinfo[.exe] <options>" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "--list [-l]      " <<
    "Print list of available devices" << std::endl;
  std::cout << "--info [-i]      " <<
    "Print general device info (default)" << std::endl;
  std::cout << "--metrics [-m]   " <<
    "Print list of available mertrics" << std::endl;
  std::cout << "--help [-h]      " <<
    "Print this help message" << std::endl;
  std::cout << "--version        " <<
    "Print this help message" << std::endl;
}

static const char* GetDeviceName(const MetricDevice& device) {
  return device->GetParams()->DeviceName;
}

static void PrintDeviceList() {
  uint32_t device_count = MetricDevice::GetDeviceCount();
  for (uint32_t i = 0; i < device_count; ++i) {
    uint32_t sub_device_count = MetricDevice::GetSubDeviceCount(i);
    if (sub_device_count == 0) {
      MetricDevice* device = MetricDevice::Create(i, 0);
      if (device == nullptr) {
        std::cout << "[WARNING] Unable to open metric device" << std::endl;
        return;
      }
      std::cout << "Device " << i << ": " <<
        GetDeviceName(*device) << std::endl;
      delete device;
    } else {
      std::cout << "Device " << i << ":" << std::endl;
      for (uint32_t j = 0; j < sub_device_count; ++j) {
        MetricDevice* device = MetricDevice::Create(i, j);
        if (device == nullptr) {
          std::cout << "[WARNING] Unable to open metric device" << std::endl;
          return;
        }
        std::cout << "-- Subdevice " << j << ": " <<
          GetDeviceName(*device) << std::endl;
        delete device;
      }
    }
  }
}

static std::string GetResultType(md::TMetricResultType type) {
  switch (type) {
    case md::RESULT_UINT32:
      return "UINT32";
    case md::RESULT_UINT64:
      return "UINT64";
    case md::RESULT_FLOAT:
      return "FLOAT32";
    case md::RESULT_BOOL:
      return "BOOL8";
    default:
      break;
  }
  return "UNKNOWN";
}

static void PrintDeviceInfo(const MetricDevice& device) {
  std::cout << "---- Name: " <<
    device->GetParams()->DeviceName << std::endl;
  PTI_ASSERT(device->GetParams()->GlobalSymbolsCount > 0);
  for (uint32_t i = 0; i < device->GetParams()->GlobalSymbolsCount; ++i) {
    md::TGlobalSymbol_1_0* symbol = device->GetGlobalSymbol(i);
    if (symbol->SymbolTypedValue.ValueType == md::VALUE_TYPE_LAST) {
      continue;
    }

    std::cout << "---- " << symbol->SymbolName << ": ";
    switch (symbol->SymbolTypedValue.ValueType) {
      case md::VALUE_TYPE_UINT32: {
        std::cout << symbol->SymbolTypedValue.ValueUInt32;
        break;
      }
      case md::VALUE_TYPE_UINT64: {
        std::cout << symbol->SymbolTypedValue.ValueUInt64;
        break;
      }
      case md::VALUE_TYPE_FLOAT: {
        std::cout << symbol->SymbolTypedValue.ValueFloat;
        break;
      }
      case md::VALUE_TYPE_BOOL: {
        std::cout << (symbol->SymbolTypedValue.ValueBool ? "true" : "false");
        break;
      }
      case md::VALUE_TYPE_CSTRING: {
        std::cout << symbol->SymbolTypedValue.ValueCString;
        break;
      }
      default: {
        PTI_ASSERT(0);
        break;
      }
    }
    std::cout << std::endl;
  }
}

static void PrintMetricsInfo(const MetricDevice& device) {
  uint32_t group_count = device->GetParams()->ConcurrentGroupsCount;
  PTI_ASSERT(group_count > 0);
  for (uint32_t gid = 0; gid < group_count; ++gid) {
    md::IConcurrentGroup_1_5* group = device->GetConcurrentGroup(gid);
    PTI_ASSERT(group != nullptr);

    const char* group_name = group->GetParams()->SymbolName;
    std::cout << "---- Metric Group " << gid << ": " << group_name << std::endl;

    uint32_t set_count = group->GetParams()->MetricSetsCount;
    PTI_ASSERT(set_count > 0);
    for (uint32_t sid = 0; sid < set_count; ++sid) {
      md::IMetricSet_1_5* set = group->GetMetricSet(sid);
      PTI_ASSERT(set != nullptr);

      const char* set_name = set->GetParams()->SymbolName;
      std::cout << "------ Metric Set " << sid << ": " << set_name <<
        " (" << set->GetParams()->ShortName << ")" << std::endl;
      
      uint32_t metric_count = set->GetParams()->MetricsCount;
      PTI_ASSERT(metric_count > 0);
      for (uint32_t mid = 0; mid < metric_count; ++mid) {
        md::IMetric_1_0* metric = set->GetMetric(mid);
        PTI_ASSERT(metric != nullptr);
        std::cout << "-------- Metric " << mid << ": " << group_name << " / " <<
          set_name << " / " << metric->GetParams()->SymbolName <<
          " (" << metric->GetParams()->ShortName << ") [" <<
          GetResultType(metric->GetParams()->ResultType) << "]" << std::endl;
      }

      uint32_t info_count = set->GetParams()->InformationCount;
      for (uint32_t iid = 0; iid < info_count; ++iid) {
        md::IInformation_1_0* info = set->GetInformation(iid);
        PTI_ASSERT(info != nullptr);
        std::cout << "-------- Info " << iid + metric_count << ": " <<
          group_name << " / " << set_name << " / " <<
          info->GetParams()->SymbolName << " (" <<
          info->GetParams()->ShortName << ")" << std::endl;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  Mode mode = GPU_INFO;

  if (argc > 1) {
    if (std::string("--list") == argv[1] ||
        std::string("-l") == argv[1]) {
      mode = GPU_LIST;
    } else if (std::string("--metrics") == argv[1] ||
               std::string("-m") == argv[1]) {
      mode = GPU_METRICS;
    } else if (std::string("--help") == argv[1] ||
               std::string("-h") == argv[1]) {
      Usage();
      return 0;
    } else if (std::string("--version") == argv[1]) {
#ifdef PTI_VERSION
      std::cout << TOSTRING(PTI_VERSION) << std::endl;
#endif
      return 0;
    }
  }

  uint32_t device_count = MetricDevice::GetDeviceCount();
  if (device_count == 0) {
    std::cout << "Unable to find GPU devices" << std::endl;
    return 0;
  }

  if (mode == GPU_LIST) {
    PrintDeviceList();
  } else {
    for (uint32_t i = 0; i < device_count; ++i) {
      std::cout << "Device " << i << ":" << std::endl;
      uint32_t sub_device_count = MetricDevice::GetSubDeviceCount(i);
      if (sub_device_count > 0) {
        for (uint32_t j = 0; j < sub_device_count; ++j) {
          std::cout << "-- Subdevice " << j << ":" << std::endl;
          MetricDevice* device = MetricDevice::Create(i, j);
          if (device == nullptr) {
            std::cout << "[WARNING] Unable to open metric device" << std::endl;
            return 0;
          }
          if (mode == GPU_INFO) {
            PrintDeviceInfo(*device);
          } else {
            PrintMetricsInfo(*device);
          }
          delete device;
        }
      } else {
        MetricDevice* device = MetricDevice::Create(i, 0);
        if (device == nullptr) {
          std::cout << "[WARNING] Unable to open metric device" << std::endl;
          return 0;
        }
        if (mode == GPU_INFO) {
          PrintDeviceInfo(*device);
        } else {
          PrintMetricsInfo(*device);
        }
        delete device;
      }
    }
  }

  return 0;
}