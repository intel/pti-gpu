//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_METRIC_DEVICE_H_
#define PTI_SAMPLES_UTILS_METRIC_DEVICE_H_

#include <string.h>

#include "metric_utils.h"
#include "pti_assert.h"
#include "shared_library.h"

namespace md = MetricsDiscovery;

class MetricDevice {
 public:
  static MetricDevice* Create() {
    SharedLibrary* lib = nullptr;

    for (auto& path : utils::metrics::GetMDLibraryPossiblePaths()) {
      lib = SharedLibrary::Create(path);
      if (lib != nullptr) {
        break;
      }
    }

    if (lib != nullptr) {
      md::IMetricsDevice_1_5* device = nullptr;

      md::OpenMetricsDevice_fn OpenMetricsDevice =
        lib->GetSym<md::OpenMetricsDevice_fn>("OpenMetricsDevice");
      PTI_ASSERT(OpenMetricsDevice != nullptr);
      md::TCompletionCode status = OpenMetricsDevice(&device);
        PTI_ASSERT(status == md::CC_OK ||
               status == md::CC_ALREADY_INITIALIZED);

      if (device != nullptr) {
        return new MetricDevice(device, lib);
      } else {
        delete lib;	
      }
    }

    return nullptr;
  }

  ~MetricDevice() {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(lib_ != nullptr);

    md::CloseMetricsDevice_fn CloseMetricsDevice =
      lib_->GetSym<md::CloseMetricsDevice_fn>("CloseMetricsDevice");
    PTI_ASSERT(CloseMetricsDevice != nullptr);
    md::TCompletionCode status = CloseMetricsDevice(device_);
    PTI_ASSERT(status == md::CC_OK);

    delete lib_;
  }

  md::IMetricsDevice_1_5* operator->() const {
    return device_;
  }

  MetricDevice(const MetricDevice& copy) = delete;
  MetricDevice& operator=(const MetricDevice& copy) = delete;

  md::IConcurrentGroup_1_5* FindMetricGroup(const char* set_name) {
    PTI_ASSERT(set_name != nullptr);

    uint32_t group_count = device_->GetParams()->ConcurrentGroupsCount;
    for (uint32_t gid = 0; gid < group_count; ++gid) {
      md::IConcurrentGroup_1_5* group = device_->GetConcurrentGroup(gid);
      PTI_ASSERT(group != nullptr);

      uint32_t set_count = group->GetParams()->MetricSetsCount;
      for (uint32_t sid = 0; sid < set_count; ++sid) {
        md::IMetricSet_1_5* set = group->GetMetricSet(sid);
        PTI_ASSERT(set != nullptr);

        if (strcmp(set_name, set->GetParams()->SymbolName) == 0) {
          return group;
        }
      }
    }

    return nullptr;
  }

  md::IMetricSet_1_5* FindMetricSet(const char* set_name) {
    PTI_ASSERT(set_name != nullptr);

    uint32_t group_count = device_->GetParams()->ConcurrentGroupsCount;
    for (uint32_t gid = 0; gid < group_count; ++gid) {
      md::IConcurrentGroup_1_5* group = device_->GetConcurrentGroup(gid);
      PTI_ASSERT(group != nullptr);

      uint32_t set_count = group->GetParams()->MetricSetsCount;
      for (uint32_t sid = 0; sid < set_count; ++sid) {
        md::IMetricSet_1_5* set = group->GetMetricSet(sid);
        PTI_ASSERT(set != nullptr);

        if (strcmp(set_name, set->GetParams()->SymbolName) == 0) {
          return set;
        }
      }
    }

    return nullptr;
  }

private:
  MetricDevice(md::IMetricsDevice_1_5* device, SharedLibrary* lib)
      : device_(device), lib_(lib) {}

  md::IMetricsDevice_1_5* device_ = nullptr;
  SharedLibrary* lib_ = nullptr;
};

#endif // PTI_SAMPLES_UTILS_METRIC_DEVICE_H_