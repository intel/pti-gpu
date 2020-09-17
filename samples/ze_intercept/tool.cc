//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "ze_kernel_collector.h"
#include "ze_tracer.h"
#include "ze_utils.h"

static ZeKernelCollector* collector = nullptr;
static ZeTracer* tracer = nullptr;
static ZeIntercept* intercept = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_intercept[.exe] [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--call-logging [-c]             Trace host API calls" <<
    std::endl;
  std::cout <<
    "--call-logging-timestamps [-t]  Show timestamps (in ns) for each host API call\n" <<
    "                                (this option should be used along with --call-logging (-c))" <<
    std::endl;
  std::cout <<
    "--host-timing  [-h]             Report host API execution time" <<
    std::endl;
  std::cout <<
    "--device-timing [-d]            Report kernels execution time" <<
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
      utils::SetEnv("ZEI_CallLogging=1");
      ++app_index;
    } else if (strcmp(argv[i], "--call-logging-timestamps") == 0 ||
               strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("ZEI_CallLoggingTimestamps=1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 ||
               strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("ZEI_HostTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("ZEI_DeviceTiming=1");
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
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
}

static unsigned SetArgs() {
  std::string value;
  unsigned options = 0;

  value = utils::GetEnv("ZEI_CallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << ZEI_CALL_LOGGING);
  }

  value = utils::GetEnv("ZEI_CallLoggingTimestamps");
  if (!value.empty() && value == "1") {
    options |= (1 << ZEI_CALL_LOGGING_TIMESTAMPS);
  }

  value = utils::GetEnv("ZEI_HostTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ZEI_HOST_TIMING);
  }

  value = utils::GetEnv("ZEI_DeviceTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << ZEI_DEVICE_TIMING);
  }

  return options;
}

void EnableProfiling() {
  intercept = new ZeIntercept(SetArgs());
  PTI_ASSERT(intercept != nullptr);
  
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

  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  if (intercept->CheckOption(ZEI_CALL_LOGGING) ||
      intercept->CheckOption(ZEI_HOST_TIMING)) {
    tracer = new ZeTracer(context, intercept);
    if (tracer == nullptr || !tracer->IsValid()) {
      std::cout << "[WARNING] Unable to create Level Zero tracer for" <<
        " target driver" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
        tracer = nullptr;
      }
    } else {
      bool enabled = tracer->Enable();
      PTI_ASSERT(enabled);
    }
  }

  if (intercept->CheckOption(ZEI_DEVICE_TIMING)) {
    collector = ZeKernelCollector::Create(context, device, intercept);
  }
}

void DisableProfiling() {
  if (tracer != nullptr) {
    bool disabled = tracer->Disable();
    PTI_ASSERT(disabled);
    delete tracer;
  }
  if (collector != nullptr) {
    collector->DisableTracing();
    delete collector;
  }
  if (intercept != nullptr) {
    delete intercept;
    std::cerr << std::endl;
    std::cerr << "[INFO] Job is successfully completed" << std::endl;
  }
}