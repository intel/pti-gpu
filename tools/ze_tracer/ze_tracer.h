//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_TRACER_ZE_TRACER_H_
#define PTI_TOOLS_ZE_TRACER_ZE_TRACER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "correlator.h"
#include "trace_options.h"
#include "utils.h"
#include "ze_api_collector.h"
#include "ze_kernel_collector.h"

const char* kChromeTraceFileName = "zet_trace";

class ZeTracer {
 public:
  static ZeTracer* Create(const TraceOptions& options) {
    ZeTracer* tracer = new ZeTracer(options);
    PTI_ASSERT(tracer != nullptr);

    ZeKernelCollector* kernel_collector = nullptr;
    if (tracer->CheckOption(TRACE_DEVICE_TIMING) ||
        tracer->CheckOption(TRACE_KERNEL_SUBMITTING) ||
        tracer->CheckOption(TRACE_DEVICE_TIMELINE) ||
        tracer->CheckOption(TRACE_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE) ||
        tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)) {

      PTI_ASSERT(!(tracer->CheckOption(TRACE_CHROME_DEVICE_TIMELINE) &&
                   tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)));
      PTI_ASSERT(!(tracer->CheckOption(TRACE_CHROME_DEVICE_TIMELINE) &&
                   tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE)));

      OnZeKernelFinishCallback callback = nullptr;
      if (tracer->CheckOption(TRACE_DEVICE_TIMELINE) &&
          tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE) &&
          tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)) {
        callback = DeviceAndChromeKernelStagesCallback;
      } else if (tracer->CheckOption(TRACE_DEVICE_TIMELINE) &&
                 tracer->CheckOption(TRACE_CHROME_DEVICE_TIMELINE)) {
        callback = DeviceAndChromeDeviceCallback;
      } else if (tracer->CheckOption(TRACE_DEVICE_TIMELINE) &&
                 tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE)) {
        callback = DeviceAndChromeKernelCallback;
      } else if (tracer->CheckOption(TRACE_DEVICE_TIMELINE) &&
                 tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)) {
        callback = DeviceAndChromeStagesCallback;
      } else if (tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE) &&
                 tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)) {
        callback = ChromeKernelStagesCallback;
      } else if (tracer->CheckOption(TRACE_DEVICE_TIMELINE)) {
        callback = DeviceTimelineCallback;
      } else if (tracer->CheckOption(TRACE_CHROME_DEVICE_TIMELINE)) {
        callback = ChromeDeviceCallback;
      } else if (tracer->CheckOption(TRACE_CHROME_KERNEL_TIMELINE)) {
        callback = ChromeKernelCallback;
      } else if (tracer->CheckOption(TRACE_CHROME_DEVICE_STAGES)) {
        callback = ChromeStagesCallback;
      }

      KernelCollectorOptions kernel_options;
      kernel_options.verbose = tracer->CheckOption(TRACE_VERBOSE);
      kernel_options.demangle = tracer->CheckOption(TRACE_DEMANGLE);
      kernel_options.kernels_per_tile =
        tracer->CheckOption(TRACE_KERNELS_PER_TILE);

      kernel_collector = ZeKernelCollector::Create(
          &(tracer->correlator_), kernel_options, callback, tracer);
      if (kernel_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create kernel collector" <<
          std::endl;
        delete tracer;
        return nullptr;
      }
      tracer->kernel_collector_ = kernel_collector;
    }

    ZeApiCollector* api_collector = nullptr;
    if (tracer->CheckOption(TRACE_CALL_LOGGING) ||
        tracer->CheckOption(TRACE_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(TRACE_HOST_TIMING)) {

      OnZeFunctionFinishCallback callback = nullptr;
      if (tracer->CheckOption(TRACE_CHROME_CALL_LOGGING)) {
        callback = ChromeLoggingCallback;
      }

      ApiCollectorOptions api_options;
      api_options.call_tracing = tracer->CheckOption(TRACE_CALL_LOGGING);
      api_options.need_tid = tracer->CheckOption(TRACE_TID);
      api_options.need_pid = tracer->CheckOption(TRACE_PID);
      api_options.demangle = tracer->CheckOption(TRACE_DEMANGLE);

      api_collector = ZeApiCollector::Create(
          &(tracer->correlator_), api_options, callback, tracer);
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

    if (CheckOption(TRACE_LOG_TO_FILE)) {
      std::cerr << "[INFO] Log was stored to " <<
        options_.GetLogFileName() << std::endl;
    }

    if (chrome_logger_ != nullptr) {
      delete chrome_logger_;
      std::cerr << "[INFO] Timeline was stored to " <<
        chrome_trace_file_name_ << std::endl;
    }
  }

  bool CheckOption(unsigned option) {
    return options_.CheckFlag(option);
  }

  ZeTracer(const ZeTracer& copy) = delete;
  ZeTracer& operator=(const ZeTracer& copy) = delete;

 private:
  ZeTracer(const TraceOptions& options)
      : options_(options),
        correlator_(options.GetLogFileName(),
          CheckOption(TRACE_CONDITIONAL_COLLECTION)) {
#if !defined(_WIN32)
    uint64_t monotonic_time = utils::GetTime(CLOCK_MONOTONIC);
    uint64_t real_time = utils::GetTime(CLOCK_REALTIME);
#endif

    if (CheckOption(TRACE_CHROME_CALL_LOGGING) ||
        CheckOption(TRACE_CHROME_DEVICE_TIMELINE) ||
        CheckOption(TRACE_CHROME_KERNEL_TIMELINE) ||
        CheckOption(TRACE_CHROME_DEVICE_STAGES)) {
      chrome_trace_file_name_ =
        TraceOptions::GetChromeTraceFileName(kChromeTraceFileName);
      chrome_logger_ = new Logger(chrome_trace_file_name_.c_str());
      PTI_ASSERT(chrome_logger_ != nullptr);

      std::stringstream stream;
      stream << "[" << std::endl;
      stream << "{\"ph\":\"M\", \"name\":\"process_name\", \"pid\":\"" <<
        utils::GetPid() << "\", \"args\":{\"name\":\"" <<
        utils::GetExecutableName() << "\"}}," << std::endl;

      stream << "{\"ph\":\"M\", \"name\":\"start_time\", \"pid\":\"" <<
        utils::GetPid() << "\", \"args\":{";
#if defined(_WIN32)
      stream << "\"QueryPerformanceCounter\":\"" <<
        correlator_.GetStartPoint() << "\"";
#else
      stream << "\"CLOCK_MONOTONIC_RAW\":\"" <<
        correlator_.GetStartPoint() << "\", ";
      stream << "\"CLOCK_MONOTONIC\":\"" <<
        monotonic_time << "\", ";
      stream << "\"CLOCK_REALTIME\":\"" <<
        real_time << "\"";
#endif
      stream << "}}," << std::endl;

      chrome_logger_->Log(stream.str());
    }
    if (CheckOption(TRACE_DEVICE_TIMELINE)) {
      std::stringstream stream;
#if defined(_WIN32)
      stream <<
        "Device Timeline: start time (QueryPerformanceCounter) [ns] = " <<
        correlator_.GetStartPoint() << std::endl;
#else
      stream <<
        "Device Timeline: start time (CLOCK_MONOTONIC_RAW) [ns] = " <<
        correlator_.GetStartPoint() << std::endl;
      stream <<
        "Device Timeline: start time (CLOCK_MONOTONIC) [ns] = " <<
        monotonic_time << std::endl;
      stream <<
        "Device Timeline: start time (CLOCK_REALTIME) [ns] = " <<
        real_time << std::endl;
#endif
      correlator_.Log(stream.str());
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

    std::stringstream stream;
    stream << std::endl;
    stream << "=== API Timing Results: ===" << std::endl;
    stream << std::endl;
    stream << std::setw(title_width) << title <<
      std::setw(time_width) << total_execution_time_ << std::endl;
    stream << std::setw(title_width) << "Total API Time (ns): " <<
      std::setw(time_width) << total_duration << std::endl;
    stream << std::endl;
    correlator_.Log(stream.str());

    if (total_duration > 0) {
      api_collector_->PrintFunctionsTable();
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
      total_duration += value.second.execute_time;
    }

    std::string title = "Total Execution Time (ns): ";
    const size_t title_width = title.size();
    const size_t time_width = 20;

    std::stringstream stream;
    stream << std::endl;
    stream << "=== Device Timing Results: ===" << std::endl;
    stream << std::endl;
    stream << std::setw(title_width) << title <<
      std::setw(time_width) << total_execution_time_ << std::endl;
    stream << std::setw(title_width) << "Total Device Time (ns): " <<
      std::setw(time_width) << total_duration << std::endl;
    stream << std::endl;
    correlator_.Log(stream.str());

    if (total_duration > 0) {
      kernel_collector_->PrintKernelsTable();
    }
  }

  void ReportKernelSubmission() {
    PTI_ASSERT(kernel_collector_ != nullptr);

    const ZeKernelInfoMap& kernel_info_map =
      kernel_collector_->GetKernelInfoMap();
    if (kernel_info_map.size() == 0) {
      return;
    }

    uint64_t total_duration = 0;
    for (auto& value : kernel_info_map) {
      total_duration += value.second.execute_time;
    }

    std::string title = "Total Execution Time (ns): ";
    const size_t title_width = title.size();
    const size_t time_width = 20;

    std::stringstream stream;
    stream << std::endl;
    stream << "=== Kernel Submission Results: ===" << std::endl;
    stream << std::endl;
    stream << std::setw(title_width) << title <<
      std::setw(time_width) << total_execution_time_ << std::endl;
    stream << std::setw(title_width) << "Total Device Time (ns): " <<
      std::setw(time_width) << total_duration << std::endl;
    stream << std::endl;
    correlator_.Log(stream.str());

    if (total_duration > 0) {
      kernel_collector_->PrintSubmissionTable();
    }
  }
  
  void Report() {
    if (CheckOption(TRACE_HOST_TIMING)) {
      ReportHostTiming();
    }
    if (CheckOption(TRACE_DEVICE_TIMING)) {
      ReportDeviceTiming();
    }
    if (CheckOption(TRACE_KERNEL_SUBMITTING)) {
      ReportKernelSubmission();
    }
    correlator_.Log("\n");
  }

  static void DeviceTimelineCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    std::stringstream stream;
    if (tracer->CheckOption(TRACE_PID)) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << "<" << id << "> [ns] = " <<
      appended << " (append) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;
    tracer->correlator_.Log(stream.str());
  }

  static void ChromeDeviceCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << queue <<
      "\", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;

    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    tracer->chrome_logger_->Log(stream.str());
  }

  static void ChromeKernelCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << name <<
      "\", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;

    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    tracer->chrome_logger_->Log(stream.str());
  }

  static void ChromeStagesCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    std::stringstream stream;

    std::string tid = id + "." + queue;

    PTI_ASSERT(submitted > appended);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << tid <<
      "\", \"name\":\"" << name << " (Appended)" <<
      "\", \"ts\": " << appended / NSEC_IN_USEC <<
      ", \"dur\":" << (submitted - appended) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_runnable\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
    stream.str(std::string());

    PTI_ASSERT(started > submitted);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << tid <<
      "\", \"name\":\"" << name << " (Submitted)" <<
      "\", \"ts\": " << submitted / NSEC_IN_USEC <<
      ", \"dur\":" << (started - submitted) / NSEC_IN_USEC <<
      ", \"cname\":\"cq_build_running\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
    stream.str(std::string());

    PTI_ASSERT(ended > started);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << tid <<
      "\", \"name\":\"" << name << " (Executed)" <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_iowait\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
  }

  static void ChromeKernelStagesCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    std::stringstream stream;

    PTI_ASSERT(submitted > appended);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << name <<
      "\", \"name\":\"" << name << " (Appended)" <<
      "\", \"ts\": " << appended / NSEC_IN_USEC <<
      ", \"dur\":" << (submitted - appended) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_runnable\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
    stream.str(std::string());

    PTI_ASSERT(started > submitted);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << name <<
      "\", \"name\":\"" << name << " (Submitted)" <<
      "\", \"ts\": " << submitted / NSEC_IN_USEC <<
      ", \"dur\":" << (started - submitted) / NSEC_IN_USEC <<
      ", \"cname\":\"cq_build_running\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
    stream.str(std::string());

    PTI_ASSERT(ended > started);
    stream << "{\"ph\":\"X\", \"pid\":\"" << utils::GetPid() <<
      "\", \"tid\":\"" << name <<
      "\", \"name\":\"" << name << " (Executed)" <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"cname\":\"thread_state_iowait\"" <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    tracer->chrome_logger_->Log(stream.str());
  }

  static void DeviceAndChromeDeviceCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeDeviceCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void DeviceAndChromeKernelCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeKernelCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void DeviceAndChromeStagesCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeStagesCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void DeviceAndChromeKernelStagesCallback(
      void* data,
      const std::string& queue,
      const std::string& id,
      const std::string& name,
      uint64_t appended,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ChromeKernelStagesCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void ChromeLoggingCallback(
      void* data, const std::string& id, const std::string& name,
      uint64_t started, uint64_t ended) {
    ZeTracer* tracer = reinterpret_cast<ZeTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    std::stringstream stream;
    stream << "{\"ph\":\"X\", \"pid\":\"" <<
      utils::GetPid() << "\", \"tid\":\"" << utils::GetTid() <<
      "\", \"name\":\"" << name <<
      "\", \"ts\": " << started / NSEC_IN_USEC <<
      ", \"dur\":" << (ended - started) / NSEC_IN_USEC <<
      ", \"args\": {\"id\": \"" << id << "\"}"
      "}," << std::endl;
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    tracer->chrome_logger_->Log(stream.str());
  }

 private:
  TraceOptions options_;

  std::string chrome_trace_file_name_;
  Logger* chrome_logger_ = nullptr;

  Correlator correlator_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* api_collector_ = nullptr;
  ZeKernelCollector* kernel_collector_ = nullptr;
};

#endif // PTI_TOOLS_ZE_TRACER_ZE_TRACER_H_