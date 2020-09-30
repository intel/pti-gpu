//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_
#define PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "ze_api_collector.h"
#include "ze_kernel_collector.h"
#include "utils.h"

#define ZEI_CALL_LOGGING 0
#define ZEI_HOST_TIMING  1
#define ZEI_DEVICE_TIMING 2

class ZeIntercept {
 public:
  static ZeIntercept* Create(ze_context_handle_t context,
                             ze_device_handle_t device,
                             unsigned options) {
    ZeIntercept* intercept = new ZeIntercept(options);

    ZeApiCollector* api_collector = nullptr;
    if (intercept->CheckOption(ZEI_CALL_LOGGING) ||
        intercept->CheckOption(ZEI_HOST_TIMING)) {
      bool call_tracing = intercept->CheckOption(ZEI_CALL_LOGGING);
      api_collector = ZeApiCollector::Create(context, call_tracing);
      if (api_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create API collector" <<
          " for target context" << std::endl;
        delete intercept;
        return nullptr;
      }
      intercept->api_collector_ = api_collector;
    }

    ZeKernelCollector* kernel_collector = nullptr;
    if (intercept->CheckOption(ZEI_DEVICE_TIMING)) {
      kernel_collector = ZeKernelCollector::Create(context, device);
      if (kernel_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create kernel collector" <<
          " for target context" << std::endl;
        delete intercept;
        return nullptr;
      }
      intercept->kernel_collector_ = kernel_collector;
    }

    return intercept;
  }

  ~ZeIntercept() {
    std::chrono::steady_clock::time_point end_time =
      std::chrono::steady_clock::now();
    std::chrono::duration<uint64_t, std::nano> duration =
      end_time - start_time_;
    total_execution_time_ = duration.count();

    if (api_collector_ != nullptr) {
      api_collector_->DisableTracing();
    }
    if (kernel_collector_ != nullptr) {
      kernel_collector_->DisableTracing();
    }
    Report();
    if (api_collector_ != nullptr) {
      delete api_collector_;
    }
    if (kernel_collector_ != nullptr) {
      delete kernel_collector_;
    }
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  ZeIntercept(const ZeIntercept& copy) = delete;
  ZeIntercept& operator=(const ZeIntercept& copy) = delete;

 private:
  ZeIntercept(unsigned options) : options_(options) {
    start_time_ = std::chrono::steady_clock::now();
  }

  void ReportHostTiming() {
    PTI_ASSERT(api_collector_ != nullptr);

    const FunctionInfoMap& function_info_map =
      api_collector_->GetFunctionInfoMap();
    if (function_info_map.size() == 0) {
      return;
    }

    uint64_t total_duration = 0;
    for (auto& value : function_info_map) {
      total_duration += value.second.total_time;
    }

    std::cerr << std::endl;
    std::cerr << "=== API Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Total Execution Time (ns): " <<
      total_execution_time_ << std::endl;
    std::cerr << "Total API Time (ns): " << total_duration << std::endl;
    std::cerr << std::endl;

    if (total_duration > 0) {
      ZeApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  void ReportDeviceTiming() {
    PTI_ASSERT(kernel_collector_ != nullptr);

    const KernelInfoMap& kernel_info_map =
      kernel_collector_->GetKernelInfoMap();
    if (kernel_info_map.size() == 0) {
      return;
    }

    uint64_t total_duration = 0;
    for (auto& value : kernel_info_map) {
      total_duration += value.second.total_time;
    }

    std::cerr << std::endl;
    std::cerr << "=== Device Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Total Execution Time (ns): " <<
      total_execution_time_ << std::endl;
    std::cerr << "Total Device Time (ns): " << total_duration << std::endl;
    std::cerr << std::endl;

    if (total_duration > 0) {
      ZeKernelCollector::PrintKernelsTable(kernel_info_map);
    }
  }
  
  void Report() {
    if (CheckOption(ZEI_HOST_TIMING)) {
      ReportHostTiming();
    }
    if (CheckOption(ZEI_DEVICE_TIMING)) {
      ReportDeviceTiming();
    }
  }

 private:
  unsigned options_;

  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* api_collector_ = nullptr;
  ZeKernelCollector* kernel_collector_ = nullptr;
};

#endif // PTI_SAMPLES_ZE_INTERCEPT_ZE_INTERCEPT_H_