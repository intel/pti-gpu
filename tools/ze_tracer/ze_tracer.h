//==============================================================
// Copyright (C) Intel Corporation
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
#include <sstream>
#include <string>

#include "correlator.h"
#include "logger.h"
#include "ze_api_collector.h"
#include "ze_kernel_collector.h"
#include "utils.h"

#define ZET_CALL_LOGGING           0
#define ZET_HOST_TIMING            1
#define ZET_DEVICE_TIMING          2
#define ZET_DEVICE_TIMELINE        3
#define ZET_CHROME_CALL_LOGGING    4
#define ZET_CHROME_DEVICE_TIMELINE 5
#define ZET_CHROME_DEVICE_STAGES   6
#define ZET_TID                    7
#define ZET_PID                    8

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

    ZeKernelCollector* kernel_collector = nullptr;
    if (tracer->CheckOption(ZET_DEVICE_TIMING) ||
        tracer->CheckOption(ZET_DEVICE_TIMELINE) ||
        tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(ZET_CHROME_DEVICE_STAGES)) {

      PTI_ASSERT(!(tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE) &&
                   tracer->CheckOption(ZET_CHROME_DEVICE_STAGES)));

      OnZeKernelFinishCallback callback = nullptr;
      if (tracer->CheckOption(ZET_DEVICE_TIMELINE) &&
          tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE)) {
        callback = DeviceAndChromeTimelineCallback;
      } else if (tracer->CheckOption(ZET_DEVICE_TIMELINE) &&
                 tracer->CheckOption(ZET_CHROME_DEVICE_STAGES)) {
        callback = DeviceAndChromeStagesCallback;
      } else if (tracer->CheckOption(ZET_DEVICE_TIMELINE)) {
        callback = DeviceTimelineCallback;
      } else if (tracer->CheckOption(ZET_CHROME_DEVICE_TIMELINE)) {
        callback = ChromeTimelineCallback;
      } else if (tracer->CheckOption(ZET_CHROME_DEVICE_STAGES)) {
        callback = ChromeStagesCallback;
      }

      kernel_collector = ZeKernelCollector::Create(
          &(tracer->correlator_), callback, tracer);
      if (kernel_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create kernel collector" <<
          std::endl;
        delete tracer;
        return nullptr;
      }
      tracer->kernel_collector_ = kernel_collector;
    }

    ZeApiCollector* api_collector = nullptr;
    if (tracer->CheckOption(ZET_CALL_LOGGING) ||
        tracer->CheckOption(ZET_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(ZET_HOST_TIMING)) {

      OnZeFunctionFinishCallback callback = nullptr;
      if (tracer->CheckOption(ZET_CHROME_CALL_LOGGING)) {
        callback = ChromeLoggingCallback;
      }

      ApiCollectorOptions options{false, false, false};
      options.call_tracing = tracer->CheckOption(ZET_CALL_LOGGING);
      options.need_tid = tracer->CheckOption(ZET_TID);
      options.need_pid = tracer->CheckOption(ZET_PID);

      api_collector = ZeApiCollector::Create(
          &(tracer->correlator_), options, callback, tracer);
      if (api_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create API collector" << std::endl;
        delete tracer;
        return nullptr;
      }
      tracer->api_collector_ = api_collector;
    }

    return tracer;
  }

  ~ZeTracer() {
    total_execution_time_ = correlator_.GetTimestamp();

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

    if (chrome_logger_ != nullptr) {
      delete chrome_logger_;
      std::cerr << "Timeline was stored to " <<
        kChromeTraceFileName << std::endl;
    }
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  ZeTracer(const ZeTracer& copy) = delete;
  ZeTracer& operator=(const ZeTracer& copy) = delete;

 private:
  ZeTracer(unsigned options) : options_(options) {
    if (CheckOption(ZET_CHROME_CALL_LOGGING) ||
        CheckOption(ZET_CHROME_DEVICE_TIMELINE) ||
        CheckOption(ZET_CHROME_DEVICE_STAGES) ) {
      chrome_logger_ = new Logger(kChromeTraceFileName);
      PTI_ASSERT(chrome_logger_ != nullptr);

      std::stringstream stream;
      stream << "[" << std::endl;
      stream << "{\"ph\":\"M\", \"name\":\"process_name\", \"pid\":" <<
        utils::GetPid() << ", \"tid\":0, \"args\":{\"name\":\"" <<
        utils::GetExecutableName() << "\"}}," << std::endl;

      chrome_logger_->Log(stream.str().c_str());
    }
  }

  void ReportHostTiming() {
    PTI_ASSERT(api_collector_ != nullptr);

    const ZeFunctionInfoMap& function_info_map =
      api_collector_->GetFunctionInfoMap();
    if (function_info_map.size() == 0) {
      return;
    }

    uint64_t total_duration = 0;
    for (auto& value : function_info_map) {
      total_duration += value.second.total_time;
    }

    std::string title = "Total Execution Time (ns): ";
    const size_t title_width = title.size();
    const size_t time_width = 20;

    std::cerr << std::endl;
    std::cerr << "=== API Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::setw(title_width) << title <<
      std::setw(time_width) << total_execution_time_ << std::endl;
    std::cerr << std::setw(title_width) << "Total API Time (ns): " <<
      std::setw(time_width) << total_duration << std::endl;
    std::cerr << std::endl;

    if (total_duration > 0) {
      ZeApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  void ReportDeviceTiming() {
    PTI_ASSERT(kernel_collector_ != nullptr);

    const ZeKernelInfoMap& kernel_info_map =
      kernel_collector_->GetKernelInfoMap();
    if (kernel_info_map.size() == 0) {
      return;
    }

    uint64_t total_duration = 0;
    for (auto& value : kernel_info_map) {
      total_duration += value.second.total_time;
    }

    std::string title = "Total Execution Time (ns): ";
    const size_t title_width = title.size();
    const size_t time_width = 20;

    std::cerr << std::endl;
    std::cerr << "=== Device Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::setw(title_width) << title <<
      std::setw(time_width) << total_execution_time_ << std::endl;
    std::cerr << std::setw(title_width) << "Total Device Time (ns): " <<
      std::setw(time_width) << total_duration << std::endl;
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
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    std::stringstream stream;
    if (tracer->CheckOption(ZET_PID)) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << "(" << id << ") [ns] = " <<
      appended << " (append) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;
    tracer->logger_.Log(stream.str().c_str());
  }

  static void ChromeTimelineCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << reinterpret_cast<uint64_t>(queue) <<
      ", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;

    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    tracer->chrome_logger_->Log(stream.str().c_str());
  }

  static void ChromeStagesCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    std::stringstream stream;

    std::string tid = id + "." +
      std::to_string(reinterpret_cast<uint64_t>(queue));

    PTI_ASSERT(submitted > appended);
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << tid <<
      ", \"name\":\"" << name << " (Appended)" <<
      "\", \"ts\": " << appended / NSEC_IN_USEC <<
      ", \"dur\":" << (submitted - appended) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_runnable\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str().c_str());
    stream.str(std::string());

    PTI_ASSERT(started > submitted);
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << tid <<
      ", \"name\":\"" << name << " (Submitted)" <<
      "\", \"ts\": " << submitted / NSEC_IN_USEC <<
      ", \"dur\":" << (started - submitted) / NSEC_IN_USEC <<
      ", \"cname\":\"cq_build_running\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str().c_str());
    stream.str(std::string());

    PTI_ASSERT(ended > started);
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << tid <<
      ", \"name\":\"" << name << " (Execution)" <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_iowait\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str().c_str());
  }

  static void DeviceAndChromeTimelineCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void DeviceAndChromeStagesCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeStagesCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void ChromeLoggingCallback(
      void* data, const std::string& id, const std::string& name,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":" <<
      utils::GetPid() << ", \"tid\":" << utils::GetTid() <<
      ", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    tracer->chrome_logger_->Log(stream.str().c_str());
  }

 private:
  unsigned options_;

  Logger* chrome_logger_ = nullptr;
  Logger logger_;

  Correlator correlator_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* api_collector_ = nullptr;
  ZeKernelCollector* kernel_collector_ = nullptr;
};

#endif // PTI_SAMPLES_ZE_TRACER_ZE_TRACER_H_