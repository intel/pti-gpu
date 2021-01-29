//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_TRACER_ZE_TRACER_H_
#define PTI_SAMPLES_ZE_TRACER_ZE_TRACER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
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

#define ZET_CALL_LOGGING           0
#define ZET_HOST_TIMING            1
#define ZET_DEVICE_TIMING          2
#define ZET_DEVICE_TIMELINE        3
#define ZET_CHROME_DEVICE_TIMELINE 4
#define ZET_CHROME_CALL_LOGGING    5

const char* kChromeTraceFileName = "zet_trace.json";

class ZeTracer {
 public:
  static ZeTracer* Create(
      ze_driver_handle_t driver,
      ze_device_handle_t device,
      unsigned options) {
    PTI_ASSERT(driver != nullptr);
    PTI_ASSERT(device != nullptr);

    ZeTracer* tracer = new ZeTracer(options);

    ZeApiCollector* api_collector = nullptr;
    if (tracer->CheckOption(ZET_CALL_LOGGING) ||
        tracer->CheckOption(ZET_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(ZET_HOST_TIMING)) {

      OnFunctionFinishCallback callback = nullptr;
      if (tracer->CheckOption(ZET_CHROME_CALL_LOGGING)) {
        callback = ChromeLoggingCallback;
      }

      bool call_tracing = tracer->CheckOption(ZET_CALL_LOGGING);
      api_collector = ZeApiCollector::Create(
          tracer->start_time_, call_tracing,
          callback, tracer);
      if (api_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create API collector" << std::endl;
        delete tracer;
        return nullptr;
      }
      tracer->api_collector_ = api_collector;
    }

    ZeKernelCollector* kernel_collector = nullptr;
    if (tracer->CheckOption(ZET_DEVICE_TIMELINE) ||
        tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(ZET_DEVICE_TIMING)) {

      OnKernelFinishCallback callback = nullptr;
      if (tracer->CheckOption(ZET_DEVICE_TIMELINE) &&
          tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE)) {
        callback = DeviceAndChromeTimelineCallback;
      } else if (tracer->CheckOption(ZET_DEVICE_TIMELINE)) {
        callback = DeviceTimelineCallback;
      } else if (tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE)) {
        callback = ChromeTimelineCallback;
      }

      kernel_collector = ZeKernelCollector::Create(
          tracer->start_time_, callback, tracer);
      if (kernel_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create kernel collector" <<
          std::endl;
        delete tracer;
        return nullptr;
      }
      tracer->kernel_collector_ = kernel_collector;
    }

    return tracer;
  }

  ~ZeTracer() {
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

    if (chrome_trace_.is_open()) {
      CloseTraceFile();
    }
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  ZeTracer(const ZeTracer& copy) = delete;
  ZeTracer& operator=(const ZeTracer& copy) = delete;

 private:
  ZeTracer(unsigned options) : options_(options) {
    start_time_ = std::chrono::steady_clock::now();

    if (CheckOption(ZET_CHROME_DEVICE_TIMELINE) ||
        CheckOption(ZET_CHROME_CALL_LOGGING)) {
      OpenTraceFile();
    }
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
    if (CheckOption(ZET_HOST_TIMING)) {
      ReportHostTiming();
    }
    if (CheckOption(ZET_DEVICE_TIMING)) {
      ReportDeviceTiming();
    }
    std::cerr << std::endl;
  }

  static void DeviceTimelineCallback(
      void* data, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    std::cerr << "Device Timeline for " << name << " [ns] = " <<
      appended << " (append) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;
  }

  void OpenTraceFile() {
    chrome_trace_.open(kChromeTraceFileName);
    PTI_ASSERT(chrome_trace_.is_open());
    chrome_trace_ << "[" << std::endl;
    chrome_trace_ <<
      "{\"ph\":\"M\", \"name\":\"process_name\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":0, \"args\":{\"name\":\"" <<
      utils::GetExecutableName() << "\"}}," << std::endl;
  }

  void CloseTraceFile() {
    PTI_ASSERT(chrome_trace_.is_open());
    chrome_trace_.close();
    std::cerr << "Timeline was stored to " <<
      kChromeTraceFileName << std::endl;
  }

  static void ChromeTimelineCallback(
      void* data, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    tracer->chrome_trace_ << "{\"ph\":\"X\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":0, \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      "}," << std::endl;
  }

  static void DeviceAndChromeTimelineCallback(
      void* data, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(data, name, appended, submitted, started, ended);
    ChromeTimelineCallback(data, name, appended, submitted, started, ended);
  }

  static void ChromeLoggingCallback(
      void* data, const std::string& name,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    tracer->chrome_trace_ << "{\"ph\":\"X\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":" << utils::GetTid() <<
      ", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      "}," << std::endl;
  }

 private:
  unsigned options_;

  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* api_collector_ = nullptr;
  ZeKernelCollector* kernel_collector_ = nullptr;

  std::ofstream chrome_trace_;
};

#endif // PTI_SAMPLES_ZE_TRACER_ZE_TRACER_H_