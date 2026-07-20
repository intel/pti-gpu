//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNIFIED_TRACER_H_
#define PTI_TOOLS_UNITRACE_UNIFIED_TRACER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "collector_options.h"
#include "logger.h"
#include "utils.h"
#include "ze_collector.h"

#if BUILD_WITH_OPENCL
  #include "cl_collector.h"
  #include "cl_api_callbacks.h"
#else /* BUILD_WITH_OPENCL */
  class ClCollector;
#endif /* BUILD_WITH_OPENCL */

#if BUILD_WITH_XPTI
  #include "xpti_collector.h"
#endif /* BUILD_WITH_XPTI */

#if BUILD_WITH_ITT
  #include "itt_collector.h"
#endif /* BUILD_WITH_ITT */

#if BUILD_WITH_OMP
  #include "ompt_collector.h"
#endif /* BUILD_WITH_OMP */

#include "chromelogger.h"
#include "unimemory.h"
#include "ze_loader.h"
#include "logger_factory.h"

class UniTracer {
 public:
  static UniTracer* Create(const CollectorOptions& options) {
    if (!InitializeL0()) {
      exit(-1);
    }

    UniTracer* tracer = new UniTracer(options);
    UniMemory::ExitIfOutOfMemory((void *)tracer);

    OnZeKernelFinishCallback ze_kcallback = nullptr;
    OnZeFunctionFinishCallback ze_fcallback = nullptr;
    ZeCollector* ze_collector = nullptr;
#if BUILD_WITH_OPENCL
    OnClKernelFinishCallback cl_kcallback = nullptr;
    OnClFunctionFinishCallback cl_fcallback = nullptr;

    ClCollector* cl_gpu_collector = nullptr;
    ClCollector* cl_cpu_collector = nullptr;
#endif /* BUILD_WITH_OPENCL */

#if BUILD_WITH_XPTI
    if (options.chrome_syclrt_logging || options.chrome_ur_logging) {
        xpti_collector = XptiCollector::Create(ChromeLogger::XptiLoggingCallback);
    }
#endif /* BUILD_WITH_XPTI */

#if BUILD_WITH_OMP
    if (options.chrome_omp_logging) {
        ompt_collector = OmptCollector::Create(ChromeLogger::OmptLoggingCallback);
    }
#endif /* BUILD_WITH_OMP */

#if BUILD_WITH_ITT
    if (options.chrome_itt_logging || options.ccl_summary_report) {
        itt_collector = IttCollector::Create(ChromeLogger::IttLoggingCallback);
        if (itt_collector) {
            if (options.ccl_summary_report) {
                itt_collector->EnableCclSummary();
            }
            if (options.chrome_itt_logging) {
                itt_collector->EnableChromeLogging();
            }
            if (options.chrome_mpi_logging) {
              itt_collector->SetMpiCallback(ChromeLogger::MpiLoggingCallback);
              itt_collector->SetMpiInternalCallback(ChromeLogger::MpiInternalLoggingCallback);
            }
        }
    }
    else {
        //TODO: clean it up later
        itt_collector = IttCollector::Create(nullptr);
    }
#endif /* BUILD_WITH_ITT */

    if (options.device_timing ||
        options.device_timeline ||
        options.kernel_submission ||
        options.chrome_device_logging ||
        options.chrome_kernel_logging) {

      if (options.chrome_kernel_logging) {
        ze_kcallback = ChromeLogger::ZeChromeKernelLoggingCallback;
        // also set fcallback functions
        ze_fcallback = ChromeLogger::ChromeCallLoggingCallback;
#if BUILD_WITH_OPENCL
        cl_kcallback = ChromeLogger::ClChromeKernelLoggingCallback;
        cl_fcallback = ChromeLogger::ClChromeCallLoggingCallback;
#endif /* BUILD_WITH_OPENCL */
      }
      else if (options.chrome_device_logging) {
        ze_kcallback = ChromeLogger::ZeChromeKernelLoggingCallback;
#if BUILD_WITH_OPENCL
        cl_kcallback = ChromeLogger::ClChromeKernelLoggingCallback;
#endif /* BUILD_WITH_OPENCL */
      }
    }

    if (options.call_logging ||
        options.chrome_call_logging ||
        options.host_timing) {

      if (options.chrome_call_logging) {
        ze_fcallback = ChromeLogger::ChromeCallLoggingCallback;
#if BUILD_WITH_OPENCL
        cl_fcallback = ChromeLogger::ClChromeCallLoggingCallback;
#endif /* BUILD_WITH_OPENCL */
      }
    }

    if (options.kernel_tracing || options.api_tracing) {
#if BUILD_WITH_OPENCL
      if (options.opencl) {
        cl_device_id cl_cpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_CPU);
        cl_device_id cl_gpu_device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);

        if (cl_cpu_device != nullptr) {
          cl_cpu_collector = ClCollector::Create(cl_cpu_device, options, cl_kcallback, cl_fcallback, tracer);
          if (cl_cpu_collector == nullptr) {
            std::cerr <<
              "[WARNING] Unable to create kernel collector for CL CPU backend" <<
              std::endl;
          }
          tracer->cl_cpu_collector_ = cl_cpu_collector;
        }

        if (cl_gpu_device != nullptr) {
          cl_gpu_collector = ClCollector::Create(cl_gpu_device, options, cl_kcallback, cl_fcallback, tracer);
          if (cl_gpu_collector == nullptr) {
            std::cerr << "[WARNING] Unable to create kernel collector for CL GPU backend" << std::endl;
          }
          tracer->cl_gpu_collector_ = cl_gpu_collector;
        }

        if (cl_cpu_collector == nullptr && cl_gpu_collector == nullptr) {
          std::cerr << "[WARNING] Unable to trace any OpenCL kernels" << std::endl;
          delete tracer;
          return nullptr;
        }
      }
#endif /* BUILD_WITH_OPENCL */

#if BUILD_WITH_L0
      ze_collector = ZeCollector::Create(options, ze_kcallback, ze_fcallback, tracer);
      tracer->ze_collector_ = ze_collector;
#endif /* BUILD_WITH_L0 */
    }

    return tracer;
  }

  void Flush() {
    total_execution_time_ = utils::GetSystemTime() - start_time_;

    if (ze_collector_ != nullptr) {
      ze_collector_->FlushData();
    }

#if BUILD_WITH_OPENCL
    if (cl_cpu_collector_ != nullptr) {
      cl_cpu_collector_->FlushData();
    }
    if (cl_gpu_collector_ != nullptr) {
      cl_gpu_collector_->FlushData();
    }
#endif /* BUILD_WITH_OPENCL */

    Report();

#if BUILD_WITH_ITT
    if (itt_collector != nullptr) {
      std::string summary = itt_collector->CclSummaryReport();
      if (summary.size() > 0) {
        std::shared_ptr<Logger> logger_ccl_summary = logger_factory_->GetLogger(LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT);
        logger_ccl_summary->Log(summary);
        // Force flush so the summary is durable on abnormal termination (e.g. SIGKILL)
        logger_ccl_summary->Flush();
      }
    }
#endif /* BUILD_WITH_ITT */

    if (chrome_logger_ != nullptr) {
      chrome_logger_->Flush();
    }

    data_flushed_ = true;
  }

  ~UniTracer() {
    if (ze_collector_ != nullptr) {
      ze_collector_->DisableTracing();
      ze_collector_->Finalize();
    }

    if (!data_flushed_) {
      Flush();
    }

#if BUILD_WITH_ITT
    if (itt_collector != nullptr){
      // Print CCL summary before deleting the object
      // If CCL summary is not enabled summary string will be empty
      std::string summary = itt_collector->CclSummaryReport();
      if (summary.size() > 0){
        std::shared_ptr<Logger> logger_ccl_summary = logger_factory_->GetLogger(LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT);
        logger_ccl_summary->Log(summary);
      }
      delete itt_collector;
    }
#endif /* BUILD_WITH_ITT */

#if BUILD_WITH_OMP
    if (ompt_collector) {
      delete ompt_collector;
      ompt_collector = nullptr;
    }
#endif /* BUILD_WITH_OMP */

    if (options_.log_to_file) {
      std::shared_ptr<Logger> logger = logger_factory_->GetLogger(LOGGER_TYPE_LEGACY_SHARED_TRACE);
      if (logger && !logger->IsEmpty()) {
        std::cerr << "[INFO] Log is stored in " <<
          logger->GetLogFileName() << std::endl;
      }
    }

    if (ze_collector_ != nullptr) {
      delete ze_collector_;
    }
#if BUILD_WITH_OPENCL
    if (cl_cpu_collector_ != nullptr) {
      delete cl_cpu_collector_;
    }
    if (cl_gpu_collector_ != nullptr) {
      delete cl_gpu_collector_;
    }
#endif /* BUILD_WITH_OPENCL */
    if (chrome_logger_ != nullptr) {
      delete chrome_logger_;
    }
  }


  UniTracer(const UniTracer& that) = delete;
  UniTracer& operator=(const UniTracer& that) = delete;

 private:
  UniTracer(const CollectorOptions& options)
      : options_(options),
        logger_factory_(LoggerFactory::Create()) {
    start_time_ = utils::GetSystemTime();
    if (options_.chrome_call_logging || options_.chrome_kernel_logging ||
        options_.chrome_device_logging || options_.chrome_syclrt_logging ||
        options_.chrome_ur_logging ||
        options_.chrome_itt_logging || options_.chrome_omp_logging) {
      chrome_logger_ = ChromeLogger::Create(logger_factory_->GetAppName().c_str());
    }

  }

  static uint64_t CalculateTotalFunctionTime(const ZeCollector* collector) {
    return collector->CalculateTotalFunctionTime();
  }

  static uint64_t CalculateTotalKernelTime(const ZeCollector* collector) {
    return collector->CalculateTotalKernelTime();
  }

