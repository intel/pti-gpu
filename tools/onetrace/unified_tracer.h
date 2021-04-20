//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_
#define PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cl_api_collector.h"
#include "cl_api_callbacks.h"
#include "cl_kernel_collector.h"
#include "logger.h"
#include "utils.h"
#include "ze_api_collector.h"
#include "ze_kernel_collector.h"

#define ONETRACE_CALL_LOGGING           0
#define ONETRACE_HOST_TIMING            1
#define ONETRACE_DEVICE_TIMING          2
#define ONETRACE_DEVICE_TIMING_VERBOSE  3
#define ONETRACE_DEVICE_TIMELINE        4
#define ONETRACE_CHROME_CALL_LOGGING    5
#define ONETRACE_CHROME_DEVICE_TIMELINE 6
#define ONETRACE_CHROME_DEVICE_STAGES   7
#define ONETRACE_TID                    8
#define ONETRACE_PID                    9

const char* kChromeTraceFileName = "onetrace";
const char* kChromeTraceFileExt = "json";

class UnifiedTracer {
 public:
  static UnifiedTracer* Create(unsigned options) {
    cl_device_id cl_cpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_CPU);
    cl_device_id cl_gpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
    if (cl_cpu_device == nullptr && cl_gpu_device == nullptr) {
      std::cerr << "[WARNING] Intel OpenCL devices are not found" << std::endl;
      return nullptr;
    }

    UnifiedTracer* tracer = new UnifiedTracer(options);

