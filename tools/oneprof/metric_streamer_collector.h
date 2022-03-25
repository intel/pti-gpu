//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_METRIC_STREAMER_COLLECTOR_H_
#define PTI_TOOLS_ONEPROF_METRIC_STREAMER_COLLECTOR_H_

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "metric_storage.h"
#include "ze_utils.h"

#define WAIT_DELAY       10000000 // 10 ms

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

class MetricStreamerCollector {
 public: // Interface
  static MetricStreamerCollector* Create(
      ze_driver_handle_t driver,
      ze_device_handle_t device,
      const char* group_name,
      uint32_t sampling_interval,
      const std::string& raw_data_path) {
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

    return new MetricStreamerCollector(
        context, sub_device_list, metric_group_list,
        sampling_interval, raw_data_path);
  }

  void DisableMetrics() {
    PTI_ASSERT(collector_thread_ != nullptr);
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_ENABLED);
    collector_state_.store(
        COLLECTOR_STATE_DISABLED, std::memory_order_release);
    collector_thread_->join();
    delete collector_thread_;

    PTI_ASSERT(metric_storage_ != nullptr);
    delete metric_storage_;
    metric_storage_ = nullptr;
  }

  ~MetricStreamerCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_DISABLED);

    PTI_ASSERT(context_ != nullptr);
    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (metric_storage_ != nullptr) {
      delete metric_storage_;
      PTI_ASSERT(0);
    }
  }

  MetricStreamerCollector(const MetricStreamerCollector& copy) = delete;
  MetricStreamerCollector& operator=(const MetricStreamerCollector& copy) = delete;

 private: // Implementation
  MetricStreamerCollector(
      ze_context_handle_t context,
      const std::vector<ze_device_handle_t>& sub_device_list,
      const std::vector<zet_metric_group_handle_t>& metric_group_list,
      uint32_t sampling_interval,
      const std::string& raw_data_path)
      : context_(context),
        sub_device_list_(sub_device_list),
        metric_group_list_(metric_group_list),
        sampling_interval_(sampling_interval),
        raw_data_path_(raw_data_path) {
    PTI_ASSERT(context_ != nullptr);
    PTI_ASSERT(!sub_device_list_.empty());
    PTI_ASSERT(!metric_group_list_.empty());
    PTI_ASSERT(sampling_interval_ > 0);

    metric_storage_ = MetricStorage::Create(
        sub_device_list_.size(), utils::GetPid(), "raw", raw_data_path_);
    PTI_ASSERT(metric_storage_ != nullptr);

    EnableMetrics();
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

  void AppendMetrics(
      const uint8_t* storage, uint32_t size, uint32_t sub_device_id) {
    PTI_ASSERT(storage != nullptr);
    PTI_ASSERT(size > 0);
    metric_storage_->Dump(storage, size, sub_device_id);
  }

  static void CollectChunk(
      MetricStreamerCollector* collector,
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

      if (data_size > 0) {
        collector->AppendMetrics(storage.data(), data_size, i);
      }
    }
  }

  static void Collect(MetricStreamerCollector* collector) {
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
        std::cerr <<
          "[WARNING] Sampling interval is not supported" << std::endl;
        collector->collector_state_.store(
          COLLECTOR_STATE_ENABLED, std::memory_order_release);
        return;
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

  uint32_t sampling_interval_ = 0;
  std::string raw_data_path_;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_STREAMER_COLLECTOR_H_