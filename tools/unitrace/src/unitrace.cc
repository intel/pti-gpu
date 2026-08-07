//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <array>
#include <set>
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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#else /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

#include <filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::filesystem

#endif /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

#include <csignal>
#include <stdlib.h>
#include "ze_metrics.h"
#include "utils.h"
#include "cli_options.h"
#include "version.h"
#include "unitrace_commit_hash.h"
#include "unicontrol.h"
#include "logger_factory.h"
#include "unitimer.h"
#include "utils_host.h"

#if !defined(UNITRACE_INSTALL_LIBDIR)
#define UNITRACE_INSTALL_LIBDIR "lib"
#endif /* UNITRACE_INSTALL_LIBDIR */

static ZeMetricProfiler* metric_profiler = nullptr;
static bool idle_sampling = false;

// ============================================================================
// CLI option table: the single source of truth for argument parsing, the
// --help text (Usage), and --print-options-schema. Add or rename a flag by editing
// one row of kCliOptions[] below; ParseArgs, Usage and the JSON schema all derive
// from it. The generic engine lives in cli_options.h for reuse by other tools;
// ArgParseState (this file's Ctx) and FinalizeParse are unitrace-specific.
// ============================================================================

// Shared state written by handlers, consumed after the parse loop by FinalizeParse.
struct ArgParseState {
  char* progname_ = nullptr;
  int   app_index_ = 1;
  bool  show_metric_list_ = false;
  bool  stall_sampling_ = false;
  bool  metric_sampling_ = false;
  bool  devices_to_sample_present_ = false;
  std::set<int> ranks_to_sample_;
  std::set<int> devices_to_sample_;
};

using CliOption = cli::CliOption<ArgParseState>;
using Status    = cli::Status;

// Row factories for the kCliOptions[] table below: Opt::Bool, Opt::Value, Opt::BoolH, ...
using Opt = cli::Factory<ArgParseState>;

void Usage(char* progname);
void PrintOptionsSchema(std::ostream& os);
static int FinalizeParse(ArgParseState& st);

bool IsAlphanumericString(const std::string& str) {
  return std::all_of(str.begin(), str.end(), [](unsigned char c) {return std::isalnum(c);});
}

bool IsNumericString(const std::string& str) {
  return std::all_of(str.begin(), str.end(), [](unsigned char c) {return std::isdigit(c);});
}

static bool IsZeroOrOne(const std::string& str) {
  return str == "0" || str == "1";
}

#if BUILD_WITH_PERFETTO
static bool IsTraceFormat(const std::string& str) {
  return str == "json" || str == "protobuf";
}
#endif /* BUILD_WITH_PERFETTO */

void SetTracingEnvironment() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
}

void SetProfilingEnvironment() {
  utils::SetEnv("ZET_ENABLE_METRICS", "1");
}

void SetSysmanEnvironment() {
  utils::SetEnv("ZES_ENABLE_SYSMAN", "1");
}

// ============================================================================
// Handlers for flags that are not a plain 1:1 "set one env var" mapping.
// ============================================================================

static Status OnOutput(ArgParseState&, const CliOption&, const char* value) {
  utils::SetEnv("UNITRACE_LogToFile", "1");
  utils::SetEnv("UNITRACE_LogFilename", value);
  return Status::Ok;
}

static Status OnOutputDirPath(ArgParseState&, const CliOption&, const char* value) {
  utils::SetEnv("UNITRACE_TraceOutputDirPath", "1");
  utils::SetEnv("UNITRACE_TraceOutputDir", value);
  return Status::Ok;
}

static Status OnResultDir(ArgParseState&, const CliOption&, const char* value) {
  utils::SetEnv("UNITRACE_UseResultDirectory", "1");
  utils::SetEnv("UNITRACE_ResultDirectory", value);
  return Status::Ok;
}

#if BUILD_WITH_ITT
static Status OnCclSummaryReport(ArgParseState&, const CliOption&, const char*) {
  utils::SetEnv("UNITRACE_CclSummaryReport", "1");
  utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
  utils::SetEnv("CCL_ITT_LEVEL", "1");
  return Status::Ok;
}

static Status OnChromeCclLogging(ArgParseState&, const CliOption&, const char*) {
  utils::SetEnv("UNITRACE_ChromeCclLogging", "1");
  utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
  utils::SetEnv("CCL_ITT_LEVEL", "1");
  return Status::Ok;
}

static Status OnChromeDnnLogging(ArgParseState&, const CliOption&, const char*) {
  utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
  // what should be set here?
  return Status::Ok;
}
#endif /* BUILD_WITH_ITT */

#if BUILD_WITH_MPI
static Status OnChromeMpiLogging(ArgParseState&, const CliOption&, const char*) {
  utils::SetEnv("UNITRACE_ChromeMpiLogging", "1");
  utils::SetEnv("UNITRACE_ChromeIttLogging", "1");
  return Status::Ok;
}
#endif /* BUILD_WITH_MPI */

#if BUILD_WITH_XPTI
// (C) SYCL runtime and Unified Runtime tracing are decoupled:
//   --chrome-syclrt-logging  traces the SYCL runtime
//   --chrome-ur-logging      traces the Unified Runtime (layer beneath SYCL rt)
//   --chrome-sycl-logging    kept for backward compatibility; both of the above
static Status OnXptiLogging(ArgParseState&, const CliOption& o, const char*) {
  const bool sycl = (strcmp(o.name_, "--chrome-sycl-logging") == 0);
  if (sycl || strcmp(o.name_, "--chrome-syclrt-logging") == 0) {
    utils::SetEnv("UNITRACE_ChromeSyclRuntimeLogging", "1");
  }
  if (sycl || strcmp(o.name_, "--chrome-ur-logging") == 0) {
    utils::SetEnv("UNITRACE_ChromeUrLogging", "1");
    utils::SetEnv("UR_ENABLE_LAYERS", "UR_LAYER_TRACING");
  }
  utils::SetEnv("XPTI_TRACE_ENABLE", "1");
#ifdef _WIN32
  utils::SetEnv("XPTI_SUBSCRIBERS", "unitrace_tool.dll");
  utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", "xptifw.dll");
#else /* _WIN32 */
  utils::SetEnv("XPTI_SUBSCRIBERS", "libunitrace_tool.so");
  utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", "libxptifw.so");
#endif /* _WIN32 */
  return Status::Ok;
}
#endif /* BUILD_WITH_XPTI */

#if BUILD_WITH_L0
// These flags only record intent; the actual logic runs in FinalizeParse
// once every flag has been seen (mutual exclusion, defaulting, MPI-rank gating).
static Status OnStallSampling(ArgParseState& st, const CliOption&, const char*) {
  st.stall_sampling_ = true;
  return Status::Ok;
}

static Status OnMetricSampling(ArgParseState& st, const CliOption&, const char*) {
  utils::SetEnv("UNITRACE_KernelMetrics", "1");
  st.metric_sampling_ = true;
  return Status::Ok;
}

static Status OnMetricList(ArgParseState& st, const CliOption&, const char*) {
  st.show_metric_list_ = true;  // PrintMetricList runs in FinalizeParse
  return Status::Ok;
}

