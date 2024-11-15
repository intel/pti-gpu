//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_METRICS_HANDLER_H_
#define SRC_API_METRICS_HANDLER_H_

#include <level_zero/ze_api.h>
#include <spdlog/cfg/env.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <random>
#include <sstream>
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
  std::vector<ze_context_handle_t> metric_contexts_;
  std::unordered_map<ze_device_handle_t, std::shared_ptr<pti_metrics_device_descriptor_t> >
      device_descriptors_;
  std::string data_dir_name_;
  std::shared_ptr<spdlog::logger> user_logger_;
  std::string log_name_;

 public:
  static uint32_t max_metric_samples_;
  PtiMetricsProfiler() = delete;
  PtiMetricsProfiler(const PtiMetricsProfiler &) = delete;
  PtiMetricsProfiler &operator=(const PtiMetricsProfiler &) = delete;
  PtiMetricsProfiler(PtiMetricsProfiler &&) = delete;
  PtiMetricsProfiler &operator=(PtiMetricsProfiler &&) = delete;

  PtiMetricsProfiler(pti_device_handle_t device_handle,
                     pti_metrics_group_handle_t metrics_group_handle) {
    auto data_dir = utils::ze::CreateTempDirectory();

    PTI_ASSERT(pti::utils::filesystem::exists(data_dir));
    SPDLOG_INFO("Temp dir {}", data_dir.string());

    data_dir_name_ = data_dir.generic_string();

    try {
      // Default user logger is to the console and it is off
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(spdlog::level::off);
      user_logger_ = std::make_shared<spdlog::logger>("logger", console_sink);

      std::string logfile;
      if (utils::GetEnv("PTI_LogToFile") == "1") {
        logfile = utils::GetEnv("PTI_LogFileName");

        if (!logfile.empty()) {
          size_t pos = logfile.find_first_of('.');

          std::string filename = (pos == std::string::npos) ? logfile : logfile.substr(0, pos);
          filename = filename + ".metrics." + std::to_string(utils::GetPid());

          std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK")
                                                                 : utils::GetEnv("PMI_RANK");
          if (!rank.empty()) {
            filename = filename + "." + rank;
          }

          if (pos != std::string::npos) {
            filename = filename + logfile.substr(pos);
          }

          // If the user sets PTI_LogToFile and specifies log file name in PTI_LogFileName
          // Then the file is created and the data is logged to is in json format
          log_name_ = std::move(filename);
          auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_name_, true);
          std::random_device rand_dev;    // uniformly-distributed integer random number generator
          std::mt19937 prng(rand_dev());  // pseudorandom number generator
          std::uniform_int_distribution<uint64_t> rand_num(0);  // random number
          user_logger_ = std::make_shared<spdlog::logger>(
              "file_logger_" + fmt::format("{:x}", rand_num(prng)), file_sink);
        } else {
          // If the user sets PTI_LogToFile but does not specify the log file name, then
          // the logging is enable to the console in json format
          log_name_ = "";  // empty log file name causes logger to log to std::cerr
          console_sink->set_level(spdlog::level::debug);
          user_logger_ = std::make_shared<spdlog::logger>("logger", console_sink);
        }
        auto format = std::make_unique<spdlog::pattern_formatter>(
            "%v", spdlog::pattern_time_type::local, std::string(""));  // disable eol
        user_logger_->set_formatter(std::move(format));
        spdlog::register_logger(user_logger_);
      }
    } catch (const spdlog::spdlog_ex &exception) {
      std::cerr << "Failed to initialize log file: " << exception.what() << std::endl;
    }
    EnumerateDevices(device_handle, metrics_group_handle);
  }

  static uint64_t GetMaxMetricBufferSize() {
    // 2 is for systems with 2 tiles
    // 512 is for extra padding
    // TODO: may need to be adjusted per specific target
    return static_cast<uint64_t>(max_metric_samples_) * PTI_METRIC_COUNT_MAX * 2 + 512;
  }

  virtual ~PtiMetricsProfiler() {
    pti::utils::filesystem::remove_all(data_dir_name_);
    metric_contexts_.clear();
    // Stopping runaway collections in case stop was not called
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->profiling_state_ != ptiMetricProfilerState::PROFILER_DISABLED) {
        SPDLOG_ERROR("Stopping runaway metrics collection");
        it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                           std::memory_order_release);
        it->second->profiling_thread_->join();
        it->second->profiling_thread_ = nullptr;
        it->second->metric_file_stream_.close();
      }
    }
    device_descriptors_.clear();
  }

  virtual void StartProfiling() = 0;

  virtual pti_result StopProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      // Collection should be running before stop is called
      if (it->second->profiling_thread_ == nullptr ||
          it->second->profiling_state_ == ptiMetricProfilerState::PROFILER_DISABLED) {
        SPDLOG_ERROR("Attempting to stop a metrics collection that hasn't been started");
        return PTI_ERROR_BAD_API_USAGE;
      }

      it->second->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                         std::memory_order_release);
      it->second->profiling_thread_->join();
      it->second->profiling_thread_ = nullptr;
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
        SPDLOG_ERROR("Attempting to dump data from a metrics collection that hasn't been stopped");
        result = PTI_ERROR_BAD_API_USAGE;
      }
    }

    // Stopping runaway collection if stop is not called before dump is called
    if (result != PTI_SUCCESS) {
      result = StopProfiling();
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
            std::shared_ptr<pti_metrics_device_descriptor_t>(new pti_metrics_device_descriptor_t);

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

        desc->profiling_thread_ = nullptr;
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
                std::shared_ptr<pti_metrics_device_descriptor_t>(
                    new pti_metrics_device_descriptor_t);

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

            sub_desc->profiling_thread_ = nullptr;
            sub_desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_DISABLED,
                                             std::memory_order_release);

            device_descriptors_[sub_devices[j]] = std::move(sub_desc);
          }
        }
      }
    }
  }
};

