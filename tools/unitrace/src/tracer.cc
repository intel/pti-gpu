//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <csignal>
#include <fstream>
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

static CollectorOptions ReadArgs() {
  std::string value;

  CollectorOptions options;

  value = utils::GetEnv("UNITRACE_CallLogging");
  if (!value.empty() && value == "1") {
    options.call_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_HostTiming");
  if (!value.empty() && value == "1") {
    options.host_timing = 1;
  }

  value = utils::GetEnv("UNITRACE_DeviceTiming");
  if (!value.empty() && value == "1") {
    options.device_timing = 1;
  }

  value = utils::GetEnv("UNITRACE_DeviceTimeline");
  if (!value.empty() && value == "1") {
    options.device_timeline = 1;
  }

  value = utils::GetEnv("UNITRACE_KernelSubmission");
  if (!value.empty() && value == "1") {
    options.kernel_submission = 1;
  }

  value = utils::GetEnv("UNITRACE_OpenCLTracing");
  if (!value.empty() && value == "1") {
    options.opencl = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeSyclRuntimeLogging");
  if (!value.empty() && value == "1") {
    options.chrome_syclrt_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeUrLogging");
  if (!value.empty() && value == "1") {
    options.chrome_ur_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_CclSummaryReport");
  if (!value.empty() && value == "1") {
    options.ccl_summary_report = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeIttLogging");
  if (!value.empty() && value == "1") {
    options.chrome_itt_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    options.chrome_call_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeKernelLogging");
  if (!value.empty() && value == "1") {
    options.chrome_kernel_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeDeviceLogging");
  if (!value.empty() && value == "1") {
    options.chrome_device_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeNoThreadOnDevice");
  if (!value.empty() && value == "1") {
    options.chrome_no_thread_on_device = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeNoEngineOnDevice");
  if (!value.empty() && value == "1") {
    options.chrome_no_engine_on_device = 1;
  }

  value = utils::GetEnv("UNITRACE_Verbose");
  if (!value.empty() && value == "1") {
    options.verbose = 1;
  }

  value = utils::GetEnv("UNITRACE_Demangle");
  if (!value.empty() && value == "1") {
    options.demangle = 1;
  }

  value = utils::GetEnv("UNITRACE_KernelOnSeparateTiles");
  if (!value.empty() && value == "1") {
    options.kernels_per_tile = 1;
  }

  value = utils::GetEnv("UNITRACE_Tid");
  if (!value.empty() && value == "1") {
    options.need_tid = 1;
  }

  value = utils::GetEnv("UNITRACE_Pid");
  if (!value.empty() && value == "1") {
    options.need_pid = 1;
  }

  value = utils::GetEnv("UNITRACE_LogToFile");
  if (!value.empty() && value == "1") {
    options.log_to_file = 1;
  }

  value = utils::GetEnv("UNITRACE_StartPaused");
  if (!value.empty() && value == "1") {
    options.conditional_collection = 1;
  }

  value = utils::GetEnv("UNITRACE_TraceOutputDirPath");
  if (!value.empty() && value == "1") {
    options.output_dir_path = 1;
  }

  value = utils::GetEnv("UNITRACE_MetricQuery");
  if (!value.empty()) {
    options.metric_query = 1;
  }

  value = utils::GetEnv("UNITRACE_KernelMetrics");
  if (!value.empty()) {
    options.metric_stream = 1;

    value = utils::GetEnv("UNITRACE_MetricGroup");
    if (!value.empty() && value == "EuStallSampling") {
      options.stall_sampling = 1;
    }
  }

  value = utils::GetEnv("UNITRACE_ChromeMpiLogging");
  if (!value.empty()) {
    options.chrome_mpi_logging = 1;
  }

  value = utils::GetEnv("UNITRACE_ChromeOmpLogging");
  if (!value.empty() && value == "1") {
    options.chrome_omp_logging = 1;
  }

#if BUILD_WITH_PERFETTO
  value = utils::GetEnv("UNITRACE_OutputFormat");
  if (value == "protobuf") {
    options.protobuf_output = 1;
  }
#endif /* BUILD_WITH_PERFETTO */

  options.DeriveFlags();
  return options;
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

void OnSessionStopped(void) {
  if (tracer != nullptr) {
    // Emit a user-facing acknowledgement that the session stop was observed,
    // BEFORE the (potentially long) flush runs. In metric-query mode there is no
    // separate parent process that waits and reports on stop: the in-process tool
    // runs inside the application process itself (launched via execvp), so the
    // metric data is flushed here and this message must be emitted here too.
    std::cerr << "[INFO] Flush profile data as the session is stopped" << std::endl;

    // Metric data flush handshake is only active for time-based metric sampling
    // (UNITRACE_KernelMetrics) and per-kernel metric query (UNITRACE_MetricQuery).
      const bool metric_handshake =
        utils::GetEnv("UNITRACE_KernelMetrics") == "1" ||
        utils::GetEnv("UNITRACE_MetricQuery") == "1";

    // Announce that this child observed the session stop and is about to flush,
    // BEFORE the flush runs. This increments the shared flush_started_ counter
    if (metric_handshake) {
      UniController::SignalChildFlushStarted();
    }
    tracer->Flush();
    // Signal the parent process that child metric data is flushed.
    // This is needed for both time-based metric sampling (UNITRACE_KernelMetrics)
    // and per-kernel metric query (UNITRACE_MetricQuery) so the parent can
    // safely run final post-processing (e.g. ComputeMetricsQueried on Windows)
    if (metric_handshake) {
      UniController::SignalChildDataReady();
    }
  }
}

void CONSTRUCTOR Init(void) {
  std::string unitrace_version = utils::GetEnv("UNITRACE_VERSION");
  if (unitrace_version.size() > 0) {
    auto libunitrace_version = get_version();
    if (unitrace_version.compare(libunitrace_version) != 0) {
      std::cerr << "[ERROR] Versions of unitrace and " << LIB_UNITRACE_TOOL_NAME << " do not match" << std::endl;
      exit(-1);
    }
  }

  if (!utils::GetEnv("UNITRACE_Session").empty()) {
    UniController::AttachTemporalControlRead(utils::GetEnv("UNITRACE_Session").c_str());
    UniController::SetSessionStoppedCallback(OnSessionStopped);
  } else if (utils::GetEnv("UNITRACE_StartPaused") == "1") {
    // No named session, but conditional collection is on (--start-paused), so
    // the application can still stop the tool from within via __itt_detach()
    // (UniController::IttStop). Register the flush callback so that stop has
    // somewhere to flush; without this the callback stays null and the data
    // would only be written at process teardown.
    UniController::SetSessionStoppedCallback(OnSessionStopped);
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