#if BUILD_WITH_OPENCL
  static uint64_t CalculateTotalFunctionTime(const ClCollector* collector) {
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

  static uint64_t CalculateTotalKernelTime(const ClCollector* collector) {
    PTI_ASSERT(collector != nullptr);
    uint64_t total_time = 0;

    const ClKernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
    if (kernel_info_map.size() != 0) {
      for (auto& value : kernel_info_map) {
        total_time += value.second.execute_time;
      }
    }

    return total_time;
  }
#endif /* BUILD_WITH_OPENCL */

  void PrintFunctionTable(std::shared_ptr<Logger> logger,
      const ZeCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalFunctionTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintFunctionsTable(logger);
    }
  }

  void PrintKernelTable(std::shared_ptr<Logger> logger,
      const ZeCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalKernelTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintKernelsTable(logger);
    }
  }

  void PrintSubmissionTable(std::shared_ptr<Logger> logger,
      const ZeCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalKernelTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintSubmissionTable(logger);
    }
  }

#if BUILD_WITH_OPENCL
  void PrintFunctionTable(std::shared_ptr<Logger> logger,
      const ClCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalFunctionTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintFunctionsTable(logger);
    }
  }

  void PrintKernelTable(std::shared_ptr<Logger> logger,
      const ClCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalKernelTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintKernelsTable(logger);
    }
  }

  void PrintSubmissionTable(std::shared_ptr<Logger> logger,
      const ClCollector* collector, const char* device_type) {
    PTI_ASSERT(collector != nullptr);
    PTI_ASSERT(device_type != nullptr);

    uint64_t total_duration = CalculateTotalKernelTime(collector);
    if (total_duration > 0) {
      std::string str("\n== ");
      str += std::string(device_type) + " Backend ==\n\n";
      logger->Log(str);
      collector->PrintSubmissionTable(logger);
    }
  }