// L0 on Windows doesn't seem to be updating notifyEveryNReports correctly
// TODO: Remove the windows specific value once issue fixed in L0
#ifdef _WIN32
uint32_t PtiMetricsProfiler::max_metric_samples_ = 1024;
#else
uint32_t PtiMetricsProfiler::max_metric_samples_ = 32768;
#endif

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

  void StartProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      if (it->second->stall_sampling_) {
        SPDLOG_WARN("EU stall sampling is not supported");
        continue;
      }
      it->second->profiling_thread_ = std::unique_ptr<std::thread>(
          new std::thread(PerDeviceStreamMetricsProfilingThread, it->second, sampling_interval_));

      // Wait for the profiling to start
      while (it->second->profiling_state_.load(std::memory_order_acquire) !=
             ptiMetricProfilerState::PROFILER_ENABLED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  // StopProfiling() is same for all profiler types and is implemented in the parent class
  // PtiMetricsProfiler

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
      // Search for the top/parant device; it doesn't have a parent
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
      uint32_t file_size = inf.tellg();
      inf.seekg(0, inf.beg);  // rewind
      std::vector<uint8_t> raw_metrics(file_size);

      inf.read(reinterpret_cast<char *>(raw_metrics.data()), file_size);
      int raw_size = inf.gcount();
      if (raw_size > 0) {
        uint32_t num_samples = 0;
        uint32_t num_metrics = 0;
        ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
            it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, raw_size,
            raw_metrics.data(), &num_samples, &num_metrics, nullptr, nullptr);

        if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
          SPDLOG_WARN("Unable to calculate required data buffer size");
        }

        // Each metric value is of type pti_value_t
        // Each sample has a value for each metric in the metric group
        // Value count num_samples * num_metrics
        // Each value is of type pti_value_t
        *metrics_values_count = num_samples * num_metrics;
      }
      inf.close();
      return;
    }

    // Option 2: user wants the buffer filled.
    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    // Search for the top/parant device; it doesn't have a parent
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
        if (!inf.is_open()) {
          continue;
        }

        user_logger_->info("{\n\t\"displayTimeUnit\": \"us\",\n\t\"traceEvents\": [");

        uint64_t cur_sampling_ts = 0;

        uint32_t buffer_idx = 0;
        // Read and process input file stream where metrics data is saved
        while (!inf.eof()) {
          inf.read(reinterpret_cast<char *>(raw_metrics.data()),
                   PtiMetricsProfiler::GetMaxMetricBufferSize());
          int raw_size = inf.gcount();
          if (raw_size > 0) {
            // first call to Calculate metrics to capture the size of the samples and metrics
            // buffers
            uint32_t num_samples = 0;
            uint32_t num_metrics = 0;
            ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
                it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                raw_size, raw_metrics.data(), &num_samples, &num_metrics, nullptr, nullptr);
            if ((status != ZE_RESULT_SUCCESS) || (num_samples == 0) || (num_metrics == 0)) {
              SPDLOG_WARN("Unable to calculate metrics");
              continue;
            }

            // alllocate buffers for samples and metrics with required sizes
            std::vector<uint32_t> samples(num_samples);
            std::vector<zet_typed_value_t> metrics(num_metrics);

            // Second call to Calculate metrics to do the metrics calculations
            status = zetMetricGroupCalculateMultipleMetricValuesExp(
                it->second->metrics_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                raw_size, raw_metrics.data(), &num_samples, &num_metrics, samples.data(),
                metrics.data());
            if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
              SPDLOG_WARN("Unable to calculate metrics");
              continue;
            }

            // Process samples
            const zet_typed_value_t *value = metrics.data();
            for (uint32_t i = 0; i < num_samples; ++i) {
              uint32_t size = samples[i];

              for (uint32_t j = 0; j < (size / metric_list.size()); ++j) {
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
              value += samples[i];
            }
          }
        }
        inf.close();
        user_logger_->info("\n\t]\n}\n");
        // break;  // TODO: only one device for now
      }
    }
  }

  static uint64_t ReadMetrics(ze_event_handle_t event, zet_metric_streamer_handle_t streamer,
                              uint8_t *storage, size_t ssize) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    status = zeEventQueryStatus(event);
    if (!(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY)) {
      SPDLOG_ERROR("zeEventQueryStatus failed with error code: 0x{:x}",
                   static_cast<std::size_t>(status));
    }
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);
    if (status == ZE_RESULT_SUCCESS) {
      status = zeEventHostReset(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    } else {
      return 0;
    }

    size_t data_size = 0;
    status = zetMetricStreamerReadData(streamer, UINT32_MAX, &data_size, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(data_size > 0);
    if (data_size > ssize) {
      data_size = ssize;
      SPDLOG_WARN("Metric samples dropped.");
    }

    status = zetMetricStreamerReadData(streamer, UINT32_MAX, &data_size, storage);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("zetMetricStreamerReadData failed with error code {:x}",
                   static_cast<std::size_t>(status));
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    return data_size;
  }

  static void PerDeviceStreamMetricsProfilingThread(
      std::shared_ptr<pti_metrics_device_descriptor_t> desc, uint32_t sampling_interval) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_context_handle_t context = desc->context_;
    ze_device_handle_t device = desc->device_;
    zet_metric_group_handle_t group = desc->metrics_group_;

    status = zetContextActivateMetricGroups(context, device, 1, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t event_pool = nullptr;
    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                            ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
    status = zeEventPoolCreate(context, &event_pool_desc, 1, &device, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_handle_t event = nullptr;
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    status = zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    zet_metric_streamer_handle_t streamer = nullptr;

    constexpr auto kDefaultSamplingInterval = 1000000U;  // 1 millisecond

    uint32_t interval = (sampling_interval == 0) ? kDefaultSamplingInterval : sampling_interval;
    // TODO: Should there be a min and/or max?

    zet_metric_streamer_desc_t streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr,
                                                max_metric_samples_, interval};
    status = zetMetricStreamerOpen(context, device, group, &streamer_desc, event, &streamer);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("Failed to open metric streamer. The sampling interval might be too small.");
#ifndef _WIN32
      SPDLOG_ERROR(
          "Please also make sure /proc/sys/dev/i915/perf_stream_paranoid "
          "is set to 0.");
#endif /* _WIN32 */

      status = zeEventDestroy(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      status = zeEventPoolDestroy(event_pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      // set state to enabled to let the parent thread continue
      desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_ENABLED,
                                   std::memory_order_release);
      return;
    }

    if (streamer_desc.notifyEveryNReports > max_metric_samples_) {
      max_metric_samples_ = streamer_desc.notifyEveryNReports;
    }

    std::vector<std::string> metrics_list;
    metrics_list = utils::ze::GetMetricList(group);
    PTI_ASSERT(!metrics_list.empty());

    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_ENABLED,
                                 std::memory_order_release);
    while (desc->profiling_state_.load(std::memory_order_acquire) !=
           ptiMetricProfilerState::PROFILER_DISABLED) {
      uint64_t size = ReadMetrics(event, streamer, raw_metrics.data(),
                                  PtiMetricsProfiler::GetMaxMetricBufferSize());
      if (size == 0) {
        if (!desc->metric_data_.empty()) {
          desc->metric_file_stream_.write(reinterpret_cast<char *>(desc->metric_data_.data()),
                                          desc->metric_data_.size());
          desc->metric_data_.clear();
        }
        continue;
      }
      desc->metric_data_.insert(desc->metric_data_.end(), raw_metrics.data(),
                                raw_metrics.data() + size);
    }
    auto size = ReadMetrics(event, streamer, raw_metrics.data(),
                            PtiMetricsProfiler::GetMaxMetricBufferSize());
    desc->metric_data_.insert(desc->metric_data_.end(), raw_metrics.data(),
                              raw_metrics.data() + size);
    if (!desc->metric_data_.empty()) {
      desc->metric_file_stream_.write(reinterpret_cast<char *>(desc->metric_data_.data()),
                                      desc->metric_data_.size());
      desc->metric_data_.clear();
    }

    status = zetMetricStreamerClose(streamer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeEventDestroy(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeEventPoolDestroy(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zetContextActivateMetricGroups(context, device, 0, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
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

    void StartProfiling() {}
    StopProfiling() is same for all profiler types and is implemented in the parent class
PtiMetricsProfiler pti_result GetCalculatedData() {}

private:
    void ComputeMetrics() {}
    static uint64_t ReadMetrics() {}
    static void
PerDeviceQueryMetricsProfilingThread(std::shared_ptr<pti_metrics_device_descriptor_t> desc)
{}

};
*/

/* Trace metrics API hooks */
using importTracerCreatePtrFnt = ze_result_t (*)(zet_context_handle_t Context, zet_device_handle_t,
                                                 uint32_t, zet_metric_group_handle_t *,
                                                 zet_metric_tracer_exp_desc_t *, ze_event_handle_t,
                                                 zet_metric_tracer_exp_handle_t *);

using importTracerDestroyPtrFnt =
    ze_result_t (*)(zet_metric_tracer_exp_handle_t metric_tracer_handle);

using importTracerEnablePtrFnt =
    ze_result_t (*)(zet_metric_tracer_exp_handle_t metric_tracer_handle, bool synchronous);

using importTracerDisablePtrFnt =
    ze_result_t (*)(zet_metric_tracer_exp_handle_t metric_tracer_handle, bool synchronous);

using importTracerReadPtrFnt = ze_result_t (*)(zet_metric_tracer_exp_handle_t metric_tracer_handle,
                                               size_t *raw_data_size, uint8_t *raw_data);

using importDecoderCreatePtrFnt =
    ze_result_t (*)(zet_metric_tracer_exp_handle_t metric_tracer_handle,
                    zet_metric_decoder_exp_handle_t *metric_decoder_handle);

using importDecoderDestroyPtrFnt =
    ze_result_t (*)(zet_metric_decoder_exp_handle_t metric_decoder_handle);

using importDecoderDecodePtrFnt = ze_result_t (*)(
    zet_metric_decoder_exp_handle_t metric_decoder_handle, size_t *raw_data_size,
    const uint8_t *raw_data, uint32_t metric_count, zet_metric_handle_t *metric_handle,
    uint32_t *metric_entries_count, zet_metric_entry_exp_t *metric_entries);

using importDecoderGetDecodableMetricsPtrFnt =
    ze_result_t (*)(zet_metric_decoder_exp_handle_t metric_decoder_handle, uint32_t *count,
                    zet_metric_handle_t *metric_handle);

using importMetricDecodeCalculateMultipleValuesPtrFnt =
    ze_result_t (*)(zet_metric_decoder_exp_handle_t metric_decoder_handle, size_t *raw_data_size,
                    const uint8_t *raw_data, zex_metric_calculate_exp_desc_t *calculate_desc,
                    uint32_t *set_count, uint32_t *metric_results_count_per_set,
                    uint32_t *total_metric_results_count, zex_metric_result_exp_t *metric_results);

struct pti_metrics_tracer_functions_t {
  importTracerCreatePtrFnt zetMetricTracerCreateExp = nullptr;
  importTracerDestroyPtrFnt zetMetricTracerDestroyExp = nullptr;
  importTracerEnablePtrFnt zetMetricTracerEnableExp = nullptr;
  importTracerDisablePtrFnt zetMetricTracerDisableExp = nullptr;
  importTracerReadPtrFnt zetMetricTracerReadDataExp = nullptr;
  importDecoderCreatePtrFnt zetMetricDecoderCreateExp = nullptr;
  importDecoderDestroyPtrFnt zetMetricDecoderDestroyExp = nullptr;
  importDecoderDecodePtrFnt zetMetricDecoderDecodeExp = nullptr;
  importDecoderGetDecodableMetricsPtrFnt zetMetricDecoderGetDecodableMetricsExp = nullptr;
  importMetricDecodeCalculateMultipleValuesPtrFnt zexMetricDecodeCalculateMultipleValuesExp =
      nullptr;
};

pti_metrics_tracer_functions_t tf;

class PtiTraceMetricsProfiler : public PtiMetricsProfiler {
 private:
  uint32_t time_aggr_window_;
  zet_metric_decoder_exp_handle_t metric_decoder_;

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

  void StartProfiling() {
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      it->second->profiling_thread_ = std::unique_ptr<std::thread>(new std::thread(
          PerDeviceTraceMetricsProfilingThread, it->second, &(this->metric_decoder_)));

      // Wait for the profiling to start
      while (it->second->profiling_state_.load(std::memory_order_acquire) !=
             ptiMetricProfilerState::PROFILER_ENABLED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  // StopProfiling() is same for all profiler types and is implemented in the parent class
  // PtiMetricsProfiler

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
    // Option 1: user wants metrics values count
    if (metrics_values_buffer == nullptr) {
      // Search for the top/parant device; it doesn't have a parent
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
      uint32_t file_size = inf.tellg();
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

        constexpr auto kDefaultTimeAggrWindow = 10000U;

        uint32_t time_aggr_window = time_aggr_window_ / 1000;  // ns to us
        if (time_aggr_window_ == 0) {
          // TODO: Should there be a min and/or max?
          // TODO: Log message saying that default is used
          time_aggr_window = kDefaultTimeAggrWindow;
        }

        zex_metric_calculate_exp_desc_t calculate_desc{
            ZET_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
            nullptr,                                              // pNext
            0,                                                    // metricGroucount
            nullptr,                                              // pmetrics_group_handles
            num_metrics,                                          // metric_count
            metrics.data(),                                       // metric_handle
            0,                                                    // timeWindowsCount
            nullptr,                                              // pCalculateTimeWindows
            time_aggr_window,                                     // timeAggregationWindow
            ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_AVERAGE  // operation
        };

        uint32_t set_count = 0;
        uint32_t total_metric_results_count = 0;

        status = tf.zexMetricDecodeCalculateMultipleValuesExp(
            metric_decoder_, &raw_size, raw_metrics.data(), &calculate_desc, &set_count, nullptr,
            &total_metric_results_count, nullptr);

        if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
          SPDLOG_WARN("Unable to calculate required data buffer size");
        }

        // We are adding two more "synthetic" metrics are begining: start and stop timestamps
        *metrics_values_count =
            total_metric_results_count + (total_metric_results_count / num_metrics) * 2;
      }
      inf.close();
      return;
    }

    // Option 2: user wants the buffer filled.
    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    // Search for the top/parant device; it doesn't have a parent
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }

      if (it->second->metrics_group_ != metrics_group_handle) {
        SPDLOG_WARN("Could not find device and metric group");
        SPDLOG_WARN("Unable to calculate collected data");
        return;
      }

      // Get metric list for metric group collected
      std::vector<std::string> metric_list;
      metric_list = utils::ze::GetMetricList(it->second->metrics_group_);
      PTI_ASSERT(!metric_list.empty());
      uint32_t metric_count = metric_list.size();

      // get group name for metric group
      zet_metric_group_properties_t group_props;
      std::memset(&group_props, 0, sizeof(group_props));
      group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
      status = zetMetricGroupGetProperties(it->second->metrics_group_, &group_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      std::string group_name = group_props.name;

      // open input file stream where metrics data is saved
      std::ifstream inf =
          std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
      if (!inf.is_open()) {
        continue;
      }

      uint32_t buffer_idx = 0;
      // Read and process input file stream where metrics data is saved
      while (!inf.eof()) {
        inf.read(reinterpret_cast<char *>(raw_metrics.data()),
                 PtiMetricsProfiler::GetMaxMetricBufferSize());
        size_t raw_size = inf.gcount();
        if (raw_size > 0) {
          // create an array of expected results types
          std::vector<zet_metric_handle_t> metrics(metric_count);
          status = zetMetricGet(it->second->metrics_group_, &metric_count, metrics.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          zex_metric_calculate_exp_desc_t calculate_desc{
              ZET_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP,
              nullptr,                                              // pNext
              0,                                                    // metricGroucount
              nullptr,                                              // pmetrics_group_handles
              metric_count,                                         // metric_count
              metrics.data(),                                       // metric_handle
              0,                                                    // timeWindowsCount
              nullptr,                                              // pCalculateTimeWindows
              10000,                                                // timeAggregationWindow
              ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_AVERAGE  // operation
          };

          uint32_t report_size = metric_count + 2;  // include two timestamps
          std::vector<zet_value_type_t> metrics_result_types(report_size);
          std::vector<std::string> metrics_names(report_size);

          metrics_result_types[0] = ZET_VALUE_TYPE_UINT64;
          metrics_names[0] = "StartTimestamp";
          metrics_result_types[1] = ZET_VALUE_TYPE_UINT64;
          metrics_names[1] = "EndTimestamp";

          for (uint32_t j = 0; j < report_size - 2; ++j) {
            const zet_metric_handle_t metric = metrics[j];
            zet_metric_properties_t metric_properties = {};
            status = zetMetricGetProperties(metric, &metric_properties);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);

            metrics_result_types[j + 2] = metric_properties.resultType;
            metrics_names[j + 2] = metric_properties.name;
          }

          uint32_t set_count = 0;
          uint32_t total_metric_results_count = 0;

          status = tf.zexMetricDecodeCalculateMultipleValuesExp(
              metric_decoder_, &raw_size, raw_metrics.data(), &calculate_desc, &set_count, nullptr,
              &total_metric_results_count, nullptr);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          std::vector<uint32_t> metric_results_count_per_set(set_count);
          std::vector<zex_metric_result_exp_t> metric_results(total_metric_results_count);

          status = tf.zexMetricDecodeCalculateMultipleValuesExp(
              metric_decoder_, &raw_size, raw_metrics.data(), &calculate_desc, &set_count,
              metric_results_count_per_set.data(), &total_metric_results_count,
              metric_results.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          std::cout << "Calculate number of sets: " << set_count
                    << ". Total number of results: " << total_metric_results_count
                    << ". Report Size: " << report_size << ". Rawdata used: " << raw_size
                    << std::endl;

          uint32_t output_index = 0;
          std::string valid_value;
          for (uint32_t set_index = 0; set_index < set_count; set_index++) {
            if (metric_results_count_per_set[set_index] % report_size) {
              SPDLOG_ERROR("Invalid report size or number of results for trace metric group");
              return;
            }
            uint32_t set_report_count = metric_results_count_per_set[set_index] / report_size;

            std::cout << "\t Set : " << set_index
                      << " Results in Set: " << metric_results_count_per_set[set_index]
                      << " Reports in set: " << set_report_count << std::endl;

            // TODO: Adjust timestamp if there is a clock overflow
            for (uint32_t report_index = 0; report_index < set_report_count; report_index++) {
              std::cout << "\t Report : " << report_index << std::endl;

              for (uint32_t result_index = 0; result_index < report_size; result_index++) {
                std::cout << "\t Index: " << output_index
                          << "\t Metric name: " << metrics_names[result_index] << " | ";

                valid_value.assign((metric_results[output_index].resultStatus ==
                                    ZET_ENUM_EXP_METRIC_CALCULATE_RESULT_VALID)
                                       ? "\tValid"
                                       : "\tInvalid");
                switch (metrics_result_types[result_index]) {
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
      inf.close();
    }

    status = tf.zetMetricDecoderDestroyExp(metric_decoder_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  static uint64_t ReadMetrics(ze_event_handle_t event, zet_metric_tracer_exp_handle_t tracer,
                              uint8_t *storage, size_t ssize) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    status = zeEventQueryStatus(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);
    if (status == ZE_RESULT_SUCCESS) {
      status = zeEventHostReset(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    } else {
      return 0;
    }

    size_t data_size = 0;
    status = tf.zetMetricTracerReadDataExp(tracer, &data_size, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(data_size > 0);
    if (data_size > ssize) {
      data_size = ssize;
      SPDLOG_WARN("Metric samples dropped.");
    }

    status = tf.zetMetricTracerReadDataExp(tracer, &data_size, storage);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return data_size;
  }

  static void PerDeviceTraceMetricsProfilingThread(
      std::shared_ptr<pti_metrics_device_descriptor_t> desc,
      zet_metric_decoder_exp_handle_t *metric_decoder) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_context_handle_t context = desc->context_;
    ze_device_handle_t device = desc->device_;
    zet_metric_group_handle_t group = desc->metrics_group_;

    status = zetContextActivateMetricGroups(context, device, 1, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t event_pool = nullptr;
    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                            ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
    status = zeEventPoolCreate(context, &event_pool_desc, 1, &device, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_handle_t event = nullptr;
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    status = zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    zet_metric_tracer_exp_handle_t tracer = nullptr;

    zet_metric_tracer_exp_desc_t tracer_desc{
        {static_cast<zet_structure_type_t>(ZET_STRUCTURE_TYPE_METRIC_TRACER_DESC_EXP), nullptr},
        max_metric_samples_};

    status = tf.zetMetricTracerCreateExp(context, device, 1, &group, &tracer_desc, event, &tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = tf.zetMetricTracerEnableExp(tracer, true);
    if (status != ZE_RESULT_SUCCESS) {
      SPDLOG_ERROR("Failed to open metric tracer.");
#ifndef _WIN32
      SPDLOG_ERROR(
          "Please also make sure /proc/sys/dev/i915/perf_stream_paranoid "
          "is set to 0.");
#endif /* _WIN32 */

      status = zeEventDestroy(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      status = zeEventPoolDestroy(event_pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      // set state to enabled to let the parent thread continue
      desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_ENABLED,
                                   std::memory_order_release);
      return;
    }
    // TODO: check if notifyEveryNBytes works well with max_metric_samples_ for tracer
    if (tracer_desc.notifyEveryNBytes > max_metric_samples_) {
      max_metric_samples_ = tracer_desc.notifyEveryNBytes;
    }

    std::vector<std::string> metrics_list;
    metrics_list = utils::ze::GetMetricList(group);
    PTI_ASSERT(!metrics_list.empty());

    std::vector<uint8_t> raw_metrics(PtiMetricsProfiler::GetMaxMetricBufferSize());

    desc->profiling_state_.store(ptiMetricProfilerState::PROFILER_ENABLED,
                                 std::memory_order_release);
    while (desc->profiling_state_.load(std::memory_order_acquire) !=
           ptiMetricProfilerState::PROFILER_DISABLED) {
      uint64_t size = ReadMetrics(event, tracer, raw_metrics.data(),
                                  PtiMetricsProfiler::GetMaxMetricBufferSize());
      if (size == 0) {
        if (!desc->metric_data_.empty()) {
          desc->metric_file_stream_.write(reinterpret_cast<char *>(desc->metric_data_.data()),
                                          desc->metric_data_.size());
          desc->metric_data_.clear();
        }
        continue;
      }
      desc->metric_data_.insert(desc->metric_data_.end(), raw_metrics.data(),
                                raw_metrics.data() + size);
    }
    auto size = ReadMetrics(event, tracer, raw_metrics.data(),
                            PtiMetricsProfiler::GetMaxMetricBufferSize());
    desc->metric_data_.insert(desc->metric_data_.end(), raw_metrics.data(),
                              raw_metrics.data() + size);
    if (!desc->metric_data_.empty()) {
      desc->metric_file_stream_.write(reinterpret_cast<char *>(desc->metric_data_.data()),
                                      desc->metric_data_.size());
      desc->metric_data_.clear();
    }

    status = tf.zetMetricDecoderCreateExp(tracer, metric_decoder);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = tf.zetMetricTracerDisableExp(tracer, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = tf.zetMetricTracerDestroyExp(tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeEventDestroy(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeEventPoolDestroy(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zetContextActivateMetricGroups(context, device, 0, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
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
          "Please also make sure /proc/sys/dev/i915/perf_stream_paranoid "
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

    // TraceMetricsProfiler relies on L0 Trace Metrics API extensions
    // First hook the API symbols successfully before enabling the collection
    trace_api_enabled_ = (HookTraceMetricsAPI() == PTI_SUCCESS) ? true : false;
  }

  ~PtiMetricsCollectorHandler() {
    names_.clear();
    stream_metrics_profilers_.clear();
    // query_metrics_profilers_.clear();
    trace_metrics_profilers_.clear();
  }

  inline pti_result HookTraceMetricsAPI() {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    // TODO: Do full discovery instead of using the first GPU driver instance.
    ze_driver_handle_t driver = utils::ze::GetGpuDriver(0);
    if (driver == nullptr) {
      SPDLOG_INFO("Could not enable trace metrics");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricTracerCreateExp",
                                  reinterpret_cast<void **>(&tf.zetMetricTracerCreateExp))) ||
        (tf.zetMetricTracerCreateExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerCreateExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricTracerDestroyExp",
                                  reinterpret_cast<void **>(&tf.zetMetricTracerDestroyExp))) ||
        (tf.zetMetricTracerDestroyExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerDestroyExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricTracerEnableExp",
                                  reinterpret_cast<void **>(&tf.zetMetricTracerEnableExp))) ||
        (tf.zetMetricTracerEnableExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerEnableExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricTracerDisableExp",
                                  reinterpret_cast<void **>(&tf.zetMetricTracerDisableExp))) ||
        (tf.zetMetricTracerDisableExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerDisableExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricTracerReadDataExp",
                                  reinterpret_cast<void **>(&tf.zetMetricTracerReadDataExp))) ||
        (tf.zetMetricTracerReadDataExp == nullptr)) {
      SPDLOG_INFO("the zetMetricTracerReadDataExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricDecoderCreateExp",
                                  reinterpret_cast<void **>(&tf.zetMetricDecoderCreateExp))) ||
        (tf.zetMetricDecoderCreateExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderCreateExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricDecoderDestroyExp",
                                  reinterpret_cast<void **>(&tf.zetMetricDecoderDestroyExp))) ||
        (tf.zetMetricDecoderDestroyExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderDestroyExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS != zeDriverGetExtensionFunctionAddress(
                                  driver, "zetMetricDecoderDecodeExp",
                                  reinterpret_cast<void **>(&tf.zetMetricDecoderDecodeExp))) ||
        (tf.zetMetricDecoderDecodeExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderDecodeExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zetMetricDecoderGetDecodableMetricsExp",
             reinterpret_cast<void **>(&tf.zetMetricDecoderGetDecodableMetricsExp))) ||
        (tf.zetMetricDecoderGetDecodableMetricsExp == nullptr)) {
      SPDLOG_INFO("the zetMetricDecoderGetDecodableMetricsExp symbol could not be loaded");
      return PTI_ERROR_DRIVER;
    }

    if ((ZE_RESULT_SUCCESS !=
         zeDriverGetExtensionFunctionAddress(
             driver, "zexMetricDecodeCalculateMultipleValuesExp",
             reinterpret_cast<void **>(&tf.zexMetricDecodeCalculateMultipleValuesExp))) ||
        (tf.zexMetricDecodeCalculateMultipleValuesExp == nullptr)) {
      SPDLOG_INFO("the zexMetricDecodeCalculateMultipleValuesExp symbol could not be loaded");
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

  inline pti_result GetDeviceCount(uint32_t *device_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
    *device_count = device_list.size();
    return PTI_SUCCESS;
  }

  inline pti_result GetDevices(pti_device_properties_t *pDevices, uint32_t *device_count) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (pDevices == nullptr || device_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
    uint32_t num_devices = device_list.size();
    if (num_devices < *device_count) {
      SPDLOG_WARN("Device buffer size too small. Device count is {}", num_devices);
      *device_count = num_devices;
      return PTI_ERROR_BAD_ARGUMENT;
    }

    for (size_t i = 0; i < num_devices; ++i) {
      ze_device_properties_t device_properties;
      std::memset(&device_properties, 0, sizeof(device_properties));
      device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

      ze_result_t status = zeDeviceGetProperties(device_list[i], &device_properties);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      ze_pci_ext_properties_t pci_props;
      std::memset(&pci_props, 0, sizeof(pci_props));
      pci_props.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;

      status = zeDevicePciGetPropertiesExt(device_list[i], &pci_props);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      pDevices[i]._handle = static_cast<pti_device_handle_t>(device_list[i]);
      pDevices[i]._address._domain = pci_props.address.domain;
      pDevices[i]._address._bus = pci_props.address.bus;
      pDevices[i]._address._device = pci_props.address.device;
      pDevices[i]._address._function = pci_props.address.function;

      pDevices[i]._model_name = GetStringPtr(device_properties.name);

      std::copy_n(device_properties.uuid.id, PTI_MAX_DEVICE_UUID_SIZE, pDevices[i]._uuid);
    }
    return PTI_SUCCESS;
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

    // Find all metric groups for given device regardless of type
    std::vector<zet_metric_group_handle_t> ze_metrics_groups;
    utils::ze::FindMetricGroups(device, ze_metrics_groups);

    *metrics_group_count = ze_metrics_groups.size();

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

    // Find all metric groups for given device regardless of type
    std::vector<zet_metric_group_handle_t> ze_metrics_groups;
    utils::ze::FindMetricGroups(device, ze_metrics_groups);

    uint32_t group_count = ze_metrics_groups.size();
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
      ze_result_t status = zetMetricGroupGetProperties(ze_metrics_groups[i], &group_props);
      if (status != ZE_RESULT_SUCCESS) {
        return PTI_ERROR_DRIVER;
      }

      metrics_groups[i]._handle = ze_metrics_groups[i];
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

    // TODO: Add support for more than 1 metric group per device at the time
    if (metrics_group_count > 1) {
      return PTI_ERROR_NOT_IMPLEMENTED;
    }

    // Only 1 group is supported per device at this time. If Configure is called more than once
    // on the same device, the new call would overwrite the previous configuration
    // TODO: Remove the clearing of the per device profilers vectors once multiple group collection
    // is enabled
    stream_metrics_profilers_[device_handle].clear();
    // query_metrics_profilers_[device].clear();
    trace_metrics_profilers_[device_handle].clear();

    zet_metric_group_handle_t group =
        static_cast<zet_metric_group_handle_t>(metric_config_params->_group_handle);

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
            std::unique_ptr<PtiStreamMetricsProfiler>(
                new PtiStreamMetricsProfiler(device_handle, group, sampling_interval));
        stream_metrics_profilers_[device_handle].push_back(std::move(stream_metrics_profiler));
        break;
      }
      /*
         case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED: {
              std::unique_ptr<PtiQuertMetricsProfiler> query_metrics_profiler =
                std::unique_ptr<PtiQueryMetricsProfiler>(new PtiQueryMetricsProfiler(device_handle,
         group));
              query_metrics_profilers_[device_handle].push_back(std::move(query_metrics_profiler));
              break;
         }
      */
      case ZET_METRIC_SAMPLING_TYPE_EXP_FLAG_TRACER_BASED: {
        // TraceMetricsProfiler relies on L0 Trace Metrics API extensions
        if (trace_api_enabled_) {
          uint32_t time_aggr_window = metric_config_params->_time_aggr_window;
          std::unique_ptr<PtiTraceMetricsProfiler> trace_metrics_profiler =
              std::unique_ptr<PtiTraceMetricsProfiler>(
                  new PtiTraceMetricsProfiler(device_handle, group, time_aggr_window));
          trace_metrics_profilers_[device_handle].push_back(std::move(trace_metrics_profiler));
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

  pti_result StartCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (stream_metrics_profilers_.empty() && trace_metrics_profilers_.empty()
        /* && query_metrics_profilers_.empty()*/) {
      SPDLOG_ERROR(
          "Metrics collection improperly configured and cannot be collected on this system");
      return PTI_ERROR_BAD_API_USAGE;
    }

    for (auto &stream_metrics_profiler : stream_metrics_profilers_[device_handle]) {
      stream_metrics_profiler->StartProfiling();
    }
    // TODO: start the Query metric profilers
    /*
        for (auto &query_metrics_profiler : query_metrics_profilers_[device_handle]) {
          query_metrics_profiler->StartProfiling();
        }
    */
    for (auto &trace_metrics_profiler : trace_metrics_profilers_[device_handle]) {
      trace_metrics_profiler->StartProfiling();
    }

    return PTI_SUCCESS;
  }

  pti_result StartCollectionPaused(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    // TODO: implement
    return PTI_ERROR_NOT_IMPLEMENTED;
  }

  pti_result PauseCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    // TODO: implement
    return PTI_ERROR_NOT_IMPLEMENTED;
  }

  pti_result ResumeCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    // TODO: implement
    return PTI_ERROR_NOT_IMPLEMENTED;
  }

  pti_result StopCollection(pti_device_handle_t device_handle) {
    if (metrics_enabled_ == false) {
      return PTI_ERROR_DRIVER;
    }

    pti_result result = PTI_SUCCESS;
    pti_result status;
    if (stream_metrics_profilers_.empty() && trace_metrics_profilers_.empty()
        /* && query_metrics_profilers_.empty()*/) {
      SPDLOG_ERROR(
          "Metrics collection improperly configured and cannot be collected on this system");
      return PTI_ERROR_BAD_API_USAGE;
    }

    for (auto &stream_metrics_profiler : stream_metrics_profilers_[device_handle]) {
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
    for (auto &trace_metrics_profiler : trace_metrics_profilers_[device_handle]) {
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

    pti_result result = PTI_SUCCESS;
    pti_result status = PTI_SUCCESS;
    if (stream_metrics_profilers_.empty() && trace_metrics_profilers_.empty()
        /* && query_metrics_profilers_.empty()*/) {
      SPDLOG_ERROR(
          "Metrics collection improperly configured and cannot be collected on this system");
      return PTI_ERROR_BAD_API_USAGE;
    }

    for (auto &stream_metrics_profiler : stream_metrics_profilers_[device_handle]) {
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
    for (auto &trace_metrics_profiler : trace_metrics_profilers_[device_handle]) {
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
  std::unordered_map<pti_device_handle_t, std::vector<std::unique_ptr<PtiStreamMetricsProfiler> > >
      stream_metrics_profilers_;
  /*
    std::unordered_map<pti_device_handle_t, std::vector<std::unique_ptr<PtiQueryMetricsProfiler> >
    > query_metrics_profilers_;
  */

  std::unordered_map<pti_device_handle_t, std::vector<std::unique_ptr<PtiTraceMetricsProfiler> > >
      trace_metrics_profilers_;
  bool metrics_enabled_;
  bool trace_api_enabled_;
};

// Required to access from ze_collector callbacks
inline static auto &MetricsCollectorInstance() {
  static PtiMetricsCollectorHandler metrics_collector{};
  return metrics_collector;
}

#endif  // SRC_API_METRICS_HANDLER_H_
