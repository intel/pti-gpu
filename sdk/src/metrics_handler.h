// ==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_METRICS_HANDLER_H_
#define SRC_API_METRICS_HANDLER_H_

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <random>
#include <thread>

#include "pti/pti_metrics.h"
#include "utils/pti_filesystem.h"
#include "utils/utils.h"
#include "utils/ze_utils.h"

// Not needed once trace API symbols and structures are available externally
#include "trace_metrics.h"

///@brief Maximum metrics per metric group
#define PTI_METRIC_COUNT_MAX 512

enum class ptiMetricProfilerState {
  PROFILER_DISABLED = 0,
  PROFILER_ENABLED = 1,
  PROFILER_PAUSED = 2,
};

struct pti_metrics_device_descriptor_t {
  ze_device_handle_t device_ = nullptr;
  ze_device_handle_t parent_device_ = nullptr;
  uint64_t host_time_origin_ = 0;
  uint64_t device_time_origin_ = 0;
  uint64_t device_timer_frequency_ = 0;
  uint64_t device_timer_mask_ = 0;
  uint64_t metric_time_origin_ = 0;
  uint64_t metric_timer_frequency_ = 0;
  uint64_t metric_timer_mask_ = 0;
  ze_driver_handle_t driver_ = nullptr;
  ze_context_handle_t context_ = nullptr;
  ze_event_pool_handle_t event_pool_ = nullptr;
  ze_event_handle_t event_ = nullptr;
  int32_t num_sub_devices_ = 0;
  zet_metric_group_handle_t metrics_group_ = nullptr;
  ze_pci_ext_properties_t pci_properties_;
  std::unique_ptr<std::thread> profiling_thread_ = nullptr;
  std::atomic<ptiMetricProfilerState> profiling_state_ = ptiMetricProfilerState::PROFILER_DISABLED;
  std::string metric_file_name_ = "";
  std::ofstream metric_file_stream_;
  std::vector<uint8_t> metric_data_ = {};
  bool stall_sampling_ = false;
};

class PtiMetricsProfiler {
 protected:
  // per driver metric contexts
  std::vector<ze_context_handle_t> metric_contexts_;
  // descriptors where device and sub device profiling information is saved
  std::unordered_map<ze_device_handle_t, std::shared_ptr<pti_metrics_device_descriptor_t>>
      device_descriptors_;
  // temportary directory name where raw data is saved to disc
  std::string data_dir_name_;
  // spdlogger for user specific log file
  std::shared_ptr<spdlog::logger> user_logger_;
  // condition variable to wait for the profiling thread to start
  std::condition_variable cv_thread_start_;
  // condition variable for the profiling thread to wait for the profiling state to change
  std::condition_variable cv_pause_;
  // max number of samples used for allocating local buffer and setting hw full notification
  static uint32_t max_metric_samples_;
  // maximum number of hw buffer reads before local buffer is written to disc
  static uint8_t max_data_capture_count_;

 public:
  PtiMetricsProfiler() = delete;
  PtiMetricsProfiler(const PtiMetricsProfiler &) = delete;
  PtiMetricsProfiler &operator=(const PtiMetricsProfiler &) = delete;
  PtiMetricsProfiler(PtiMetricsProfiler &&) = delete;
  PtiMetricsProfiler &operator=(PtiMetricsProfiler &&) = delete;

  PtiMetricsProfiler(pti_device_handle_t device_handle,
                     pti_metrics_group_handle_t metrics_group_handle) {
    auto data_dir = utils::CreateTempDirectory();

    PTI_ASSERT(pti::utils::filesystem::exists(data_dir));
    SPDLOG_INFO("Temp dir {}", data_dir.string());

    data_dir_name_ = data_dir.generic_string();

    bool enable_logging = (utils::GetEnv("PTI_LogToFile") == "1");
    std::string log_filename = enable_logging ? utils::GetEnv("PTI_LogFileName") : "";

    user_logger_ = utils::GetLogStream(enable_logging, std::move(log_filename));

    EnumerateDevices(device_handle, metrics_group_handle);
  }

  static uint64_t GetMaxMetricBufferSize() {
    // 2 is for systems with 2 tiles
    // 512 is for extra padding
    // TODO: may need to be adjusted per specific target
    return static_cast<uint64_t>(max_metric_samples_) * PTI_METRIC_COUNT_MAX * 2 + 512;
  }

  virtual ~PtiMetricsProfiler() {
    metric_contexts_.clear();

    // Stopping runaway collections in case stop was not called
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      // close the data file
      it->second->metric_file_stream_.close();

      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->profiling_state_ != ptiMetricProfilerState::PROFILER_DISABLED) {
        SPDLOG_ERROR("Stopping runaway metrics collection");
        it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                           std::memory_order_release);

