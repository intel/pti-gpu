//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iomanip>
#include <iostream>
#include <set>

#include "cl_metric_collector.h"
#include "cl_kernel_collector.h"

struct Kernel {
  uint64_t total_time;
  uint64_t call_count;
  float eu_active;
  float eu_stall;

  bool operator>(const Kernel& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const Kernel& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using KernelMap = std::map<std::string, Kernel>;

const uint32_t kKernelLength = 10;
const uint32_t kCallsLength = 12;
const uint32_t kTimeLength = 20;
const uint32_t kPercentLength = 16;

static ClMetricCollector* metric_collector = nullptr;
static ClKernelCollector* kernel_collector = nullptr;

static std::chrono::steady_clock::time_point start;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_gpu_metrics[.exe] <application> <args>" <<
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
void SetToolEnv() {}

// Internal Tool Functionality ////////////////////////////////////////////////

static KernelMap GetKernelMap() {
  PTI_ASSERT(kernel_collector != nullptr);
  PTI_ASSERT(metric_collector != nullptr);

  std::vector<md::TTypedValue_1_0> report_list =
    metric_collector->GetReportList();
  if (report_list.size() == 0) {
    return KernelMap();
  }

  const KernelIntervalList& kernel_interval_list =
    kernel_collector->GetKernelIntervalList();
  if (kernel_interval_list.size() == 0) {
    return KernelMap();
  }

  KernelMap kernel_map;

  int gpu_timestamp_id = metric_collector->GetMetricId("QueryBeginTime");
  PTI_ASSERT(gpu_timestamp_id >= 0);
  int eu_active_id = metric_collector->GetMetricId("EuActive");
  PTI_ASSERT(eu_active_id >= 0);
  int eu_stall_id = metric_collector->GetMetricId("EuStall");
  PTI_ASSERT(eu_stall_id >= 0);

  uint32_t report_size = metric_collector->GetReportSize();
  PTI_ASSERT(report_size > 0);

  for (auto& kernel : kernel_interval_list) {
    uint32_t sample_count = 0;
    float eu_active = 0.0f, eu_stall = 0.0f;

    const md::TTypedValue_1_0* report = report_list.data();
    while (report < report_list.data() + report_list.size()) {
      PTI_ASSERT(report[gpu_timestamp_id].ValueType == md::VALUE_TYPE_UINT64);
      uint64_t report_timestamp =
        metric_collector->GetKernelTimestamp(
            report[gpu_timestamp_id].ValueUInt64);

      if (report_timestamp >= kernel.start && report_timestamp <= kernel.end) {
        PTI_ASSERT(report[eu_active_id].ValueType == md::VALUE_TYPE_FLOAT);
        eu_active += report[eu_active_id].ValueFloat;
        PTI_ASSERT(report[eu_stall_id].ValueType == md::VALUE_TYPE_FLOAT);
        eu_stall += report[eu_stall_id].ValueFloat;
        ++sample_count;
      }

      if (report_timestamp > kernel.end) {
        break;
      }

      report += report_size;
    }

    if (sample_count > 0) {
      eu_active /= sample_count;
      eu_stall /= sample_count;
    } else {
      std::cerr << "[WARNING] No samples found for a kernel instance of " <<
        kernel.name << ", results may be inaccurate" << std::endl;
    }

    if (kernel_map.count(kernel.name) == 0) {
      kernel_map[kernel.name] =
        {kernel.end - kernel.start, 1, eu_active, eu_stall};
    } else {
      Kernel& kernel_info = kernel_map[kernel.name];
      kernel_info.total_time += (kernel.end - kernel.start);
      kernel_info.eu_active =
        (kernel_info.eu_active * kernel_info.call_count + eu_active) /
        (kernel_info.call_count + 1);
      kernel_info.eu_stall =
        (kernel_info.eu_stall * kernel_info.call_count + eu_stall) /
        (kernel_info.call_count + 1);
      kernel_info.call_count += 1;
    }
  }

  return kernel_map;
}

static void PrintResults() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  KernelMap kernel_map = GetKernelMap();
  if (kernel_map.size() == 0) {
    return;
  }

  std::set< std::pair<std::string, Kernel>,
            utils::Comparator > sorted_list(
      kernel_map.begin(), kernel_map.end());

  uint64_t total_duration = 0;
  size_t max_name_length = kKernelLength;
  for (auto& value : sorted_list) {
    total_duration += value.second.total_time;
    if (value.first.size() > max_name_length) {
      max_name_length = value.first.size();
    }
  }

  if (total_duration == 0) {
    return;
  }

  std::cerr << std::endl;
  std::cerr << "=== Device Metrics: ===" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Total Execution Time (ns): " << time.count() << std::endl;
  std::cerr << "Total Kernel Time (ns): " << total_duration << std::endl;
  std::cerr << std::endl;

  std::cerr << std::setw(max_name_length) << "Kernel" << "," <<
    std::setw(kCallsLength) << "Calls" << "," <<
    std::setw(kTimeLength) << "Time (ns)" << "," <<
    std::setw(kPercentLength) << "Time (%)" << "," <<
    std::setw(kTimeLength) << "Average (ns)" << "," <<
    std::setw(kPercentLength) << "EU Active (%)" << "," <<
    std::setw(kPercentLength) << "EU Stall (%)" << "," <<
    std::setw(kPercentLength) << "EU Idle (%)" << std::endl;

  for (auto& value : sorted_list) {
    const std::string& kernel = value.first;
    uint64_t call_count = value.second.call_count;
    uint64_t duration = value.second.total_time;
    uint64_t avg_duration = duration / call_count;
    float percent_duration = 100.0f * duration / total_duration;
    float eu_active = value.second.eu_active;
    float eu_stall = value.second.eu_stall;
    float eu_idle = 0.0f;
    if (eu_active + eu_stall < 100.0f) {
      eu_idle = 100.f - eu_active - eu_stall;
    }
    std::cerr << std::setw(max_name_length) << kernel << "," <<
      std::setw(kCallsLength) << call_count << "," <<
      std::setw(kTimeLength) << duration << "," <<
      std::setw(kPercentLength) << std::setprecision(2) <<
        std::fixed << percent_duration << "," <<
      std::setw(kTimeLength) << avg_duration << "," <<
      std::setw(kPercentLength) << std::setprecision(2) <<
        std::fixed << eu_active << "," <<
      std::setw(kPercentLength) << std::setprecision(2) <<
        std::fixed << eu_stall << "," <<
      std::setw(kPercentLength) << std::setprecision(2) <<
        std::fixed << eu_idle << std::endl;
  }

  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cerr << "[WARNING] Unable to find target GPU device for tracing" <<
      std::endl;
    return;
  }

  kernel_collector = ClKernelCollector::Create(device);
  if (kernel_collector == nullptr) {
    return;
  }

  metric_collector = ClMetricCollector::Create(device, "ComputeBasic");
  if (metric_collector == nullptr) {
    kernel_collector->DisableTracing();
    delete kernel_collector;
    kernel_collector = nullptr;
    return;
  }

  start = std::chrono::steady_clock::now();
}

void DisableProfiling() {
  if (kernel_collector != nullptr || metric_collector != nullptr) {
    PTI_ASSERT(kernel_collector != nullptr);
    PTI_ASSERT(metric_collector != nullptr);
    kernel_collector->DisableTracing();
    metric_collector->DisableTracing();
    PrintResults();
    delete kernel_collector;
    delete metric_collector;
  }
}