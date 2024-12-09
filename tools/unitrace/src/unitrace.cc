//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <array>
#include <iostream>

#if !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__))

#if CXX_STD_FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::experimental::filesystem
#else /* CXX_STD_FILESYSTEM_EXPERIMENTAL */
#include <filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::filesystem
#endif /* CXX_STD_FILESYSTEM_EXPERIMENTAL */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dlfcn.h>

#else /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

#include <filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::filesystem

#endif /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

#include <csignal>
#include <stdlib.h>
#include "ze_metrics.h"
#include "utils.h"
#include "version.h"
#include "unitrace_commit_hash.h"

static ZeMetricProfiler* metric_profiler = nullptr;
static bool idle_sampling = false;

void Usage(char * progname) {
  std::cout <<
    "Usage: " << progname << " [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--call-logging [-c]            " <<
    "Trace host API calls" <<
    std::endl;
  std::cout <<
    "--host-timing  [-h]            " <<
    "Report host API execution time" <<
    std::endl;
  std::cout <<
    "--device-timing [-d]           " <<
    "Report kernels execution time" <<
    std::endl;
  std::cout <<
    "--ccl-summary-report [-r]      " <<
    "Report CCL execution time summary" <<
    std::endl;
  std::cout <<
    "--kernel-submission [-s]       " <<
    "Report append (queued), submit and execute intervals for kernels" <<
    std::endl;
  std::cout <<
    "--device-timeline [-t]         " <<
    "Report device timeline" <<
    std::endl;
  std::cout <<
    "--opencl                       " <<
    "Trace OpenCL" <<
    std::endl;
#if BUILD_WITH_MPI
  std::cout <<
    "--chrome-mpi-logging           " <<
    "Trace MPI" <<
    std::endl;
#endif /* BUILD_WITH_MPI */
  std::cout <<
    "--chrome-sycl-logging          " <<
    "Trace SYCL runtime and plugin" <<
    std::endl;
  std::cout <<
    "--chrome-ccl-logging           " <<
    "Trace oneCCL" <<
    std::endl;
  std::cout <<
    "--chrome-dnn-logging           " <<
    "Trace oneDNN" <<
    std::endl;
  std::cout <<
    "--chrome-call-logging          " <<
    "Trace Level Zero and/or OpenCL host calls" <<
    std::endl;
  std::cout <<
    "--chrome-kernel-logging        " <<
    "Trace device and host kernel activities" <<
    std::endl;
  std::cout <<
    "--chrome-device-logging        " <<
    "Trace device activities" <<
    std::endl;
  std::cout <<
    "--chrome-itt-logging           " <<
    "Trace activities in applications instrumented using Intel(R) Instrumentation and Tracing Technology APIs" <<
    std::endl;
  std::cout <<
    "--chrome-no-thread-on-device   " <<
    "Trace device activities without per-thread info." << std::endl << 
    "                               Device activities are traced per thread if this option is not present" <<
    std::endl;
  std::cout <<
    "--chrome-no-engine-on-device   " <<
    "Trace device activities without per-Level-Zero-engine-or-OpenCL-queue info." << std::endl << 
    "                               Device activities are traced per Level-Zero engine or OpenCL queue if this option is not present" <<
    std::endl;
  std::cout <<
    "--chrome-event-buffer-size <number-of-events>    " <<
    "Size of event buffer on host per host thread(default is -1 or unlimited)" <<
    std::endl;
  std::cout <<
    "--chrome-device-timeline       " <<
    "DEPRECATED - use --chrome-kernel-logging instead" <<
    std::endl;
  std::cout <<
    "--chrome-kernel-timeline       " <<
    "DEPRECATED - use --chrome-kernel-logging instead" <<
    std::endl;
  std::cout <<
    "--chrome-device-stages         " <<
    "DEPRECATED - use --chrome-kernel-logging instead" <<
    std::endl;
  std::cout <<
    "--verbose [-v]                 " <<
    "Enable verbose mode to show kernel shapes" << std::endl <<
    "                               Kernel shapes are always enabled in timelines for Level Zero backend" <<
    std::endl;
  std::cout <<
    "--demangle                     " <<
    "Demangle kernel names. For OpenCL backend only. Kernel names are always demangled for Level Zero backend" <<
    std::endl;
  std::cout <<
    "--kernels-per-tile             " <<
    "DEPRECATED - use --separate-tiles instead" <<
    std::endl;
  std::cout <<
    "--separate-tiles               " <<
    "Trace each tile separately in case of implicit scaling" <<
    std::endl;
  std::cout <<
    "--tid                          " <<
    "Output TID in host API trace" <<
    std::endl;
  std::cout <<
    "--pid                          " <<
    "Output PID in host API and device activity trace" <<
    std::endl;
  std::cout <<
    "--output [-o] <filename>       " <<
    "Output profiling result to file" <<
    std::endl;
  std::cout <<
    "--conditional-collection       " <<
    "Enable conditional collection" <<
    std::endl;
  std::cout <<
    "--output-dir-path <path>       " <<
    "Output directory path for result files" <<
    std::endl;
  std::cout <<
    "--metric-query [-q]            " <<
    "Query hardware metrics for each kernel instance is enabled for level-zero." <<
    std::endl;
  std::cout <<
    "--metric-sampling [-k]         " <<
    "Sample hardware performance metrics for each kernel instance in time-based mode" <<
    std::endl;
  std::cout <<
    "--group [-g] <metric-group>    " <<
    "Hardware metric group (ComputeBasic by default)" <<
    std::endl;
  std::cout <<
    "--sampling-interval [-i] <interval> " <<
    "Hardware performance metric sampling interval in us (default is 50 us) in time-based mode" <<
    std::endl;
  std::cout <<
    "--device-list                  " <<
    "Print available devices" <<
    std::endl;
  std::cout <<
    "--metric-list                  " <<
    "Print available metric groups and metrics" <<
    std::endl;
  std::cout <<
    "--stall-sampling               " <<
    "Sample hardware execution unit stalls. Valid for Intel(R) Data Center GPU Max Series and later GPUs" <<
    std::endl;
  std::cout <<
    "--ranks-to-sample <ranks>      " <<
    "MPI ranks to sample. The argument <ranks> is a list of comma separated MPI ranks" <<
    std::endl;
  std::cout <<
    "--version                      " <<
    "Print version" <<
    std::endl;
  std::cout <<
    "--help                         " <<
    "Show the help message and exit" <<
    std::endl;
}