static Status OnIdleSampling(ArgParseState&, const CliOption&, const char*) {
  idle_sampling = true;  // file-scope global, consumed by main()
  return Status::Ok;
}

// Repeated --ranks-to-sample flags accumulate into UNITRACE_RanksToSample.
static Status OnRanksToSample(ArgParseState& st, const CliOption&, const char* value) {
  std::string ranks = utils::GetEnv("UNITRACE_RanksToSample");
  if (ranks.empty()) {
    utils::SetEnv("UNITRACE_RanksToSample", value);
  } else {
    ranks += ",";
    ranks += value;
    utils::SetEnv("UNITRACE_RanksToSample", ranks.c_str());
  }
  st.ranks_to_sample_.clear();
  if (GetZeRanksToSample(st.ranks_to_sample_) != 0) {
    std::cerr << "[ERROR] Invalid MPI ranks to sample" << std::endl;
    return Status::Error;
  }
  return Status::Ok;
}

// Repeated --devices-to-sample flags accumulate into UNITRACE_DevicesToSampleArg.
static Status OnDevicesToSample(ArgParseState& st, const CliOption&, const char* value) {
  st.devices_to_sample_present_ = true;
  std::string devices = utils::GetEnv("UNITRACE_DevicesToSampleArg");
  if (devices.empty()) {
    utils::SetEnv("UNITRACE_DevicesToSampleArg", value);
  } else {
    devices += ",";
    devices += value;
    utils::SetEnv("UNITRACE_DevicesToSampleArg", devices.c_str());
  }
  st.devices_to_sample_.clear();
  if (GetZeDevicesToSample(st.devices_to_sample_) != 0) {
    std::cout << "[ERROR] Invalid devices to sample" << std::endl;
    return Status::Error;
  }
  return Status::Ok;
}

// Print-and-exit.
static Status OnDeviceList(ArgParseState&, const CliOption&, const char*) {
  SetSysmanEnvironment();  // enable ZES_ENABLE_SYSMAN
  PrintDeviceList();
  return Status::ExitOk;
}
#endif /* BUILD_WITH_L0 */

static Status OnHelp(ArgParseState& st, const CliOption&, const char*) {
  Usage(st.progname_);
  return Status::ExitOk;
}

static Status OnVersion(ArgParseState&, const CliOption&, const char*) {
  std::cout << UNITRACE_VERSION << " (" << COMMIT_HASH << ")" << std::endl;
  return Status::ExitOk;
}

static Status OnPrintOptionsSchema(ArgParseState&, const CliOption&, const char*) {
  PrintOptionsSchema(std::cout);  // runs before any Level Zero init => HW-independent
  return Status::ExitOk;
}

