//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iomanip>
#include <iostream>
#include <set>

#include "utils.h"
#include "ze_kernel_collector.h"

#define KERNEL_LENGTH 10
#define CALLS_LENGTH 12
#define SIMD_LENGTH 5
#define TRANSFERED_LENGHT 20
#define TIME_LENGTH 20
#define PERCENT_LENGTH 10

const char* kLine =
  "+------------------------------------------------"
  "------------------------------------------------+";
const char* kHeader =
  "|                                           | Call "
  " | SIMD  | Total MBytes |   Avg    |  Total   |\n"
  "| Kernel                                    | Count"
  " | Width |  Transfered  | Time, ms | Time, ms |";

ZeKernelCollector* collector = nullptr;
std::chrono::steady_clock::time_point start;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_hot_kernels[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static void PrintResults() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  PTI_ASSERT(collector != nullptr);
  const KernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
  if (kernel_info_map.size() == 0) {
    return;
  }

  std::set<std::pair<std::string, KernelInfo>, utils::Comparator> set(
    kernel_info_map.begin(), kernel_info_map.end());

  uint64_t total_duration = 0;
  size_t max_name_length = KERNEL_LENGTH;
  for (auto& value : set) {
    total_duration += value.second.total_time;
    if (value.first.size() > max_name_length) {
      max_name_length = value.first.size();
    }
  }

  std::cerr << std::endl;
  std::cerr << "=== Device Timing Results: ===" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Total Exectuion Time (ns): " << time.count() << std::endl;
  std::cerr << "Total Device Time (ns): " << total_duration << std::endl;
  std::cerr << std::endl;
  if (total_duration == 0) {
    return;
  }

  std::cerr << std::setw(max_name_length) << "Kernel" << "," <<
    std::setw(CALLS_LENGTH) << "Calls" << "," <<
    std::setw(SIMD_LENGTH) << "SIMD" << "," <<
    std::setw(TRANSFERED_LENGHT) << "Transfered (bytes)" << "," <<
    std::setw(TIME_LENGTH) << "Time (ns)" << "," <<
    std::setw(PERCENT_LENGTH) << "Time (%)" << "," <<
    std::setw(TIME_LENGTH) << "Average (ns)" << std::endl;

  for (auto& value : set) {
    const std::string& kernel = value.first;
    uint64_t call_count = value.second.call_count;
    size_t simd_width = value.second.simd_width;
    size_t bytes_transfered = value.second.bytes_transfered;
    uint64_t duration = value.second.total_time;
    uint64_t avg_duration = duration / call_count;
    float percent_duration = 100.0f * duration / total_duration;
    std::cerr << std::setw(max_name_length) << kernel << "," <<
      std::setw(CALLS_LENGTH) << call_count << "," <<
      std::setw(SIMD_LENGTH) << simd_width << "," <<
      std::setw(TRANSFERED_LENGHT) << bytes_transfered << "," <<
      std::setw(TIME_LENGTH) << duration << "," <<
      std::setw(PERCENT_LENGTH) << std::setprecision(2) << std::fixed <<
          percent_duration << "," <<
      std::setw(TIME_LENGTH) << avg_duration << std::endl;
  }

  std::cerr << std::endl;

  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_driver_handle_t driver = nullptr;
  ze_device_handle_t device = nullptr;
  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU, device, driver);
  if (device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target device" << std::endl;
    return;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  collector = ZeKernelCollector::Create(context, device);
  start = std::chrono::steady_clock::now();
}

void DisableProfiling() {
  if (collector != nullptr) {
    collector->DisableTracing();
    PrintResults();
    delete collector;
  }
}