//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_METRIC_STREAMER_ZE_METRIC_COLLECTOR_H_
#define PTI_SAMPLES_ZE_METRIC_STREAMER_ZE_METRIC_COLLECTOR_H_

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <level_zero/layers/zel_tracing_api.h>

#include "ze_utils.h"

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

class ZeMetricCollector {
 public: // Interface
  static ZeMetricCollector* Create(
      ze_driver_handle_t driver,
      ze_device_handle_t device,
      const char* group_name) {
    PTI_ASSERT(driver != nullptr);
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(group_name != nullptr);

    zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
        device, group_name, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    if (group == nullptr) {
      std::cerr << "[WARNING] Unable to find target metric group: " <<
        group_name << std::endl;
      return nullptr;
    }

    ze_context_handle_t context = utils::ze::GetContext(driver);
    PTI_ASSERT(context != nullptr);

    ZeMetricCollector* collector = new ZeMetricCollector(
        device, context, group);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {
        ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;
    status = zelTracerCreate(&tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr <<
        "[WARNING] Unable to create Level Zero tracer for target context" <<
        std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ZeMetricCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    PTI_ASSERT(collector_thread_ == nullptr);
    PTI_ASSERT(collector_state_ == COLLECTOR_STATE_IDLE);

    if (tracer_ != nullptr) {
      status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    PTI_ASSERT(context_ != nullptr);
    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  std::vector<zet_typed_value_t> GetReportList() const {
    ze_result_t status = ZE_RESULT_SUCCESS;
    std::vector<zet_typed_value_t> report_list;
    PTI_ASSERT(metric_group_ != nullptr);

    if (metric_storage_.size() == 0) {
      return report_list;
    }

    uint32_t value_count = 0;
    status = zetMetricGroupCalculateMetricValues(
        metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        metric_storage_.size(), metric_storage_.data(), &value_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(value_count > 0);

    report_list.resize(value_count);
    status = zetMetricGroupCalculateMetricValues(
        metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        metric_storage_.size(), metric_storage_.data(),
        &value_count, report_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    report_list.resize(value_count);

    return report_list;
  }

  int GetMetricId(const char* metric_name) const {
    PTI_ASSERT(metric_name != nullptr);
    PTI_ASSERT(metric_group_ != nullptr);
    return utils::ze::GetMetricId(metric_group_, metric_name);
  }

  uint32_t GetReportSize() const {
    PTI_ASSERT(metric_group_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    status = zetMetricGroupGetProperties(metric_group_, &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    return group_props.metricCount;
  }

 private: // Implementation
  ZeMetricCollector(
      ze_device_handle_t device, ze_context_handle_t context,
      zet_metric_group_handle_t group)
      : device_(device), context_(context), metric_group_(group) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(context_ != nullptr);
    PTI_ASSERT(metric_group_ != nullptr);
  }

  void EnableTracing(zel_tracer_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t epilogue_callbacks{};

    epilogue_callbacks.CommandQueue.pfnCreateCb =
      OnExitCommandQueueCreate;
    epilogue_callbacks.CommandQueue.pfnDestroyCb =
      OnExitCommandQueueDestroy;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void EnableMetrics() {
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(queue_count_ >= 0);
    if (queue_count_ == 0) {
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

    ++queue_count_;
  }

  void DisableMetrics() {
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(queue_count_ > 0);
    --queue_count_;
    if (queue_count_ == 0) {
      PTI_ASSERT(collector_thread_ != nullptr);
      collector_state_.store(
          COLLECTOR_STATE_DISABLED, std::memory_order_release);
      collector_thread_->join();
      delete collector_thread_;
      collector_thread_ = nullptr;
      collector_state_.store(
          COLLECTOR_STATE_IDLE, std::memory_order_release);
    }
  }

  void AppendMetrics(const std::vector<uint8_t>& storage) {
    PTI_ASSERT(storage.size() > 0);

    size_t intial_size = metric_storage_.size();
    metric_storage_.resize(intial_size + storage.size());
    std::copy(storage.begin(), storage.end(),
              metric_storage_.begin() + intial_size);
  }

  static void Collect(ZeMetricCollector* collector) {
    PTI_ASSERT(collector != nullptr);

    PTI_ASSERT(collector->context_ != nullptr);
    PTI_ASSERT(collector->device_ != nullptr);
    PTI_ASSERT(collector->metric_group_ != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetContextActivateMetricGroups(
        collector->context_, collector->device_, 1, &collector->metric_group_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
    ze_event_pool_handle_t event_pool = nullptr;
    status = zeEventPoolCreate(collector->context_, &event_pool_desc,
                               0, nullptr, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_desc_t event_desc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    ze_event_handle_t event = nullptr;
    status = zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    zet_metric_streamer_desc_t metric_streamer_desc = {
        ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC,
        nullptr,
        32768, /* reports to collect before notify */
        100000 /* sampling period in nanoseconds */};
    zet_metric_streamer_handle_t metric_streamer = nullptr;
    status = zetMetricStreamerOpen(
        collector->context_, collector->device_, collector->metric_group_,
        &metric_streamer_desc, event, &metric_streamer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    collector->collector_state_.store(
        COLLECTOR_STATE_ENABLED, std::memory_order_release);

    std::vector<uint8_t> storage;
    while (collector->collector_state_.load(std::memory_order_acquire) !=
           COLLECTOR_STATE_DISABLED) {
      status = zeEventHostSynchronize(
          event, 50000000 /* wait delay in nanoseconds */);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);

      size_t data_size = 0;
      status = zetMetricStreamerReadData(
          metric_streamer, UINT32_MAX, &data_size, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (data_size > 0) {
        storage.resize(data_size);
        status = zetMetricStreamerReadData(
            metric_streamer, UINT32_MAX, &data_size, storage.data());
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        storage.resize(data_size);
        PTI_ASSERT(storage.size() > 0);

        collector->AppendMetrics(storage);
      }
    }

    status = zetMetricStreamerClose(metric_streamer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeEventDestroy(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zeEventPoolDestroy(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zetContextActivateMetricGroups(
        collector->context_, collector->device_, 0, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

 private: // Callbacks
  static void OnExitCommandQueueCreate(
      ze_command_queue_create_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeMetricCollector* collector =
        reinterpret_cast<ZeMetricCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->EnableMetrics();
    }
  }

  static void OnExitCommandQueueDestroy(
      ze_command_queue_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeMetricCollector* collector =
        reinterpret_cast<ZeMetricCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->DisableMetrics();
    }
  }

 private: // Data
  ze_device_handle_t device_ = nullptr;
  ze_context_handle_t context_ = nullptr;
  zel_tracer_handle_t tracer_ = nullptr;

  std::thread* collector_thread_ = nullptr;
  std::atomic<CollectorState> collector_state_{COLLECTOR_STATE_IDLE};

  zet_metric_group_handle_t metric_group_ = nullptr;
  std::vector<uint8_t> metric_storage_;

  std::mutex lock_;
  int queue_count_ = 0;
};

#endif // PTI_SAMPLES_ZE_METRIC_STREAMER_ZE_METRIC_COLLECTOR_H_