#endif /* BUILD_WITH_OPENCL */

  void ReportTiming(
      const ZeCollector* ze_collector,
      const ClCollector* cl_cpu_collector,
      const ClCollector* cl_gpu_collector,
      const char* type) {

    if ((ze_collector == nullptr) && (cl_cpu_collector == nullptr) && (cl_gpu_collector == nullptr)) {
        return;
    }

    std::string stype = std::string(type);

    std::string ze_title =
      std::string("Total ") + stype +
      " Time for L0 backend (ns): ";
    std::string cl_cpu_title =
      std::string("Total ") + stype +
      " Time for CL CPU backend (ns): ";
    std::string cl_gpu_title =
      std::string("Total ") + stype +
      " Time for CL GPU backend (ns): ";
    size_t title_width = std::max(cl_cpu_title.size(), cl_gpu_title.size());
    title_width = std::max(title_width, ze_title.size());
    const size_t time_width = 20;

    std::string str("\n=== ");
    str += stype + " Timing Summary ===\n\n" +
           std::string(std::max(int(title_width - sizeof("Total Execution Time (ns): ") + 1), 0), ' ') +
           "Total Execution Time (ns): " +
           std::string(std::max(int(time_width - std::to_string(total_execution_time_).length()), 0), ' ') +
           std::to_string(total_execution_time_) +
           "\n";

    if (ze_collector != nullptr) {
      uint64_t total_time{0};
      if (stype == "API") {
        total_time = CalculateTotalFunctionTime(ze_collector);
      }
      if (stype == "Device") {
        total_time = CalculateTotalKernelTime(ze_collector);
      }
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - ze_title.length()), 0), ' ') + ze_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }
#if BUILD_WITH_OPENCL
    if (cl_cpu_collector != nullptr) {
      uint64_t total_time{0};
      if (stype == "API") {
        total_time = CalculateTotalFunctionTime(cl_cpu_collector);
      }
      if (stype == "Device") {
        total_time = CalculateTotalKernelTime(cl_cpu_collector);
      }
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - cl_cpu_title.length()), 0), ' ') +  cl_cpu_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }

    if (cl_gpu_collector != nullptr) {
      uint64_t total_time{0};
      if (stype == "API") {
        total_time = CalculateTotalFunctionTime(cl_gpu_collector);
      }
      if (stype == "Device") {
        total_time = CalculateTotalKernelTime(cl_gpu_collector);
      }
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - cl_gpu_title.length()), 0), ' ') + cl_gpu_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }
#endif /* BUILD_WITH_OPENCL */
    std::shared_ptr<Logger> timing_logger = (stype == "API") ? 
                              logger_factory_->GetLogger(LOGGER_TYPE_TRACE_HOST_TIMING) :
                              logger_factory_->GetLogger(LOGGER_TYPE_TRACE_DEVICE_TIMING);
    timing_logger->Log(str);

    if (ze_collector != nullptr) {
      if (stype == "API") {
        PrintFunctionTable(timing_logger, ze_collector, "L0");
      }
      if (stype == "Device") {
        PrintKernelTable(timing_logger, ze_collector, "L0");
      }
    }
