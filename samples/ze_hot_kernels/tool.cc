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

const char* kLine =
  "+------------------------------------------------"
  "------------------------------------------------+";
const char* kHeader =
  "|                                           | Call "
  " | SIMD  | Total MBytes |   Avg    |  Total   |\n"
  "| Kernel                                    | Count"
  " | Width |  Transfered  | Time, ms | Time, ms |";

ZeKernelCollector* collector = nullptr;

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
  PTI_ASSERT(collector != nullptr);
  const KernelInfoMap& kernel_info_map = collector->GetKernelInfoMap();
  if (kernel_info_map.size() == 0) {
    return;
  }

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  std::set<std::pair<std::string, KernelInfo>, utils::Comparator> set(
    kernel_info_map.begin(), kernel_info_map.end());
  for (auto& pair : set) {
    PTI_ASSERT(pair.second.call_count > 0);
    float total_time = pair.second.total_time /
      static_cast<float>(NSEC_IN_MSEC);
    float avg_time = total_time / pair.second.call_count;
    float mbytes_transfered = pair.second.bytes_transfered /
      static_cast<float>(BYTES_IN_MBYTES);

    std::string kernel_name = pair.first;
    if (kernel_name.size() > 40) {
      kernel_name = kernel_name.substr(0, 32) + "<...>";
    }
    std::cout << "| " << std::left << std::setw(41) << kernel_name << " | ";
    std::cout << std::right << std::setw(5) << pair.second.call_count << " | ";
    std::cout << std::setw(5);
    if (pair.second.simd_width > 0) {
      std::cout << pair.second.simd_width << " | ";
    } else {
      std::cout << "-" << " | ";
    }
    std::cout << std::setw(12) << std::fixed << std::setprecision(2);
    if (mbytes_transfered > 0) {
      std::cout << mbytes_transfered << " | ";
    } else {
      std::cout << "-" << " | ";
    }
    std::cout << std::setw(8) << avg_time << " | ";
    std::cout << std::setw(8) << total_time << " |" << std::endl;
  }

  std::cout << kLine << std::endl;
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
}

void DisableProfiling() {
  if (collector != nullptr) {
    collector->DisableTracing();
    PrintResults();
    delete collector;
  }
}