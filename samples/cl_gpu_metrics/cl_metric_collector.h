//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_GPU_METRICS_CL_METRIC_COLLECTOR_H_
#define PTI_SAMPLES_CL_GPU_METRICS_CL_METRIC_COLLECTOR_H_

#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "cl_utils.h"
#include "metric_device.h"

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

class ClMetricCollector {
 public:
  static ClMetricCollector* Create(
      cl_device_id device, const char* set_name) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(set_name != nullptr);

    std::string device_string = utils::GetEnv("PTI_DEVICE_ID");
    uint32_t device_id =
      device_string.empty() ? 0 : std::stoul(device_string);
    std::string sub_device_string = utils::GetEnv("PTI_SUB_DEVICE_ID");
    uint32_t sub_device_id =
      sub_device_string.empty() ? 0 : std::stoul(sub_device_string);

    MetricDevice* metric_device =
      MetricDevice::Create(device_id, sub_device_id);
    if (metric_device == nullptr) {
      std::cerr << "[WARNING] Unable to find MD library" << std::endl;
      return nullptr;
    }

    md::IConcurrentGroup_1_5* group =
      metric_device->FindMetricGroup(set_name);
    md::IMetricSet_1_5* set = metric_device->FindMetricSet(set_name);
    if (group == nullptr || set == nullptr) {
      std::cerr << "[WARNING] Metric set is not found: " <<
        set_name << std::endl;
      delete metric_device;
      return nullptr;
    }

