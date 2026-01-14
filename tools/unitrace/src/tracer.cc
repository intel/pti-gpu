//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <csignal>
#include <iostream>

#include "tracer.h"
#include "unitimer.h"
#include "unicontrol.h"

#include "version.h"
#include "unitrace_tool_commit_hash.h"

#ifdef _WIN32
#define CONSTRUCTOR
#define DESTRUCTOR
#else /* _WIN32 */
#define CONSTRUCTOR __attribute__((constructor))
#define DESTRUCTOR __attribute__((destructor))
#endif /* _WIN32 */

static UniTracer* tracer = nullptr;

static TraceOptions ReadArgs() {
  std::string value;
  uint32_t flags = 0;
  std::string log_file;

  value = utils::GetEnv("UNITRACE_CallLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CALL_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_HostTiming");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_HOST_TIMING);
  }

  value = utils::GetEnv("UNITRACE_DeviceTiming");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMING);
  }

  value = utils::GetEnv("UNITRACE_DeviceTimeline");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("UNITRACE_KernelSubmission");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_KERNEL_SUBMITTING);
  }

  value = utils::GetEnv("UNITRACE_DeviceTimeline");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("UNITRACE_OpenCLTracing");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_OPENCL);
  }

  value = utils::GetEnv("UNITRACE_ChromeSyclLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_SYCL_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_CclSummaryReport");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CCL_SUMMARY_REPORT);
  }

  value = utils::GetEnv("UNITRACE_ChromeIttLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_ITT_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_CALL_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_ChromeKernelLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_KERNEL_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_ChromeDeviceLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_DEVICE_LOGGING);
  }

  value = utils::GetEnv("UNITRACE_ChromeNoThreadOnDevice");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_NO_THREAD_ON_DEVICE);
  }

  value = utils::GetEnv("UNITRACE_ChromeNoEngineOnDevice");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_NO_ENGINE_ON_DEVICE);
  }

  value = utils::GetEnv("UNITRACE_Verbose");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_VERBOSE);
  }

  value = utils::GetEnv("UNITRACE_Demangle");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEMANGLE);
  }

  value = utils::GetEnv("UNITRACE_KernelOnSeparateTiles");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_KERNELS_PER_TILE);
  }

  value = utils::GetEnv("UNITRACE_Tid");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_TID);
  }

  value = utils::GetEnv("UNITRACE_Pid");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_PID);
  }

  value = utils::GetEnv("UNITRACE_LogToFile");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_LOG_TO_FILE);
    log_file = utils::GetEnv("UNITRACE_LogFilename");
    PTI_ASSERT(!log_file.empty());
  }

  value = utils::GetEnv("UNITRACE_StartPaused");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CONDITIONAL_COLLECTION);
  }

  value = utils::GetEnv("UNITRACE_TraceOutputDirPath");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_OUTPUT_DIR_PATH);
  }

  value = utils::GetEnv("UNITRACE_MetricQuery");
  if (!value.empty()) {
    flags |= (1 << TRACE_METRIC_QUERY);
  }

  value = utils::GetEnv("UNITRACE_KernelMetrics");
  if (!value.empty()) {
    flags |= (1 << TRACE_METRIC_STREAM);
  }

  value = utils::GetEnv("UNITRACE_ChromeMpiLogging");
  if (!value.empty()) {
    flags |= (1 << TRACE_CHROME_MPI_LOGGING);
  }

  return TraceOptions(flags, log_file);
}

std::string get_version() {
  return std::string(UNITRACE_VERSION) + " ("+ std::string(COMMIT_HASH) + ")";
}

void Teardown(void) {
  if (tracer != nullptr) {
    delete tracer;
    tracer = nullptr;
  }
}

typedef void (*SignalHandler)(int);
static SignalHandler sigint_handler = nullptr;
static SignalHandler sigabrt_handler = nullptr;
static SignalHandler sigfpe_handler = nullptr;
static SignalHandler sigill_handler = nullptr;
static SignalHandler sigsegv_handler = nullptr;
static SignalHandler sigterm_handler = nullptr;
static SignalHandler siguser_handler = nullptr;

void HandleAbnormalTermination(int sig) {
  Teardown();
  switch (sig) {
    case SIGINT: 
      if (sigint_handler) {
        sigint_handler(sig);
      }
      break;
    case SIGABRT:
      if (sigabrt_handler) {
        sigabrt_handler(sig);
      }
      break;
    case SIGFPE:
      if (sigfpe_handler) {
        sigfpe_handler(sig);
      }
      break;
    case SIGILL:
      if (sigill_handler) {
        sigill_handler(sig);
      }
      break;
    case SIGSEGV:
      if (sigsegv_handler) {
        sigsegv_handler(sig);
      }
      break;
    case SIGTERM:
      if (sigterm_handler) {
        sigterm_handler(sig);
      }
      break;
    default:
      if (siguser_handler) {
        siguser_handler(sig);
      }
      break;
  }
}

void CONSTRUCTOR Init(void) {
  std::string unitrace_version = utils::GetEnv("UNITRACE_VERSION");
  if (unitrace_version.size() > 0) {
    auto libunitrace_version = get_version();
    if (unitrace_version.compare(libunitrace_version) != 0) {
      std::cerr << "[ERROR] Versions of unitrace and " << LIB_UNITRACE_TOOL_NAME << " do not match." << std::endl;
      exit(-1);
    }
  }

  if (!utils::GetEnv("UNITRACE_Session").empty()) {
    UniController::AttachTemporalControlRead(utils::GetEnv("UNITRACE_Session").c_str());
  }

  if (!utils::GetEnv("UNITRACE_TeardownOnSignal").empty()) {
    // save previous handlers and install new handlers
    // default signals first
    auto handler = std::signal(SIGINT, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigint_handler = handler;
    }
    handler = std::signal(SIGABRT, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigabrt_handler = handler;
    }
    handler = std::signal(SIGFPE, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigfpe_handler = handler;
    }
    handler = std::signal(SIGILL, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigill_handler = handler;
    }
    handler = std::signal(SIGSEGV, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigsegv_handler = handler;
    }
    handler = std::signal(SIGTERM, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      sigterm_handler = handler;
    }

    // signals user specified
    int signum = std::atoi(utils::GetEnv("UNITRACE_TeardownOnSignal").c_str());
    handler = std::signal(signum, HandleAbnormalTermination);
    if (handler != SIG_ERR) {
      siguser_handler = handler;
    }
    else {
      std::cerr << "[ERROR] Invalid signal specified for teardown" << std::endl;
    }
  }

  if (!tracer) {
    UniTimer::StartUniTimer();
    tracer = UniTracer::Create(ReadArgs());
  }

  if (utils::GetEnv("UNITRACE_FollowChildProcess") == "0") {
    // restore LD_PRELOAD from UNITRACE_LD_PRELOAD_OLD to prevent the unitrace library
    // from being loaded and this Init() function being called in a child process
    // to disable child process following
#ifndef _WIN32
    auto oldpreload = utils::GetEnv("UNITRACE_LD_PRELOAD_OLD");
    if (oldpreload.empty()) {
     unsetenv("LD_PRELOAD");
    }
    else {
      utils::SetEnv("LD_PRELOAD", oldpreload.c_str());
    }
#endif /* _WIN32 */
  }
}

void DESTRUCTOR Fini(void) {
  Teardown();
}

#ifdef _WIN32
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved)  // reserved
{
  switch (fdwReason)
  {
    case DLL_PROCESS_ATTACH:
      Init();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      Fini();
      break;
  }
  return TRUE;
}
#endif /* _WIN32 */