// ============================================================================
// The option table.
// ============================================================================
inline constexpr CliOption kCliOptions[] = {
#if BUILD_WITH_L0 || BUILD_WITH_OPENCL
  Opt::Bool("--call-logging", "-c", "core", "Trace host API calls", "UNITRACE_CallLogging"),
  Opt::Bool("--host-timing", "-h", "core", "Report host API execution time", "UNITRACE_HostTiming"),
  Opt::Bool("--device-timing", "-d", "core", "Report kernels execution time", "UNITRACE_DeviceTiming"),
#endif /* BUILD_WITH_L0 || BUILD_WITH_OPENCL */
#if BUILD_WITH_ITT
  Opt::BoolH("--ccl-summary-report", "-r", "core", "Report CCL execution time summary", OnCclSummaryReport),
#endif /* BUILD_WITH_ITT */
#if BUILD_WITH_L0 || BUILD_WITH_OPENCL
  Opt::Bool("--kernel-submission", "-s", "core",
       "Report append (queued), submit and execute intervals for kernels", "UNITRACE_KernelSubmission"),
  Opt::Bool("--device-timeline", "-t", "core", "Report device timeline", "UNITRACE_DeviceTimeline"),
#endif /* BUILD_WITH_L0 || BUILD_WITH_OPENCL */
#if BUILD_WITH_OPENCL
  Opt::Bool("--opencl", nullptr, "core", "Trace OpenCL", "UNITRACE_OpenCLTracing"),
#endif /* BUILD_WITH_OPENCL */
#if BUILD_WITH_OMP
  Opt::Bool("--chrome-omp-logging", nullptr, "chrome", "Trace OpenMP", "UNITRACE_ChromeOmpLogging"),
#endif /* BUILD_WITH_OMP */
#if BUILD_WITH_MPI
  Opt::BoolH("--chrome-mpi-logging", nullptr, "chrome", "Trace MPI", OnChromeMpiLogging),
#endif /* BUILD_WITH_MPI */
#if BUILD_WITH_XPTI
  Opt::BoolH("--chrome-syclrt-logging", nullptr, "chrome", "Trace SYCL runtime", OnXptiLogging),
  Opt::BoolH("--chrome-ur-logging", nullptr, "chrome",
        "Trace Unified Runtime (implementation layer beneath the SYCL runtime)", OnXptiLogging),
  Opt::BoolH("--chrome-sycl-logging", nullptr, "chrome",
        "Trace SYCL runtime and Unified Runtime (same as --chrome-syclrt-logging --chrome-ur-logging)",
        OnXptiLogging),
#endif /* BUILD_WITH_XPTI */
#if BUILD_WITH_ITT
  Opt::BoolH("--chrome-ccl-logging", nullptr, "chrome", "Trace oneCCL", OnChromeCclLogging),
  Opt::BoolH("--chrome-dnn-logging", nullptr, "chrome", "Trace oneDNN", OnChromeDnnLogging),
#endif /* BUILD_WITH_ITT */
#if BUILD_WITH_L0 || BUILD_WITH_OPENCL
  Opt::Bool("--chrome-call-logging", nullptr, "chrome",
       "Trace Level Zero and/or OpenCL host calls", "UNITRACE_ChromeCallLogging"),
  Opt::Bool("--chrome-kernel-logging", nullptr, "chrome",
       "Trace device and host kernel activities", "UNITRACE_ChromeKernelLogging"),
  Opt::Bool("--chrome-device-logging", nullptr, "chrome", "Trace device activities", "UNITRACE_ChromeDeviceLogging"),
#endif /* BUILD_WITH_L0 || BUILD_WITH_OPENCL */
#if BUILD_WITH_ITT
  Opt::Bool("--chrome-itt-logging", nullptr, "chrome",
       "Trace activities in applications instrumented using Intel(R) Instrumentation and Tracing Technology APIs",
       "UNITRACE_ChromeIttLogging"),
#endif /* BUILD_WITH_ITT */
#if BUILD_WITH_L0 || BUILD_WITH_OPENCL
  Opt::Bool("--chrome-no-thread-on-device", nullptr, "chrome",
       "Trace device activities without per-thread info", "UNITRACE_ChromeNoThreadOnDevice"),
#endif /* BUILD_WITH_L0 || BUILD_WITH_OPENCL */
#if BUILD_WITH_L0
  Opt::Bool("--chrome-no-engine-on-device", nullptr, "chrome",
       "Trace device activities without per-Level-Zero-engine-or-OpenCL-queue info",
       "UNITRACE_ChromeNoEngineOnDevice"),
#endif /* BUILD_WITH_L0 */
  Opt::Value("--chrome-event-buffer-size", nullptr, "int", "chrome", "number-of-events",
        "Size of event buffer on host per host thread (default is -1 or unlimited)",
        "UNITRACE_ChromeEventBufferSize", nullptr, "-1"),
#ifndef _WIN32
  Opt::Value("--chrome-kmd-logging", nullptr, "path", "chrome", "script",
        "Trace OS/KMD activities. The argument <script> file defines the OS kernel or device driver activities to trace",
        "UNITRACE_ChromeKmdLogging").PathFile(),
#endif /* _WIN32 */

#if BUILD_WITH_L0
  Opt::Bool("--metric-query", "-q", "metrics",
       "Query hardware metrics for each kernel instance (Level Zero)", "UNITRACE_MetricQuery")
      .Conflicts("--metric-sampling,--stall-sampling"),
  Opt::BoolH("--metric-sampling", "-k", "metrics",
        "Sample hardware performance metrics for each kernel instance in time-based mode", OnMetricSampling)
      .Conflicts("--metric-query,--stall-sampling"),
  Opt::BoolH("--stall-sampling", nullptr, "metrics",
        "Sample hardware execution unit stalls. Valid for Intel(R) Data Center GPU Max Series and later GPUs",
        OnStallSampling)
      .Conflicts("--metric-query,--metric-sampling"),
  Opt::Value("--group", "-g", "enum", "metrics", "metric-group",
        "Hardware metric group (ComputeBasic by default)", "UNITRACE_MetricGroup", nullptr, "ComputeBasic")
      .ChoicesFrom("metric-groups").Requires("--metric-query,--metric-sampling,--stall-sampling"),
  Opt::Value("--sampling-interval", "-i", "int", "metrics", "interval",
        "Hardware performance metric sampling interval in us (default is 50 us) in time-based mode",
        "UNITRACE_SamplingInterval", nullptr, "50")
      .Requires("--metric-query,--metric-sampling,--stall-sampling"),
  Opt::ValueH("--ranks-to-sample", nullptr, "string", "metrics", "ranks",
         "MPI ranks to sample. The argument <ranks> is a list of comma separated MPI ranks", OnRanksToSample),
  Opt::ValueH("--devices-to-sample", nullptr, "string", "metrics", "devices",
         "Devices ID to sample. The argument <devices> is a list of comma separated devices as reported by --device-list",
         OnDevicesToSample),
  Opt::BoolH("--idle-sampling", nullptr, "metrics", "Continue metric sampling while the device is idle", OnIdleSampling),
  Opt::Action("--device-list", "metrics", "Print available devices", OnDeviceList),
  Opt::Action("--metric-list", "metrics", "Print available metric groups and metrics", OnMetricList),
  Opt::Value("--include-kernels", nullptr, "string", "filter", "kernel-names",
        "Trace kernels, the names of which contain substrings in the comma-separated <kernel-names> (Level Zero only)",
        "UNITRACE_IncludeKernels"),
  Opt::Value("--exclude-kernels", nullptr, "string", "filter", "kernel-names",
        "Trace kernels, the names of which do not contain any substrings in the comma-separated <kernel-names> (Level Zero only)",
        "UNITRACE_ExcludeKernels"),
  Opt::Value("--include-kernels-file", nullptr, "path", "filter", "file",
        "Same as --include-kernels, except that the kernel name substrings are in <file>",
        "UNITRACE_IncludeKernelsFile").PathFile(),
  Opt::Value("--exclude-kernels-file", nullptr, "path", "filter", "file",
        "Same as --exclude-kernels, except that the kernel name substrings are in <file>",
        "UNITRACE_ExcludeKernelsFile").PathFile(),
#endif /* BUILD_WITH_L0 */

  Opt::ValueH("--result-dir", nullptr, "path", "output", "path",
         "Output result to a hierarchical directory", OnResultDir)
      .PathDir().Conflicts("--output,--output-dir-path"),
  Opt::ValueH("--output", "-o", "path", "output", "filename",
         "Output profiling result to file", OnOutput)
      .PathFile().Conflicts("--result-dir,--output-dir-path"),
  Opt::ValueH("--output-dir-path", nullptr, "path", "output", "path",
         "Output result to a flat directory", OnOutputDirPath)
      .PathDir().Conflicts("--result-dir,--output"),
#if BUILD_WITH_PERFETTO
  Opt::Value("--output-format", nullptr, "enum", "output", "format",
        "Timeline trace format: \"json\" (Chrome trace, default) or \"protobuf\" (Perfetto)",
        "UNITRACE_OutputFormat", IsTraceFormat, "json", "json,protobuf"),
#endif /* BUILD_WITH_PERFETTO */

  Opt::Bool("--start-paused", nullptr, "session",
       "Start the tool with tracing and profiling paused", "UNITRACE_StartPaused"),
  Opt::Value("--follow-child-process", nullptr, "enum", "session", "0/1",
        "0: Do not follow or profile child processes on Linux; 1: Follow and profile child processes on Linux (default)",
        "UNITRACE_FollowChildProcess", IsZeroOrOne, "1", "0,1"),
  Opt::Value("--session", nullptr, "string", "session", "session",
        "Name this session for dynamic control. The argument <session> is an alphanumeric string",
        "UNITRACE_Session", IsAlphanumericString),
  Opt::Value("--teardown-on-signal", nullptr, "int", "session", "signum",
        "Try to gracefully shut down in case the application crashes or is terminated or <signum> is raised",
        "UNITRACE_TeardownOnSignal", IsNumericString),
  // pause/resume/stop stay in --help and dispatch, but are SchemaOmit()ted:
  // the launcher UI drives session control with dedicated buttons, not the form.
  Opt::Value("--pause", nullptr, "string", "session", "session",
        "Pause session <session>. The argument <session> must be the same session named with --session option",
        "UNITRACE_PauseSession", IsAlphanumericString).SchemaOmit(),
  Opt::Value("--resume", nullptr, "string", "session", "session",
        "Resume session <session>. The argument <session> must be the same session named with --session option",
        "UNITRACE_ResumeSession", IsAlphanumericString).SchemaOmit(),
  Opt::Value("--stop", nullptr, "string", "session", "session",
        "Stop session <session>. The argument <session> must be the same session named with --session option",
        "UNITRACE_StopSession", IsAlphanumericString).SchemaOmit(),

  Opt::Bool("--verbose", "-v", "misc", "Enable verbose mode to show kernel shapes", "UNITRACE_Verbose"),
  Opt::Bool("--demangle", nullptr, "misc",
       "Demangle kernel names. For OpenCL backend only. Kernel names are always demangled for Level Zero backend",
       "UNITRACE_Demangle"),
#if BUILD_WITH_L0 || BUILD_WITH_OPENCL
  Opt::Bool("--separate-tiles", nullptr, "misc",
       "Trace each tile separately in case of implicit scaling", "UNITRACE_KernelOnSeparateTiles"),
#endif /* BUILD_WITH_L0 || BUILD_WITH_OPENCL */
  Opt::Bool("--tid", nullptr, "misc", "Output TID in host API trace", "UNITRACE_Tid"),
  Opt::Bool("--pid", nullptr, "misc", "Output PID in host API and device activity trace", "UNITRACE_Pid"),

  Opt::Action("--version", "misc", "Print version", OnVersion),
  Opt::Action("--help", "misc",
         "Show this help message and exit. Please refer to the README.md file for further details", OnHelp),

  // ---- hidden: internal/deprecated flags (excluded from --help, still in JSON) ----
  Opt::Bool("--conditional-collection", nullptr, "session",
       "Enable conditional collection. This option is deprecated. Use --start-paused instead",
       "UNITRACE_StartPaused", /*hidden=*/true),
  Opt::Value("--reset-event-on-device", nullptr, "enum", "session", "0/1",
        "Internal: reset events on device", "UNITRACE_ResetEventOnDevice", IsZeroOrOne,
        /*def=*/nullptr, /*choices=*/"0,1", /*hidden=*/true),
  Opt::Bool("--system-time", nullptr, "misc", "Internal: use system time", "UNITRACE_SystemTime", /*hidden=*/true),
  Opt::Action("--print-options-schema", "misc", "Print the option schema as JSON and exit", OnPrintOptionsSchema, /*hidden=*/true),
};

