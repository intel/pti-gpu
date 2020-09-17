//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_GPU_METRICS_METRIC_COLLECTOR_H_
#define PTI_SAMPLES_CL_GPU_METRICS_METRIC_COLLECTOR_H_

#include <string.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "metric_device.h"

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

class MetricCollector {
 public:
  MetricCollector(const char* name) : state_(COLLECTOR_STATE_IDLE) {
    if (name == nullptr) {
      return;
    }

    device_ = MetricDevice::Create();
    if (device_ == nullptr) {
      std::cout << "[WARNING] Unable to find MD library" << std::endl;
      return;
    }

    if (!FindMetricSet(name)) {
      std::cout << "[WARNING] Metric set is not found: " << name << std::endl;
      return;
    }
  }

  ~MetricCollector() {
    if (device_ != nullptr) {
      delete device_;
    }
  }

  bool IsValid() const {
    return !(device_ == nullptr || group_ == nullptr || set_ == nullptr);
  }

  bool Enable() {
    if (!IsValid()) {
      return false;
    }

    if (collector_ != nullptr) {
      return false;
    }

    PTI_ASSERT(state_.load(std::memory_order_acquire) == COLLECTOR_STATE_IDLE);
    collector_ = new std::thread(Collect, this);
    PTI_ASSERT(collector_ != nullptr);

    while (state_.load(std::memory_order_acquire) != COLLECTOR_STATE_ENABLED) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return true;
  }

  bool Disable() {
    if (!IsValid()) {
      return false;
    }

    if (collector_ == nullptr) {
      return false;
    }

    PTI_ASSERT(
        state_.load(std::memory_order_acquire) == COLLECTOR_STATE_ENABLED);
    state_.store(COLLECTOR_STATE_DISABLED, std::memory_order_release);
    collector_->join();
    delete collector_;

    collector_ = nullptr;
    state_.store(COLLECTOR_STATE_IDLE, std::memory_order_release);

    return true;
  }

  bool GetGpuCpuTimestamps(uint64_t* gpu_timestamp,
                           uint64_t* cpu_timestamp) const {
    if (!IsValid()) {
      return false;
    }

    if (gpu_timestamp == nullptr || cpu_timestamp == nullptr) {
      return false;
    }

    md::TCompletionCode status = md::CC_OK;
    const MetricDevice& device = *device_;
    status = device->GetGpuCpuTimestamps(
        gpu_timestamp, cpu_timestamp, nullptr);
    PTI_ASSERT(status == md::CC_OK);

    return true;
  }