void SetTracingEnvironment() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
}

void SetProfilingEnvironment() {
  utils::SetEnv("ZET_ENABLE_METRICS", "1");
}

void SetSysmanEnvironment() {
  utils::SetEnv("ZES_ENABLE_SYSMAN", "1");
}

int ParseArgs(int argc, char* argv[]) {
  bool show_metric_list = false;
  bool stall_sampling = false;
  bool metric_sampling = false;
  std::vector<int> ranks_to_sample;
  int app_index = 1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--call-logging") == 0 || strcmp(argv[i], "-c") == 0) {
      utils::SetEnv("UNITRACE_CallLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 || strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("UNITRACE_HostTiming", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 || strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("UNITRACE_DeviceTiming", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--ccl-summary-report") == 0 || strcmp(argv[i], "-r") == 0) {
      utils::SetEnv("UNITRACE_CclSummaryReport", "1");
      utils::SetEnv("CCL_ITT_LEVEL", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--kernel-submission") == 0 || strcmp(argv[i], "-s") == 0) {
      utils::SetEnv("UNITRACE_KernelSubmission", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timeline") == 0 || strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("UNITRACE_DeviceTimeline", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--opencl") == 0) {
      utils::SetEnv("UNITRACE_OpenCLTracing", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-mpi-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeMpiLogging", "1");
      utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-sycl-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeSyclLogging", "1");
      utils::SetEnv("XPTI_TRACE_ENABLE", "1");
      utils::SetEnv("UR_ENABLE_LAYERS", "UR_LAYER_TRACING");
#ifdef _WIN32
      utils::SetEnv("XPTI_SUBSCRIBERS", "unitrace_tool.dll");
      utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", "xptifw.dll");
#else
      utils::SetEnv("XPTI_SUBSCRIBERS", "libunitrace_tool.so");
      utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", "libxptifw.so");
#endif
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-ccl-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeCclLogging", "1");
      utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
      utils::SetEnv("CCL_ITT_LEVEL", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-dnn-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
      // what should be set here?
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-call-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeCallLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-kernel-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeKernelLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-logging") == 0) {
      utils::SetEnv("UNITRACE_ChromeDeviceLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-no-thread-on-device") == 0) {
      utils::SetEnv("UNITRACE_ChromeNoThreadOnDevice", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-no-engine-on-device") == 0) {
      utils::SetEnv("UNITRACE_ChromeNoEngineOnDevice", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-event-buffer-size") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Event buffer size is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("UNITRACE_ChromeEventBufferSize", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--chrome-device-timeline") == 0) {
      std::cerr << "Deprecated: --chrome-device-timeline is deprecated, using --chrome-device-logging" << std::endl;
      utils::SetEnv("UNITRACE_ChromeDeviceLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-kernel-timeline") == 0) {
      std::cerr << "Deprecated: --chrome-kernel-timeline is deprecated, using --chrome-kernel-logging" << std::endl;
      utils::SetEnv("UNITRACE_ChromeKernelLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-stages") == 0) {
      std::cerr << "Deprecated: --chrome-device-stages is deprecated, using --chrome-kernel-logging" << std::endl;
      utils::SetEnv("UNITRACE_ChromeKernelLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
      utils::SetEnv("UNITRACE_Verbose", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--demangle") == 0) {
      utils::SetEnv("UNITRACE_Demangle", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--kernels-per-tile") == 0) {
      std::cerr << "Deprecated: --kernels-per-tile is deprecated, using --separate-tiles" << std::endl;
      utils::SetEnv("UNITRACE_KernelOnSeparateTiles", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--separate-tiles") == 0) {
      utils::SetEnv("UNITRACE_KernelOnSeparateTiles", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--tid") == 0) {
      utils::SetEnv("UNITRACE_Tid", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--pid") == 0) {
      utils::SetEnv("UNITRACE_Pid", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
      utils::SetEnv("UNITRACE_LogToFile", "1");
      ++i;
      if (i >= argc) {
        std::cerr << "[ERROR] Log file name is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("UNITRACE_LogFilename", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--conditional-collection") == 0) {
      utils::SetEnv("UNITRACE_ConditionalCollection", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--output-dir-path") == 0) {
      ++i;
      utils::SetEnv("UNITRACE_TraceOutputDirPath", "1");
      utils::SetEnv("UNITRACE_TraceOutputDir", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--metric-query") == 0 || strcmp(argv[i], "-q") == 0) {
      utils::SetEnv("UNITRACE_MetricQuery", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--group") == 0 || strcmp(argv[i], "-g") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Metric group is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("UNITRACE_MetricGroup", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--stall-sampling") == 0) {
      stall_sampling = true;
      ++app_index;
    } else if (strcmp(argv[i], "--ranks-to-sample") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] MPI ranks to sample are not specified" << std::endl;
        return -1;
      }
      char *c = argv[i];
      char *p = c;
      // rank identifiers start with a digit
      if (isdigit(*c) == 0) {
        std::cout << "[ERROR] Invalid MPI ranks to sample" << std::endl;
        return -1;
      }
      c++;
      while (*c) {
        if ((isdigit(*c) == 0) && (*c != ',')) {
          std::cout << "[ERROR] Invalid MPI ranks to sample" << std::endl;
          return -1;
        }
        if (*c == ',') {
          *c = 0;
          ranks_to_sample.push_back(atoi(p));
          p = c + 1;
        }
        c++;
      }
      c--;
      // rank identifiers end with a digit
      if (isdigit(*c) == 0) {
        std::cout << "[ERROR] Invalid MPI ranks to sample" << std::endl;
        return -1;
      }
      else {
        ranks_to_sample.push_back(atoi(p));
      }
      app_index += 2;
    } else if (strcmp(argv[i], "--metric-sampling") == 0 || strcmp(argv[i], "-k") == 0) {
      utils::SetEnv("UNITRACE_KernelMetrics", "1");
      metric_sampling = true;
      ++app_index;
    } else if (strcmp(argv[i], "--idle-sampling") == 0) {
      idle_sampling = true;
      ++app_index;
    } else if (strcmp(argv[i], "--system-time") == 0) {
      utils::SetEnv("UNITRACE_SystemTime", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--sampling-interval") == 0 || strcmp(argv[i], "-i") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Sampling interval is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("UNITRACE_SamplingInterval", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--device-list") == 0) {
      SetSysmanEnvironment();	// enable ZES_ENABLE_SYSMAN
      PrintDeviceList();
      return 0;
    } else if (strcmp(argv[i], "--metric-list") == 0) {
      show_metric_list = true;
      ++app_index;
    } else if (strcmp(argv[i], "--version") == 0) {
      std::cout << UNITRACE_VERSION << " (" << COMMIT_HASH << ")" << std::endl;
      return 0;
    } else if (strcmp(argv[i], "--help") == 0) {
      Usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--chrome-itt-logging") == 0 ) {
      utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
      ++app_index;
    } else {
      break;
    }
  }

  if (stall_sampling || metric_sampling) {
    static std::string myrank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
    if (!myrank.empty()) {
      if (ranks_to_sample.empty()) {
        std::cout << "[WARNING] MPI ranks to smaple are not specified" << std::endl;
      }
      else {
        auto r = std::stoi(myrank);
        bool found = false;
        for (auto i : ranks_to_sample) {
          if (r == i) {
            found = true;
            break;
          }
        } 
        if (found == false) {
          // turn off sampling on this rank
          stall_sampling = false;
          metric_sampling = false;
          // reset UNITRACE_KernelMetrics
          utils::SetEnv("UNITRACE_KernelMetrics", "");
        }
      }
    }
  }

  if (stall_sampling) {
    if (metric_sampling && (utils::GetEnv("UNITRACE_MetricGroup") != "EuStallSampling")) {
      std::cerr << "[ERROR] Stall sampling cannot be enabled together with other metric group sampling" << std::endl;
      return 0;
    }
    if (utils::GetEnv("UNITRACE_MetricGroup").empty()) {
      utils::SetEnv("UNITRACE_MetricGroup", "EuStallSampling");
    }
    utils::SetEnv("UNITRACE_KernelMetrics", "1");
  }

  if (utils::GetEnv("UNITRACE_MetricQuery") == "1") {
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
      std::cerr << "[ERROR] Hardware performance metric query mode cannot be used together with time-based mode" << std::endl;
      return 0;
    }
  }

  if ((utils::GetEnv("UNITRACE_MetricQuery") == "1") || (utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
    // kernel tracing must be on 
    if (utils::GetEnv("UNITRACE_DeviceTiming").empty() && utils::GetEnv("UNITRACE_ChromeKernelLogging").empty() && utils::GetEnv("UNITRACE_ChromeDeviceLogging").empty()) {
      utils::SetEnv("UNITRACE_DeviceTiming", "1");
    }

    // default metric group is "ComputeBasic"
    if (utils::GetEnv("UNITRACE_MetricGroup").empty()) {
      utils::SetEnv("UNITRACE_MetricGroup", "ComputeBasic");
    }

    // default sampling interval is 50 us
    // does not hurt to set UNITRACE_SamplingInterval in query mode
    if (utils::GetEnv("UNITRACE_SamplingInterval").empty()) {
      utils::SetEnv("UNITRACE_SamplingInterval", "50");
    }
  }

  if (!utils::GetEnv("UNITRACE_SamplingInterval").empty() || !utils::GetEnv("UNITRACE_MetricGroup").empty()) {
    if (utils::GetEnv("UNITRACE_DeviceTiming").empty() && utils::GetEnv("UNITRACE_ChromeKernelLogging").empty() && utils::GetEnv("UNITRACE_ChromeDeviceLogging").empty()) {
      std::cerr << "[ERROR] No time-based hardware performance metric sampling option (-k/--stall-sampling) specified" << std::endl;
    }
  }

  if (utils::GetEnv("UNITRACE_ChromeEventBufferSize").empty()) {
    utils::SetEnv("UNITRACE_ChromeEventBufferSize", "-1");	// does not hurt to set to default even if chrome logging is not enabled
  }

#if 0
  if (utils::GetEnv("UNITRACE_TraceKernelFilterIn") == "1" &&
      utils::GetEnv("UNITRACE_TraceKernelFilterOut") == "1") {
    std::cerr << "[ERROR] Options --filter-in and --filter-out can't be used together, choose one of them" << std::endl;
    return -1;
  }
  if (utils::GetEnv("UNITRACE_TraceKernelFilterIn") == "1" || utils::GetEnv("UNITRACE_TraceKernelFilterOut") == "1") {
      if (utils::GetEnv("UNITRACE_TraceKernelFilterFile") != "1" && utils::GetEnv("UNITRACE_TraceKernelFilter") != "1") {
        std::cerr << "[ERROR] Options --filter-in or --filter-out require kernelname via --filter-file or --filter" << std::endl;
        return -1;
      }
  }
  if (utils::GetEnv("UNITRACE_TraceKernelFilterFile") == "1" || utils::GetEnv("UNITRACE_TraceKernelFilter") == "1") {
      if (utils::GetEnv("UNITRACE_TraceKernelFilterIn") != "1" && utils::GetEnv("UNITRACE_TraceKernelFilterOut") != "1") {
        std::cerr << "[ERROR] Options --filter or --filter-file require include/exclude via --filter-in or --filter-out" << std::endl;
        return -1;
      }
  }
#endif /* 0 */


  if (show_metric_list) {
    SetProfilingEnvironment();	// enable ZET_ENABLE_METRICS
    std::string value = utils::GetEnv("UNITRACE_DeviceId");
    uint32_t device_id = value.empty() ? 0 : std::stoul(value);
    PrintMetricList(device_id);
    return 0;
  }

  // __itt_pause()/__itt_resume() support always enabled
  utils::SetEnv("INTEL_LIBITTNOTIFY64", "libunitrace_tool.so");

  return app_index;
}

ZeMetricProfiler *EnableProfiling(char *dir, std::string& logfile, bool idle_sampling) {
  if (zeInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) {
    std::cerr << "[ERROR] Failed to initialize Level Zero runtime" << std::endl;
#ifndef _WIN32
    std::cerr << "[INFO] Please ensure that either /proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid are set to 0." << std::endl;
#endif /* _WIN32 */
    return nullptr;
  }
  return ZeMetricProfiler::Create(dir, logfile, idle_sampling);
}

void DisableProfiling() {
  if (metric_profiler != nullptr) {
    delete metric_profiler;
  }
}

static char pattern[] = "/tmp/tmpdir.XXXXXX";
static char *data_dir = nullptr;

void CleanUp(int sig) {
  if (data_dir == nullptr) {
    return;
  }
  for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
    CXX_STD_FILESYSTEM_NAMESPACE::remove_all(e.path());
  }
  if (CXX_STD_FILESYSTEM_NAMESPACE::remove(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
    std::cerr << "[WARNING] " << data_dir << " is not removed. Please manually remove it." << std::endl;
  }
  _Exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    Usage(argv[0]);
    return 0;
  }

#if !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__))
  struct rlimit	rlim;
  rlim.rlim_cur = RLIM_INFINITY;
  rlim.rlim_max = RLIM_INFINITY;
  if (setrlimit(RLIMIT_STACK, &rlim)) {
    // cannot set stack size to unlimited
    // set soft limit to the same as hard limit
    getrlimit(RLIMIT_STACK, &rlim);
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit(RLIMIT_STACK, &rlim);
  }
#endif /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

  std::string executable_path = utils::GetExecutablePath();

  bool use_ld_lib_path = false;
  std::string lib_path = executable_path + LIB_UNITRACE_TOOL_NAME;
  FILE *fp = fopen(lib_path.c_str(), "rb");
  if (fp == nullptr) {
    lib_path = executable_path + "/../lib/" + LIB_UNITRACE_TOOL_NAME;
    fp = fopen(lib_path.c_str(), "rb");
    if (fp == nullptr) {
      use_ld_lib_path = true;
      lib_path = LIB_UNITRACE_TOOL_NAME;
    } else {
      fclose(fp);
    }
  } else {
    fclose(fp);
  }

#if BUILD_WITH_MPI
  std::string mpi_interceptor_path = executable_path + LIB_UNITRACE_MPI_NAME;
  if (use_ld_lib_path) {
    mpi_interceptor_path = LIB_UNITRACE_MPI_NAME;
  } else {
    fp = fopen(mpi_interceptor_path.c_str(), "rb");
    if (fp == nullptr) {
      mpi_interceptor_path = executable_path + "/../lib/" + LIB_UNITRACE_MPI_NAME;
      fp = fopen(mpi_interceptor_path.c_str(), "rb");
      if (fp == nullptr) {
        std::cerr << "[ERROR] Library " << mpi_interceptor_path << " cannot be found or opened. " << std::endl;
        return -1;
      }
      else {
        fclose(fp);
      }
    } else {
      fclose(fp);
    }
  }
#endif /* BUILD_WITH_MPI */

  // Set unitrace version
  auto unitrace_version =  std::string(UNITRACE_VERSION) + " (" +  std::string(COMMIT_HASH) + ")";
  utils::SetEnv("UNITRACE_VERSION", unitrace_version.c_str());

  int app_index = ParseArgs(argc, argv);
  if (app_index <= 0 || app_index >= argc) {
    if (app_index >= argc) {
      std::cerr << "[ERROR] Application is missing" << std::endl;
      Usage(argv[0]);
    } else if (app_index < 0) {
      std::cerr << "[ERROR] Invalid command line" << std::endl;
      Usage(argv[0]);
    }
    return 0;
  }
  std::vector<char*> app_args;
  
  for (int i = app_index; i < argc; ++i) {
    app_args.push_back(argv[i]);
  }
  app_args.push_back(nullptr);

  std::string preload = utils::GetEnv("LD_PRELOAD");
  if (preload.empty()) {
#ifdef _WIN32
    preload = lib_path;	// lib_path is needed later
#else /* _WIN32 */
    preload = std::move(lib_path);
#endif /* _WIN32 */
  }
  else {
    preload = preload + ":" + lib_path;
  }

#if BUILD_WITH_MPI
  if (utils::GetEnv("UNITRACE_ChromeMpiLogging") == "1") {
    preload = preload + ":" + mpi_interceptor_path;
    // For tracing MPI calls from oneCCL, we need to set CCL_MPI_LIBRARY_PATH
    // with unitrace's MPI intercepter path, because oneCCL directly picks up
    // MPI functions with dlopen/dlsym, not through the dynamic linker. Thus,
    // LD_PRELOAD would not work.
    // TODO: We have to consider a case where CCL_MPI_LIBRARY_PATH is already
    //       set. In this case, unitrace needs to call MPIs in the specified libs
    //       before/after ITT annotation.
    utils::SetEnv("CCL_MPI_LIBRARY_PATH", mpi_interceptor_path.c_str());
  }
#endif

  SetTracingEnvironment();
  
  if (utils::GetEnv("UNITRACE_OpenCLTracing") == "1") {
    // opencl tracing requires sysman enabled for device enumeration
    SetSysmanEnvironment();
  }

  if (utils::GetEnv("UNITRACE_MetricQuery") == "1") {
    // UNITRACE_KernelMetrics is not set
    SetProfilingEnvironment();
  }

  utils::SetEnv("PTI_ENABLE", "1");

  std::string logfile;
  if (utils::GetEnv("UNITRACE_LogToFile") == "1") {
    logfile = utils::GetEnv("UNITRACE_LogFilename");
  }
    
#ifndef _WIN32
  utils::SetEnv("LD_PRELOAD", preload.c_str());

  if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {

    // UNITRACE_MetricQuery is not set
    SetProfilingEnvironment();

    data_dir = mkdtemp(pattern);
    if (data_dir == nullptr) {
      std::cerr << "[ERROR] Failed to create data folder" << std::endl;
      exit(-1);
    }

    std::signal(SIGABRT, CleanUp);
    std::signal(SIGFPE, CleanUp);
    std::signal(SIGILL, CleanUp);
    std::signal(SIGINT, CleanUp);
    std::signal(SIGSEGV, CleanUp);
    std::signal(SIGTERM, CleanUp);

    metric_profiler = EnableProfiling(data_dir, logfile, idle_sampling);

    int child;

    child = fork();
    if (child == 0) {
      // child process
      utils::SetEnv("UNITRACE_DataDir", data_dir);
      if (execvp(app_args[0], app_args.data())) {
        std::cerr << "[ERROR] Failed to launch target application: " << app_args[0] << std::endl;
        Usage(argv[0]);
       
        std::_Exit(-1);
      }
    } else if (child > 0) {
      // parent process

      // wait for child process to complete
      while (wait(nullptr) > 0);

      if (metric_profiler != nullptr) {
        DisableProfiling();
      }

      if (CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
        for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
          CXX_STD_FILESYSTEM_NAMESPACE::remove_all(e.path());
        }
        if (remove(data_dir)) {
          std::cerr << "[WARNING] " << data_dir << " is not removed. Please manually remove it." << std::endl;
        }
      }
    } else {
      std::cerr << "[ERROR] Failed to create child process" << std::endl;
      if (metric_profiler != nullptr) {
        DisableProfiling();
      }

      if (CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
        for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
          CXX_STD_FILESYSTEM_NAMESPACE::remove_all(e.path());
        }
        if (remove(data_dir)) {
          std::cerr << "[WARNING] " << data_dir << " is not removed. Please manually remove it." << std::endl;
        }
      }
    }
  }
  else {
    if (execvp(app_args[0], app_args.data())) {
      std::cerr << "[ERROR] Failed to launch target application: " << app_args[0] << std::endl;
      Usage(argv[0]);
    }
  }
#else /* _WIN32 */
  bool metrics_enabled = (utils::GetEnv("UNITRACE_KernelMetrics") == "1");

  // metric data collection
  if (metrics_enabled) {
    char tpath[MAX_PATH];
    auto tpath_length = GetTempPathA(MAX_PATH, tpath);
    if (tpath_length == 0) {
      std::cerr << "[ERROR] Path for temporary files does not exit." << std::endl;
      exit(-1);
    }

    if (!CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(tpath))) {
      // First check if folder for temporary files exist
      std::cerr << "[ERROR] Directory for temporary files does not exist." << std::endl;
      exit(-1);
    }

    // set data_dir for cleaning up
    data_dir = (char *)malloc(strlen(tpath) + sizeof("\.data.") + 32);	// enough for the data_dir
    UniMemory::ExitIfOutOfMemory(data_dir);
    sprintf(data_dir, "%s\.data.%d", tpath, utils::GetPid());
    auto status = CreateDirectoryA(LPCSTR(data_dir), nullptr);
    if (status == false) {
      std::cerr << "[ERROR] Failed to create temporary data folder." << std::endl;
      free(data_dir);
      exit(-1);
    }

    std::signal(SIGABRT, CleanUp);
    std::signal(SIGFPE, CleanUp);
    std::signal(SIGILL, CleanUp);
    std::signal(SIGINT, CleanUp);
    std::signal(SIGSEGV, CleanUp);
    std::signal(SIGTERM, CleanUp);

    utils::SetEnv("UNITRACE_DataDir", data_dir);
  }

  std::string cmdline = "";
  for (int i = 0; i < app_args.size() - 1; ++i) {
    cmdline += app_args[i];
    cmdline += " ";
  }

  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
      
  if (CreateProcessA(app_args[0], LPSTR(cmdline.c_str()), nullptr, nullptr, false, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
    BOOL success = FALSE;
    do {
      void *pathname = VirtualAllocEx(pi.hProcess, nullptr, lib_path.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (pathname == nullptr) {
        break;
      }

      if (!WriteProcessMemory(pi.hProcess, pathname, lib_path.c_str(), lib_path.size() + 1, nullptr)) {
        break;
      }

      LPTHREAD_START_ROUTINE loadlibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));
      if (!loadlibrary) {
        break;
      }

      HANDLE thr = CreateRemoteThread(pi.hProcess, nullptr, CREATE_SUSPENDED, loadlibrary, pathname, 0, nullptr);
      if (!thr) {
        break;
      }

      if (WaitForSingleObject(thr, INFINITE) != WAIT_OBJECT_0) {
        break;
      }
      DWORD ret = 0;
      GetExitCodeThread(thr, &ret);
      if (ret == 0) {
        break;
      }

      CloseHandle(thr);

      if (metrics_enabled) {
        SetProfilingEnvironment();
	metric_profiler = EnableProfiling(data_dir, logfile, idle_sampling);
      }

      ResumeThread(pi.hThread); 
      WaitForSingleObject(pi.hProcess, INFINITE);

      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);

      if (metrics_enabled && (metric_profiler != nullptr)) {
        DisableProfiling();
      }

      success = TRUE;
    }
    while (0);

    if (!success) {
      std::cerr << "[ERROR] Failed to initialize the tool " << std::endl;
    }
  }
  else {
    std::cerr << "[ERROR] Failed to launch target application: " << app_args[0] << std::endl;
    Usage(argv[0]);
  }

  if (metrics_enabled) {
    if (CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
      for (const auto& e : CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
        CXX_STD_FILESYSTEM_NAMESPACE::remove_all(e.path());
      }
      if (RemoveDirectory(LPCSTR(data_dir)) == 0) {
        std::cerr << "[WARNING] " << data_dir << " is not removed. Please manually remove it." << std::endl;
      }
    }
  }

  if (data_dir) {
    free(data_dir);
  }
#endif /* _WIN32 */

  return 0;
}
