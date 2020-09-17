//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_METRIC_DEVICE_H_
#define PTI_SAMPLES_UTILS_METRIC_DEVICE_H_

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

private:
  MetricDevice(md::IMetricsDevice_1_5* device, SharedLibrary* lib)
      : device_(device), lib_(lib) {}

  md::IMetricsDevice_1_5* device_ = nullptr;
  SharedLibrary* lib_ = nullptr;
};

#endif // PTI_SAMPLES_UTILS_METRIC_DEVICE_H_