  int GetMetricInfoId(const char* name) const {
    if (!IsValid()) {
      return -1;
    }

    if (name == nullptr) {
      return -1;
    }

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

  uint32_t GetCalculatedReportSize() const {
    if (!IsValid()) {
      return 0;
    }

    return set_->GetParams()->MetricsCount +
           set_->GetParams()->InformationCount;
  }

  std::vector<md::TTypedValue_1_0> Calculate() const {
    if (!IsValid()) {
      return std::vector<md::TTypedValue_1_0>();
    }

    if (collector_ != nullptr) {
      return std::vector<md::TTypedValue_1_0>();
    }

    if (storage_.size() == 0) {
      return std::vector<md::TTypedValue_1_0>();
    }

    size_t raw_report_count =
      storage_.size() / set_->GetParams()->RawReportSize;
    PTI_ASSERT(storage_.size() ==
               raw_report_count * set_->GetParams()->RawReportSize);

    uint32_t calculated_report_size = GetCalculatedReportSize();
    std::vector<md::TTypedValue_1_0> calculated_reports(
        calculated_report_size * raw_report_count);

    uint32_t calculated_report_count = 0;
    md::TCompletionCode status = md::CC_OK;

    PTI_ASSERT(storage_.size() < (std::numeric_limits<uint32_t>::max)());
    PTI_ASSERT(calculated_reports.size() * sizeof(md::TTypedValue_1_0) <
               (std::numeric_limits<uint32_t>::max)());
    status = set_->CalculateMetrics(storage_.data(),
                                    static_cast<uint32_t>(storage_.size()),
                                    calculated_reports.data(),
                                    static_cast<uint32_t>(
                                        calculated_reports.size() *
                                        sizeof(md::TTypedValue_1_0)),
                                    &calculated_report_count, nullptr, 0);
    PTI_ASSERT(status == md::CC_OK);
    calculated_reports.resize(calculated_report_count * calculated_report_size);

    return calculated_reports;
  }

  MetricCollector(const MetricCollector& copy) = delete;
  MetricCollector& operator=(const MetricCollector& copy) = delete;

 private:
  bool FindMetricSet(const char* name) {
    PTI_ASSERT(name != nullptr);
    PTI_ASSERT(device_ != nullptr);

    const MetricDevice& device = *device_;
    uint32_t group_count = device->GetParams()->ConcurrentGroupsCount;
    for (uint32_t gid = 0; gid < group_count; ++gid) {
      md::IConcurrentGroup_1_5* group = device->GetConcurrentGroup(gid);
      PTI_ASSERT(group != nullptr);

      uint32_t set_count = group->GetParams()->MetricSetsCount;
      for (uint32_t sid = 0; sid < set_count; ++sid) {
        md::IMetricSet_1_5* set = group->GetMetricSet(sid);
        PTI_ASSERT(set != nullptr);

        if (strcmp(name, set->GetParams()->SymbolName) == 0) {
          group_ = group;
          set_ = set;
          return true;
        }
      }
    }
    return false;
  }

  static void Collect(MetricCollector* collector) {
    PTI_ASSERT(collector->IsValid());

    md::IConcurrentGroup_1_5* group = collector->group_;
    md::IMetricSet_1_5* set = collector->set_;

    std::vector<uint8_t>& storage = collector->storage_;
    uint32_t sampling_interval = 100000; // nanoseconds
    uint32_t buffer_size = 0; // defined by MDAPI
    md::TCompletionCode status = md::CC_OK;

    status = set->SetApiFiltering(md::API_TYPE_IOSTREAM);
    PTI_ASSERT(status == md::CC_OK);

    status = group->OpenIoStream(set, 0, &sampling_interval, &buffer_size);
    if (status != md::CC_OK) {
      std::cout << "[WARNING] Cannot start metrics collection " <<
        "(OpenIOStream error code " << status << ")" << std::endl;
      collector->state_.store(COLLECTOR_STATE_ENABLED,
                              std::memory_order_release);
      return;
    } else {
      std::cout << "[INFO] Metrics collection is started with " <<
        "sampling interval " << sampling_interval << " ns and buffer size " <<
        buffer_size << " bytes" << std::endl;
    }

    PTI_ASSERT(buffer_size > 0);
    uint32_t max_report_count = buffer_size / set->GetParams()->RawReportSize;
    uint32_t wait_time = 500; // milliseconds

    collector->state_.store(COLLECTOR_STATE_ENABLED,
                            std::memory_order_release);
    while (collector->state_.load(std::memory_order_acquire) !=
           COLLECTOR_STATE_DISABLED) {
      status = group->WaitForReports(wait_time);
      PTI_ASSERT(status == md::CC_OK || status == md::CC_WAIT_TIMEOUT ||
            status == md::CC_INTERRUPTED);

      size_t size = storage.size();
      storage.resize(size + max_report_count * set->GetParams()->RawReportSize);

      uint32_t report_count = max_report_count;
      status = group->ReadIoStream(&report_count, (char*)(storage.data() + size),
          md::IO_READ_FLAG_DROP_OLD_REPORTS);
      PTI_ASSERT(status == md::CC_OK || status == md::CC_READ_PENDING);

      PTI_ASSERT(report_count <= max_report_count);
      storage.resize(size + report_count * set->GetParams()->RawReportSize);
    }

    status = group->CloseIoStream();
    PTI_ASSERT(status == md::CC_OK);
  }

 private:
  std::vector<uint8_t> storage_;
  MetricDevice* device_ = nullptr;

  md::IConcurrentGroup_1_5* group_ = nullptr;
  md::IMetricSet_1_5* set_ = nullptr;

  std::atomic<uint32_t> state_;
  std::thread* collector_ = nullptr;
};

#endif // PTI_SAMPLES_CL_GPU_METRICS_METRIC_COLLECTOR_H_