#if BUILD_WITH_OPENCL
    if (cl_cpu_collector != nullptr) {
      if (stype == "API") {
        PrintFunctionTable(timing_logger, cl_cpu_collector, "CL CPU");
      }
      if (stype == "Device") {
        PrintKernelTable(timing_logger, cl_cpu_collector, "CL CPU");
      }
    }
    if (cl_gpu_collector != nullptr) {
      if (stype == "API") {
        PrintFunctionTable(timing_logger, cl_gpu_collector, "CL GPU");
      }
      if (stype == "Device") {
        PrintKernelTable(timing_logger, cl_gpu_collector, "CL GPU");
      }
    }
#endif /* BUILD_WITH_OPENCL */
    timing_logger->Log("\n\n");
  }

  void ReportKernelSubmission(
      const ZeCollector* ze_collector,
      const ClCollector* cl_cpu_collector,
      const ClCollector* cl_gpu_collector,
      const char* type) {

    if ((ze_collector == nullptr) && (cl_cpu_collector == nullptr) && (cl_gpu_collector == nullptr)) {
      return;
    }

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

    std::string str("\n=== Kernel Submission Summary ===\n\n");
    str += std::string(std::max(int(title_width - sizeof("Total Execution Time (ns): ") + 1), 0), ' ') + "Total Execution Time (ns): " +
           std::string(std::max(int(time_width - std::to_string(total_execution_time_).length()), 0), ' ') + std::to_string(total_execution_time_)           + "\n";

    if (ze_collector != nullptr) {
      uint64_t total_time = CalculateTotalKernelTime(ze_collector);
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - ze_title.length()), 0), ' ') + ze_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }
#if BUILD_WITH_OPENCL
    if (cl_cpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalKernelTime(cl_cpu_collector);
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - cl_cpu_title.length()), 0), ' ') + cl_cpu_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }
    if (cl_gpu_collector != nullptr) {
      uint64_t total_time = CalculateTotalKernelTime(cl_gpu_collector);
      if (total_time > 0) {
        str += std::string(std::max(int(title_width - cl_gpu_title.length()), 0), ' ') + cl_gpu_title +
               std::string(std::max(int(time_width - std::to_string(total_time).length()), 0), ' ') + std::to_string(total_time) +
               "\n";
      }
    }
