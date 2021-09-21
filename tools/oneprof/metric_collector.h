//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_METRIC_COLLECTOR_H_
#define PTI_TOOLS_ONEPROF_METRIC_COLLECTOR_H_

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "metric_storage.h"
#include "ze_utils.h"

#define MAX_REPORT_SIZE  512
#define MAX_REPORT_COUNT 32768
#define MAX_BUFFER_SIZE  (MAX_REPORT_COUNT * MAX_REPORT_SIZE)
#define WAIT_DELAY       10000000 // 10 ms

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

class MetricCollector {
 public: // Interface
  static MetricCollector* Create(
      ze_driver_handle_t driver,
      ze_device_handle_t device,
      const char* group_name,
      uint32_t sampling_interval) {
    PTI_ASSERT(driver != nullptr);
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(group_name != nullptr);
    PTI_ASSERT(sampling_interval > 0);

    ze_context_handle_t context = utils::ze::GetContext(driver);
    PTI_ASSERT(context != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::vector<zet_metric_group_handle_t> metric_group_list;
    for (auto sub_device : sub_device_list) {
      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device, group_name,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      if (group == nullptr) {
        std::cerr << "[WARNING] Unable to find target metric group: " <<
          group_name << std::endl;
        return nullptr;
      }
      metric_group_list.push_back(group);
    }
    PTI_ASSERT(metric_group_list.size() == sub_device_list.size());

    return new MetricCollector(
        context, sub_device_list, metric_group_list, sampling_interval);
  }

  void DisableCollection() {
    DisableMetrics();

    PTI_ASSERT(metric_storage_ != nullptr);
    delete metric_storage_;
    metric_storage_ = nullptr;

    ComputeMetrics();

    metric_reader_ = new MetricReader(sub_device_list_.size(), "bin");
    PTI_ASSERT(metric_reader_ != nullptr);
  }

  ~MetricCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_DISABLED);

    PTI_ASSERT(context_ != nullptr);
    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (metric_storage_ != nullptr) {
      delete metric_storage_;
    }