    if (tracer->CheckOption(ONETRACE_DEVICE_TIMING) ||
        tracer->CheckOption(ONETRACE_DEVICE_TIMING_VERBOSE) ||
        tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) ||
        tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(ONETRACE_CHROME_DEVICE_STAGES)) {

      PTI_ASSERT(!(tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) &&
                   tracer->CheckOption(ONETRACE_CHROME_DEVICE_STAGES)));

      ZeKernelCollector* ze_kernel_collector = nullptr;
      ClKernelCollector* cl_cpu_kernel_collector = nullptr;
      ClKernelCollector* cl_gpu_kernel_collector = nullptr;

      OnZeKernelFinishCallback ze_callback = nullptr;
      OnClKernelFinishCallback cl_callback = nullptr;
      if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) &&
          tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE)) {
        ze_callback = ZeDeviceAndChromeTimelineCallback;
        cl_callback = ClDeviceAndChromeTimelineCallback;
      } else if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE) &&
                 tracer->CheckOption(ONETRACE_CHROME_DEVICE_STAGES)) {
        ze_callback = ZeDeviceAndChromeStagesCallback;
        cl_callback = ClDeviceAndChromeStagesCallback;
      } else if (tracer->CheckOption(ONETRACE_DEVICE_TIMELINE)) {
        ze_callback = ZeDeviceTimelineCallback;
        cl_callback = ClDeviceTimelineCallback;
      } else if (tracer->CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE)) {
        ze_callback = ZeChromeTimelineCallback;
        cl_callback = ClChromeTimelineCallback;
      } else if (tracer->CheckOption(ONETRACE_CHROME_DEVICE_STAGES)) {
        ze_callback = ZeChromeStagesCallback;
        cl_callback = ClChromeStagesCallback;
      }

      bool verbose = tracer->CheckOption(ONETRACE_DEVICE_TIMING_VERBOSE);

      ze_kernel_collector = ZeKernelCollector::Create(
          &tracer->correlator_, verbose, ze_callback, tracer);
      if (ze_kernel_collector == nullptr) {
        std::cerr <<
          "[WARNING] Unable to create kernel collector for L0 backend" <<
          std::endl;
      }
      tracer->ze_kernel_collector_ = ze_kernel_collector;

      if (cl_cpu_device != nullptr) {
        cl_cpu_kernel_collector = ClKernelCollector::Create(
            cl_cpu_device, &tracer->correlator_, verbose, cl_callback, tracer);
        if (cl_cpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for CL CPU backend" <<
            std::endl;
        }
        tracer->cl_cpu_kernel_collector_ = cl_cpu_kernel_collector;
      }

      if (cl_gpu_device != nullptr) {
        cl_gpu_kernel_collector = ClKernelCollector::Create(
            cl_gpu_device, &tracer->correlator_, verbose, cl_callback, tracer);
        if (cl_gpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for CL GPU backend" <<
            std::endl;
        }
        tracer->cl_gpu_kernel_collector_ = cl_gpu_kernel_collector;
      }

      if (ze_kernel_collector == nullptr &&
          cl_cpu_kernel_collector == nullptr &&
          cl_gpu_kernel_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    if (tracer->CheckOption(ONETRACE_CALL_LOGGING) ||
        tracer->CheckOption(ONETRACE_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(ONETRACE_HOST_TIMING)) {

      ZeApiCollector* ze_api_collector = nullptr;
      ClApiCollector* cl_cpu_api_collector = nullptr;
      ClApiCollector* cl_gpu_api_collector = nullptr;

      OnZeFunctionFinishCallback ze_callback = nullptr;
      OnClFunctionFinishCallback cl_callback = nullptr;
      if (tracer->CheckOption(ONETRACE_CHROME_CALL_LOGGING)) {
        ze_callback = ZeChromeLoggingCallback;
        cl_callback = ClChromeLoggingCallback;
      }

      ApiCollectorOptions options{false, false, false};
      options.call_tracing = tracer->CheckOption(ONETRACE_CALL_LOGGING);
      options.need_tid = tracer->CheckOption(ONETRACE_TID);
      options.need_pid = tracer->CheckOption(ONETRACE_PID);

      ze_api_collector = ZeApiCollector::Create(
          &tracer->correlator_, options, ze_callback, tracer);
      if (ze_api_collector == nullptr) {
        std::cerr << "[WARNING] Unable to create L0 API collector" <<
          std::endl;
      }
      tracer->ze_api_collector_ = ze_api_collector;

      if (cl_cpu_device != nullptr) {
        cl_cpu_api_collector = ClApiCollector::Create(
            cl_cpu_device, &tracer->correlator_,
            options, cl_callback, tracer);
        if (cl_cpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create CL API collector for CPU backend" <<
            std::endl;
        }
        tracer->cl_cpu_api_collector_ = cl_cpu_api_collector;
      }

      if (cl_gpu_device != nullptr) {
        cl_gpu_api_collector = ClApiCollector::Create(
            cl_gpu_device, &tracer->correlator_,
            options, cl_callback, tracer);
        if (cl_gpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create CL API collector for GPU backend" <<
            std::endl;
        }
        tracer->cl_gpu_api_collector_ = cl_gpu_api_collector;
      }

      if (ze_api_collector == nullptr &&
          cl_gpu_api_collector == nullptr &&
          cl_cpu_api_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    return tracer;
  }

  ~UnifiedTracer() {
    total_execution_time_ = correlator_.GetTimestamp();

    if (cl_cpu_api_collector_ != nullptr) {
      cl_cpu_api_collector_->DisableTracing();
    }
    if (cl_gpu_api_collector_ != nullptr) {
      cl_gpu_api_collector_->DisableTracing();
    }
    if (ze_api_collector_ != nullptr) {
      ze_api_collector_->DisableTracing();
    }

    if (cl_cpu_kernel_collector_ != nullptr) {
      cl_cpu_kernel_collector_->DisableTracing();
    }
    if (cl_gpu_kernel_collector_ != nullptr) {
      cl_gpu_kernel_collector_->DisableTracing();
    }

    Report();

    if (cl_cpu_api_collector_ != nullptr) {
      delete cl_cpu_api_collector_;
    }
    if (cl_gpu_api_collector_ != nullptr) {
      delete cl_gpu_api_collector_;
    }
    if (ze_api_collector_ != nullptr) {
      delete ze_api_collector_;
    }

    if (cl_cpu_kernel_collector_ != nullptr) {
      delete cl_cpu_kernel_collector_;
    }
    if (cl_gpu_kernel_collector_ != nullptr) {
      delete cl_gpu_kernel_collector_;
    }

    if (chrome_logger_ != nullptr) {
      delete chrome_logger_;
      std::cerr << "Timeline was stored to " <<
        chrome_trace_file_name_ << std::endl;
    }
  }

  bool CheckOption(unsigned option) {
    return (options_ & (1 << option));
  }

  UnifiedTracer(const UnifiedTracer& copy) = delete;
  UnifiedTracer& operator=(const UnifiedTracer& copy) = delete;

 private:
  UnifiedTracer(unsigned options) : options_(options) {
    if (CheckOption(ONETRACE_CHROME_CALL_LOGGING) ||
        CheckOption(ONETRACE_CHROME_DEVICE_TIMELINE) ||
        CheckOption(ONETRACE_CHROME_DEVICE_STAGES)) {
      chrome_trace_file_name_ =
        std::string(kChromeTraceFileName) +
          "." + std::to_string(utils::GetPid()) +
          "." + kChromeTraceFileExt;
      chrome_logger_ = new Logger(chrome_trace_file_name_.c_str());
      PTI_ASSERT(chrome_logger_ != nullptr);

      std::stringstream stream;
      stream << "[" << std::endl;
      stream << "{\"ph\":\"M\", \"name\":\"process_name\", \"pid\":" <<
        utils::GetPid() << ", \"tid\":0, \"args\":{\"name\":\"" <<
        utils::GetExecutableName() << "\"}}," << std::endl;
      stream << "{\"ph\":\"M\", \"name\":\"onetrace_start_time\", \"pid\":" <<
        utils::GetPid() << ", \"tid\":0, \"args\":{\"start_time\":\"" <<
        correlator_.GetStartPoint() << "\"}}," << std::endl;

      chrome_logger_->Log(stream.str().c_str());
    }
  }

  static uint64_t CalculateTotalTime(const ZeApiCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ZeFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
    if (function_info_map.size() != 0) {
      for (auto& value : function_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ZeKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ZeKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
    if (kernel_info_map.size() != 0) {
      for (auto& value : kernel_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ClApiCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ClFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
    if (function_info_map.size() != 0) {
      for (auto& value : function_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static uint64_t CalculateTotalTime(const ClKernelCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ClKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
    if (kernel_info_map.size() != 0) {
      for (auto& value : kernel_info_map) {
        total_time += value.second.total_time;
      }
    }

    return total_time;
  }

  static void PrintBackendTable(
      const ZeApiCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ZeFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
      PTI_ASSERT(function_info_map.size() > 0);
      ZeApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  static void PrintBackendTable(
      const ZeKernelCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ZeKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
      PTI_ASSERT(kernel_info_map.size() > 0);
      ZeKernelCollector::PrintKernelsTable(kernel_info_map);
    }
  }

  static void PrintBackendTable(
      const ClApiCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ClFunctionInfoMap& function_info_map = collector->GetFunctionInfoMap();
      PTI_ASSERT(function_info_map.size() > 0);
      ClApiCollector::PrintFunctionsTable(function_info_map);
    }
  }

  static void PrintBackendTable(
      const ClKernelCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalTime(collector);
    if (total_duration > 0) {
      std::cerr << std::endl;
      std::cerr << "== " << device_type << " Backend: ==" << std::endl;
      std::cerr << std::endl;

      const ClKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
      PTI_ASSERT(kernel_info_map.size() > 0);
      ClKernelCollector::PrintKernelsTable(kernel_info_map);
    }
  }

  template <class ZeCollector, class ClCollector>
  void ReportTiming(
      const ZeCollector* ze_collector,
      const ClCollector* cl_cpu_collector,
      const ClCollector* cl_gpu_collector,
      const char* type) {
    PTI_ASSERT(
        ze_collector != nullptr ||
        cl_cpu_collector != nullptr ||
        cl_gpu_collector != nullptr);

    std::string ze_title =
      std::string("Total ") + std::string(type) +
      " Time for L0 backend (ns): ";
    std::string cl_cpu_title =
      std::string("Total ") + std::string(type) +
      " Time for CL CPU backend (ns): ";
    std::string cl_gpu_title =
      std::string("Total ") + std::string(type) +
      " Time for CL GPU backend (ns): ";
    size_t title_width = std::max(cl_cpu_title.size(), cl_gpu_title.size());
    title_width = std::max(title_width, ze_title.size());
    const size_t time_width = 20;

    std::cerr << std::endl;
    std::cerr << "=== " << type << " Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::setw(title_width) << "Total Execution Time (ns): " <<
      std::setw(time_width) << total_execution_time_ << std::endl;

    if (ze_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(ze_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << ze_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }
    if (cl_cpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(cl_cpu_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << cl_cpu_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }
    if (cl_gpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalTime(cl_gpu_collector);
      if (total_time > 0) {
        std::cerr << std::setw(title_width) << cl_gpu_title <<
          std::setw(time_width) << total_time <<
          std::endl;
      }
    }

    if (ze_collector != nullptr) {
      PrintBackendTable(ze_collector, "L0");
    }
    if (cl_cpu_collector != nullptr) {
      PrintBackendTable(cl_cpu_collector, "CL CPU");
    }
    if (cl_gpu_collector != nullptr) {
      PrintBackendTable(cl_gpu_collector, "CL GPU");
    }

    std::cerr << std::endl;
  }

  void Report() {
    if (CheckOption(ONETRACE_HOST_TIMING)) {
      ReportTiming(
          ze_api_collector_,
          cl_cpu_api_collector_,
          cl_gpu_api_collector_,
          "API");
    }
    if (CheckOption(ONETRACE_DEVICE_TIMING) ||
        CheckOption(ONETRACE_DEVICE_TIMING_VERBOSE)) {
      ReportTiming(
          ze_kernel_collector_,
          cl_cpu_kernel_collector_,
          cl_gpu_kernel_collector_,
          "Device");
    }
    std::cerr << std::endl;
  }

  static void ZeDeviceTimelineCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    if (tracer->CheckOption(ONETRACE_PID)) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << " [ns] = " <<
      queued << " (append) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;

    tracer->logger_.Log(stream.str().c_str());
  }

  static void ClDeviceTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    if (tracer->CheckOption(ONETRACE_PID)) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << " [ns] = " <<
      queued << " (queued) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;

    tracer->logger_.Log(stream.str().c_str());
  }

  static void ZeChromeTimelineCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
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

  static void ClChromeTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
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

  static void ZeChromeStagesCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
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

  static void ClChromeStagesCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
    PTI_ASSERT(tracer != nullptr);
    PTI_ASSERT(tracer->chrome_logger_ != nullptr);
    std::stringstream stream;

    std::string tid =
      std::to_string(id) +
      "." + std::to_string(reinterpret_cast<uint64_t>(queue));

    PTI_ASSERT(submitted > queued);
    stream << "{\"ph\":\"X\", \"pid\":" << utils::GetPid() <<
      ", \"tid\":" << tid <<
      ", \"name\":\"" << name << " (Queued)" <<
      "\", \"ts\": " << queued / NSEC_IN_USEC <<
      ", \"dur\":" << (submitted - queued) / NSEC_IN_USEC <<
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

  static void ZeDeviceAndChromeTimelineCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeDeviceTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
    ZeChromeTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
  }

  static void ClDeviceAndChromeTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ClDeviceTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
    ClChromeTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
  }

  static void ZeDeviceAndChromeStagesCallback(
      void* data, void* queue,
      const std::string& id, const std::string& name,
      uint64_t appended, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ZeDeviceTimelineCallback(
        data, queue, id, name, appended, submitted, started, ended);
    ZeChromeStagesCallback(
        data, queue, id, name, appended, submitted, started, ended);
  }

  static void ClDeviceAndChromeStagesCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ClDeviceTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
    ClChromeStagesCallback(
        data, queue, id, name, queued, submitted, started, ended);
  }

  static void ZeChromeLoggingCallback(
      void* data, const std::string& id, const std::string& name,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
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

  static void ClChromeLoggingCallback(
      void* data, uint64_t id, const std::string& name,
      uint64_t started, uint64_t ended) {
    UnifiedTracer* tracer = reinterpret_cast<UnifiedTracer*>(data);
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

  Correlator correlator_;
  uint64_t total_execution_time_ = 0;

  ZeApiCollector* ze_api_collector_ = nullptr;
  ClApiCollector* cl_cpu_api_collector_ = nullptr;
  ClApiCollector* cl_gpu_api_collector_ = nullptr;

  ZeKernelCollector* ze_kernel_collector_ = nullptr;
  ClKernelCollector* cl_cpu_kernel_collector_ = nullptr;
  ClKernelCollector* cl_gpu_kernel_collector_ = nullptr;

  std::string chrome_trace_file_name_;
  Logger* chrome_logger_ = nullptr;

  Logger logger_;
};

#endif // PTI_SAMPLES_ONETRACE_UNIFIED_TRACER_H_