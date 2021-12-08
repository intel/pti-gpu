//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_METRIC_DEVICE_H_
#define PTI_UTILS_METRIC_DEVICE_H_

#include <string.h>

#include "metric_utils.h"
#include "pti_assert.h"
#include "shared_library.h"
#include "utils.h"

namespace md = MetricsDiscovery;

class MetricDevice {
 public:
  static uint32_t GetDeviceCount() {
    SharedLibrary* lib = OpenMetricsLibrary();
    if (lib == nullptr) {
      return 0;
    }

    md::OpenAdapterGroup_fn OpenAdapterGroup =
      lib->GetSym<md::OpenAdapterGroup_fn>("OpenAdapterGroup");
    PTI_ASSERT(OpenAdapterGroup != nullptr);

    md::IAdapterGroupLatest* adapter_group = nullptr;
    md::TCompletionCode status = OpenAdapterGroup(&adapter_group);
    PTI_ASSERT(status == md::CC_OK);
    PTI_ASSERT(adapter_group != nullptr);

    uint32_t device_count = adapter_group->GetParams()->AdapterCount;
    delete lib;
    return device_count;
  }

  static uint32_t GetSubDeviceCount(uint32_t device_id) {
    SharedLibrary* lib = OpenMetricsLibrary();
    if (lib == nullptr) {
      return 0;
    }

    md::OpenAdapterGroup_fn OpenAdapterGroup =
      lib->GetSym<md::OpenAdapterGroup_fn>("OpenAdapterGroup");
    PTI_ASSERT(OpenAdapterGroup != nullptr);

    md::IAdapterGroupLatest* adapter_group = nullptr;
    md::TCompletionCode status = OpenAdapterGroup(&adapter_group);
    PTI_ASSERT(status == md::CC_OK);
    PTI_ASSERT(adapter_group != nullptr);

    uint32_t sub_device_count = 0;
    if (device_id < adapter_group->GetParams()->AdapterCount) {
      md::IAdapterLatest* adapter = adapter_group->GetAdapter(device_id);
      PTI_ASSERT(adapter != nullptr);
      sub_device_count = adapter->GetParams()->SubDevicesCount;
    }

    delete lib;
    return sub_device_count;
  }

  static MetricDevice* Create(uint32_t device_id, uint32_t sub_device_id) {
    SharedLibrary* lib = OpenMetricsLibrary();
    if (lib == nullptr) {
      return nullptr;
    }

    md::IMetricsDeviceLatest* device = nullptr;
    md::TCompletionCode status = md::CC_OK;

    md::OpenAdapterGroup_fn OpenAdapterGroup =
      lib->GetSym<md::OpenAdapterGroup_fn>("OpenAdapterGroup");
    PTI_ASSERT(OpenAdapterGroup != nullptr);

    md::IAdapterGroupLatest* adapter_group = nullptr;
    status = OpenAdapterGroup(&adapter_group);
    PTI_ASSERT(status == md::CC_OK);
    PTI_ASSERT(adapter_group != nullptr);

    if (adapter_group->GetParams()->AdapterCount == 0) {
      delete lib;
      return nullptr;
    }

    PTI_ASSERT(device_id < adapter_group->GetParams()->AdapterCount);
    md::IAdapterLatest* adapter = adapter_group->GetAdapter(device_id);
    PTI_ASSERT(adapter != nullptr);

    uint32_t sub_device_count = adapter->GetParams()->SubDevicesCount;
    if (sub_device_count == 0) {
      status = adapter->OpenMetricsDevice(&device);
    } else {
      PTI_ASSERT(sub_device_id < sub_device_count);
      status = adapter->OpenMetricsSubDevice(sub_device_id, &device);
    }
    PTI_ASSERT(status == md::CC_OK || status == md::CC_ALREADY_INITIALIZED);

    return new MetricDevice(adapter_group, adapter, device, lib);
  }

  ~MetricDevice() {
    PTI_ASSERT(adapter_group_ != nullptr);
    PTI_ASSERT(adapter_ != nullptr);
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(lib_ != nullptr);

    md::TCompletionCode status = md::CC_OK;

    status = adapter_->CloseMetricsDevice(device_);
    PTI_ASSERT(status == md::CC_OK);
    status = adapter_group_->Close();
    PTI_ASSERT(status == md::CC_OK);

    delete lib_;
  }

  md::IMetricsDeviceLatest* operator->() const {
    return device_;
  }

  MetricDevice(const MetricDevice& copy) = delete;
  MetricDevice& operator=(const MetricDevice& copy) = delete;

  md::IConcurrentGroupLatest* FindMetricGroup(const char* set_name) {
    PTI_ASSERT(set_name != nullptr);

    uint32_t group_count = device_->GetParams()->ConcurrentGroupsCount;
    for (uint32_t gid = 0; gid < group_count; ++gid) {
      md::IConcurrentGroupLatest* group = device_->GetConcurrentGroup(gid);
      PTI_ASSERT(group != nullptr);

      uint32_t set_count = group->GetParams()->MetricSetsCount;
      for (uint32_t sid = 0; sid < set_count; ++sid) {
        md::IMetricSetLatest* set = group->GetMetricSet(sid);
        PTI_ASSERT(set != nullptr);

        if (strcmp(set_name, set->GetParams()->SymbolName) == 0) {
          return group;
        }
      }
    }

    return nullptr;
  }

  md::IMetricSetLatest* FindMetricSet(const char* set_name) {
    PTI_ASSERT(set_name != nullptr);

    uint32_t group_count = device_->GetParams()->ConcurrentGroupsCount;
    for (uint32_t gid = 0; gid < group_count; ++gid) {
      md::IConcurrentGroupLatest* group = device_->GetConcurrentGroup(gid);
      PTI_ASSERT(group != nullptr);

      uint32_t set_count = group->GetParams()->MetricSetsCount;
      for (uint32_t sid = 0; sid < set_count; ++sid) {
        md::IMetricSetLatest* set = group->GetMetricSet(sid);
        PTI_ASSERT(set != nullptr);

        if (strcmp(set_name, set->GetParams()->SymbolName) == 0) {
          return set;
        }
      }
    }

    return nullptr;
  }

private:
  static SharedLibrary* OpenMetricsLibrary() {
    SharedLibrary* lib = nullptr;
    for (auto& path : utils::metrics::GetMDLibraryPossiblePaths()) {
      lib = SharedLibrary::Create(path);
      if (lib != nullptr) {
        break;
      }
    }
    return lib;
  }

  MetricDevice(
      md::IAdapterGroupLatest* adapter_group, md::IAdapterLatest* adapter,
      md::IMetricsDeviceLatest* device, SharedLibrary* lib)
      : adapter_group_(adapter_group), adapter_(adapter),
        device_(device), lib_(lib) {}

  md::IAdapterGroupLatest* adapter_group_ = nullptr;
  md::IAdapterLatest* adapter_ = nullptr;
  md::IMetricsDeviceLatest* device_ = nullptr;
  SharedLibrary* lib_ = nullptr;
};

#endif // PTI_UTILS_METRIC_DEVICE_H_