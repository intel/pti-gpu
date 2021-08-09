//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "unified_tracer.h"

static UnifiedTracer* tracer = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./onetrace[.exe] [options] <application> <args>" <<
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
    "--device-timing-verbose [-v]   " <<
    "Report kernels execution time with SIMD width and global/local sizes" <<
    std::endl;
  std::cout <<
    "--device-timeline [-t]         " <<
    "Trace device activities" <<
    std::endl;
  std::cout <<
    "--output [-o] <filename>       " <<
    "Print console logs into the file" <<
    std::endl;
  std::cout <<
    "--chrome-call-logging          " <<
    "Dump host API calls to JSON file" <<
    std::endl;
  std::cout <<
    "--chrome-device-timeline       " <<
    "Dump device activities to JSON file per command queue" <<
    std::endl;
  std::cout <<
    "--chrome-kernel-timeline       " <<
    "Dump device activities to JSON file per kernel name" <<
    std::endl;
  std::cout <<
    "--chrome-device-stages         " <<
    "Dump device activities by stages to JSON file" <<
    std::endl;
  std::cout <<
    "--tid                          " <<
    "Print thread ID into host API trace" <<
    std::endl;
  std::cout <<
    "--pid                          " <<
    "Print process ID into host API and device activity trace" <<
    std::endl;
  std::cout <<
    "--version                      " <<
    "Print version" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--call-logging") == 0 ||
        strcmp(argv[i], "-c") == 0) {
      utils::SetEnv("ONETRACE_CallLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 ||
               strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("ONETRACE_HostTiming", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("ONETRACE_DeviceTiming", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing-verbose") == 0 ||
               strcmp(argv[i], "-v") == 0) {
      utils::SetEnv("ONETRACE_DeviceTimingVerbose", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timeline") == 0 ||
               strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("ONETRACE_DeviceTimeline", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--output") == 0 ||
               strcmp(argv[i], "-o") == 0) {
      utils::SetEnv("ONETRACE_LogToFile", "1");
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Log file name is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("ONETRACE_LogFilename", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--chrome-call-logging") == 0) {
      utils::SetEnv("ONETRACE_ChromeCallLogging", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-timeline") == 0) {
      utils::SetEnv("ONETRACE_ChromeDeviceTimeline", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-kernel-timeline") == 0) {
      utils::SetEnv("ONETRACE_ChromeKernelTimeline", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-stages") == 0) {
      utils::SetEnv("ONETRACE_ChromeDeviceStages", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--tid") == 0) {
      utils::SetEnv("ONETRACE_Tid", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--pid") == 0) {
      utils::SetEnv("ONETRACE_Pid", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--version") == 0) {
#ifdef PTI_VERSION
      std::cout << TOSTRING(PTI_VERSION) << std::endl;
#endif
      return 0;
    } else {
      break;
    }
  }

  if (utils::GetEnv("ONETRACE_ChromeDeviceTimeline") == "1" &&
      utils::GetEnv("ONETRACE_ChromeDeviceStages") == "1") {
    std::cout <<
      "[ERROR] Options --chrome-device-timeline and " <<
      "--chrome-device-stages can't be used together, " <<
      "choose one of them" << std::endl;
    return -1;
  }
  if (utils::GetEnv("ONETRACE_ChromeDeviceTimeline") == "1" &&
      utils::GetEnv("ONETRACE_ChromeKernelTimeline") == "1") {
    std::cout <<
      "[ERROR] Options --chrome-device-timeline and " <<
      "--chrome-kernel-timeline can't be used together, " <<
      "choose one of them" << std::endl;
    return -1;
  }
  if (utils::GetEnv("ONETRACE_ChromeKernelTimeline") == "1" &&
      utils::GetEnv("ONETRACE_ChromeDeviceStages") == "1") {
    std::cout <<
      "[ERROR] Options --chrome-kernel-timeline and " <<
      "--chrome-device-stages can't be used together, " <<
      "choose one of them" << std::endl;
    return -1;
  }

  return app_index;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
  utils::SetEnv("NEOReadDebugKeys", "1");
  utils::SetEnv("UseCyclesPerSecondTimer", "1");
}

static TraceOptions ReadArgs() {
  std::string value;
  uint32_t flags = 0;
  std::string log_file;

  value = utils::GetEnv("ONETRACE_CallLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CALL_LOGGING);
  }

  value = utils::GetEnv("ONETRACE_HostTiming");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_HOST_TIMING);
  }

  value = utils::GetEnv("ONETRACE_DeviceTiming");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMING);
  }

  value = utils::GetEnv("ONETRACE_DeviceTimingVerbose");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMING_VERBOSE);
  }

  value = utils::GetEnv("ONETRACE_DeviceTimeline");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ONETRACE_LogToFile");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_LOG_TO_FILE);
    log_file = utils::GetEnv("ONETRACE_LogFilename");
    PTI_ASSERT(!log_file.empty());
  }

  value = utils::GetEnv("ONETRACE_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_CALL_LOGGING);
  }

  value = utils::GetEnv("ONETRACE_ChromeDeviceTimeline");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ONETRACE_ChromeKernelTimeline");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_KERNEL_TIMELINE);
  }

  value = utils::GetEnv("ONETRACE_ChromeDeviceStages");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_CHROME_DEVICE_STAGES);
  }

  value = utils::GetEnv("ONETRACE_Tid");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_TID);
  }

  value = utils::GetEnv("ONETRACE_Pid");
  if (!value.empty() && value == "1") {
    flags |= (1 << TRACE_PID);
  }

  return TraceOptions(flags, log_file);
}

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  tracer = UnifiedTracer::Create(ReadArgs());
}

void DisableProfiling() {
  if (tracer != nullptr) {
    delete tracer;
  }
}