        cv_pause_.notify_one();
        it->second->profiling_thread_->join();
        it->second->profiling_thread_.reset();
        it->second->metric_file_stream_.close();
      }
    }
    device_descriptors_.clear();
    user_logger_.reset();
    try {
      pti::utils::filesystem::remove_all(data_dir_name_);
    } catch (...) {
      SPDLOG_ERROR("Failed to delete temporary data directory: {:s} ", data_dir_name_);
    }
  }

  virtual pti_result StartProfiling(bool start_paused) = 0;

  virtual pti_result PauseProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_ENABLED) {
        SPDLOG_INFO("Pausing profiling");
        it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_PAUSED,
                                           std::memory_order_release);
      } else {
        if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_DISABLED) {
          SPDLOG_ERROR("Attempted to pause a disabled metrics profiling session");
          return PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED;
        } else if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_PAUSED) {
          SPDLOG_ERROR("Attempted to pause an already pause metrics profiling session");
          return PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED;
        }
      }
    }
    return PTI_SUCCESS;
  }

  virtual pti_result ResumeProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_PAUSED) {
        SPDLOG_INFO("Resume profiling");
        it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_ENABLED,
                                           std::memory_order_release);
        cv_pause_.notify_one();
      } else {
        if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_DISABLED) {
          SPDLOG_ERROR("Attempted to resume a disabled metrics profiling session");
          return PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED;
        } else if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_ENABLED) {
          SPDLOG_ERROR("Attempted to resume an already running metrics profiling session");
          return PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED;
        }
      }
    }
    return PTI_SUCCESS;
  }

  virtual pti_result StopProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      // Collection should be running or paused before stop is called
      if (it->second->profiling_thread_ == nullptr ||
          it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_DISABLED) {
        SPDLOG_ERROR("Attempting to stop a metrics collection that hasn't been started");
        return PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED;
      }

      it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                         std::memory_order_release);

      // If profiling state is in paused mode when stop is called, unblock the profiling thread by
      // notifying that the state has changed
      cv_pause_.notify_one();
      it->second->profiling_thread_->join();
      it->second->profiling_thread_.reset();
      it->second->metric_file_stream_.close();
    }
    return PTI_SUCCESS;
  }

  // Default implementation. Inheriting class should invoke parent implementation as well as
  // implement class specific handling of the data.
  virtual pti_result GetCalculatedData(
      [[maybe_unused]] pti_metrics_group_handle_t metrics_group_handle,
      [[maybe_unused]] pti_value_t *metrics_values_buffer,
      [[maybe_unused]] uint32_t *metrics_values_count) {
    pti_result result = PTI_SUCCESS;
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      // Collection should be stopped before dump is called
      if (it->second->profiling_state_ != ptiMetricProfilerState::PROFILER_DISABLED) {
        SPDLOG_ERROR(
            "Attempting to calculate data from a metrics collection that hasn't been stopped");
        result = PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED;
      }
    }

    // Stopping runaway collection if stop is not called before dump is called
    if (result != PTI_SUCCESS) {
      // Don't capture result from Stop and return previous error
      SPDLOG_WARN("Stopping runaway collection");
      StopProfiling();
    }
    return result;
  }

 private:
  void EnumerateDevices(pti_device_handle_t device_handle,
                        pti_metrics_group_handle_t metrics_group_handle) {
    bool stall_sampling = false;
    ze_device_handle_t device = static_cast<ze_device_handle_t>(device_handle);
    PTI_ASSERT(device != nullptr);
    zet_metric_group_handle_t group = static_cast<zet_metric_group_handle_t>(metrics_group_handle);
    PTI_ASSERT(group != nullptr);

    // get group name for metric group
    zet_metric_group_properties_t group_props;
    std::memset(&group_props, 0, sizeof(group_props));
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = zetMetricGroupGetProperties(
        static_cast<zet_metric_group_handle_t>(metrics_group_handle), &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    std::string group_name = group_props.name;
    if (group_name == "EuStallSampling") {
      stall_sampling = true;
    }

    uint32_t num_drivers = 0;
    status = zeDriverGet(&num_drivers, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (num_drivers > 0) {
      //      int32_t did = 0;
      std::vector<ze_driver_handle_t> drivers(num_drivers);
      status = zeDriverGet(&num_drivers, drivers.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      for (auto driver : drivers) {
        ze_context_handle_t context = nullptr;
        ze_context_desc_t cdesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

        status = zeContextCreate(driver, &cdesc, &context);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        metric_contexts_.push_back(context);

        uint32_t num_sub_devices = 0;
        status = zeDeviceGetSubDevices(device, &num_sub_devices, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        std::shared_ptr<pti_metrics_device_descriptor_t> desc =
            std::make_shared<pti_metrics_device_descriptor_t>();

        desc->stall_sampling_ = stall_sampling;

        desc->device_ = device;
        desc->parent_device_ = nullptr;
        desc->num_sub_devices_ = num_sub_devices;

        desc->device_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(device);
        desc->device_timer_mask_ = utils::ze::GetDeviceTimestampMask(device);
        desc->metric_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(device);
        desc->metric_timer_mask_ = utils::ze::GetMetricTimestampMask(device);

        ze_pci_ext_properties_t pci_device_properties;
        std::memset(&pci_device_properties, 0, sizeof(pci_device_properties));
        ze_result_t status = zeDevicePciGetPropertiesExt(device, &pci_device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        desc->pci_properties_ = pci_device_properties;

        desc->driver_ = driver;
        desc->context_ = context;
        desc->metrics_group_ = group;

        uint64_t host_time = 0;
        uint64_t ticks = 0;
        uint64_t device_time = 0;
        uint64_t metric_time = 0;

        status = zeDeviceGetGlobalTimestamps(device, &host_time, &ticks);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        device_time = ticks & desc->device_timer_mask_;
        device_time = device_time * NSEC_IN_SEC / desc->device_timer_frequency_;

        metric_time = ticks & desc->metric_timer_mask_;
        metric_time = metric_time * NSEC_IN_SEC / desc->metric_timer_frequency_;

        desc->host_time_origin_ = host_time;
        desc->device_time_origin_ = device_time;
        desc->metric_time_origin_ = metric_time;

        desc->profiling_thread_.reset();
        desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                     std::memory_order_release);

        desc->metric_file_name_ =
            data_dir_name_ + "/." + group_name + "." + std::to_string(utils::GetPid()) + ".t";

        desc->metric_file_stream_ = std::ofstream(
            desc->metric_file_name_, std::ios::out | std::ios::trunc | std::ios::binary);

        device_descriptors_[device] = std::move(desc);

        if (num_sub_devices > 0) {
          std::vector<ze_device_handle_t> sub_devices(num_sub_devices);

          status = zeDeviceGetSubDevices(device, &num_sub_devices, sub_devices.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          for (uint32_t j = 0; j < num_sub_devices; j++) {
            std::shared_ptr<pti_metrics_device_descriptor_t> sub_desc =
                std::make_shared<pti_metrics_device_descriptor_t>();

            sub_desc->stall_sampling_ = stall_sampling;

            sub_desc->device_ = sub_devices[j];
            sub_desc->parent_device_ = device;
            sub_desc->num_sub_devices_ = 0;

            sub_desc->driver_ = driver;
            sub_desc->context_ = context;
            sub_desc->metrics_group_ = group;

            sub_desc->device_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(sub_devices[j]);
            sub_desc->device_timer_mask_ = utils::ze::GetDeviceTimestampMask(sub_devices[j]);
            sub_desc->metric_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(sub_devices[j]);
            sub_desc->metric_timer_mask_ = utils::ze::GetMetricTimestampMask(sub_devices[j]);

            ze_pci_ext_properties_t pci_device_properties;
            std::memset(&pci_device_properties, 0, sizeof(pci_device_properties));
            ze_result_t status =
                zeDevicePciGetPropertiesExt(sub_devices[j], &pci_device_properties);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            sub_desc->pci_properties_ = pci_device_properties;

            ticks = 0;
            host_time = 0;
            device_time = 0;
            metric_time = 0;

            status = zeDeviceGetGlobalTimestamps(sub_devices[j], &host_time, &ticks);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            device_time = ticks & sub_desc->device_timer_mask_;
            device_time = device_time * NSEC_IN_SEC / sub_desc->device_timer_frequency_;

            metric_time = ticks & sub_desc->metric_timer_mask_;
            metric_time = metric_time * NSEC_IN_SEC / sub_desc->metric_timer_frequency_;

            sub_desc->host_time_origin_ = host_time;
            sub_desc->device_time_origin_ = device_time;
            sub_desc->metric_time_origin_ = metric_time;

            sub_desc->driver_ = driver;
            sub_desc->context_ = context;
            sub_desc->metrics_group_ = group;

            sub_desc->profiling_thread_.reset();
            sub_desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                             std::memory_order_release);

            device_descriptors_[sub_devices[j]] = std::move(sub_desc);
          }
        }
      }
    }
  }

 protected:
  pti_result CollectionInitialize(std::shared_ptr<pti_metrics_device_descriptor_t> desc) {
    PTI_ASSERT(desc != nullptr);

    // Activate the metric groups
    ze_result_t status =
        zetContextActivateMetricGroups(desc->context_, desc->device_, 1, &(desc->metrics_group_));
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    // Create an event pool for the device and context
    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                            ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
    status = zeEventPoolCreate(desc->context_, &event_pool_desc, 1, &(desc->device_),
                               &(desc->event_pool_));
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    // Create an event from the event pool
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    status = zeEventCreate(desc->event_pool_, &event_desc, &(desc->event_));
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    return PTI_SUCCESS;
  }

  pti_result CollectionFinalize(std::shared_ptr<pti_metrics_device_descriptor_t> desc) {
    PTI_ASSERT(desc != nullptr);

    // Destroy the event
    ze_result_t status = zeEventDestroy(desc->event_);
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    // Destroy the event pool
    status = zeEventPoolDestroy(desc->event_pool_);
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    // Disactivate the metric groups
    status =
        zetContextActivateMetricGroups(desc->context_, desc->device_, 0, &(desc->metrics_group_));
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    return PTI_SUCCESS;
  }

  void SaveRawData(std::shared_ptr<pti_metrics_device_descriptor_t> desc, uint8_t *storage,
                   size_t data_size, bool immediate_save_to_disc) {
    static uint8_t capture_count = 0;
    if (data_size != 0) {
      // Save the date to local memory
      desc->metric_data_.insert(desc->metric_data_.end(), storage, storage + data_size);
      capture_count++;
    }

    // Save local memory to cache file if there is something to write and
    // either we need an immediate save to disc or
    // the local buffer is getting too big after a few captures or
    // no data was captured from the hw buffer
    if (!desc->metric_data_.empty() &&
        (immediate_save_to_disc || capture_count > 10 || data_size == 0)) {
      desc->metric_file_stream_.write(reinterpret_cast<char *>(desc->metric_data_.data()),
                                      desc->metric_data_.size());
      desc->metric_data_.clear();
      capture_count = 0;
    }
  }
};
#ifndef _WIN32
uint32_t PtiMetricsProfiler::max_metric_samples_ = 2048;
#else   // Linux
uint32_t PtiMetricsProfiler::max_metric_samples_ = 32768;
#endif  //_WIN32

// TODO: experiment with the max capture count to find the optimal number of hw buffer
// to local buffer reads before writing to disc while minimizing collection overhead
// and preventing data loss by not reading the hw buffer fast enough when data is ready
uint8_t PtiMetricsProfiler::max_data_capture_count_ = 10;

class PtiStreamMetricsProfiler : public PtiMetricsProfiler {
 private:
  uint32_t sampling_interval_;

 public:
  PtiStreamMetricsProfiler() = delete;
  PtiStreamMetricsProfiler(const PtiStreamMetricsProfiler &) = delete;
  PtiStreamMetricsProfiler &operator=(const PtiStreamMetricsProfiler &) = delete;
  PtiStreamMetricsProfiler(PtiStreamMetricsProfiler &&) = delete;
  PtiStreamMetricsProfiler &operator=(PtiStreamMetricsProfiler &&) = delete;

  PtiStreamMetricsProfiler(pti_device_handle_t device_handle,
                           pti_metrics_group_handle_t metrics_group_handle,
                           uint32_t sampling_interval)
      : PtiMetricsProfiler(device_handle, metrics_group_handle) {
    sampling_interval_ = sampling_interval;
  }

  ~PtiStreamMetricsProfiler() {}

  pti_result StartProfiling(bool start_paused) {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->stall_sampling_) {
        SPDLOG_WARN("EU stall sampling is not supported");
        continue;
      }

      // Collection should be stopped before start is called
      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_ENABLED) {
        SPDLOG_ERROR("Attempting to start a metrics collection that isn't stopped");
        return PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED;
      }

      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_PAUSED) {
        SPDLOG_ERROR("Attempting to start instead of resume a metrics collection that is paused");
        return PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED;
      }

      it->second->profiling_thread_ = std::make_unique<std::thread>(
          &PtiStreamMetricsProfiler::PerDeviceStreamMetricsProfilingThread, this, it->second,
          sampling_interval_, start_paused);

      // Wait for the profiling thread to start
      std::mutex thread_start_mutex;
      std::unique_lock thread_start_lock(thread_start_mutex);
      while (it->second->profiling_state_.load(std::memory_order_acquire) ==
             ptiMetricProfilerState::PROFILER_DISABLED) {
        cv_thread_start_.wait(thread_start_lock);
      }
    }
    return PTI_SUCCESS;
  }

  // PauseProfiling(), ResumeProfiling and StopProfiling() are same for all profiler types and are
  // implemented in the parent class PtiMetricsProfiler

  pti_result GetCalculatedData(pti_metrics_group_handle_t metrics_group_handle,
                               pti_value_t *metrics_values_buffer, uint32_t *metrics_values_count) {
    pti_result result = PtiMetricsProfiler::GetCalculatedData(
        metrics_group_handle, metrics_values_buffer, metrics_values_count);
    if (result != PTI_SUCCESS) {
      return result;
    }
    ComputeMetrics(metrics_group_handle, metrics_values_buffer, metrics_values_count);
    return PTI_SUCCESS;
  }

 private:
  void ComputeMetrics(pti_metrics_group_handle_t metrics_group_handle,
                      pti_value_t *metrics_values_buffer, uint32_t *metrics_values_count) {
    PTI_ASSERT(metrics_values_count != nullptr);
    // Option 1: user wants metrics values count
    if (metrics_values_buffer == nullptr) {
      // Search for the top/parent device; it doesn't have a parent
      auto it = device_descriptors_.begin();
      while (it != device_descriptors_.end() && it->second->parent_device_ != nullptr) {
        it++;
      }

      if (it == device_descriptors_.end() || it->second->metrics_group_ != metrics_group_handle) {
        SPDLOG_WARN("Could not find device and metric group");
        SPDLOG_WARN("Unable to calculate required data buffer size");
        return;
      }

      std::ifstream inf =
          std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
      PTI_ASSERT(inf.is_open());
      inf.seekg(0, inf.end);
      std::streamsize stream_file_size = inf.tellg();
      PTI_ASSERT(stream_file_size >= 0);
      size_t file_size = static_cast<size_t>(stream_file_size);

      inf.seekg(0, inf.beg);  // rewind
      std::vector<uint8_t> raw_metrics(file_size);

      inf.read(reinterpret_cast<char *>(raw_metrics.data()), file_size);
      int raw_size = inf.gcount();
      if (raw_size > 0) {
        uint32_t num_reports = 0;
        uint32_t total_values_count = 0;
        ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
            it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, raw_size,
            raw_metrics.data(), &num_reports, &total_values_count, nullptr, nullptr);

        if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
          SPDLOG_WARN("Unable to calculate required data buffer size");
        }

        *metrics_values_count = total_values_count;
      }
      inf.close();
      return;
    }

    // Option 2: user wants the buffer filled.

    *metrics_values_count = 0;

    // Search for the top/parent device; it doesn't have a parent
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }

      if (it->second->metrics_group_ != metrics_group_handle) {
        SPDLOG_WARN("Could not find device and metric group");
        SPDLOG_WARN("Unable to calculate process collected data");
        return;
      }

      // Note: EU Stall sampling data is not logged in json format
      if (it->second->stall_sampling_) {
        SPDLOG_WARN("EU stall sampling is not supported");
        continue;
      } else {  // not stall sampling

        // Get metric list for metric group collected
        std::vector<std::string> metric_list;
        metric_list = utils::ze::GetMetricList(it->second->metrics_group_);
        PTI_ASSERT(!metric_list.empty());

        // get group name for metric group
        zet_metric_group_properties_t group_props;
        std::memset(&group_props, 0, sizeof(group_props));
        group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        ze_result_t status = zetMetricGroupGetProperties(it->second->metrics_group_, &group_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::string group_name = group_props.name;

        // Get the index of the start timestamp from the metric list
        uint32_t ts_idx = utils::ze::GetMetricId(metric_list, "QueryBeginTime");
        if (ts_idx >= metric_list.size()) {
          // no QueryBeginTime metric
          continue;
        }
        // TODO: handle subdevices in case of implicit scaling
        uint64_t time_span_between_clock_resets = (it->second->metric_timer_mask_ + 1ull) *
                                                  static_cast<uint64_t>(NSEC_IN_SEC) /
                                                  it->second->metric_timer_frequency_;

        // open input file stream where metrics data is saved
        std::ifstream inf =
            std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
        PTI_ASSERT(inf.is_open());
        inf.seekg(0, inf.end);
        std::streamsize stream_file_size = inf.tellg();
        PTI_ASSERT(stream_file_size >= 0);
        size_t file_size = static_cast<size_t>(stream_file_size);

        inf.seekg(0, inf.beg);  // rewind
        std::vector<uint8_t> raw_metrics(file_size);

        user_logger_->info("{\n\t\"displayTimeUnit\": \"us\",\n\t\"traceEvents\": [");

        uint64_t cur_sampling_ts = 0;

        uint32_t buffer_idx = 0;
        // Read and process input file stream where metrics data is saved
        while (!inf.eof()) {
          inf.read(reinterpret_cast<char *>(raw_metrics.data()), file_size);

          int raw_size = inf.gcount();
          if (raw_size > 0) {
            // first call to Calculate metrics to capture the size of reports and values
            // buffers
            uint32_t num_reports = 0;
            uint32_t total_values_count = 0;
            ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
                it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                raw_size, raw_metrics.data(), &num_reports, &total_values_count, nullptr, nullptr);
            if ((status != ZE_RESULT_SUCCESS) || (num_reports == 0) || (total_values_count == 0)) {
              SPDLOG_WARN("Unable to calculate metrics");
              continue;
            }

            // alllocate buffers for reports and values with required sizes
            std::vector<uint32_t> reports(num_reports);
            std::vector<zet_typed_value_t> values(total_values_count);

            // Second call to Calculate metrics to do the metrics calculations
            status = zetMetricGroupCalculateMultipleMetricValuesExp(
                it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                raw_size, raw_metrics.data(), &num_reports, &total_values_count, reports.data(),
                values.data());
            if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
              SPDLOG_WARN("Unable to calculate metrics");
              continue;
            }
            // Note: There is a bug in L0 where the total value count returned from the second call
            // to zetMetricGroupCalculateMultipleMetricValuesExp is less than the value obtained
            // from the first call and used to allocate the buffer
            *metrics_values_count += total_values_count;

            // Process values
            const zet_typed_value_t *value = values.data();
            for (uint32_t i = 0; i < num_reports; ++i) {
              uint32_t per_report_values_count = reports[i];
              uint32_t num_samples = per_report_values_count / metric_list.size();

              for (uint32_t j = 0; j < num_samples; ++j) {
                // v is array of metric_count values
                const zet_typed_value_t *v = value + j * metric_list.size();

                // Capture the timestamp using the timestamp index
                uint64_t ts = v[ts_idx].value.ui64;

                // Adjust timestamp if there is a clock overflow
                if (cur_sampling_ts != 0) {
                  while (cur_sampling_ts >= ts) {  // clock overflow
                    ts += time_span_between_clock_resets;
                  }
                }
                cur_sampling_ts = ts;

                std::string str = "";
                if (j != 0) str += ",";
                str += " {\n\t\t\"args\": {\n";

                // Walk through the metric list and add metric values to output buffer
                for (uint32_t k = 0; k < metric_list.size(); k++) {
                  if (k == ts_idx) {
                    metrics_values_buffer[buffer_idx++].ui64 = ts;
                  } else {
                    metrics_values_buffer[buffer_idx++].ui64 = v[k].value.ui64;
                  }
                }
                // Walk through the metric list and log the metric parameters and values
                for (uint32_t k = 0; k < metric_list.size(); k++) {
                  // Skip the timestamp, it is logged separately
                  if (k == ts_idx) {
                    continue;
                  }

                  if (k != 0) str += ",\n";
                  str +=
                      "\t\t\t\"" + metric_list[k] + "\": " + utils::ze::GetMetricTypedValue(v[k]);
                }
                str += "\n\t\t\t},\n";
                str += "\t\t\t\"cat\": \"" + group_name + "\",\n";
                str += "\t\t\t\"name\": \"" + group_name + "\",\n";
                str += "\t\t\t\"ph\": \"C\",\n";
                str += "\t\t\t\"pid\": 0,\n";
                str += "\t\t\t\"tid\": 0,\n";
                str += "\t\t\t\"ts\": " + std::to_string(ts / NSEC_IN_USEC) + "\n";
                str += "\t\t}";

                user_logger_->info(str);
              }
              value += reports[i];
            }
          }
        }
        inf.close();
        user_logger_->info("\n\t]\n}\n");
        user_logger_->flush();
        // break;  // TODO: only one device for now
      }
    }
  }

  void CaptureRawMetrics(zet_metric_streamer_handle_t streamer, uint8_t *storage, size_t ssize,
                         std::shared_ptr<pti_metrics_device_descriptor_t> desc,
                         bool immediate_save_to_disc = false) {
    PTI_ASSERT(desc != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;

    size_t data_size = ssize;
    status = zetMetricStreamerReadData(streamer, UINT32_MAX, &data_size, storage);
    if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
      SPDLOG_WARN("Metric samples dropped.");
    } else if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("zetMetricStreamerReadData failed with error code {:x}",
                   static_cast<std::size_t>(status));
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    SaveRawData(std::move(desc), storage, data_size, immediate_save_to_disc);
  }

  void EventBasedCaptureRawMetrics(zet_metric_streamer_handle_t streamer, uint8_t *storage,
                                   size_t ssize,
                                   std::shared_ptr<pti_metrics_device_descriptor_t> desc) {
    PTI_ASSERT(desc != nullptr);

    ze_result_t status = zeEventQueryStatus(desc->event_);
    if (!(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY)) {
      SPDLOG_ERROR("zeEventQueryStatus failed with error code: 0x{:x}",
                   static_cast<std::size_t>(status));
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);

    if (status == ZE_RESULT_SUCCESS) {
      status = zeEventHostReset(desc->event_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    } else { /* ZE_RESULT_NOT_READY */
      return;
    }
    bool immediate_save_to_disc = false;
    CaptureRawMetrics(streamer, storage, ssize, std::move(desc), immediate_save_to_disc);
  }

  void PerDeviceStreamMetricsProfilingThread(std::shared_ptr<pti_metrics_device_descriptor_t> desc,
                                             uint32_t sampling_interval, bool start_paused) {
    PTI_ASSERT(desc != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;

    pti_result result = CollectionInitialize(desc);
    PTI_ASSERT(result == PTI_SUCCESS);

    zet_metric_streamer_handle_t streamer = nullptr;

    constexpr auto kDefaultSamplingInterval = 1000000U;  // 1 millisecond

    uint32_t interval = (sampling_interval == 0) ? kDefaultSamplingInterval : sampling_interval;
    // TODO: Should there be a min and/or max?

    zet_metric_streamer_desc_t streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr,
                                                max_metric_samples_, interval};

    if (streamer_desc.notifyEveryNReports > max_metric_samples_) {
      max_metric_samples_ = streamer_desc.notifyEveryNReports;
    }

    std::vector<std::string> metrics_list;
    metrics_list = utils::ze::GetMetricList(desc->metrics_group_);
    PTI_ASSERT(!metrics_list.empty());

    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    bool streamer_open = false;
    ptiMetricProfilerState profiling_state = start_paused
                                                 ? ptiMetricProfilerState::PROFILER_PAUSED
                                                 : ptiMetricProfilerState::PROFILER_ENABLED;
    desc->profiling_state_.store(profiling_state, std::memory_order_release);

    // Unblock main thread
    cv_thread_start_.notify_one();

    bool immediate_save_to_disc = true;
    while (desc->profiling_state_.load(std::memory_order_acquire) !=
           ptiMetricProfilerState::PROFILER_DISABLED) {
      if (desc->profiling_state_.load(std::memory_order_acquire) ==
          ptiMetricProfilerState::PROFILER_PAUSED) {
        // close the streamer when profiler is paused
        if (streamer_open == true) {
          // Capture hw buffered raw data and immediately write it to disc before closing the
          // streamer
          CaptureRawMetrics(streamer, raw_metrics.data(),
                            PtiMetricsProfiler::GetMaxMetricBufferSize(), desc,
                            immediate_save_to_disc);

          status = zetMetricStreamerClose(streamer);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          streamer_open = false;
        }

        // Wait for the profiling state to change
        std::mutex pause_mutex;
        std::unique_lock pause_lock(pause_mutex);
        while (desc->profiling_state_.load(std::memory_order_acquire) ==
               ptiMetricProfilerState::PROFILER_PAUSED) {
          cv_pause_.wait(pause_lock);
        }
      } else {  // PROFILER_ENABLED
        // open the streamer when profiler is enabled
        if (streamer_open == false) {
          status = zetMetricStreamerOpen(desc->context_, desc->device_, desc->metrics_group_,
                                         &streamer_desc, desc->event_, &streamer);
          if (status != ZE_RESULT_SUCCESS) {
            SPDLOG_ERROR(
                "Failed to open metric streamer. The sampling interval might be too small.");
#ifndef _WIN32
            SPDLOG_ERROR(
                "Please also make sure: "
                "on PVC: /proc/sys/dev/i915/perf_stream_paranoid "
                "OR on BMG (or later): /proc/sys/dev/xe/observation_paranoid "
                "is set to 0.");
#endif /* _WIN32 */
            break;
          }
          streamer_open = true;
        }
        // Capture hw buffered raw data to local memory. Local memory is not written to disc
        // immediately, It is written to disc after a few hw buffer reads or if local buffer is not
        // empty but no data is captured from the hw buffer in this iteration
        EventBasedCaptureRawMetrics(streamer, raw_metrics.data(),
                                    PtiMetricsProfiler::GetMaxMetricBufferSize(), desc);
      }
    }

    if (streamer_open) {
      // Capture hw buffered raw data and immediately write it to disc before closing the streamer
      CaptureRawMetrics(streamer, raw_metrics.data(), PtiMetricsProfiler::GetMaxMetricBufferSize(),
                        desc, immediate_save_to_disc);

      status = zetMetricStreamerClose(streamer);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    result = CollectionFinalize(std::move(desc));
    PTI_ASSERT(result == PTI_SUCCESS);
  }
};

/*
class PtiQueryMetricsProfiler : public PtiMetricsProfiler
{
public:
    PtiQueryMetricsProfiler() = delete;
    PtiQueryMetricsProfiler(const PtiQueryMetricsProfiler &) = delete;
    PtiQueryMetricsProfiler &operator=(const PtiQueryMetricsProfiler &) = delete;
    PtiQueryMetricsProfiler(PtiQueryMetricsProfiler &&) = delete;
    PtiQueryMetricsProfiler &operator=(PtiQueryMetricsProfiler &&) = delete;

    PtiQueryMetricsProfiler(std::string &group_name) : PtiMetricsProfiler(group_name) {}

    void StartProfiling(bool start_paused) {}
    // PauseProfiling(), ResumeProfiling and StopProfiling() are same for all profiler types and are
    // implemented in the parent class PtiMetricsProfiler

private:
    void ComputeMetrics() {}
    void CaptureRawMetrics() {}
    void
PerDeviceQueryMetricsProfilingThread(std::shared_ptr<pti_metrics_device_descriptor_t> desc)
{}

};
*/

/* Trace metrics API hooks */

// These are available as experimental as part of loader version 1.19.2
// The loader with these symbols hasn't been released as of yet.
// Using the symbols directly will cause symbols not found compilation errors if the system does not
// have a suitable loader version with these symbols.
// We are attempting to hook the symbols dynamically to decide whether we can use the trace metrics
// functionality or not without causing compilation errors.
// Essentially, having a built in backwards compilability mechanism
using importTracerCreatePtrFnt = ze_result_t (*)(
    zet_context_handle_t context_handle, zet_device_handle_t device_handle, uint32_t,
    zet_metric_group_handle_t *metric_group_handle,
    external::L0::zet_metric_tracer_exp_desc_t *tracer_desc, ze_event_handle_t event_handle,
    external::L0::zet_metric_tracer_exp_handle_t *tracer_handle);

using importTracerDestroyPtrFnt =
    ze_result_t (*)(external::L0::zet_metric_tracer_exp_handle_t metric_tracer_handle);

using importTracerEnablePtrFnt = ze_result_t (*)(
    external::L0::zet_metric_tracer_exp_handle_t metric_tracer_handle, ze_bool_t synchronous);

using importTracerDisablePtrFnt = ze_result_t (*)(
    external::L0::zet_metric_tracer_exp_handle_t metric_tracer_handle, ze_bool_t synchronous);

using importTracerReadPtrFnt =
    ze_result_t (*)(external::L0::zet_metric_tracer_exp_handle_t metric_tracer_handle,
                    size_t *raw_data_size, uint8_t *raw_data);
using importDecoderCreatePtrFnt =
    ze_result_t (*)(external::L0::zet_metric_tracer_exp_handle_t metric_tracer_handle,
                    external::L0::zet_metric_decoder_exp_handle_t *metric_decoder_handle);

using importDecoderDestroyPtrFnt =
    ze_result_t (*)(external::L0::zet_metric_decoder_exp_handle_t metric_decoder_handle);

using importTracerDecodePtrFnt = ze_result_t (*)(
    external::L0::zet_metric_decoder_exp_handle_t metric_decoder_handle, size_t *raw_data_size,
    const uint8_t *raw_data, uint32_t metric_count, zet_metric_handle_t *metric_handle,
    uint32_t *metric_entries_count, external::L0::zet_metric_entry_exp_t *metric_entries);

using importDecoderGetDecodableMetricsPtrFnt =
    ze_result_t (*)(external::L0::zet_metric_decoder_exp_handle_t metric_decoder_handle,
                    uint32_t *count, zet_metric_handle_t *metric_handle);

// These are available internally only as of 12/24
using importIntelMetricCalculateOperationCreatePtrFnt = ze_result_t (*)(
    zet_context_handle_t context_handle, zet_device_handle_t device_handle,
    external::L0::zet_intel_metric_calculate_exp_desc_t *calculate_desc,
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t *calculate_op_handle);

using importIntelMetricCalculateOperationDestroyPtrFnt = ze_result_t (*)(
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle);

using importIntelMetricCalculateGetReportFormaPtrFnt = ze_result_t (*)(
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle,
    uint32_t *metric_count, zet_metric_handle_t *metrics_handles);

using importIntelMetricDecodeCalculateMultipleValuesPtrFnt = ze_result_t (*)(
    external::L0::zet_metric_decoder_exp_handle_t decoder_handle, size_t *raw_data_size,
    const uint8_t *raw_data,
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle,
    uint32_t *set_count, uint32_t *report_count_per_set, uint32_t *metric_report_count,
    external::L0::zet_intel_metric_result_exp_t *metric_results);

using importIntelMetricDecodeToBinaryBufferPtrFnt = ze_result_t (*)(
    external::L0::zet_metric_decoder_exp_handle_t decoder_handle, size_t *raw_data_size,
    const uint8_t *raw_data,
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle,
    external::L0::zet_intel_metric_decoded_buffer_exp_properties_t *decoded_buffer_props,
    size_t *decoded_buffer_size, uint8_t *decoded_buffer);

struct pti_metrics_tracer_functions_t {
  // These symbols are available only with later versions of the loader.
  importTracerCreatePtrFnt zetMetricTracerCreateExp = nullptr;
  importTracerDestroyPtrFnt zetMetricTracerDestroyExp = nullptr;
  importTracerEnablePtrFnt zetMetricTracerEnableExp = nullptr;
  importTracerDisablePtrFnt zetMetricTracerDisableExp = nullptr;
  importTracerReadPtrFnt zetMetricTracerReadDataExp = nullptr;
  importDecoderCreatePtrFnt zetMetricDecoderCreateExp = nullptr;
  importDecoderDestroyPtrFnt zetMetricDecoderDestroyExp = nullptr;
  importTracerDecodePtrFnt zetMetricTracerDecodeExp = nullptr;
  importDecoderGetDecodableMetricsPtrFnt zetMetricDecoderGetDecodableMetricsExp = nullptr;

  // These symbols are available internally only
  importIntelMetricCalculateOperationCreatePtrFnt zetIntelMetricCalculateOperationCreateExp =
      nullptr;
  importIntelMetricCalculateOperationDestroyPtrFnt zetIntelMetricCalculateOperationDestroyExp =
      nullptr;
  importIntelMetricCalculateGetReportFormaPtrFnt zetIntelMetricCalculateGetReportFormatExp =
      nullptr;
  importIntelMetricDecodeCalculateMultipleValuesPtrFnt
      zetIntelMetricDecodeCalculateMultipleValuesExp = nullptr;
  importIntelMetricDecodeToBinaryBufferPtrFnt zetIntelMetricDecodeToBinaryBufferExp = nullptr;
};

pti_metrics_tracer_functions_t tf;

class PtiTraceMetricsProfiler : public PtiMetricsProfiler {
 private:
  uint32_t time_aggr_window_;
  external::L0::zet_metric_decoder_exp_handle_t metric_decoder_;

 public:
  PtiTraceMetricsProfiler() = delete;
  PtiTraceMetricsProfiler(const PtiTraceMetricsProfiler &) = delete;
  PtiTraceMetricsProfiler &operator=(const PtiTraceMetricsProfiler &) = delete;
  PtiTraceMetricsProfiler(PtiTraceMetricsProfiler &&) = delete;
  PtiTraceMetricsProfiler &operator=(PtiTraceMetricsProfiler &&) = delete;

  PtiTraceMetricsProfiler(pti_device_handle_t device_handle,
                          pti_metrics_group_handle_t metrics_group_handle,
                          uint32_t time_aggr_window)
      : PtiMetricsProfiler(device_handle, metrics_group_handle) {
    time_aggr_window_ = time_aggr_window;
    metric_decoder_ = nullptr;
  }

  ~PtiTraceMetricsProfiler() {}

  pti_result StartProfiling(bool start_paused) {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }

      // Collection should be stopped before start is called
      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_ENABLED) {
        SPDLOG_ERROR("Attempting to start a metrics collection that isn't stopped");
        return PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED;
      }

      if (it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_PAUSED) {
        SPDLOG_ERROR("Attempting to start instead of resume a metrics collection that is paused");
        return PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED;
      }

      it->second->profiling_thread_ = std::make_unique<std::thread>(
          &PtiTraceMetricsProfiler::PerDeviceTraceMetricsProfilingThread, this, it->second,
          &(this->metric_decoder_), start_paused);

      // Wait for the profiling thread to start
      std::mutex thread_start_mutex;
      std::unique_lock thread_start_lock(thread_start_mutex);
      while (it->second->profiling_state_.load(std::memory_order_acquire) ==
             ptiMetricProfilerState::PROFILER_DISABLED) {
        cv_thread_start_.wait(thread_start_lock);
      }
    }
    return PTI_SUCCESS;
  }

  // PauseProfiling(), ResumeProfiling and StopProfiling() are same for all profiler types and are
  // implemented in the parent class PtiMetricsProfiler

  pti_result GetCalculatedData(pti_metrics_group_handle_t metrics_group_handle,
                               pti_value_t *metrics_values_buffer, uint32_t *metrics_values_count) {
    pti_result result = PtiMetricsProfiler::GetCalculatedData(
        metrics_group_handle, metrics_values_buffer, metrics_values_count);
    if (result != PTI_SUCCESS) {
      return result;
    }
    ComputeMetrics(metrics_group_handle, metrics_values_buffer, metrics_values_count);
    return PTI_SUCCESS;
  }

 private:
  void ComputeMetrics(pti_metrics_group_handle_t metrics_group_handle,
                      pti_value_t *metrics_values_buffer, uint32_t *metrics_values_count) {
    PTI_ASSERT(metrics_values_count != nullptr);
    ze_result_t status;

    // Search for the top/parent device; it doesn't have a parent
    auto it = device_descriptors_.begin();
    while (it != device_descriptors_.end() && it->second->parent_device_ != nullptr) {
      it++;
    }

    if (it == device_descriptors_.end() || it->second->metrics_group_ != metrics_group_handle) {
      SPDLOG_WARN("Could not find device and metric group");
      SPDLOG_WARN("Unable to calculate required data buffer size");
      return;
    }

    std::ifstream inf =
        std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
    PTI_ASSERT(inf.is_open());

    constexpr auto kDefaultTimeAggrWindow = 10000U;
    uint32_t time_aggr_window = time_aggr_window_ / 1000;  // ns to us
    if (time_aggr_window_ == 0) {
      // TODO: Should there be a min and/or max?
      // TODO: Log message saying that default is used
      time_aggr_window = kDefaultTimeAggrWindow;
    }

    // calculate operation description
    external::L0::zet_intel_metric_calculate_exp_desc_t calculate_desc{
        external::L0::ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
        nullptr,                      // pNext
        1,                            // metricGroupCount
        &it->second->metrics_group_,  // phMetricGroups
        0,                            // metricCount
        nullptr,                      // phMetrics
        0,                            // timeWindowsCount
        nullptr,                      // pCalculateTimeWindows
        time_aggr_window,             // timeAggregationWindow
        external::L0::ZET_INTEL_METRIC_CALCULATE_OPERATION_EXP_FLAG_AVERAGE,  // operation
        0                                                                     // startingTime
    };

    // Create calaculate operation
    external::L0::zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle = nullptr;
    status = tf.zetIntelMetricCalculateOperationCreateExp(it->second->context_, it->second->device_,
                                                          &calculate_desc, &calculate_op_handle);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    // Option 1: user wants metrics values count
    if (metrics_values_buffer == nullptr) {
      inf.seekg(0, inf.end);
      std::streamsize stream_file_size = inf.tellg();
      PTI_ASSERT(stream_file_size >= 0);
      size_t file_size = static_cast<size_t>(stream_file_size);

      inf.seekg(0, inf.beg);  // rewind
      std::vector<uint8_t> raw_metrics(file_size);

      inf.read(reinterpret_cast<char *>(raw_metrics.data()), file_size);
      size_t raw_size = inf.gcount();
      if (raw_size > 0) {
        uint32_t num_metrics = 0;
        status = zetMetricGet(it->second->metrics_group_, &num_metrics, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        std::vector<zet_metric_handle_t> metrics(num_metrics);
        status = zetMetricGet(it->second->metrics_group_, &num_metrics, metrics.data());
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        uint32_t report_size = 0;
        status = tf.zetIntelMetricCalculateGetReportFormatExp(calculate_op_handle, &report_size,
                                                              nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::cout << "Calculate Report size: " << report_size << "\n";

        // get report format
        std::vector<zet_metric_handle_t> metrics_in_report(report_size);
        status = tf.zetIntelMetricCalculateGetReportFormatExp(calculate_op_handle, &report_size,
                                                              metrics_in_report.data());
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        // Get total number of sets and reports
        uint32_t total_report_count = 0;
        uint32_t set_count = 0;
        status = tf.zetIntelMetricDecodeCalculateMultipleValuesExp(
            metric_decoder_, &raw_size, raw_metrics.data(), calculate_op_handle, &set_count,
            nullptr, &total_report_count, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        // Note: report size is the number of metrics in the metric group + 2 synthetically added
        // timestamp markers: start and stop timestamps. Total number of values that will be written
        // to the buffer is the total number of reports multiplied by the size of each report
        // (metric_count + 2timestamps)
        *metrics_values_count = total_report_count * report_size;
      }
    } else {
      // Option 2: user wants the buffer filled.
      std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

      uint32_t buffer_idx = 0;
      // Read and process input file stream where metrics data is saved
      while (!inf.eof()) {
        inf.read(reinterpret_cast<char *>(raw_metrics.data()),
                 PtiMetricsProfiler::GetMaxMetricBufferSize());
        size_t raw_size = inf.gcount();
        if (raw_size > 0) {
          // get report size
          uint32_t report_size = 0;
          status = tf.zetIntelMetricCalculateGetReportFormatExp(calculate_op_handle, &report_size,
                                                                nullptr);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          std::cout << "Calculate Report size: " << report_size << "\n";

          // get report format
          std::vector<zet_metric_handle_t> metrics_in_report(report_size);
          status = tf.zetIntelMetricCalculateGetReportFormatExp(calculate_op_handle, &report_size,
                                                                metrics_in_report.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          // Get total number of sets and reports
          uint32_t total_report_count = 0;
          uint32_t set_count = 0;
          status = tf.zetIntelMetricDecodeCalculateMultipleValuesExp(
              metric_decoder_, &raw_size, raw_metrics.data(), calculate_op_handle, &set_count,
              nullptr, &total_report_count, nullptr);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          // Decode and calculate metrics
          std::vector<uint32_t> report_count_per_set(set_count);
          std::vector<external::L0::zet_intel_metric_result_exp_t> metric_results(
              total_report_count * report_size);
          std::cout << "Calculate number of sets: " << set_count
                    << ". Total number of results: " << total_report_count
                    << ". Rawdata used: " << raw_size << std::endl;

          status = tf.zetIntelMetricDecodeCalculateMultipleValuesExp(
              metric_decoder_, &raw_size, raw_metrics.data(), calculate_op_handle, &set_count,
              report_count_per_set.data(), &total_report_count, metric_results.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          uint32_t output_index = 0;
          std::string valid_value;
          // walk through the sets
          for (uint32_t set_index = 0; set_index < set_count; set_index++) {
            std::cout << "Set : " << set_index
                      << " Reports in set: " << report_count_per_set[set_index] << std::endl;
            // For each set, walk through the reports
            for (uint32_t report_index = 0; report_index < report_count_per_set[set_index];
                 report_index++) {
              std::cout << " Report : " << report_index
                        << " Metrics in result: " << metrics_in_report[report_index] << std::endl;

              // For each report, walk through the results
              for (uint32_t result_index = 0; result_index < report_size; result_index++) {
                zet_metric_properties_t metric_properties = {};
                status =
                    zetMetricGetProperties(metrics_in_report[result_index], &metric_properties);
                PTI_ASSERT(status == ZE_RESULT_SUCCESS);
                std::cout << "   Index: " << output_index
                          << " Component: " << metric_properties.component
                          << "\t Metric name: " << metric_properties.name << " | ";

                valid_value.assign((metric_results[output_index].resultStatus ==
                                    external::L0::ZET_INTEL_METRIC_CALCULATE_EXP_RESULT_VALID)
                                       ? " Valid"
                                       : "Invalid");
                switch (metric_properties.resultType) {
                  case ZET_VALUE_TYPE_UINT32:
                  case ZET_VALUE_TYPE_UINT8:
                  case ZET_VALUE_TYPE_UINT16:
                    metrics_values_buffer[buffer_idx++].ui32 =
                        metric_results[output_index].value.ui32;
                    std::cout << "\t value: " << metric_results[output_index].value.ui32 << " | ";
                    break;
                  case ZET_VALUE_TYPE_UINT64:
                    metrics_values_buffer[buffer_idx++].ui64 =
                        metric_results[output_index].value.ui64;
                    std::cout << "\t value: " << metric_results[output_index].value.ui64 << " | ";
                    break;
                  case ZET_VALUE_TYPE_FLOAT32:
                    metrics_values_buffer[buffer_idx++].fp32 =
                        metric_results[output_index].value.fp32;
                    std::cout << "\t value: " << metric_results[output_index].value.fp32 << " | ";
                    break;
                  case ZET_VALUE_TYPE_FLOAT64:
                    metrics_values_buffer[buffer_idx++].fp64 =
                        metric_results[output_index].value.fp64;
                    std::cout << "\t value: " << metric_results[output_index].value.fp64 << " | ";
                    break;
                  case ZET_VALUE_TYPE_BOOL8:
                    metrics_values_buffer[buffer_idx++].b8 = metric_results[output_index].value.b8;
                    std::cout << "\t value: " << metric_results[output_index].value.b8 << " | ";
                    break;
                  default:
                    std::cout << "[ERROR] Encountered unsupported Type";
                    break;
                }
                std::cout << valid_value << " | " << std::endl;

                output_index++;
              }
            }
          }
        }
      }
    }
    inf.close();
    status = tf.zetIntelMetricCalculateOperationDestroyExp(calculate_op_handle);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = tf.zetMetricDecoderDestroyExp(metric_decoder_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void CaptureRawMetrics(external::L0::zet_metric_tracer_exp_handle_t tracer, uint8_t *storage,
                         size_t ssize, std::shared_ptr<pti_metrics_device_descriptor_t> desc,
                         bool immediate_save_to_disc = false) {
    PTI_ASSERT(desc != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    size_t data_size = ssize;
    status = tf.zetMetricTracerReadDataExp(tracer, &data_size, storage);
    if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
      SPDLOG_WARN("Metric samples dropped.");
    } else if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("zetMetricTracerReadData failed with error code {:x}",
                   static_cast<std::size_t>(status));
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    SaveRawData(std::move(desc), storage, data_size, immediate_save_to_disc);
  }

  void EventBasedCaptureRawMetrics(external::L0::zet_metric_tracer_exp_handle_t tracer,
                                   uint8_t *storage, size_t ssize,
                                   std::shared_ptr<pti_metrics_device_descriptor_t> desc) {
    PTI_ASSERT(desc != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;

    status = zeEventQueryStatus(desc->event_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);
    if (status == ZE_RESULT_SUCCESS) {
      status = zeEventHostReset(desc->event_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    } else { /* ZE_RESULT_NOT_READY */
      return;
    }
    bool immediate_save_to_disc = false;
    CaptureRawMetrics(tracer, storage, ssize, std::move(desc), immediate_save_to_disc);
  }

  void PerDeviceTraceMetricsProfilingThread(
      std::shared_ptr<pti_metrics_device_descriptor_t> desc,
      external::L0::zet_metric_decoder_exp_handle_t *metric_decoder, bool start_paused) {
    PTI_ASSERT(desc != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;

    pti_result result = CollectionInitialize(desc);
    PTI_ASSERT(result == PTI_SUCCESS);

    external::L0::zet_metric_tracer_exp_handle_t tracer = nullptr;

    external::L0::zet_metric_tracer_exp_desc_t tracer_desc;
    tracer_desc.stype = external::L0::ZET_STRUCTURE_TYPE_METRIC_TRACER_EXP_DESC;
    tracer_desc.pNext = nullptr;
    tracer_desc.notifyEveryNBytes = max_metric_samples_;

    status = tf.zetMetricTracerCreateExp(desc->context_, desc->device_, 1, &(desc->metrics_group_),
                                         &tracer_desc, desc->event_, &tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    // TODO: check if notifyEveryNBytes works well with max_metric_samples_ for tracer
    if (tracer_desc.notifyEveryNBytes > max_metric_samples_) {
      max_metric_samples_ = tracer_desc.notifyEveryNBytes;
    }

    std::vector<std::string> metrics_list;
    metrics_list = utils::ze::GetMetricList(desc->metrics_group_);
    PTI_ASSERT(!metrics_list.empty());

    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    bool tracer_enabled = false;
    ptiMetricProfilerState profiling_state = start_paused
                                                 ? ptiMetricProfilerState::PROFILER_PAUSED
                                                 : ptiMetricProfilerState::PROFILER_ENABLED;
    desc->profiling_state_.store(profiling_state, std::memory_order_release);

    // Unblock the main thread
    cv_thread_start_.notify_one();

    bool immediate_save_to_disc = true;
    while (desc->profiling_state_.load(std::memory_order_acquire) !=
           ptiMetricProfilerState::PROFILER_DISABLED) {
      if (desc->profiling_state_.load(std::memory_order_acquire) ==
          ptiMetricProfilerState::PROFILER_PAUSED) {
        // close the tracer when profiler is paused
        if (tracer_enabled == true) {
          // Capture hw buffered raw data and immediately write it to disc before disabling the
          // tracer
          CaptureRawMetrics(tracer, raw_metrics.data(),
                            PtiMetricsProfiler::GetMaxMetricBufferSize(), desc,
                            immediate_save_to_disc);

          status = tf.zetMetricTracerDisableExp(tracer, false);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          tracer_enabled = false;
        }
        // Wait for the profiling state to change
        std::mutex pause_mutex;
        std::unique_lock pause_lock(pause_mutex);
        while (desc->profiling_state_.load(std::memory_order_acquire) ==
               ptiMetricProfilerState::PROFILER_PAUSED) {
          cv_pause_.wait(pause_lock);
        }
      } else {  // PROFILER_ENABLED
        // open the tracer when profiler is enabled
        if (tracer_enabled == false) {
          status = tf.zetMetricTracerEnableExp(tracer, true);
          if (status != ZE_RESULT_SUCCESS) {
            SPDLOG_ERROR("Failed to open metric tracer.");
#ifndef _WIN32
            SPDLOG_ERROR(
                "Please also make sure: "
                "on PVC: /proc/sys/dev/i915/perf_stream_paranoid "
                "OR on BMG (or later): /proc/sys/dev/xe/observation_paranoid "
                "is set to 0.");
#endif /* _WIN32 */
            break;
          }
          tracer_enabled = true;
        }
        // Capture hw buffered raw data to local memory. Local memory is not written to disc
        // immediately, It is written to disc after a few hw buffer reads or if local buffer is not
        // empty but no data is captured from the hw buffer in this iteration
        EventBasedCaptureRawMetrics(tracer, raw_metrics.data(),
                                    PtiMetricsProfiler::GetMaxMetricBufferSize(), desc);
      }
    }

    // Create raw data decoder before disabling and destroying the tracer
    status = tf.zetMetricDecoderCreateExp(tracer, metric_decoder);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (tracer_enabled) {
      // Capture hw buffered raw data and immediately write it to disc before disabling the tracer
      CaptureRawMetrics(tracer, raw_metrics.data(), PtiMetricsProfiler::GetMaxMetricBufferSize(),
                        desc, immediate_save_to_disc);

      status = tf.zetMetricTracerDisableExp(tracer, false);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    status = tf.zetMetricTracerDestroyExp(tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    result = CollectionFinalize(std::move(desc));
    PTI_ASSERT(result == PTI_SUCCESS);
  }
};

class PtiMetricsCollectorHandler {
 public:
  PtiMetricsCollectorHandler(const PtiMetricsCollectorHandler &) = delete;
  PtiMetricsCollectorHandler &operator=(const PtiMetricsCollectorHandler &) = delete;
  PtiMetricsCollectorHandler(PtiMetricsCollectorHandler &&) = delete;
  PtiMetricsCollectorHandler &operator=(PtiMetricsCollectorHandler &&) = delete;

  PtiMetricsCollectorHandler() {
    // initially set logging level to warn
    // need to use warnings very carefully, only when absolutely necessary
    // as on Windows encountered it is INFO (taken from compiler define) by default (?)
    spdlog::set_level(spdlog::level::warn);
    // Read Logging level required
    // set environment variable PTILOG_LEVEL=<level>, where level=TRACE/DEBUG/INFO..
    // Logs appear only when PTI_ENABLE_LOGGING=ON => SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    std::string env_string = utils::GetEnv("PTILOG_LEVEL");
    if (!env_string.empty()) {
      spdlog::cfg::helpers::load_levels(env_string);
    }

    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    spdlog::set_pattern("[%H:%M][%^-%l-%$]%P:%t %s:%# %v");

    // Initialize L0
    ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    bool l0_initialized, metrics_enabled;
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("Failed to initialize Level Zero runtime");
#ifndef _WIN32
      SPDLOG_ERROR(
          "Please also make sure: "
          "on PVC: /proc/sys/dev/i915/perf_stream_paranoid "
          "OR on BMG (or later): /proc/sys/dev/xe/observation_paranoid "
          "is set to 0.");
#endif /* _WIN32 */
      l0_initialized = false;
    } else {
      l0_initialized = true;
    }

    if (utils::GetEnv("ZET_ENABLE_METRICS") == "1") {
      metrics_enabled = true;
    } else {
      SPDLOG_ERROR(
          "Metrics collection is not enabled on this system. Please make sure environment variable "
          "ZET_ENABLE_METRICS is set to 1.");
      metrics_enabled = false;
    }
    metrics_enabled_ = (l0_initialized && metrics_enabled);

#ifdef _WIN32
    std::string loader_lib_name = "libze_loader.dll";  // TODO check???
#else
    std::string loader_lib_name = "libze_loader.so.1";
#endif
    // get loader handle to verify trace metric API exists in the loader
    loader_lib_ = utils::LoadLibrary(loader_lib_name.c_str());

    // TraceMetricsProfiler relies on L0 Trace Metrics API extensions
    // First hook the API symbols successfully before enabling the collection
    trace_api_enabled_ = (HookTraceMetricsAPI() == PTI_SUCCESS) ? true : false;
  }

  ~PtiMetricsCollectorHandler() {
    devices_.clear();
    for (auto it : metric_groups_) {
      it.second.clear();
    }
    metric_groups_.clear();
    names_.clear();
    stream_metrics_profilers_.clear();
    // query_metrics_profilers_.clear();
    trace_metrics_profilers_.clear();
    utils::UnloadLibrary(loader_lib_);
  }

  inline pti_result ptiDriverGetExtensionFunctionAddress(HMODULE lib, const char *symbol_name,
                                                         void **function_address) {
    if (function_address == nullptr) {
      return PTI_ERROR_DRIVER;
    }
    *function_address = utils::GetFunctionPtr(lib, symbol_name);

    return PTI_SUCCESS;
  }

  inline pti_result HookTraceMetricsAPI() {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    // TODO: Do full discovery instead of using the first GPU driver instance.
    ze_driver_handle_t driver = utils::ze::GetGpuDriver(0);
    if (loader_lib_ == nullptr || driver == nullptr) {
      SPDLOG_INFO("Could not enable trace metrics");
      return PTI_ERROR_DRIVER;
    }

    // These symbols are available only with the latest version of the loader
    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerCreateExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerCreateExp))) ||
        tf.zetMetricTracerCreateExp == nullptr) {
      SPDLOG_INFO("the zetMetricTracerCreateExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerDestroyExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerDestroyExp))) ||
        (tf.zetMetricTracerDestroyExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerDestroyExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerEnableExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerEnableExp))) ||
        (tf.zetMetricTracerEnableExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerEnableExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerDisableExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerDisableExp))) ||
        (tf.zetMetricTracerDisableExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerDisableExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerReadDataExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerReadDataExp))) ||
        (tf.zetMetricTracerReadDataExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerReadDataExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricDecoderCreateExp",
                            reinterpret_cast<void **>(&tf.zetMetricDecoderCreateExp))) ||
        (tf.zetMetricDecoderCreateExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderCreateExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricDecoderDestroyExp",
                            reinterpret_cast<void **>(&tf.zetMetricDecoderDestroyExp))) ||
        (tf.zetMetricDecoderDestroyExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderDestroyExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS != ptiDriverGetExtensionFunctionAddress(
                            loader_lib_, "zetMetricTracerDecodeExp",
                            reinterpret_cast<void **>(&tf.zetMetricTracerDecodeExp))) ||
        (tf.zetMetricTracerDecodeExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerDecodeExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((PTI_SUCCESS !=
         ptiDriverGetExtensionFunctionAddress(
             loader_lib_, "zetMetricDecoderGetDecodableMetricsExp",
             reinterpret_cast<void **>(&tf.zetMetricDecoderGetDecodableMetricsExp))) ||
        (tf.zetMetricDecoderGetDecodableMetricsExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderGetDecodableMetricsExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    // These symbols are internal only as of 12/24
    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetIntelMetricCalculateOperationCreateExp",
             reinterpret_cast<void **>(&tf.zetIntelMetricCalculateOperationCreateExp))) ||
        (tf.zetIntelMetricCalculateOperationCreateExp == nullptr)) {
      SPDLOG_INFO("the zetIntelMetricCalculateOperationCreateExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetIntelMetricCalculateOperationDestroyExp",
             reinterpret_cast<void **>(&tf.zetIntelMetricCalculateOperationDestroyExp))) ||
        (tf.zetIntelMetricCalculateOperationDestroyExp == nullptr)) {
      SPDLOG_INFO("the zetIntelMetricCalculateOperationDestroyExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetIntelMetricCalculateGetReportFormatExp",
             reinterpret_cast<void **>(&tf.zetIntelMetricCalculateGetReportFormatExp))) ||
        (tf.zetIntelMetricCalculateGetReportFormatExp == nullptr)) {
      SPDLOG_INFO("the zetIntelMetricCalculateGetReportFormatExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetIntelMetricDecodeCalculateMultipleValuesExp",
             reinterpret_cast<void **>(&tf.zetIntelMetricDecodeCalculateMultipleValuesExp))) ||
        (tf.zetIntelMetricDecodeCalculateMultipleValuesExp == nullptr)) {
      SPDLOG_INFO("the zetIntelMetricDecodeCalculateMultipleValuesExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetIntelMetricDecodeToBinaryBufferExp",
             reinterpret_cast<void **>(&tf.zetIntelMetricDecodeToBinaryBufferExp))) ||
        (tf.zetIntelMetricDecodeToBinaryBufferExp == nullptr)) {
      SPDLOG_INFO("the zetIntelMetricDecodeToBinaryBufferExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    return PTI_SUCCESS;
  }

  inline const char *GetStringPtr(char *input_name) {
    std::string name(input_name);
    if (names_.find(name) == names_.end()) {
      names_[name] = nullptr;
      auto it = names_.find(name);
      names_[name] = reinterpret_cast<const char *>(&it->first[0]);
    }
    return names_[name];
  }

  inline bool IsDeviceHandleValid(ze_device_handle_t device_handle) {
    if (devices_.empty()) {
      devices_ = utils::ze::GetDeviceList();
    }

    for (auto *device : devices_) {
      if (device_handle == device) {
        return true;
      }
    }
    return false;
  }

  inline pti_result GetDeviceCount(uint32_t *device_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (devices_.empty()) {
      devices_ = utils::ze::GetDeviceList();
    }

    *device_count = devices_.size();
    return PTI_SUCCESS;
  }

  inline pti_result GetDevices(pti_device_properties_t *pDevices, uint32_t *device_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (pDevices == nullptr || device_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (devices_.empty()) {
      devices_ = utils::ze::GetDeviceList();
    }
    uint32_t num_devices = devices_.size();
    if (num_devices < *device_count) {
      SPDLOG_WARN("Device buffer size too small. Device count is {}", num_devices);
      *device_count = num_devices;
      return PTI_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < num_devices; ++i) {
      ze_device_properties_t device_properties;
      std::memset(&device_properties, 0, sizeof(device_properties));
      device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

      ze_result_t status = zeDeviceGetProperties(devices_[i], &device_properties);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      ze_pci_ext_properties_t pci_props;
      std::memset(&pci_props, 0, sizeof(pci_props));
      pci_props.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;

      status = zeDevicePciGetPropertiesExt(devices_[i], &pci_props);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      pDevices[i]._handle = static_cast<pti_device_handle_t>(devices_[i]);
      pDevices[i]._address._domain = pci_props.address.domain;
      pDevices[i]._address._bus = pci_props.address.bus;
      pDevices[i]._address._device = pci_props.address.device;
      pDevices[i]._address._function = pci_props.address.function;

      pDevices[i]._model_name = GetStringPtr(device_properties.name);

      std::copy_n(device_properties.uuid.id, PTI_MAX_DEVICE_UUID_SIZE, pDevices[i]._uuid);
    }
    return PTI_SUCCESS;
  }

  // Check metric group handle in all available devices
  inline bool IsMetricGroupHandleValid(zet_metric_group_handle_t metric_group_handle) {
    if (devices_.empty()) {
      devices_ = utils::ze::GetDeviceList();
    }

    for (auto device : devices_) {
      if (IsMetricGroupHandleValid(device, metric_group_handle)) {
        return true;
      }
    }

    return false;
  }

  // check metric group handle for specified device
  inline bool IsMetricGroupHandleValid(pti_device_handle_t device_handle,
                                       zet_metric_group_handle_t metric_group_handle) {
    // Fist make sure the device handle is valid
    ze_device_handle_t device = static_cast<ze_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      return false;
    }

    if (metric_groups_.find(device) == metric_groups_.end()) {
      utils::ze::FindMetricGroups(device, metric_groups_[device]);
    }

    for (auto *group : metric_groups_[device]) {
      if (metric_group_handle == group) {
        return true;
      }
    }

    return false;
  }

  inline pti_result GetMetricGroupCount(pti_device_handle_t device_handle,
                                        uint32_t *metrics_group_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr || metrics_group_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    ze_device_handle_t device = static_cast<ze_device_handle_t>(device_handle);

    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (metric_groups_.find(device) == metric_groups_.end()) {
      // Find all metric groups for given device regardless of type
      utils::ze::FindMetricGroups(device, metric_groups_[device]);
    }

    *metrics_group_count = metric_groups_[device].size();

    return PTI_SUCCESS;
  }

  inline pti_result GetMetricGroups(pti_device_handle_t device_handle,
                                    pti_metrics_group_properties_t *metrics_groups,
                                    uint32_t *metrics_group_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr || metrics_groups == nullptr || metrics_group_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    ze_device_handle_t device = static_cast<ze_device_handle_t>(device_handle);

    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (metric_groups_.find(device) == metric_groups_.end()) {
      // Find all metric groups for given device regardless of type
      utils::ze::FindMetricGroups(device, metric_groups_[device]);
    }

    uint32_t group_count = metric_groups_[device].size();
    if (group_count < *metrics_group_count) {
      SPDLOG_WARN("Metric Group buffer size too small. Group count is {}", group_count);
      *metrics_group_count = group_count;
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Populate the supplied buffer with discovered metric group properties
    for (size_t i = 0; i < group_count; ++i) {
      zet_metric_group_properties_t group_props;
      std::memset(&group_props, 0, sizeof(group_props));
      group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
      ze_result_t status = zetMetricGroupGetProperties(metric_groups_[device][i], &group_props);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      metrics_groups[i]._handle = metric_groups_[device][i];
      // PTI sampling types for performance metrics should match L0 sampling types
      metrics_groups[i]._type = static_cast<pti_metrics_group_type>(group_props.samplingType);
      metrics_groups[i]._domain = group_props.domain;
      metrics_groups[i]._metric_count = group_props.metricCount;
      // User must allocate metric properties buffer and get it populated as a separate step.
      metrics_groups[i]._metric_properties = nullptr;
      metrics_groups[i]._name = GetStringPtr(group_props.name);
      metrics_groups[i]._description = GetStringPtr(group_props.description);
    }

    return PTI_SUCCESS;
  }

  inline pti_result GetMetrics(pti_metrics_group_handle_t metrics_group_handle,
                               pti_metric_properties_t *metrics) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (metrics_group_handle == nullptr || metrics == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_metric_group_handle_t group = static_cast<zet_metric_group_handle_t>(metrics_group_handle);

    if (IsMetricGroupHandleValid(group) != true) {
      SPDLOG_ERROR("Invalid metric group handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_metric_group_properties_t group_props;
    std::memset(&group_props, 0, sizeof(group_props));
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    std::vector<zet_metric_handle_t> metric_list(group_props.metricCount, nullptr);
    status = zetMetricGet(group, &group_props.metricCount, metric_list.data());
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    for (uint32_t i = 0; i < group_props.metricCount; ++i) {
      zet_metric_properties_t metric_props;
      std::memset(&metric_props, 0, sizeof(metric_props));
      metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
      status = zetMetricGetProperties(metric_list[i], &metric_props);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      metrics[i]._handle = static_cast<pti_metric_handle_t>(metric_list[i]);
      metrics[i]._metric_type = static_cast<pti_metric_type>(metric_props.metricType);
      metrics[i]._value_type = static_cast<pti_metric_value_type>(metric_props.resultType);
      metrics[i]._name = GetStringPtr(metric_props.name);
      metrics[i]._description = GetStringPtr(metric_props.description);
      metrics[i]._units = GetStringPtr(metric_props.resultUnits);
    }
    return PTI_SUCCESS;
  }

  // Note: ConfigureMetricGroups currently supports only one metric group
  // TODO: Add support for multiple metric groups
  inline pti_result ConfigureMetricGroups(
      pti_device_handle_t device_handle,
      pti_metrics_group_collection_params_t *metric_config_params, uint32_t metrics_group_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (metric_config_params == nullptr || device_handle == nullptr ||
        metric_config_params->_group_handle == nullptr || metrics_group_count == 0) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_metric_group_handle_t group =
        static_cast<zet_metric_group_handle_t>(metric_config_params->_group_handle);

    if (IsMetricGroupHandleValid(device, group) != true) {
      SPDLOG_ERROR("Invalid metric group handle configured");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // TODO: Add support for more than 1 metric group per device at the time
    if (metrics_group_count > 1) {
      return PTI_ERROR_NOT_IMPLEMENTED;
    }

    // If Configure is called more than once on the same device, the new call would overwrite the
    // previous configuration
    if (stream_metrics_profilers_.find(device_handle) != stream_metrics_profilers_.end()) {
      stream_metrics_profilers_[device_handle].reset();
    }
    // if (query_metrics_profilers_.find(device_handle) != query_metrics_profilers_.end()) {
    //   query_metrics_profilers_[device_handle].clear();
    // }
    if (trace_metrics_profilers_.find(device_handle) != trace_metrics_profilers_.end()) {
      trace_metrics_profilers_[device_handle].reset();
    }

    zet_metric_group_properties_t group_props;
    std::memset(&group_props, 0, sizeof(group_props));
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
    if (status != ZE_RESULT_SUCCESS) {
      return PTI_ERROR_DRIVER;
    }

    switch (group_props.samplingType) {
      case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED: {
        uint32_t sampling_interval = metric_config_params->_sampling_interval;
        std::unique_ptr<PtiStreamMetricsProfiler> stream_metrics_profiler =
            std::make_unique<PtiStreamMetricsProfiler>(device_handle, group, sampling_interval);
        stream_metrics_profilers_[device_handle] = std::move(stream_metrics_profiler);
        break;
      }
      /*
         case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED: {
              std::unique_ptr<PtiQuertMetricsProfiler> query_metrics_profiler =
                std::make_unique<PtiQueryMetricsProfiler>(device_handle, group);
              query_metrics_profilers_[device_handle].push_back(std::move(query_metrics_profiler));
              break;
         }
      */
      case external::L0::ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EXP_TRACER_BASED: {
        // TraceMetricsProfiler relies on L0 Trace Metrics API extensions
        if (trace_api_enabled_) {
          uint32_t time_aggr_window = metric_config_params->_time_aggr_window;
          std::unique_ptr<PtiTraceMetricsProfiler> trace_metrics_profiler =
              std::make_unique<PtiTraceMetricsProfiler>(device_handle, group, time_aggr_window);
          trace_metrics_profilers_[device_handle] = std::move(trace_metrics_profiler);
        } else {
          SPDLOG_ERROR("Trace metrics cannot be collected on this system");
          return PTI_ERROR_DRIVER;
        }
        break;
      }
      default: {
        return PTI_ERROR_NOT_IMPLEMENTED;
      }
    }

    return PTI_SUCCESS;
  }

  pti_result StartCollection(pti_device_handle_t device_handle, bool start_paused = false) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;
    pti_result status = PTI_SUCCESS;
    if (stream_metrics_profilers_.find(device_handle) == stream_metrics_profilers_.end() &&
        trace_metrics_profilers_.find(device_handle) == trace_metrics_profilers_.end()
        /* && query_metrics_profilers_.find(device_handle) == query_metrics_profilers_.end() */) {
      SPDLOG_ERROR(
          "Attempted to start a metrics collection on a device that has not been configured.");
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    auto &stream_metrics_profiler = stream_metrics_profilers_[device_handle];
    if (stream_metrics_profiler) {
      status = stream_metrics_profiler->StartProfiling(start_paused);
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }

    // TODO: start the Query metric profilers
    /*
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          query_metrics_profiler->StartProfiling(start_paused);
        }
    */
    auto &trace_metrics_profiler = trace_metrics_profilers_[device_handle];
    if (trace_metrics_profiler) {
      status = trace_metrics_profiler->StartProfiling(start_paused);
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }

    return result;
  }

  pti_result StartCollectionPaused(pti_device_handle_t device_handle) {
    bool start_paused = true;
    return StartCollection(device_handle, start_paused);
  }

  pti_result PauseCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // TODO: implement
    return PTI_ERROR_NOT_IMPLEMENTED;
    /*
        pti_result result = PTI_SUCCESS;
        pti_result status = PTI_SUCCESS;
        if (stream_metrics_profilers_.find(device_handle) == stream_metrics_profilers_.end() &&
            trace_metrics_profilers_.find(device_handle) == trace_metrics_profilers_.end() &&
            query_metrics_profilers_.find(device_handle) == query_metrics_profilers_.end()) {
          SPDLOG_ERROR("Attempted to pause a metrics collection on a device that has not been
       configured for metrics collection."); return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
        }

        for (auto &stream_metrics_profiler : stream_metrics_profilers_[device_handle]) {
          status = stream_metrics_profiler->PauseProfiling();
          if (status != PTI_SUCCESS) {
            result = status;
          }
        }

        // TODO: pause the Query metric profilers
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          status = query_metrics_profiler->PauseProfiling();
          if (status != PTI_SUCCESS) {
            result = status;
          }
        }

        for (auto &trace_metrics_profiler : trace_metrics_profilers_[device_handle]) {
          status = trace_metrics_profiler->PauseProfiling();
          if (status != PTI_SUCCESS) {
            result = status;
          }
        }

        return result;
    */
  }

  pti_result ResumeCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;
    pti_result status = PTI_SUCCESS;
    if (stream_metrics_profilers_.find(device_handle) == stream_metrics_profilers_.end() &&
        trace_metrics_profilers_.find(device_handle) == trace_metrics_profilers_.end()
        /* && query_metrics_profilers_.find(device_handle) == query_metrics_profilers_.end() */) {
      SPDLOG_ERROR(
          "Attempted to resume a metrics collection on a device that has not been configured.");
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    auto &stream_metrics_profiler = stream_metrics_profilers_[device_handle];
    if (stream_metrics_profiler) {
      status = stream_metrics_profiler->ResumeProfiling();
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }
    // TODO: resume the Query metric profilers
    /*
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          query_metrics_profiler->ResumeProfiling();
        }
    */
    auto &trace_metrics_profiler = trace_metrics_profilers_[device_handle];
    if (trace_metrics_profiler) {
      status = trace_metrics_profiler->ResumeProfiling();
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }

    return result;
  }

  pti_result StopCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;
    pti_result status = PTI_SUCCESS;
    if (stream_metrics_profilers_.find(device_handle) == stream_metrics_profilers_.end() &&
        trace_metrics_profilers_.find(device_handle) == trace_metrics_profilers_.end()
        /* && query_metrics_profilers_.find(device_handle) == query_metrics_profilers_.end() */) {
      SPDLOG_ERROR(
          "Attempted to stop a metrics collection on a device that has not been configured.");
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    auto &stream_metrics_profiler = stream_metrics_profilers_[device_handle];
    if (stream_metrics_profiler) {
      status = stream_metrics_profiler->StopProfiling();
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }
    /*
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          query_metrics_profiler->StopProfiling();
        }
    */
    auto &trace_metrics_profiler = trace_metrics_profilers_[device_handle];
    if (trace_metrics_profiler) {
      status = trace_metrics_profiler->StopProfiling();
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }

    return result;
  }

  pti_result GetCalculatedData(pti_device_handle_t device_handle,
                               pti_metrics_group_handle_t metrics_group_handle,
                               pti_value_t *metrics_values_buffer, uint32_t *metrics_values_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    zet_device_handle_t device = static_cast<zet_device_handle_t>(device_handle);
    if (IsDeviceHandleValid(device) != true) {
      SPDLOG_ERROR("Invalid device handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    zet_metric_group_handle_t group = static_cast<zet_metric_group_handle_t>(metrics_group_handle);
    if (IsMetricGroupHandleValid(device, group) != true) {
      SPDLOG_ERROR("Invalid metric group handle used");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;
    pti_result status = PTI_SUCCESS;
    if (stream_metrics_profilers_.find(device_handle) == stream_metrics_profilers_.end() &&
        trace_metrics_profilers_.find(device_handle) == trace_metrics_profilers_.end()
        /* && query_metrics_profilers_.find(device_handle) == query_metrics_profilers_.end() */) {
      SPDLOG_ERROR("Attempted to calculate metrics on a device that has not been configured.");
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    auto &stream_metrics_profiler = stream_metrics_profilers_[device_handle];
    if (stream_metrics_profiler) {
      status = stream_metrics_profiler->GetCalculatedData(
          metrics_group_handle, metrics_values_buffer, metrics_values_count);
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }
    /*
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          query_metrics_profiler->GetCalculatedData();
        }
    */
    auto &trace_metrics_profiler = trace_metrics_profilers_[device_handle];
    if (trace_metrics_profiler) {
      status = trace_metrics_profiler->GetCalculatedData(
          metrics_group_handle, metrics_values_buffer, metrics_values_count);
      if (status != PTI_SUCCESS) {
        result = status;
      }
    }

    return result;
  }

 private:
  std::unordered_map<std::string, const char *> names_;
  std::vector<ze_device_handle_t> devices_;
  std::map<ze_device_handle_t, std::vector<zet_metric_group_handle_t>> metric_groups_;

  std::unordered_map<pti_device_handle_t, std::unique_ptr<PtiStreamMetricsProfiler>>
      stream_metrics_profilers_;
  /*
    std::unordered_map<pti_device_handle_t, std::unique_ptr<PtiQueryMetricsProfiler> >
    > query_metrics_profilers_;
  */

  std::unordered_map<pti_device_handle_t, std::unique_ptr<PtiTraceMetricsProfiler>>
      trace_metrics_profilers_;
  bool metrics_enabled_;
  bool trace_api_enabled_;
  HMODULE loader_lib_;
};

// Required to access from ze_collector callbacks
inline static auto &MetricsCollectorInstance() {
  static PtiMetricsCollectorHandler metrics_collector{};
  return metrics_collector;
}

#endif  // SRC_API_METRICS_HANDLER_H_