    return new ClMetricCollector(metric_device, group, set);
  }

  ~ClMetricCollector() {
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_DISABLED);
    PTI_ASSERT(device_ != nullptr);
    delete device_;
  }

  void DisableCollection() {
    DisableMetrics();
  }

  uint64_t GetKernelTimestamp(uint64_t report_timestamp) const {
    md::TCompletionCode status = md::CC_OK;
    uint64_t gpu_snap_point = 0, cpu_snap_point = 0;

    const MetricDevice& device = *device_;
    status = device->GetGpuCpuTimestamps(
        &gpu_snap_point, &cpu_snap_point, nullptr);
    PTI_ASSERT(status == md::CC_OK);

    uint64_t cpu_timestamp = cpu_snap_point -
      (gpu_snap_point - report_timestamp);
#if defined(__gnu_linux__)
    cpu_timestamp = utils::ConvertClockMonotonicToRaw(cpu_timestamp);
#endif
    return cpu_timestamp;
  }

  int GetMetricId(const char* name) const {
    PTI_ASSERT(name != nullptr);
    PTI_ASSERT(set_ != nullptr);

    for (uint32_t mid = 0; mid < set_->GetParams()->MetricsCount; ++mid) {
      md::IMetric_1_0* metric = set_->GetMetric(mid);
      PTI_ASSERT(metric != nullptr);
      if (strcmp(metric->GetParams()->SymbolName, name) == 0) {
        return mid;
      }
    }

    for (uint32_t iid = 0; iid < set_->GetParams()->InformationCount; ++iid) {
      md::IInformation_1_0* info = set_->GetInformation(iid);
      PTI_ASSERT(info != nullptr);
      if (strcmp(info->GetParams()->SymbolName, name) == 0) {
        return iid + set_->GetParams()->MetricsCount;
      }
    }

    return -1;
  }

  uint32_t GetReportSize() const {
    PTI_ASSERT(set_ != nullptr);
    return set_->GetParams()->MetricsCount +
      set_->GetParams()->InformationCount;
  }

  std::vector<md::TTypedValue_1_0> GetReportList() const {
    PTI_ASSERT(set_ != nullptr);

    if (metric_storage_.size() == 0) {
      return std::vector<md::TTypedValue_1_0>();
    }

    size_t raw_report_count =
      metric_storage_.size() / set_->GetParams()->RawReportSize;
    PTI_ASSERT(
        metric_storage_.size() ==
        raw_report_count * set_->GetParams()->RawReportSize);

    uint32_t calculated_report_size = GetReportSize();
    std::vector<md::TTypedValue_1_0> calculated_reports(
        calculated_report_size * raw_report_count);

    uint32_t calculated_report_count = 0;
    md::TCompletionCode status = md::CC_OK;

    PTI_ASSERT(
        metric_storage_.size() < (std::numeric_limits<uint32_t>::max)());
    PTI_ASSERT(
        calculated_reports.size() * sizeof(md::TTypedValue_1_0) <
        (std::numeric_limits<uint32_t>::max)());
    status = set_->CalculateMetrics(
        metric_storage_.data(),
        static_cast<uint32_t>(metric_storage_.size()),
        calculated_reports.data(),
        static_cast<uint32_t>(
            calculated_reports.size() * sizeof(md::TTypedValue_1_0)),
        &calculated_report_count, nullptr, 0);
    PTI_ASSERT(status == md::CC_OK);
    calculated_reports.resize(
        calculated_report_count * calculated_report_size);

    return calculated_reports;
  }

  ClMetricCollector(const ClMetricCollector& copy) = delete;
  ClMetricCollector& operator=(const ClMetricCollector& copy) = delete;

 private: // Implementation Details
  ClMetricCollector(
      MetricDevice* device, md::IConcurrentGroup_1_5* group,
      md::IMetricSet_1_5* set)
      : device_(device), group_(group), set_(set) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(group_ != nullptr);
    PTI_ASSERT(set_ != nullptr);
    EnableMetrics();
  }

  void EnableMetrics() {
    PTI_ASSERT(collector_thread_ == nullptr);
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_IDLE);

    collector_thread_ = new std::thread(Collect, this);
    PTI_ASSERT(collector_thread_ != nullptr);

    while (collector_state_.load(std::memory_order_acquire) !=
          COLLECTOR_STATE_ENABLED) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  void DisableMetrics() {
    PTI_ASSERT(collector_thread_ != nullptr);
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_ENABLED);

    collector_state_.store(
        COLLECTOR_STATE_DISABLED, std::memory_order_release);
    collector_thread_->join();
    delete collector_thread_;
  }

  void AppendMetrics(const std::vector<uint8_t>& storage) {
    PTI_ASSERT(storage.size() > 0);

    size_t intial_size = metric_storage_.size();
    metric_storage_.resize(intial_size + storage.size());
    std::copy(storage.begin(), storage.end(),
              metric_storage_.begin() + intial_size);
  }

  static void Collect(ClMetricCollector* collector) {
    PTI_ASSERT(collector != nullptr);

    PTI_ASSERT(collector->group_ != nullptr);
    PTI_ASSERT(collector->set_ != nullptr);

    md::IConcurrentGroup_1_5* group = collector->group_;
    md::IMetricSet_1_5* set = collector->set_;

    uint32_t sampling_interval = 100000; // nanoseconds
    uint32_t buffer_size = 0; // defined by MDAPI
    md::TCompletionCode status = md::CC_OK;

    status = set->SetApiFiltering(md::API_TYPE_IOSTREAM);
    PTI_ASSERT(status == md::CC_OK);

    status = group->OpenIoStream(set, 0, &sampling_interval, &buffer_size);
    PTI_ASSERT(status == md::CC_OK);

    PTI_ASSERT(buffer_size > 0);
    uint32_t max_report_count = buffer_size / set->GetParams()->RawReportSize;
    uint32_t wait_time = 500; // milliseconds

    collector->collector_state_.store(
        COLLECTOR_STATE_ENABLED, std::memory_order_release);

    std::vector<uint8_t> storage;
    while (collector->collector_state_.load(std::memory_order_acquire) !=
           COLLECTOR_STATE_DISABLED) {
      status = group->WaitForReports(wait_time);
      PTI_ASSERT(status == md::CC_OK || status == md::CC_WAIT_TIMEOUT ||
            status == md::CC_INTERRUPTED);

      storage.resize(max_report_count * set->GetParams()->RawReportSize);

      uint32_t report_count = max_report_count;
      status = group->ReadIoStream(
          &report_count, reinterpret_cast<char*>(storage.data()),
          md::IO_READ_FLAG_DROP_OLD_REPORTS);
      PTI_ASSERT(status == md::CC_OK || status == md::CC_READ_PENDING);

      PTI_ASSERT(report_count <= max_report_count);
      storage.resize(report_count * set->GetParams()->RawReportSize);

      collector->AppendMetrics(storage);
    }

    status = group->CloseIoStream();
    PTI_ASSERT(status == md::CC_OK);
  }

 private: // Data
  MetricDevice* device_ = nullptr;
  md::IConcurrentGroup_1_5* group_ = nullptr;
  md::IMetricSet_1_5* set_ = nullptr;

  std::atomic<CollectorState> collector_state_{COLLECTOR_STATE_IDLE};
  std::thread* collector_thread_ = nullptr;

  std::vector<uint8_t> metric_storage_;
};

#endif // PTI_SAMPLES_CL_GPU_METRICS_CL_METRIC_COLLECTOR_H_