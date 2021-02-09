//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_OMP_HOT_REGIONS_OMP_REGION_COLLECTOR_H_
#define PTI_SAMPLES_OMP_HOT_REGIONS_OMP_REGION_COLLECTOR_H_

#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include "pti_assert.h"
#include "utils.h"

enum RegionType {
  REGION_TYPE_PARALLEL,
  REGION_TYPE_TARGET,
  REGION_TYPE_TRANSFER_TO_DEVICE,
  REGION_TYPE_TRANSFER_FROM_DEVICE
};

struct RegionInfo {
  RegionType type;
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;
  size_t bytes_transferred;

  bool operator>(const RegionInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const RegionInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using RegionMap = std::map<uint64_t, RegionInfo>;

class OmpRegionCollector {
 public: // Interface
  static OmpRegionCollector* Create() {
    return new OmpRegionCollector();
  }

  ~OmpRegionCollector() {}

  void AddRegion(
      uint64_t ra, RegionType type, uint64_t time, size_t bytes_transferred) {
    const std::lock_guard<std::mutex> lock(lock_);
    uint64_t id = ra + type;
    if (region_map_.count(id) == 0) {
      region_map_[id] = {type, time, time, time, 1, bytes_transferred};
    } else {
      RegionInfo& region = region_map_[id];
      PTI_ASSERT(region.type == type);
      region.total_time += time;
      if (time < region.min_time) {
        region.min_time = time;
      }
      if (time > region.max_time) {
        region.max_time = time;
      }
      region.call_count += 1;
      region.bytes_transferred += bytes_transferred;
    }
  }

  const RegionMap& GetRegionMap() const {
    return region_map_;
  }

  static void PrintRegionTable(const RegionMap& region_map) {
    std::set< std::pair<uint64_t, RegionInfo>,
              utils::Comparator > sorted_list(
        region_map.begin(), region_map.end());

    uint64_t total_duration = 0;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
    }

    if (total_duration == 0) {
      return;
    }

    std::cerr << std::setw(kRegionIDLength) << "Region ID" << "," <<
      std::setw(kRegionTypeLength) << "Region Type" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kTransferredLength) <<
        "Transferred (bytes)" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      uint64_t id = value.first;
      std::string type = GetTypeString(value.second.type);
      uint64_t call_count = value.second.call_count;
      size_t bytes_transferred = value.second.bytes_transferred;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(kRegionIDLength) << id << "," <<
        std::setw(kRegionTypeLength) << type << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kTransferredLength) <<
          bytes_transferred << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kTimeLength) << min_duration << "," <<
        std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

 private: // Implementation Details
  OmpRegionCollector() {}

  static const char* GetTypeString(RegionType type) {
    switch(type) {
      case REGION_TYPE_PARALLEL:
        return "Parallel";
      case REGION_TYPE_TARGET:
        return "Target";
      case REGION_TYPE_TRANSFER_FROM_DEVICE:
        return "TransferFromDevice";
      case REGION_TYPE_TRANSFER_TO_DEVICE:
        return "TransferToDevice";
      default:
        PTI_ASSERT(0);
        break;
    }
    return "";
  }

 private: // Data
  RegionMap region_map_;
  std::mutex lock_;

  static const uint32_t kRegionIDLength = 20;
  static const uint32_t kRegionTypeLength = 20;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTransferredLength = 20;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_OMP_HOT_REGIONS_OMP_REGION_COLLECTOR_H_