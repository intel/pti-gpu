//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_TRACER_CL_TRACER_H_
#define PTI_SAMPLES_CL_TRACER_CL_TRACER_H_

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

#define CLT_CALL_LOGGING           0
#define CLT_HOST_TIMING            1
#define CLT_DEVICE_TIMING          2
#define CLT_DEVICE_TIMELINE        3
#define CLT_CHROME_CALL_LOGGING    4
#define CLT_CHROME_DEVICE_TIMELINE 5
#define CLT_CHROME_DEVICE_STAGES   6
#define CLT_TID                    7
#define CLT_PID                    8

const char* kChromeTraceFileName = "clt_trace.json";

class ClTracer {
 public:
  static ClTracer* Create(unsigned options) {
    cl_device_id cpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_CPU);
    cl_device_id gpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
    if (cpu_device == nullptr && gpu_device == nullptr) {
      std::cerr << "[WARNING] Intel OpenCL devices are not found" << std::endl;
      return nullptr;
    }

    ClTracer* tracer = new ClTracer(options);

    if (tracer->CheckOption(CLT_DEVICE_TIMING) ||
        tracer->CheckOption(CLT_DEVICE_TIMELINE) ||
        tracer->CheckOption(CLT_CHROME_DEVICE_TIMELINE) ||
        tracer->CheckOption(CLT_CHROME_DEVICE_STAGES)) {

      PTI_ASSERT(!(tracer->CheckOption(CLT_CHROME_DEVICE_TIMELINE) &&
                   tracer->CheckOption(CLT_CHROME_DEVICE_STAGES)));

      ClKernelCollector* cpu_kernel_collector = nullptr;
      ClKernelCollector* gpu_kernel_collector = nullptr;

      OnClKernelFinishCallback callback = nullptr;
      if (tracer->CheckOption(CLT_DEVICE_TIMELINE) &&
          tracer->CheckOption(CLT_CHROME_DEVICE_TIMELINE)) {
        callback = DeviceAndChromeTimelineCallback;
      } else if (tracer->CheckOption(CLT_DEVICE_TIMELINE) &&
                 tracer->CheckOption(CLT_CHROME_DEVICE_STAGES)) {
        callback = DeviceAndChromeStagesCallback;
      } else if (tracer->CheckOption(CLT_DEVICE_TIMELINE)) {
        callback = DeviceTimelineCallback;
      } else if (tracer->CheckOption(CLT_CHROME_DEVICE_TIMELINE)) {
        callback = ChromeTimelineCallback;
      } else if (tracer->CheckOption(CLT_CHROME_DEVICE_STAGES)) {
        callback = ChromeStagesCallback;
      }

      if (cpu_device != nullptr) {
        cpu_kernel_collector = ClKernelCollector::Create(
            cpu_device, &tracer->correlator_, callback, tracer);
        if (cpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for CPU backend" <<
            std::endl;
        }
        tracer->cpu_kernel_collector_ = cpu_kernel_collector;
      }

      if (gpu_device != nullptr) {
        gpu_kernel_collector = ClKernelCollector::Create(
            gpu_device, &tracer->correlator_, callback, tracer);
        if (gpu_kernel_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create kernel collector for GPU backend" <<
            std::endl;
        }
        tracer->gpu_kernel_collector_ = gpu_kernel_collector;
      }

      if (cpu_kernel_collector == nullptr && gpu_kernel_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    if (tracer->CheckOption(CLT_CALL_LOGGING) ||
        tracer->CheckOption(CLT_CHROME_CALL_LOGGING) ||
        tracer->CheckOption(CLT_HOST_TIMING)) {

      ClApiCollector* cpu_api_collector = nullptr;
      ClApiCollector* gpu_api_collector = nullptr;

      OnClFunctionFinishCallback callback = nullptr;
      if (tracer->CheckOption(CLT_CHROME_CALL_LOGGING)) {
        callback = ChromeLoggingCallback;
      }

      ApiCollectorOptions cl_api_options{false, false, false};
      cl_api_options.call_tracing = tracer->CheckOption(CLT_CALL_LOGGING);
      cl_api_options.need_tid = tracer->CheckOption(CLT_TID);
      cl_api_options.need_pid = tracer->CheckOption(CLT_PID);

      if (cpu_device != nullptr) {
        cpu_api_collector = ClApiCollector::Create(
            cpu_device, &tracer->correlator_,
            cl_api_options, callback, tracer);
        if (cpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create API collector for CPU backend" <<
            std::endl;
        }
        tracer->cpu_api_collector_ = cpu_api_collector;
      }

      if (gpu_device != nullptr) {
        gpu_api_collector = ClApiCollector::Create(
            gpu_device, &tracer->correlator_,
            cl_api_options, callback, tracer);
        if (gpu_api_collector == nullptr) {
          std::cerr <<
            "[WARNING] Unable to create API collector for GPU backend" <<
            std::endl;
        }
        tracer->gpu_api_collector_ = gpu_api_collector;
      }

      if (gpu_api_collector == nullptr && cpu_api_collector == nullptr) {
        delete tracer;
        return nullptr;
      }
    }

    return tracer;
  }

  ~ClTracer() {
    total_execution_time_ = correlator_.GetTimestamp();

    if (cpu_api_collector_ != nullptr) {
      cpu_api_collector_->DisableTracing();
    }
    if (gpu_api_collector_ != nullptr) {
      gpu_api_collector_->DisableTracing();
    }

    if (cpu_kernel_collector_ != nullptr) {
      cpu_kernel_collector_->DisableTracing();
    }
    if (gpu_kernel_collector_ != nullptr) {
      gpu_kernel_collector_->DisableTracing();
    }

    Report();

    if (cpu_api_collector_ != nullptr) {
      delete cpu_api_collector_;
    }
    if (gpu_api_collector_ != nullptr) {
      delete gpu_api_collector_;
    }

    if (cpu_kernel_collector_ != nullptr) {
      delete cpu_kernel_collector_;
    }
    if (gpu_kernel_collector_ != nullptr) {
      delete gpu_kernel_collector_;
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

  ClTracer(const ClTracer& copy) = delete;
  ClTracer& operator=(const ClTracer& copy) = delete;

 private:
  ClTracer(unsigned options)
      : options_(options) {
    if (CheckOption(CLT_CHROME_CALL_LOGGING) ||
        CheckOption(CLT_CHROME_DEVICE_TIMELINE) ||
        CheckOption(CLT_CHROME_DEVICE_STAGES)) {
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

  template <class Collector>
  void ReportTiming(
      const Collector* cpu_collector,
      const Collector* gpu_collector,
      const char* type) {
    PTI_ASSERT (cpu_collector != nullptr || gpu_collector != nullptr);

    std::string cpu_title =
      std::string("Total ") + std::string(type) +
      " Time for CPU backend (ns): ";
    std::string gpu_title =
      std::string("Total ") + std::string(type) +
      " Time for GPU backend (ns): ";
    size_t title_width = std::max(cpu_title.size(), gpu_title.size());
    const size_t time_width = 20;

    std::cerr << std::endl;
    std::cerr << "=== " << type << " Timing Results: ===" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::setw(title_width) << "Total Execution Time (ns): " <<
      std::setw(time_width) << total_execution_time_ << std::endl;

    if (cpu_collector != nullptr) {
      std::cerr << std::setw(title_width) << cpu_title <<
        std::setw(time_width) << CalculateTotalTime(cpu_collector) <<
        std::endl;
    }
    if (gpu_collector != nullptr) {
      std::cerr << std::setw(title_width) << gpu_title <<
        std::setw(time_width) << CalculateTotalTime(gpu_collector) <<
        std::endl;
    }

    if (cpu_collector != nullptr) {
      PrintBackendTable(cpu_collector, "CPU");
    }
    if (gpu_collector != nullptr) {
      PrintBackendTable(gpu_collector, "GPU");
    }

    std::cerr << std::endl;
  }

  void Report() {
    if (CheckOption(CLT_HOST_TIMING)) {
      ReportTiming(cpu_api_collector_, gpu_api_collector_, "API");
    }
    if (CheckOption(CLT_DEVICE_TIMING)) {
      ReportTiming(cpu_kernel_collector_, gpu_kernel_collector_, "Device");
    }
    std::cerr << std::endl;
  }

  static void DeviceTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ClTracer* tracer = reinterpret_cast<ClTracer*>(data);
    PTI_ASSERT(tracer != nullptr);

    std::stringstream stream;
    if (tracer->CheckOption(CLT_PID)) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    stream << "Device Timeline (queue: " << queue <<
      "): " << name << "(" << id << ") [ns] = " <<
      queued << " (queued) " <<
      submitted << " (submit) " <<
      started << " (start) " <<
      ended << " (end)" << std::endl;
    tracer->logger_.Log(stream.str().c_str());
  }

  static void ChromeTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ClTracer* tracer = reinterpret_cast<ClTracer*>(data);
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
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    ClTracer* tracer = reinterpret_cast<ClTracer*>(data);
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

  static void DeviceAndChromeTimelineCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
    ChromeTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
  }

  static void DeviceAndChromeStagesCallback(
      void* data, void* queue,
      uint64_t id, const std::string& name,
      uint64_t queued, uint64_t submitted,
      uint64_t started, uint64_t ended) {
    DeviceTimelineCallback(
        data, queue, id, name, queued, submitted, started, ended);
    ChromeStagesCallback(
        data, queue, id, name, queued, submitted, started, ended);
  }

  static void ChromeLoggingCallback(
      void* data, uint64_t id, const std::string& name,
      uint64_t started, uint64_t ended) {
    ClTracer* tracer = reinterpret_cast<ClTracer*>(data);
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

  ClApiCollector* cpu_api_collector_ = nullptr;
  ClApiCollector* gpu_api_collector_ = nullptr;

  ClKernelCollector* cpu_kernel_collector_ = nullptr;
  ClKernelCollector* gpu_kernel_collector_ = nullptr;

  Logger* chrome_logger_ = nullptr;
  Logger logger_;
};

#endif // PTI_SAMPLES_CL_TRACER_CL_TRACER_H_