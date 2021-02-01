//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "ze_tracer.h"
#include "ze_utils.h"

static ZeTracer* tracer = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_tracer[.exe] [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--call-logging [-c]             Trace host API calls" <<
    std::endl;
  std::cout <<
    "--host-timing  [-h]             Report host API execution time" <<
    std::endl;
  std::cout <<
    "--device-timing [-d]            Report kernels execution time" <<
    std::endl;
  std::cout <<
    "--device-timeline [-t]          Trace device activities" <<
    std::endl;
  std::cout <<
    "--chrome-device-timeline        Dump device activities to JSON file" <<
    std::endl;
  std::cout <<
    "--chrome-call-logging           Dump host API calls to JSON file" <<
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
      utils::SetEnv("ZET_CallLogging=1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 ||
               strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("ZET_HostTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("ZET_DeviceTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timeline") == 0 ||
               strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("ZET_DeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-timeline") == 0) {
      utils::SetEnv("ZET_ChromeDeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-call-logging") == 0) {
      utils::SetEnv("ZET_ChromeCallLogging=1");
      ++app_index;
    } else {
      break;
    }
  }
  return app_index;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZE_ENABLE_TRACING_LAYER=1");
}

static unsigned ReadArgs() {
  std::string value;
  unsigned options = 0;

  value = utils::GetEnv("ZET_CallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_CALL_LOGGING);
  }

  value = utils::GetEnv("ZET_HostTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_HOST_TIMING);
  }

  value = utils::GetEnv("ZET_DeviceTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_DEVICE_TIMING);
  }

  value = utils::GetEnv("ZET_DeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ZET_ChromeDeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_CHROME_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("ZET_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << ZET_CHROME_CALL_LOGGING);
  }

  return options;
}

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_device_handle_t device = nullptr;
  ze_driver_handle_t driver = nullptr;

  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU, device, driver);
  if (device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target" <<
      " device for tracing" << std::endl;
    return;
  }

  tracer = ZeTracer::Create(driver, device, ReadArgs());
}

void DisableProfiling() {
  if (tracer != nullptr) {
    delete tracer;
  }
}