// Group id -> human label, for the launcher form's section headers.
using CliGroup = cli::CliGroup;
inline constexpr CliGroup kCliGroups[] = {
  {"core",    "Core Collection"},
  {"chrome",  "Chrome Timeline"},
  {"metrics", "Hardware Metrics"},
  {"filter",  "Kernel Filtering"},
  {"output",  "Output"},
  {"session", "Session & Control"},
  {"misc",    "Formatting & Misc"},
};

// Emit the build's flag schema for the launcher UI. Covers the mechanical
// attributes, the group labels, and the schema-only relations set via
// CliOption's chained modifiers (pathKind, conflictsWith, requires,
// choicesFrom). Flags marked SchemaOmit() (pause/resume/stop) are excluded --
// the extension drives those with dedicated UI, not the generic form.
void PrintOptionsSchema(std::ostream& os) {
  std::string header("  \"schemaVersion\": 1,\n");
  header += "  \"unitraceVersion\": \"";
  header += UNITRACE_VERSION;
  header += " (";
  header += COMMIT_HASH;
  header += ")\",\n";
  header += "  \"build\": { \"L0\": " + std::to_string(BUILD_WITH_L0);
  header += ", \"OPENCL\": " + std::to_string(BUILD_WITH_OPENCL);
  header += ", \"ITT\": " + std::to_string(BUILD_WITH_ITT);
  header += ", \"XPTI\": " + std::to_string(BUILD_WITH_XPTI);
  header += ", \"MPI\": " + std::to_string(BUILD_WITH_MPI);
  header += ", \"OMP\": " + std::to_string(BUILD_WITH_OMP);
  header += ", \"PERFETTO\": " + std::to_string(BUILD_WITH_PERFETTO);
  header += " },\n";
  cli::PrintOptionsSchema<ArgParseState>(os, kCliGroups, std::size(kCliGroups),
                                     kCliOptions, std::size(kCliOptions), header);
}

static void PrintBuildBanner() {
  std::cout << "(Built with ";
  std::cout << "BUILD_WITH_L0=" << BUILD_WITH_L0 << ", ";
  std::cout << "BUILD_WITH_OPENCL=" << BUILD_WITH_OPENCL << ", ";
  std::cout << "BUILD_WITH_ITT=" << BUILD_WITH_ITT << ", ";
  std::cout << "BUILD_WITH_XPTI=" << BUILD_WITH_XPTI << ", ";
  std::cout << "BUILD_WITH_MPI=" << BUILD_WITH_MPI << ", ";
  std::cout << "BUILD_WITH_OMP=" << BUILD_WITH_OMP << ", ";
  std::cout << "BUILD_WITH_PERFETTO=" << BUILD_WITH_PERFETTO;
  std::cout << ")" << std::endl;
}

void Usage(char* progname) {
  PrintBuildBanner();
  cli::Usage<ArgParseState>(std::cout, progname, kCliOptions, std::size(kCliOptions));
}

int ParseArgs(int argc, char* argv[]) {
  ArgParseState st;
  st.progname_ = argv[0];

  int app_index = cli::ParseArgs<ArgParseState>(argc, argv, kCliOptions, std::size(kCliOptions), st);
  if (app_index <= 0) {
    return app_index;
  }
  st.app_index_ = app_index;

  return FinalizeParse(st);
}

