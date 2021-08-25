//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "profiler.h"

static Profiler* profiler = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./oneprof[.exe] [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--raw-metrics [-m]               " <<
    "Collect raw metric stream for the device" <<
    std::endl;
  std::cout <<
    "--kernel-intrevals [-i]          " <<
    "Collect raw kernel intervals for the device" <<
    std::endl;
  std::cout <<
    "--kernel-metrics [-k]            " <<
    "Collect metrics for each kernel" <<
    std::endl;
  std::cout <<
    "--aggregation [-a]               " <<
    "Aggregate metrics for each kernel" <<
    std::endl;
  std::cout <<
    "--device [-d] <ID>               " <<
    "Target device for profiling (default is 0)" <<
    std::endl;
  std::cout <<
    "--group [-g] <NAME>              " <<
    "Target metric group to collect (default is ComputeBasic)" <<
    std::endl;
  std::cout <<
    "--sampling-interval [-s] <VALUE> " <<
    "Sampling interval for metrics collection in us (default is 1000 us)" <<
    std::endl;
  std::cout <<
    "--output [-o] <filename>         " <<
    "Print console logs into the file" <<
    std::endl;
  std::cout <<
    "--device-list                    " <<
    "Print list of available devices" <<
    std::endl;
  std::cout <<
    "--metric-list                    " <<
    "Print list of available metrics" <<
    std::endl;
  std::cout <<
    "--version                        " <<
    "Print version" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  bool metric_list = false;

  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--raw-metrics") == 0 ||
        strcmp(argv[i], "-m") == 0) {
      utils::SetEnv("ONEPROF_RawMetrics", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--kernel-intervals") == 0 ||
               strcmp(argv[i], "-i") == 0) {
      utils::SetEnv("ONEPROF_KernelIntervals", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--kernel-metrics") == 0 ||
               strcmp(argv[i], "-k") == 0) {
      utils::SetEnv("ONEPROF_KernelMetrics", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--aggregation") == 0 ||
               strcmp(argv[i], "-a") == 0) {
      utils::SetEnv("ONEPROF_Aggregation", "1");
      ++app_index;
    } else if (strcmp(argv[i], "--device") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Device ID is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("ONEPROF_DeviceId", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--group") == 0 ||
               strcmp(argv[i], "-g") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Metric group is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("ONEPROF_MetricGroup", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--sampling-interval") == 0 ||
               strcmp(argv[i], "-s") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Sampling is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("ONEPROF_SamplingInterval", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--output") == 0 ||
               strcmp(argv[i], "-o") == 0) {
      ++i;
      if (i >= argc) {
        std::cout << "[ERROR] Log file name is not specified" << std::endl;
        return -1;
      }
      utils::SetEnv("ONEPROF_LogFilename", argv[i]);
      app_index += 2;
    } else if (strcmp(argv[i], "--device-list") == 0) {
      PrintDeviceList();
      return 0;
    } else if (strcmp(argv[i], "--metric-list") == 0) {
      metric_list = true;
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

  if (metric_list) {
    std::string value = utils::GetEnv("ONEPROF_DeviceId");
    uint32_t device_id = value.empty() ? 0 : std::stoul(value);
    PrintMetricList(device_id);
    return 0;
  }

  return app_index;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
  utils::SetEnv("ZET_ENABLE_METRICS", "1");
  utils::SetEnv("NEOReadDebugKeys", "1");
  utils::SetEnv("UseCyclesPerSecondTimer", "1");
}

static ProfOptions ReadArgs() {
  std::string value;
  uint32_t flags = 0;
  uint32_t device_id = 0;
  uint32_t sampling_interval = 1000000;
  std::string metric_group("ComputeBasic");
  std::string log_file;

  value = utils::GetEnv("ONEPROF_RawMetrics");
  if (!value.empty()) {
    flags |= (1 << PROF_RAW_METRICS);
  }

  value = utils::GetEnv("ONEPROF_KernelIntervals");
  if (!value.empty()) {
    flags |= (1 << PROF_KERNEL_INTERVALS);
  }

  value = utils::GetEnv("ONEPROF_KernelMetrics");
  if (!value.empty()) {
    flags |= (1 << PROF_KERNEL_METRICS);
  }

  value = utils::GetEnv("ONEPROF_Aggregation");
  if (!value.empty()) {
    flags |= (1 << PROF_AGGREGATION);
  }

  value = utils::GetEnv("ONEPROF_MetricGroup");
  if (!value.empty()) {
    metric_group = value;
  }

  value = utils::GetEnv("ONEPROF_LogFilename");
  if (!value.empty()) {
    log_file = value;
  }

  value = utils::GetEnv("ONEPROF_DeviceId");
  if (!value.empty()) {
    device_id = std::stoul(value);
  }

  value = utils::GetEnv("ONEPROF_SamplingInterval");
  if (!value.empty()) {
    sampling_interval = std::stoul(value);
    sampling_interval *= 1000;
  }

  return ProfOptions(
      flags, device_id, sampling_interval, metric_group, log_file);
}

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  profiler = Profiler::Create(ReadArgs());
}

void DisableProfiling() {
  if (profiler != nullptr) {
    delete profiler;
  }
}