#endif /* BUILD_WITH_OPENCL */

    std::shared_ptr<Logger> submission_logger = logger_factory_->GetLogger(LOGGER_TYPE_TRACE_DEVICE_SUBMISSION);
    submission_logger->Log(str);

    if (ze_collector != nullptr) {
      PrintSubmissionTable(submission_logger, ze_collector, "L0");
    }
#if BUILD_WITH_OPENCL
    if (cl_cpu_collector != nullptr) {
      PrintSubmissionTable(submission_logger, cl_cpu_collector, "CL CPU");
    }
    if (cl_gpu_collector != nullptr) {
      PrintSubmissionTable(submission_logger, cl_gpu_collector, "CL GPU");
    }
#endif /* BUILD_WITH_OPENCL */

    submission_logger->Log("\n\n");
  }

  void Report() {
#if BUILD_WITH_OPENCL
    if (options_.host_timing) {
      ReportTiming(
          ze_collector_,
          cl_cpu_collector_,
          cl_gpu_collector_,
          "API");
    }
    if (options_.device_timing) {
      ReportTiming(
          ze_collector_,
          cl_cpu_collector_,
          cl_gpu_collector_,
          "Device");
    }
    if (options_.kernel_submission) {
      ReportKernelSubmission(
          ze_collector_,
          cl_cpu_collector_,
          cl_gpu_collector_,
          "Device");
    }
#else /* BUILD_WITH_OPENCL */
    if (options_.host_timing) {
      ReportTiming(
          ze_collector_,
          nullptr,
          nullptr,
          "API");
    }
    if (options_.device_timing) {
      ReportTiming(
        ze_collector_,
        nullptr,
        nullptr,
          "Device");
    }
    if (options_.kernel_submission) {
      ReportKernelSubmission(
        ze_collector_,
        nullptr,
        nullptr,
          "Device");
    }
#endif /* BUILD_WITH_OPENCL */
  }

 private:
  CollectorOptions options_;

  LoggerFactory* logger_factory_;
  uint64_t start_time_;
  uint64_t total_execution_time_ = 0;
  bool data_flushed_ = false;

  ZeCollector* ze_collector_ = nullptr;

#if BUILD_WITH_OPENCL
  ClCollector* cl_cpu_collector_ = nullptr;
  ClCollector* cl_gpu_collector_ = nullptr;
#endif /* BUILD_WITH_OPENCL */
  ChromeLogger* chrome_logger_ = nullptr;
};

#endif // PTI_TOOLS_UNITRACE_UNIFIED_TRACER_H_