static int FinalizeParse(ArgParseState& st) {
#ifndef _WIN32
  if (!utils::GetEnv("UNITRACE_ChromeKmdLogging").empty()) {
    if (geteuid() != 0) {
      std::cout << "[ERROR] OS kernel tracing requires root privilege" << std::endl;
      return -1;
    }
  }
#endif /* _WIN32 */
  if (!utils::GetEnv("UNITRACE_PauseSession").empty()) {
    UniController::TemporalPause(utils::GetEnv("UNITRACE_PauseSession").c_str());
    return 0;
  }

  if (!utils::GetEnv("UNITRACE_ResumeSession").empty()) {
    UniController::TemporalResume(utils::GetEnv("UNITRACE_ResumeSession").c_str());
    return 0;
  }

  if (!utils::GetEnv("UNITRACE_StopSession").empty()) {
    UniController::TemporalStop(utils::GetEnv("UNITRACE_StopSession").c_str());
    return 0;
  }

  if (utils::GetEnv("UNITRACE_FollowChildProcess").empty()) {
    utils::SetEnv("UNITRACE_FollowChildProcess", "1");  // default is to follow child processes
  }

  if (st.stall_sampling_) {
    if (st.metric_sampling_ && (utils::GetEnv("UNITRACE_MetricGroup") != "EuStallSampling")) {
      std::cerr << "[ERROR] Stall sampling cannot be enabled together with other metric group sampling" << std::endl;
      return -1;
    }
    if (utils::GetEnv("UNITRACE_MetricGroup").empty()) {
      utils::SetEnv("UNITRACE_MetricGroup", "EuStallSampling");
    }
    utils::SetEnv("UNITRACE_KernelMetrics", "1");
  }

  if (st.stall_sampling_ || st.metric_sampling_) {
    auto my_MPI_rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
    if (!my_MPI_rank.empty()) {
      if (st.ranks_to_sample_.empty()) {
        std::cout << "[WARNING] MPI ranks to sample are not specified" << std::endl;
      }
      else {
        auto my_MPI_rank_ = std::stoi(my_MPI_rank);
        if (st.ranks_to_sample_.find(my_MPI_rank_) == st.ranks_to_sample_.end()) {
          // turn off sampling on this rank
          st.stall_sampling_ = false;
          st.metric_sampling_ = false;
          // reset UNITRACE_KernelMetrics
          utils::SetEnv("UNITRACE_KernelMetrics", "");
          // ignore devices to samples
          st.devices_to_sample_present_ = false;
        }
      }
    }

    if (st.devices_to_sample_present_) {
      // Need to initialize L0 now to get the device list
      SetTracingEnvironment();
      SetSysmanEnvironment();
      if (utils::GetEnv("UNITRACE_MetricQuery") == "1" || utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
        SetProfilingEnvironment();
      }
      if (!InitializeL0()) {
        return -1;
      }
      auto device_list = GetDeviceList();
      auto device_count = device_list.size();
      if (device_count == 0) {
        std::cerr << "[ERROR] No Level Zero devices found" << std::endl;
        return -1;
      }
      std::string s;
      for (const auto &device_no : st.devices_to_sample_) {
        if ((0 <= device_no) && (device_no < device_count)) {
          if (s.length() == 0) {
            s = std::to_string(device_no);
          } else {
            s += "," + std::to_string(device_no);
          }
        } else {
          std::cout << "[WARNING] Given device to sample (" << device_no << ") does not exist. Ignoring." << std::endl;
        }
      }
      if (s.length() > 0) {
        utils::SetEnv("UNITRACE_DevicesToSample", s.c_str());
      } else {
        std::cout << "[WARNING] Given devices to sample resulted in an empty device list. Skipping metric sampling." << std::endl;
        utils::SetEnv("UNITRACE_KernelMetrics", "");
      }
    }
  }

  if (utils::GetEnv("UNITRACE_MetricQuery") == "1") {
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
      std::cerr << "[ERROR] Hardware performance metric query mode cannot be used together with time-based mode" << std::endl;
      return -1;
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
    utils::SetEnv("UNITRACE_ChromeEventBufferSize", "-1");  // does not hurt to set to default even if chrome logging is not enabled
  }

  if (!utils::GetEnv("UNITRACE_UseResultDirectory").empty() && utils::GetEnv("UNITRACE_UseResultDirectory") == "1") {
    if ((!utils::GetEnv("UNITRACE_TraceOutputDirPath").empty()) || !utils::GetEnv("UNITRACE_LogToFile").empty()) {
        std::cerr << "[ERROR] Option --result-dir cannot be used together with --output-dir-path or --output." << std::endl;
      return -1;
    }
  }

  std::string include_kernels_file = utils::GetEnv("UNITRACE_IncludeKernelsFile");
  if (!include_kernels_file.empty()) {
      if (!CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(include_kernels_file))) {
          std::cerr << "[ERROR] Include kernels file does not exist: " << include_kernels_file << std::endl;
          return -1;
      }
  }
  std::string exclude_kernels_file = utils::GetEnv("UNITRACE_ExcludeKernelsFile");
  if (!exclude_kernels_file.empty()) {
      if (!CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(exclude_kernels_file))) {
          std::cerr << "[ERROR] Filter-out file does not exist: " << exclude_kernels_file << std::endl;
          return -1;
      }
  }
  if (st.show_metric_list_) {
    SetProfilingEnvironment(); // enable ZET_ENABLE_METRICS
    std::string value = utils::GetEnv("UNITRACE_DeviceId");
    uint32_t device_id = value.empty() ? 0 : std::stoul(value);
    PrintMetricList(device_id);
    return 0;
  }


  // __itt_pause()/__itt_resume()/__itt_detach() support always enabled.
  // On Linux the LD_PRELOAD'd collector resolves the bare SONAME. On Windows
  // the collector DLL is not on the default DLL search path, so this must be
  // the FULL path to unitrace_tool.dll -- set from main() once lib_path is
  // resolved (see below).
#ifndef _WIN32
  utils::SetEnv("INTEL_LIBITTNOTIFY64", "libunitrace_tool.so");
#endif /* _WIN32 */

  return st.app_index_;
}

ZeMetricProfiler *EnableProfiling(uint32_t app_pid, char *dir, std::string& logfile, bool idle_sampling) {
#if BUILD_WITH_L0
  if (!InitializeL0()) {
    return nullptr;
  } else {
    return ZeMetricProfiler::Create(app_pid, dir, logfile, idle_sampling, utils::GetEnv("UNITRACE_DevicesToSample"));
  }
#else /* BUILD_WITH_L0 */
  return nullptr;
#endif /* BUILD_WITH_L0 */
}

void DisableProfiling() {
  if (metric_profiler != nullptr) {
    delete metric_profiler;
    metric_profiler = nullptr;
  }
}

#ifndef _WIN32
static void TerminateProcess(pid_t pid) {
  if (pid <= 0) {
    return;
  }

  if (kill(pid, SIGINT) != 0) {
    // Already terminated
    return;
  }

  usleep(1000 * 1000);

  auto r = waitpid(pid, nullptr, WNOHANG);
  if ((r == pid) || (r == -1)) {
    // Already terminated
    return;
  }

  if (kill(pid, SIGTERM) != 0) {
    // Already terminated
    return;
  }

  usleep(1000 * 1000);

  r = waitpid(pid, nullptr, WNOHANG);
  if ((r == pid) || (r == -1)) {
    // Already terminated
    return;
  }

  kill(pid, SIGKILL);
  waitpid(pid, nullptr, 0);
}
#endif /* _WIN32 */

static char *data_dir = nullptr;

void TearDown() {
  if (data_dir == nullptr) {
    return;
  }
  if (CXX_STD_FILESYSTEM_NAMESPACE::exists(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
    for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
      CXX_STD_FILESYSTEM_NAMESPACE::remove_all(e.path());
    }
    if (!CXX_STD_FILESYSTEM_NAMESPACE::remove(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir))) {
      std::cerr << "[WARNING] " << data_dir << " is not removed. Please manually remove it." << std::endl;
    }
  }
#ifdef _WIN32
  free(data_dir);
#endif /* _WIN32 */
  data_dir = nullptr;

  if (!utils::GetEnv("UNITRACE_Session").empty()) {
    UniController::ReleaseTemporalControl();
#ifdef _WIN32
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1" || utils::GetEnv("UNITRACE_MetricQuery") == "1") {
#else /* _WIN32 */
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1" || utils::GetEnv("UNITRACE_ChromeKmdLogging") == "1") {
#endif /* _WIN32 */
      UniController::ReleaseMetricFlushControl();
    }
  } else if (utils::GetEnv("UNITRACE_StartPaused") == "1" &&
             utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
    // Nameless conditional-collection session created the flush control above;
    // release it here (no-op if it was never created).
    UniController::ReleaseMetricFlushControl();
  }

  if ((utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
    UniController::ReleaseMetricSamplingControl();
  }
}

void CleanUp(int /* sig */) {
  TearDown();
  _Exit(-1);
}

std::string StringifyJsonArray(const char* label, char* const* arr, size_t start_idx = 0) {
  std::string json = std::string("  \"") + label + "\": [";
  bool first = true;
  for (size_t i = start_idx; arr[i] != nullptr; ++i) {
    if (!first) json += ", ";
    json += "\"" + std::string(arr[i]) + "\"";
    first = false;
  }
  json += "],\n";
  return json;
}

