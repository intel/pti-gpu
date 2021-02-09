//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <string.h>

#include <iostream>

#include <MD/metrics_discovery_api.h>

#include "metric_device.h"

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
  std::cout << "Device Information:" << std::endl;
  std::cout << "\tDevice Name: " <<
    device->GetParams()->DeviceName << std::endl;
  PTI_ASSERT(device->GetParams()->GlobalSymbolsCount > 0);
  for (uint32_t i = 0; i < device->GetParams()->GlobalSymbolsCount; ++i) {
    md::TGlobalSymbol_1_0* symbol = device->GetGlobalSymbol(i);
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
  std::cout << "Available Metrics:" << std::endl;

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

int main(int argc, char* argv[]) {
  MetricDevice* device = MetricDevice::Create();
  if (device == nullptr) {
    std::cout << "[Warning] Unable to find MD library" << std::endl;
    return 0;
  }

  if (argc > 1) {
    if (strcmp(argv[1], "-d") == 0) {
      PrintDeviceInfo(*device);
    } else if (strcmp(argv[1], "-m") == 0) {
      PrintMetricsInfo(*device);    
    } else {
      std::cout << "Unknown option: " << argv[1] << std::endl;
    }
  } else {
    PrintDeviceInfo(*device);
  }

  delete device;
  std::cout << "[INFO] Job is successfully completed" << std::endl;

  return 0;
}