    PTI_ASSERT(metric_reader_ != nullptr);
    delete metric_reader_;
  }

  void ResetReportReader() {
    PTI_ASSERT(metric_reader_ != nullptr);
    metric_reader_->Reset();
  }

  std::vector<zet_typed_value_t> GetReportChunk(uint32_t sub_device_id) const {
    ze_result_t status = ZE_RESULT_SUCCESS;

    PTI_ASSERT(sub_device_id < sub_device_list_.size());
    PTI_ASSERT(sub_device_list_.size() == metric_group_list_.size());
    PTI_ASSERT(!metric_group_list_.empty());
    PTI_ASSERT(metric_storage_ == nullptr);
    PTI_ASSERT(metric_reader_ != nullptr);

    uint32_t report_size = GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    uint32_t report_size_in_bytes = report_size * sizeof(zet_typed_value_t);
    uint32_t chunk_size = report_size_in_bytes * MAX_REPORT_COUNT;
    std::vector<uint8_t> metric_data =
      metric_reader_->ReadChunk(chunk_size, sub_device_id);
    if (metric_data.empty()) {
      return std::vector<zet_typed_value_t>();
    }

    uint32_t report_count = metric_data.size() / report_size_in_bytes;
    PTI_ASSERT(report_count * report_size_in_bytes == metric_data.size());
    std::vector<zet_typed_value_t> report_chunk(report_count * report_size);
    memcpy(report_chunk.data(), metric_data.data(), metric_data.size());

    return report_chunk;
  }

  uint32_t GetReportSize(uint32_t sub_device_id) const {
    PTI_ASSERT(sub_device_id < sub_device_list_.size());
    PTI_ASSERT(sub_device_list_.size() == metric_group_list_.size());
    PTI_ASSERT(!metric_group_list_.empty());

    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = zetMetricGroupGetProperties(
        metric_group_list_[sub_device_id], &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    return group_props.metricCount;
  }

  std::vector<std::string> GetMetricList(uint32_t sub_device_id) const {
    ze_result_t status = ZE_RESULT_SUCCESS;

    PTI_ASSERT(sub_device_id < sub_device_list_.size());
    PTI_ASSERT(sub_device_list_.size() == metric_group_list_.size());
    PTI_ASSERT(!metric_group_list_.empty());

    uint32_t metric_count = GetReportSize(sub_device_id);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metric_list(metric_count);
    status = zetMetricGet(
        metric_group_list_[sub_device_id], &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metric_list.size());

    std::vector<std::string> name_list;
    for (auto metric : metric_list) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = zetMetricGetProperties(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      name_list.push_back(metric_props.name);
    }

    return name_list;
  }

  std::vector<zet_metric_type_t> GetMetricTypeList(uint32_t sub_device_id) const {
    ze_result_t status = ZE_RESULT_SUCCESS;

    PTI_ASSERT(sub_device_id < sub_device_list_.size());
    PTI_ASSERT(sub_device_list_.size() == metric_group_list_.size());
    PTI_ASSERT(!metric_group_list_.empty());

    uint32_t metric_count = GetReportSize(sub_device_id);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metric_list(metric_count);
    status = zetMetricGet(
        metric_group_list_[sub_device_id], &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metric_list.size());

    std::vector<zet_metric_type_t> type_list;
    for (auto metric : metric_list) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = zetMetricGetProperties(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      type_list.push_back(metric_props.metricType);
    }

    return type_list;
  }

  MetricCollector(const MetricCollector& copy) = delete;
  MetricCollector& operator=(const MetricCollector& copy) = delete;

 private: // Implementation
  MetricCollector(
      ze_context_handle_t context,
      const std::vector<ze_device_handle_t>& sub_device_list,
      const std::vector<zet_metric_group_handle_t>& metric_group_list,
      uint32_t sampling_interval)
      : context_(context),
        sub_device_list_(sub_device_list),
        metric_group_list_(metric_group_list),
        sampling_interval_(sampling_interval) {
    PTI_ASSERT(context_ != nullptr);
    PTI_ASSERT(!sub_device_list_.empty());
    PTI_ASSERT(!metric_group_list_.empty());
    PTI_ASSERT(sampling_interval_ > 0);

    metric_storage_ = new MetricStorage(sub_device_list_.size(), "raw");
    PTI_ASSERT(metric_storage_ != nullptr);

    EnableMetrics();
  }

  void ComputeMetrics() {
    PTI_ASSERT(metric_storage_ == nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    MetricReader reader(sub_device_list_.size(), "raw");
    MetricStorage storage(sub_device_list_.size(), "bin");

    for (size_t i = 0; i < sub_device_list_.size(); ++i) {
      while (true) {
        PTI_ASSERT(sub_device_list_.size() == metric_group_list_.size());
        PTI_ASSERT(!metric_group_list_.empty());

        std::vector<uint8_t> metric_data =
          reader.ReadChunk(MAX_BUFFER_SIZE, i);
        if (metric_data.empty()) {
          break;
        }

        uint32_t value_count = 0;
        status = zetMetricGroupCalculateMetricValues(
            metric_group_list_[i],
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            metric_data.size(), metric_data.data(), &value_count, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        PTI_ASSERT(value_count > 0);

        std::vector<zet_typed_value_t> report_chunk(value_count);
        status = zetMetricGroupCalculateMetricValues(
            metric_group_list_[i],
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            metric_data.size(), metric_data.data(),
            &value_count, report_chunk.data());
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        report_chunk.resize(value_count);

        storage.Dump(
            reinterpret_cast<uint8_t*>(report_chunk.data()),
            report_chunk.size() * sizeof(zet_typed_value_t), i);
      }
    }
  }

  void EnableMetrics() {
    PTI_ASSERT(collector_thread_ == nullptr);
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_IDLE);

    collector_state_.store(COLLECTOR_STATE_IDLE, std::memory_order_release);
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

  void AppendMetrics(
      const uint8_t* storage, uint32_t size, uint32_t sub_device_id) {
    PTI_ASSERT(storage != nullptr);
    PTI_ASSERT(size > 0);
    metric_storage_->Dump(storage, size, sub_device_id);
  }

  static void CollectChunk(
      MetricCollector* collector,
      const std::vector<ze_event_handle_t>& event_list,
      const std::vector<zet_metric_streamer_handle_t>& metric_streamer_list,
      std::vector<uint8_t>& storage) {
    PTI_ASSERT(collector != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    for (size_t i = 0; i < event_list.size(); ++i) {
      status = zeEventHostSynchronize(event_list[i], WAIT_DELAY);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);
      if (status == ZE_RESULT_SUCCESS) {
        status = zeEventHostReset(event_list[i]);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }

      size_t data_size = 0;
      status = zetMetricStreamerReadData(
          metric_streamer_list[i], UINT32_MAX, &data_size, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      PTI_ASSERT(data_size > 0);
      PTI_ASSERT(data_size <= storage.size());

      status = zetMetricStreamerReadData(
          metric_streamer_list[i], UINT32_MAX, &data_size, storage.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      collector->AppendMetrics(storage.data(), data_size, i);
    }
  }

  static void Collect(MetricCollector* collector) {
    PTI_ASSERT(collector != nullptr);

    PTI_ASSERT(collector->context_ != nullptr);
    PTI_ASSERT(!collector->metric_group_list_.empty());
    PTI_ASSERT(collector->sub_device_list_.size() ==
               collector->metric_group_list_.size());

    ze_result_t status = ZE_RESULT_SUCCESS;

    for (size_t i = 0; i < collector->sub_device_list_.size(); ++i) {
      status = zetContextActivateMetricGroups(
          collector->context_, collector->sub_device_list_[i],
          1, &collector->metric_group_list_[i]);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    PTI_ASSERT(collector->sub_device_list_.size() <
               (std::numeric_limits<uint32_t>::max)());
    uint32_t sub_device_count =
      static_cast<uint32_t>(collector->sub_device_list_.size());

    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE, sub_device_count};
    ze_event_pool_handle_t event_pool = nullptr;
    status = zeEventPoolCreate(
        collector->context_, &event_pool_desc, sub_device_count,
        collector->sub_device_list_.data(), &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::vector<ze_event_handle_t> event_list;
    std::vector<zet_metric_streamer_handle_t> metric_streamer_list;
    for (uint32_t i = 0; i < sub_device_count; ++i) {
      ze_event_desc_t event_desc = {
          ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, i,
          ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
      ze_event_handle_t event = nullptr;
      status = zeEventCreate(event_pool, &event_desc, &event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      event_list.push_back(event);

      zet_metric_streamer_desc_t metric_streamer_desc = {
          ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC,
          nullptr,
          MAX_REPORT_COUNT,
          collector->sampling_interval_};
      zet_metric_streamer_handle_t metric_streamer = nullptr;
      status = zetMetricStreamerOpen(
          collector->context_, collector->sub_device_list_[i],
          collector->metric_group_list_[i], &metric_streamer_desc,
          event, &metric_streamer);
      if (status != ZE_RESULT_SUCCESS) {
        std::cout <<
          "[WARNING] Sampling interval is not supported" << std::endl;
        break;
      }

      PTI_ASSERT(metric_streamer_desc.notifyEveryNReports == MAX_REPORT_COUNT);
      metric_streamer_list.push_back(metric_streamer);
    }

    std::vector<uint8_t> storage(MAX_BUFFER_SIZE);

    collector->collector_state_.store(
        COLLECTOR_STATE_ENABLED, std::memory_order_release);

    if (metric_streamer_list.size() == sub_device_count) {
      while (collector->collector_state_.load(std::memory_order_acquire) !=
            COLLECTOR_STATE_DISABLED) {
        CollectChunk(collector, event_list, metric_streamer_list, storage);
      }
      CollectChunk(collector, event_list, metric_streamer_list, storage);
    }

    for (auto metric_streamer : metric_streamer_list) {
      status = zetMetricStreamerClose(metric_streamer);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    for (auto event : event_list) {
      status = zeEventDestroy(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    status = zeEventPoolDestroy(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (auto sub_device : collector->sub_device_list_) {
      status = zetContextActivateMetricGroups(
          collector->context_,sub_device, 0, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

 private: // Data
  std::vector<ze_device_handle_t> sub_device_list_;
  ze_context_handle_t context_ = nullptr;

  std::thread* collector_thread_ = nullptr;
  std::atomic<CollectorState> collector_state_{COLLECTOR_STATE_IDLE};

  std::vector<zet_metric_group_handle_t> metric_group_list_;
  MetricStorage* metric_storage_ = nullptr;
  MetricReader* metric_reader_ = nullptr;

  uint32_t sampling_interval_ = 0;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_COLLECTOR_H_