void CreateConfigLog(const std::string& unitrace_version, const std::vector<char*>& unitrace_args, const std::vector<char*>& app_args) {
  std::string value = utils::GetEnv("UNITRACE_UseResultDirectory");
  if (!value.empty() && value == "1") {
    LoggerFactory* logger_factory = LoggerFactory::Create();
    std::shared_ptr<Logger> config_logger = logger_factory->GetLogger(LOGGER_TYPE_CONFIG, true, true);

    config_logger->Log("{ \"unitrace version\": \"" + unitrace_version + "\",\n");
    config_logger->Log("  \"app\": \"" + std::string(app_args[0]) + "\",\n");
    config_logger->Log(StringifyJsonArray("unitrace_args", unitrace_args.data()));
    config_logger->Log(StringifyJsonArray("app_args", app_args.data(), 1));
    config_logger->Log("  \"Host\": \"" + GetHostName() + "\",\n");
    config_logger->Log("  \"pid\": \"" + std::to_string(utils::GetPid()) + "\"\n");
    config_logger->Log("}\n");
    config_logger->Flush();
  }
}

static void DumpKmdTraceData(std::string& raw_data_file) {
  std::ifstream inf = std::ifstream(raw_data_file);
  if (!inf.is_open()) {
    std::cerr << "[ERROR] Failed to open raw kernel/kmd tracing date file" << std::endl;
    return;
  }

  UniTimer::StartUniTimer();  // need the timer to get the epoch time of system boot and difference between boot time and monotonic time

  LoggerFactory* logger_factory = LoggerFactory::Create();
  std::shared_ptr<Logger> oskmd_logger = logger_factory->GetLogger(LOGGER_TYPE_KMD_TRACE, true, true);
  if (oskmd_logger == nullptr) {
    std::cerr << "[ERROR] Failed to create kernel/kmd trace file" << std::endl;
    return;
  }

  oskmd_logger->Log("{ \"traceEvents\":[\n");

  std::string str("{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": 0,");  // 0 as dummy process id

  str += "\"args\": {\"name\": \"";

  std::string host = GetHostName();

  if (logger_factory->GetRank().empty()) {
    str += "HOST-OS-KMD<" + host + ">\"}}";
  }
  else {
    str += "RANK " + logger_factory->GetRank() + " HOST-OS-KMD<" + host + ">\"}}";
  }

  oskmd_logger->Log(str);

  while (!inf.eof()) {
    std::string tid;
    std::string kfunc;
    std::string ts;
    std::string dur;
    uint64_t t;
    uint64_t d;

    std::getline(inf, tid, ',');
    if (inf.eof()) {
      break;
    }
    std::getline(inf, kfunc, ',');
    if (inf.eof()) {
      break;
    }
    std::getline(inf, ts, ',');
    if (inf.eof()) {
      break;
    }
    std::getline(inf, dur);
    if (inf.eof()) {
      break;
    }

    // check if optional data is present after duration
    std::string args;
    auto n = dur.find(',');
    if (n != dur.npos) {
      args = dur.substr(n + 1);  // optional data
      dur = dur.substr(0, n);
    }

    try {
      t = std::stol(ts);
      d = std::stol(dur);
    }
    catch (...) {
      break;
    }

    str = ",\n{";
    str += "\"ph\": \"X\"";
    str += ", \"tid\": " + tid;
    str += ", \"pid\": 0";
    if (!kfunc.empty()) {
      str += ", \"name\": \"" + kfunc + "\"";
    }

    str += ", \"cat\": \"os_op\"";
    str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestampFromBootTimestamp(t)));
    str += ", \"dur\": " + std::to_string(UniTimer::GetTimeInUs(d));
    if (!args.empty()) {
      str += ", \"args\": {\"data\": \"" + args + "\"}";
    }
    str += "}";

    oskmd_logger->Log(str);
  }

  oskmd_logger->Log("\n]}");
  oskmd_logger->Flush();

  std::cerr << "[INFO] KMD profiling data are stored in " << oskmd_logger->GetLogFileName() << std::endl;
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
  FILE *fp;
#ifdef _WIN32
  errno_t err = fopen_s(&fp, lib_path.c_str(), "rb");
  if (err != 0 || fp == nullptr) {
    lib_path = executable_path + "/../" UNITRACE_INSTALL_LIBDIR "/" + LIB_UNITRACE_TOOL_NAME;
    err = fopen_s(&fp, lib_path.c_str(), "rb");
    if (err != 0 || fp == nullptr) {
      use_ld_lib_path = true;
      lib_path = LIB_UNITRACE_TOOL_NAME;
    } else {
      fclose(fp);
    }
  } else {
    fclose(fp);
  }
