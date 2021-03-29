//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>
#include <string>

#include <MD/metrics_discovery_api.h>

#include "metric_device.h"

enum Mode {
  GPU_LIST = 0,
  GPU_INFO = 1,
  GPU_METRICS = 2
};

struct Options {
  Mode mode;
  uint32_t device;
  uint32_t sub_device;
};

static void Usage() {
  std::cout << "Usage: ./gpu_info[.exe] <options>" << std::endl;
  std::cout << "Options:" << std::endl;
   std::cout <<
    "--list      [-l] Print list of available devices" <<
    std::endl;
  std::cout <<
    "--info      [-i] Print general device info" <<
    std::endl;
  std::cout <<
    "--metrics   [-m] Print list of available mertrics" <<
    std::endl;
  std::cout <<
    "--device    [-d] Target device id" <<
    std::endl;
  std::cout <<
    "--subdevice [-s] Target subdevice id" <<
    std::endl;
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
        std::cout << "[Warning] Unable to open metric device" << std::endl;
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
          std::cout << "[Warning] Unable to open metric device" << std::endl;
          return;
        }
        std::cout << "\tSubdevice " << j << ": " <<
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
  std::cout << "\tName: " <<
    device->GetParams()->DeviceName << std::endl;
  PTI_ASSERT(device->GetParams()->GlobalSymbolsCount > 0);
  for (uint32_t i = 0; i < device->GetParams()->GlobalSymbolsCount; ++i) {
    md::TGlobalSymbol_1_0* symbol = device->GetGlobalSymbol(i);
    if (symbol->SymbolTypedValue.ValueType == md::VALUE_TYPE_LAST) {
      continue;
    }

    std::cout << "\t" << symbol->SymbolName << ": ";
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
    std::cout << "\tMetric Group " << gid << ": " << group_name << std::endl;

    uint32_t set_count = group->GetParams()->MetricSetsCount;
    PTI_ASSERT(set_count > 0);
    for (uint32_t sid = 0; sid < set_count; ++sid) {
      md::IMetricSet_1_5* set = group->GetMetricSet(sid);
      PTI_ASSERT(set != nullptr);

      const char* set_name = set->GetParams()->SymbolName;
      std::cout << "\t\tMetric Set " << sid << ": " << set_name <<
        " (" << set->GetParams()->ShortName << ")" << std::endl;
      
      uint32_t metric_count = set->GetParams()->MetricsCount;
      PTI_ASSERT(metric_count > 0);
      for (uint32_t mid = 0; mid < metric_count; ++mid) {
        md::IMetric_1_0* metric = set->GetMetric(mid);
        PTI_ASSERT(metric != nullptr);
        std::cout << "\t\t\tMetric " << mid << ": " << group_name << " / " <<
          set_name << " / " << metric->GetParams()->SymbolName <<
          " (" << metric->GetParams()->ShortName << ") [" <<
          GetResultType(metric->GetParams()->ResultType) << "]" << std::endl;
      }

      uint32_t info_count = set->GetParams()->InformationCount;
      for (uint32_t iid = 0; iid < info_count; ++iid) {
        md::IInformation_1_0* info = set->GetInformation(iid);
        PTI_ASSERT(info != nullptr);
        std::cout << "\t\t\tInfo " << iid + metric_count << ": " <<
          group_name << " / " << set_name << " / " <<
          info->GetParams()->SymbolName << " (" <<
          info->GetParams()->ShortName << ")" << std::endl;
      }
    }
  }
}

static Options ParseArgs(int argc, char* argv[]) {
  Options options{GPU_LIST, 0, 0};

  int i = 1;
  while (i < argc) {
    std::string option(argv[i]);
    if (option == "--list"|| option == "-l") {
      options.mode = GPU_LIST;
      ++i;
    } else if (option == "--info"|| option == "-i") {
      options.mode = GPU_INFO;
      ++i;
    } else if (option == "--metrics" || option == "-m") {
      options.mode = GPU_METRICS;
      ++i;
    } else if (option == "--device" || option == "-d") {
      ++i;
      if (i < argc) {
        std::string value(argv[i]);
        if (value.find_first_not_of("0123456789") == std::string::npos) {
          options.device = std::stoul(value);
        }
        ++i;
      }
    } else if (option == "--subdevice" || option == "-s") {
      ++i;
      if (i < argc) {
        std::string value(argv[i]);
        if (value.find_first_not_of("0123456789") == std::string::npos) {
          options.sub_device = std::stoul(value);
        }
        ++i;
      }
    } else {
      ++i;
    }
  }

  return options;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    Usage();
    return 0;
  }

  Options options = ParseArgs(argc, argv);
  switch (options.mode) {
    case GPU_LIST:
      PrintDeviceList();
      break;
    case GPU_INFO: {
        MetricDevice* device =
          MetricDevice::Create(options.device, options.sub_device);
        if (device == nullptr) {
          std::cout << "[Warning] Unable to open metric device" << std::endl;
          return 0;
        }
        std::cout << "Device (" << options.device << ", " <<
          options.sub_device << "):" << std::endl;
        PrintDeviceInfo(*device);
        delete device;
      }
      break;
    case GPU_METRICS: {
        MetricDevice* device =
          MetricDevice::Create(options.device, options.sub_device);
        if (device == nullptr) {
          std::cout << "[Warning] Unable to open metric device" << std::endl;
          return 0;
        }
        std::cout << "Device (" << options.device << ", " <<
          options.sub_device << "):" << std::endl;
        PrintMetricsInfo(*device);
        delete device;
      }
      break;
    default:
      break;
  }

  return 0;
}