#else /* _WIN32 */
  fp = fopen(lib_path.c_str(), "rb");
  if (fp == nullptr) {
      lib_path = executable_path + "/../" UNITRACE_INSTALL_LIBDIR "/" + LIB_UNITRACE_TOOL_NAME;
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
#endif /* _WIN32 */

#ifdef _WIN32
  // The static ittnotify in the target app loads the collector named by
  // INTEL_LIBITTNOTIFY64 via LoadLibrary. On Windows unitrace_tool.dll is not
  // on the default DLL search path, so pass the full resolved path; otherwise
  // the load fails and __itt_pause/resume/detach silently no-op. (Linux sets
  // the bare SONAME in ParseArgs.)
  utils::SetEnv("INTEL_LIBITTNOTIFY64", lib_path.c_str());
#endif /* _WIN32 */

#if BUILD_WITH_MPI
  std::string mpi_interceptor_path = executable_path + LIB_UNITRACE_MPI_NAME;
  if (use_ld_lib_path) {
    mpi_interceptor_path = LIB_UNITRACE_MPI_NAME;
  } else {
    fp = fopen(mpi_interceptor_path.c_str(), "rb");
    if (fp == nullptr) {
      mpi_interceptor_path = executable_path + "/../" UNITRACE_INSTALL_LIBDIR "/" + LIB_UNITRACE_MPI_NAME;
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
    return app_index? 1 : 0;
  }

  std::vector<char*> app_args;
  std::vector<char*> unitrace_args;

  for (int i = 1; i < app_index; ++i) {
    unitrace_args.push_back(argv[i]);
  }
  unitrace_args.push_back(nullptr);

  for (int i = app_index; i < argc; ++i) {
    app_args.push_back(argv[i]);
  }
  app_args.push_back(nullptr);

  CreateConfigLog(unitrace_version, unitrace_args, app_args);

  const bool named_session = !utils::GetEnv("UNITRACE_Session").empty();
  // A nameless session can still be stopped from within the application via
  // __itt_detach() or by setting PTI_ENABLE_COLLECTION=-1 when conditional
  // collection (--start-paused) is enabled. That stop is reflected in the
  // metric-sampling shared memory, which IsSessionStopped() also inspects.
  const bool app_can_stop_collection =
      (utils::GetEnv("UNITRACE_StartPaused") == "1") &&
      (utils::GetEnv("UNITRACE_KernelMetrics") == "1");
  // Collection may stop before the application exits: either externally via
  // --stop (named session) or from within the application (nameless session).
  const bool stoppable_session = named_session || app_can_stop_collection;

  if (named_session) {
    UniController::CreateTemporalControl(utils::GetEnv("UNITRACE_Session").c_str());
    if (!utils::GetEnv("UNITRACE_StartPaused").empty()) {
      UniController::TemporalPause(utils::GetEnv("UNITRACE_Session").c_str());
    }
#ifdef _WIN32
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1" || utils::GetEnv("UNITRACE_MetricQuery") == "1") {
#else /* _WIN32 */
    if (utils::GetEnv("UNITRACE_KernelMetrics") == "1" || utils::GetEnv("UNITRACE_ChromeKmdLogging") == "1") {
#endif /* _WIN32 */
      UniController::CreateMetricFlushControl();
    }
  } else if (app_can_stop_collection) {
    // Nameless conditional-collection session: enable the flush handshake so the
    // parent can wait for the child to flush metric data on an in-application
    // __itt_detach()/PTI_ENABLE_COLLECTION=-1 stop, just like a named session.
    UniController::CreateMetricFlushControl();
  }

  std::string preload = utils::GetEnv("LD_PRELOAD");
  utils::SetEnv("UNITRACE_LD_PRELOAD_OLD", preload.c_str());
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
#endif /* BUILD_WITH_MPI */

  std::string logfile;
  if (utils::GetEnv("UNITRACE_LogToFile") == "1") {
    logfile = utils::GetEnv("UNITRACE_LogFilename");
  }

  SetTracingEnvironment();
  //OpenCL and oneCCL require sysman enabled
  SetSysmanEnvironment();

  if (utils::GetEnv("UNITRACE_MetricQuery") == "1" || utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
    SetProfilingEnvironment();
    if ((utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
        // create shared memory for metric sampling control between the profiler and the application
        UniController::CreateMetricSamplingControl();
    }
  }

#ifndef _WIN32
  // For some environments (e.g. virtualenv), the required shared libraries may be in the virtualenv folder which is not in LD_LIBRARY_PATH.
  // To address this, we append the virtualenv lib folder to LD_LIBRARY_PATH if VIRTUAL_ENV is set.
  // It is noticed while running pytorch test apps.
  std::string virtual_env = utils::GetEnv("VIRTUAL_ENV");
  if (!virtual_env.empty()) {
    std::string ld_library_path = utils::GetEnv("LD_LIBRARY_PATH");
    // append virtualenv lib folder to LD_LIBRARY_PATH
    virtual_env = virtual_env + "/lib";
    if (ld_library_path.empty()) {
      ld_library_path = std::move(virtual_env);
    } else {
      ld_library_path = ld_library_path + ":" + virtual_env;
    }
    utils::SetEnv("LD_LIBRARY_PATH", ld_library_path.c_str());
  }

  utils::SetEnv("LD_PRELOAD", preload.c_str());


  if (utils::GetEnv("UNITRACE_KernelMetrics") == "1" || !utils::GetEnv("UNITRACE_ChromeKmdLogging").empty()) {

    char pattern[] = "/tmp/tmpdir.XXXXXX";

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

    std::string latch_file_name = std::string(data_dir) + "/latch.tmp";
    std::string oskmd_data_file_name = std::string(data_dir) + "/.oskmd.csv";

    utils::SetEnv("UNITRACE_DataDir", data_dir);

    int child;

    child = fork();

    if (child == 0) {
      if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
        // child process
        // wait for the profiler to be ready
        std::ifstream inf;

        inf.open(latch_file_name, std::ios_base::in);
        uint32_t t = 0;
        while (!inf.is_open() && (t < 10)) {	// wait for no more than 10s
          sleep(1);
          t += 1;
          inf.open(latch_file_name, std::ios_base::in);
        }
        if (inf.is_open()) {
          inf.close();
        }
      }

      // ready to go
      utils::SetEnv("UNITRACE_DataDir", data_dir);

      int ret = 0;

      if (!utils::GetEnv("UNITRACE_ChromeKmdLogging").empty()) {
        std::string cmdline(app_args[0]);

        for (int i = 1; i < app_args.size() - 1; ++i) {
          cmdline += " ";
          cmdline += app_args[i];
        }

        ret = execlp("bpftrace", "bpftrace", "-q", "-o", oskmd_data_file_name.c_str(), "-c", cmdline.c_str(), utils::GetEnv("UNITRACE_ChromeKmdLogging").c_str(), nullptr);
      }
      else {
        ret = execvp(app_args[0], app_args.data());
      }

      if (ret) {
        std::cerr << "[ERROR] Failed to launch target application: " << app_args[0] << std::endl;
        Usage(argv[0]);
        std::_Exit(-1);
      }
    } else if (child > 0) {
      // parent process
      pid_t bpftrace_pid = 0; // keep bpftrace pid if KMD profiling is enabled

      if (!utils::GetEnv("UNITRACE_ChromeKmdLogging").empty()) {
        bpftrace_pid = child;
      }
      if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {

        metric_profiler = EnableProfiling(child, data_dir, logfile, idle_sampling);

        // create a latch file to notify the application process to proceed
        std::ofstream outf(latch_file_name, std::ios_base::out);
        if (!outf.is_open()) {
          std::cerr << "[ERROR] Failed to create profiler latch file: " << app_args[0] << std::endl;
        }
        else {
          outf.close();
        }
      }

      // Poll for session stop while waiting for child processes.
      // This allows --stop to trigger metric finalization without waiting
      // for the application to exit naturally.
      bool session_stopped = false;
      if (stoppable_session) {
        // Poll for stop signal: external --stop (named session) or an
        // in-application __itt_detach()/PTI_ENABLE_COLLECTION=-1 (nameless session)
        while (true) {
          // Check if child(ren) have exited
          if (waitpid(-1, nullptr, WNOHANG) < 0) {
            // No more children (ECHILD) or error
            break;
          }
          if (UniController::IsSessionStopped()) {
            session_stopped = true;
            std::cerr << "[INFO] Stop profiling as the session is stopped" << std::endl;
            break;
          }
          // Sleep briefly before polling again
          usleep(100 * 1000);  // 100ms
        }
        if (session_stopped) {
          // Give child processes some time to flush profile data
          usleep(1000 * 1000);  // 1000ms
          // Just like a namesless session if all child processes exited
          if (waitpid(-1, nullptr, WNOHANG) < 0) {
             session_stopped = false;
          }
          else {
            // Session was stopped via --stop command
            // Stop profiling, wait for children to flush data, then compute metrics
            std::cerr << "[INFO] Wait for child process(es) to flush profile data" << std::endl;
            constexpr int kTimeoutSeconds = 5;
            constexpr int kPollIntervalMs = 100;
            const int max_iters = kTimeoutSeconds * 1000 / kPollIntervalMs;

            bool data_ready = false;
            int elapsed_ms = 0;
            for (int i = 0; i < max_iters; ++i) {
              if (waitpid(-1, nullptr, WNOHANG) < 0) {
                // Just like a namesless session if all child processes exited
                session_stopped = false;
                data_ready = true;
                break;
              }
              // IsChildDataReadyTrusted() applies the start-of-flush grace: an early
              // ready result (the shared flush counters showing no flush in flight)
              // is not trusted until the grace elapses, so a child that has not yet
              // observed the stop and begun flushing is not skipped.
              if (UniController::IsChildDataReadyTrusted(elapsed_ms)) {
                data_ready = true;
                break;
              }

              usleep(kPollIntervalMs * 1000);
              elapsed_ms += kPollIntervalMs;
            }

            if (!data_ready) {
              std::cerr << "[WARNING] Timed out waiting for child process(es) to flush profile data after " << kTimeoutSeconds << "s. Metric output may be incomplete." << std::endl;
            }
          }
          if (bpftrace_pid && session_stopped) {
            // Exception to the "wait for the application to complete" rule
            // below: with --chrome-kmd-logging the application runs as a -c
            // grandchild of bpftrace, so once the session is stopped we stop
            // the bpftrace process here. Terminating bpftrace propagates the
            // signal to its -c grandchild, so the application process also
            // exits as a side effect.
            TerminateProcess(bpftrace_pid);
          }
        }
      } else {
        // Collection cannot be stopped before the app exits: just wait for
        // all children to exit
        while (wait(nullptr) > 0);
      }

      DisableProfiling();
      if (bpftrace_pid) {
        DumpKmdTraceData(oskmd_data_file_name);
      }

      if (session_stopped) {
        // After the session stop is handled we still need to wait for the
        // parent application process to complete. Otherwise we would let the
        // application process exit prematurely, which may not be what the user
        // wants -- stopping the profiling session should not kill the workload.
        //
        // One exception: with --chrome-kmd-logging, stopping the session
        // already terminated the bpftrace process above, and because the
        // application runs as bpftrace's -c grandchild it exits along with it.
        // In that case there is nothing left to wait for here.
        while (wait(nullptr) > 0);
      }
    } else {
      std::cerr << "[ERROR] Failed to create child process" << std::endl;
      DisableProfiling();
    }
  }
  else {
    if (execvp(app_args[0], app_args.data())) {
      std::cerr << "[ERROR] Failed to launch target application: " << app_args[0] << std::endl;
      Usage(argv[0]);
      return 1;
    }
  }
#else /* _WIN32 */
  bool metrics_sampling_enabled = (utils::GetEnv("UNITRACE_KernelMetrics") == "1");
  bool metrics_query_enabled = (utils::GetEnv("UNITRACE_MetricQuery") == "1");

  // metric data collection
  if (metrics_sampling_enabled || metrics_query_enabled) {
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
    data_dir = (char *)malloc(strlen(tpath) + sizeof("\\.data.") + 32);	// enough for the data_dir
    UniMemory::ExitIfOutOfMemory(data_dir);
    sprintf(data_dir, "%s\\.data.%d", tpath, utils::GetPid());
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
        std::cerr << "[ERROR] Failed to allocate memory: " << GetLastError() << std::endl;
        break;
      }

      if (!WriteProcessMemory(pi.hProcess, pathname, lib_path.c_str(), lib_path.size() + 1, nullptr)) {
        std::cerr << "[ERROR] Failed to write target process memory: " << GetLastError() << std::endl;
        break;
      }

      LPTHREAD_START_ROUTINE loadlibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));
      if (!loadlibrary) {
        std::cerr << "[ERROR] Failed to get address of LoadLibraryA: " << GetLastError() << std::endl;
        break;
      }

      HANDLE thr = CreateRemoteThread(pi.hProcess, nullptr, CREATE_SUSPENDED, loadlibrary, pathname, 0, nullptr);
      if (!thr) {
        std::cerr << "[ERROR] Failed to create thread for initialization: " << GetLastError() << std::endl;
        break;
      }

      if (WaitForSingleObject(thr, INFINITE) != WAIT_OBJECT_0) {
        std::cerr << "[ERROR] Failed to wait for thread to complete the initialization: " << GetLastError() << std::endl;
        break;
      }
      DWORD ret = 0;
      GetExitCodeThread(thr, &ret);
      if (ret == 0) {
        std::cerr << "[ERROR] Initialization thread failed: " << GetLastError() << std::endl;
        break;
      }

      CloseHandle(thr);

      DWORD app_pid = pi.dwProcessId;

      if (metrics_sampling_enabled) {
        SetProfilingEnvironment();
        metric_profiler = EnableProfiling(app_pid, data_dir, logfile, idle_sampling);
      }

      ResumeThread(pi.hThread);

      if (metrics_query_enabled || metrics_sampling_enabled) {
        bool session_stopped = false;
        if (stoppable_session) {
          constexpr DWORD kPollIntervalMs = 100;
          while (true) {
            if (WaitForSingleObject(pi.hProcess, kPollIntervalMs) == WAIT_OBJECT_0) {
              break;  // application exited
            }
            if (UniController::IsSessionStopped()) {
              session_stopped = true;
              std::cerr << "[INFO] Stop profiling as the session is stopped" << std::endl;
              break;
            }
          }
          if (session_stopped) {
            if (WaitForSingleObject(pi.hProcess, kPollIntervalMs) == WAIT_OBJECT_0) {
              session_stopped = false;
            }
            else {
              // Session was stopped via --stop command
              // Stop profiling, wait for children to flush data, then compute metrics
              // Wait for all child processes to flush their metric data
              std::cerr << "[INFO] Wait for child process(es) to flush profile data" << std::endl;
              constexpr int kTimeoutSeconds = 5;
              const int max_iters = kTimeoutSeconds * 1000 / kPollIntervalMs;

              bool data_ready = false;
              int elapsed_ms = 0;
              for (int i = 0; i < max_iters; ++i) {
                // IsChildDataReadyTrusted() applies the start-of-flush grace: an early
                // ready result (the shared flush counters showing no flush in flight)
                // is not trusted until the grace elapses, so a child that has not yet
                // observed the stop and begun flushing is not skipped.
                if (UniController::IsChildDataReadyTrusted(elapsed_ms)) {
                  data_ready = true;
                  break;
                }

                if (WaitForSingleObject(pi.hProcess, kPollIntervalMs) == WAIT_OBJECT_0) {
                  session_stopped = false;
                  data_ready = true;
                  break;
                }
                elapsed_ms += kPollIntervalMs;
              }

              if (!data_ready) {
                std::cerr << "[WARNING] Timed out waiting for child process(es) to flush profile data after " << kTimeoutSeconds << "s. Metric profile data may be incomplete." << std::endl;
              }
            }
          }
        }
        else {
          WaitForSingleObject(pi.hProcess, INFINITE);
        }
        DisableProfiling();
        if (metrics_query_enabled) {
          // compute metrics
	  ZeMetricProfiler::ComputeMetricsQueried(app_pid);
        }
        if (session_stopped) {
          // wait for the application to complete
          WaitForSingleObject(pi.hProcess, INFINITE);
        }
      } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
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
    return 1;
  }
#endif /* _WIN32 */

  TearDown();

  return 0;
}
