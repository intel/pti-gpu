//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef METRICS_UTILS_H_
#define METRICS_UTILS_H_

#ifdef __clang__
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#include <spdlog/cfg/env.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iomanip>
#include <random>

#include "pti/pti_metrics.h"
#include "samples_utils.h"
#include "utils.h"

namespace metrics_utils {

inline std::string GetGroupType(pti_metrics_group_type type) {
  std::string flag_name = "";
  if (type & PTI_METRIC_GROUP_TYPE_EVENT_BASED) {
    flag_name = "EVENT";
  }

  if (type & PTI_METRIC_GROUP_TYPE_TIME_BASED) {
    if (!flag_name.empty()) flag_name += "|";
    flag_name += "TIME";
  }

  if (type & PTI_METRIC_GROUP_TYPE_TRACE_BASED) {
    if (!flag_name.empty()) flag_name += "|";
    flag_name += "TRACE";
  }

  if (!flag_name.empty()) {
    return flag_name;
  }

  return "UNKNOWN";
}

inline std::string GetMetricType(pti_metric_type type) {
  switch (type) {
    case PTI_METRIC_TYPE_DURATION:
      return "DURATION";
    case PTI_METRIC_TYPE_EVENT:
      return "EVENT";
    case PTI_METRIC_TYPE_EVENT_WITH_RANGE:
      return "EVENT_WITH_RANGE";
    case PTI_METRIC_TYPE_THROUGHPUT:
      return "THROUGHPUT";
    case PTI_METRIC_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case PTI_METRIC_TYPE_FLAG:
      return "FLAG";
    case PTI_METRIC_TYPE_RATIO:
      return "RATIO";
    case PTI_METRIC_TYPE_RAW:
      return "RAW";
    case PTI_METRIC_TYPE_IP:
      return "IP";
    default:
      break;
  }
  return "UNKNOWN";
}

inline std::string GetMetricValueType(pti_metric_value_type type) {
  switch (type) {
    case PTI_METRIC_VALUE_TYPE_UINT32:
      return "UINT32";
    case PTI_METRIC_VALUE_TYPE_UINT64:
      return "UINT64";
    case PTI_METRIC_VALUE_TYPE_FLOAT32:
      return "FLOAT32";
    case PTI_METRIC_VALUE_TYPE_FLOAT64:
      return "FLOAT64";
    case PTI_METRIC_VALUE_TYPE_BOOL8:
      return "BOOL8";
    case PTI_METRIC_VALUE_TYPE_STRING:
      return "STRING";
    case PTI_METRIC_VALUE_TYPE_UINT8:
      return "UINT8";
    case PTI_METRIC_VALUE_TYPE_UINT16:
      return "BOOL8";
    default:
      break;
  }
  return "UNKNOWN";
}

inline pti_device_handle_t GetDevice(uint32_t device_id) {
  // Step1: discover available supported device count
  uint32_t device_count = 0;
  pti_result result = ptiMetricsGetDevices(nullptr, &device_count);
  if (result != PTI_SUCCESS || device_count == 0) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    return nullptr;  // exit function
  }

  // Step2: get available devices in device_buffer
  std::vector<pti_device_properties_t> device_buffer(device_count);

  result = ptiMetricsGetDevices(device_buffer.data(), &device_count);
  if (result != PTI_SUCCESS || device_buffer.empty()) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    return nullptr;  // exit function
  }

  pti_device_handle_t device_handle = nullptr;
  if (device_id < device_count) {
    device_handle = device_buffer[device_id]._handle;
  }

  return device_handle;
}

inline pti_metrics_group_handle_t FindMetricGroup(pti_device_handle_t device_handle,
                                                  std::string &group_name,
                                                  pti_metrics_group_type group_type) {
  if (device_handle == nullptr) {
    return nullptr;
  }

  uint32_t group_count = 0;
  pti_result result = ptiMetricsGetMetricGroups(device_handle, nullptr, &group_count);
  if (result != PTI_SUCCESS || group_count == 0) {
    std::cerr << "Failed to discover metric groups on specified device:" << std::endl;
    return nullptr;
  }

  std::vector<pti_metrics_group_properties_t> groups_buffer(group_count);

  result = ptiMetricsGetMetricGroups(device_handle, groups_buffer.data(), &group_count);
  if (result != PTI_SUCCESS || group_count == 0 || groups_buffer.empty()) {
    std::cerr << "Failed to discover metric groups" << std::endl;
    return nullptr;
  }

  pti_metrics_group_handle_t group_handle = nullptr;
  // Iterate through metric groups
  for (uint32_t i = 0; i < group_count; i++) {
    if (group_name == groups_buffer[i]._name && groups_buffer[i]._type == group_type) {
      group_handle = groups_buffer[i]._handle;
      break;
    }
  }

  return group_handle;
}

inline std::string PrintTypedValue(const pti_value_t value, pti_metric_value_type type,
                                   uint8_t precision = 2) {
  std::stringstream stream;
  switch (type) {
    case PTI_METRIC_VALUE_TYPE_UINT32:
      return std::to_string(value.ui32);
    case PTI_METRIC_VALUE_TYPE_UINT64:
      return std::to_string(value.ui64);
    case PTI_METRIC_VALUE_TYPE_FLOAT32:
      stream << std::fixed << std::setprecision(precision) << value.fp32;
      return stream.str();
    case PTI_METRIC_VALUE_TYPE_FLOAT64:
      stream << std::fixed << std::setprecision(precision) << value.fp64;
      return stream.str();
    case PTI_METRIC_VALUE_TYPE_BOOL8:
      return std::to_string(static_cast<uint32_t>(value.b8));
    default:
      break;
  }
  return "UNKNOWN";
}

inline bool CompareFiles(const std::string &filename1, const std::string &filename2) {
  std::ifstream file1(filename1, std::ifstream::binary | std::ifstream::ate);
  std::ifstream file2(filename2, std::ifstream::binary | std::ifstream::ate);

  bool match = false;
  if (file1.fail() || file2.fail()) {
    // files not available for comparison
    return true;
  }
  if (file1.tellg() != file2.tellg()) {
    match = false;  // sizes don't match
  } else {
    file1.seekg(0, std::ifstream::beg);
    file2.seekg(0, std::ifstream::beg);
    // compare content of both files
    match =
        std::equal(std::istreambuf_iterator<char>(file1.rdbuf()), std::istreambuf_iterator<char>(),
                   std::istreambuf_iterator<char>(file2.rdbuf()));
  }
  std::cout << "file 1: " << filename1 << " and file 2: " << filename2;
  if (match) {
    std::cout << " match : Success" << std::endl;
  } else {
    std::cout << " don't match : Fail" << std::endl;
  }
  std::cout << "--------------------------------------" << std::endl;

  return match;
}

inline void DeleteFile(const std::string &filename) {
  try {
    pti::utils::filesystem::remove(filename);
  } catch (const pti::utils::filesystem::filesystem_error &err) {
    std::cout << "filesystem error: " << err.what() << '\n';
  }
}

class MetricsProfiler {
 private:
  MetricsProfiler() {
    configured_device_handle_ = nullptr;
    configured_group_handle_ = nullptr;
    data_checked_ = false;
    data_valid_ = true;
  }

 public:
  MetricsProfiler(const MetricsProfiler &) = delete;
  MetricsProfiler &operator=(const MetricsProfiler &) = delete;
  MetricsProfiler(MetricsProfiler &&) = delete;
  MetricsProfiler &operator=(MetricsProfiler &&) = delete;

  ~MetricsProfiler() {
    devices_.clear();
    for (auto it = groups_.begin(); it != groups_.end(); it++) {
      free(it->second._metric_properties);
    }
    groups_.clear();
  }

  // Initializes the metric collection by dicovering devices the metrics
  // collection can be done on, the metric groups that each device supports
  // and metrics in each metric group
  // If log_metrics is true,
  //  then function will also log discovered devices, metric groups
  //  supported per device and metrics within each metric group.
  //  if file_name is specified, a file with specified name is used for logging
  //  if fine_name is empty, function will log to console
  bool InitializeMetricsCollection(bool log_metrics = false, std::string filename = std::string()) {
    // Step1: discover available supported devices
    uint32_t device_count = 0;
    pti_result result = ptiMetricsGetDevices(nullptr, &device_count);
    if (result != PTI_SUCCESS || device_count == 0) {
      std::cerr << "Failed to discover supportted devices" << std::endl;
      return false;
    }

    std::shared_ptr<spdlog::logger> logger = utils::GetLogStream(log_metrics, filename);

    std::vector<pti_device_properties_t> device_buffer(device_count);

    result = ptiMetricsGetDevices(device_buffer.data(), &device_count);
    if (result != PTI_SUCCESS || device_buffer.empty()) {
      std::cerr << "Failed to discover supportted devices" << std::endl;
      return false;
    }

    std::stringstream out;
    // Iterate through devices
    for (uint32_t i = 0; i < device_count; i++) {
      devices_[device_buffer[i]._handle] = device_buffer[i];
      out << "\nDEVICE(" << i << ")"
          << "-> handle: "
          << std::to_string((unsigned long long)(void **)(device_buffer[i]._handle))
          << " | model name: " << device_buffer[i]._model_name << " | dbdf: ["
          << +device_buffer[i]._address._domain << ":" << +device_buffer[i]._address._bus << ":"
          << +device_buffer[i]._address._device << ":" << +device_buffer[i]._address._function
          << "]";

      std::string uuid = samples_utils::stringify_uuid(device_buffer[i]._uuid, " | UUID: ");

      out << uuid << std::endl;

      // Step2: discover available metric groups
      uint32_t group_count = 0;
      result = ptiMetricsGetMetricGroups(device_buffer[i]._handle, nullptr, &group_count);
      if (result != PTI_SUCCESS || group_count == 0) {
        std::cerr << "Failed to discover metric groups on device:" << i << std::endl;
        continue;  // Try next device
      }

      std::vector<pti_metrics_group_properties_t> groups_buffer(group_count);

      result =
          ptiMetricsGetMetricGroups(device_buffer[i]._handle, groups_buffer.data(), &group_count);
      if (result != PTI_SUCCESS || group_count == 0 || groups_buffer.empty()) {
        std::cerr << "Failed to discover metric groups" << std::endl;
        continue;  // Try next device
      }

      // Iterate through metric groups
      for (uint32_t j = 0; j < group_count; j++) {
        out << "\t METRIC GROUP(" << j << ")"
            << "-> handle: "
            << std::to_string((unsigned long long)(void **)(groups_buffer[j]._handle))
            << " | name: " << groups_buffer[j]._name
            << " | description: " << groups_buffer[j]._description << std::endl
            << "\t\t | type: " << groups_buffer[j]._type << "["
            << GetGroupType(groups_buffer[j]._type) << "]"
            << " | metric count: " << groups_buffer[j]._metric_count
            << " | domain: " << groups_buffer[j]._domain << std::endl;

        // Step3: discover available metrics in metric group
        if (groups_buffer[i]._metric_count == 0) {
          std::cerr << "Failed to discover metrics in metric group:" << j << " on device:" << i
                    << std::endl;
          continue;  // Try next metric group
        }

        groups_buffer[j]._metric_properties = (pti_metric_properties_t *)malloc(
            sizeof(pti_metric_properties_t) * groups_buffer[j]._metric_count);

        result = ptiMetricsGetMetricsProperties(groups_buffer[j]._handle,
                                                groups_buffer[j]._metric_properties);

        if (result != PTI_SUCCESS || groups_buffer[j]._metric_properties == nullptr) {
          std::cerr << "Failed to discover metrics in metric group:" << j << " on device: " << i
                    << std::endl;
          free(groups_buffer[j]._metric_properties);
          continue;  // Try next metric group
        }
        groups_[groups_buffer[j]._handle] = groups_buffer[j];
        // Iterate through metrics
        for (uint32_t k = 0; k < groups_buffer[j]._metric_count; k++) {
          out << "\t\t\t METRIC(" << k << ")"
              << "-> handle: "
              << std::to_string(
                     (unsigned long long)(void **)(groups_buffer[j]._metric_properties[k]._handle))
              << " | name: " << groups_buffer[j]._metric_properties[k]._name
              << " | description: " << groups_buffer[j]._metric_properties[k]._description
              << std::endl
              << "\t\t\t\t | metric type: " << groups_buffer[j]._metric_properties[k]._metric_type
              << "[" << GetMetricType(groups_buffer[j]._metric_properties[k]._metric_type) << "]"
              << " | value type: " << groups_buffer[j]._metric_properties[k]._value_type << "["
              << GetMetricValueType(groups_buffer[j]._metric_properties[k]._value_type) << "]"
              << " | units: " << groups_buffer[j]._metric_properties[k]._units << std::endl;
        }
      }
    }
    logger->info(out.str());
    return true;
  }

  // configure the metric groups for collection
  //  if log_data is true,
  //  if filename is specified, then the library will log calculated data to the specificed file
  //  name when Calculate API is called if filename is not specified, the the library will log
  //  calculated data to console when calculate API is called
  bool ConfigureMetricGroups(std::string &group_name, pti_metrics_group_type group_type,
                             bool log_data = false, std::string filename = std::string()) {
    // Basic group info
    uint32_t group_count = 1;

    // Setup env vars
    if (log_data) {
      utils::SetEnv("PTI_LogToFile", "1");
      std::cout << "Environment variable PTI_LogToFile set to 1" << std::endl;

      if (filename != "") {
        utils::SetEnv("PTI_LogFileName", filename.c_str());
        std::cout << "Environment variable PTI_LogFileName set to " << filename << std::endl;
      }
    }

    // Get a device handle
    configured_device_handle_ = GetDevice(0);  // first device
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Can't find a supported device" << std::endl;
      return false;
    }

    // Get group handle
    configured_group_handle_ = FindMetricGroup(configured_device_handle_, group_name, group_type);
    pti_metrics_group_handle_t groups[] = {configured_group_handle_};
    if (groups[0] == nullptr) {
      std::cerr << "Can't find metric group on specified device" << std::endl;
      return false;
    }

    pti_metrics_group_collection_params_t config_collection_params;
    config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
    config_collection_params._group_handle = configured_group_handle_;
    config_collection_params._sampling_interval = 100000;   // ns
    config_collection_params._time_aggr_window = 10000000;  // ns
    std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {
        config_collection_params};
    if (ptiMetricsConfigureCollection(configured_device_handle_,
                                      config_collection_params_buffer.data(),
                                      group_count) != PTI_SUCCESS) {
      std::cerr << "Failed to configure collection" << std::endl;
      return false;
    }
    return true;
  }

  bool StartCollection() {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Start: Invalid device handle" << std::endl;
      return false;
    }

    if (ptiMetricsStartCollection(configured_device_handle_) != PTI_SUCCESS) {
      std::cerr << "Failed to start collection" << std::endl;
      return false;
    }
    return true;
  }

  bool StartCollectionPaused() {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "StartPaused: Invalid device handle" << std::endl;
      return false;
    }

    if (ptiMetricsStartCollectionPaused(configured_device_handle_) != PTI_SUCCESS) {
      std::cerr << "Failed to start collection in paused mode" << std::endl;
      return false;
    }
    return true;
  }

  // Not implemented yet
  bool PauseCollection() {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Pause: Invalid device handle" << std::endl;
      return false;
    }

    if (ptiMetricsPauseCollection(configured_device_handle_) != PTI_SUCCESS) {
      std::cerr << "Failed to pause collection" << std::endl;
      return false;
    }
    return true;
  }

  bool ResumeCollection() {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Resume: Invalid device handle" << std::endl;
      return false;
    }

    if (ptiMetricsResumeCollection(configured_device_handle_) != PTI_SUCCESS) {
      std::cerr << "Failed to resume collection" << std::endl;
      return false;
    }
    return true;
  }

  bool StopCollection() {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Stop: Invalid device handle" << std::endl;
      return false;
    }

    if (ptiMetricsStopCollection(configured_device_handle_) != PTI_SUCCESS) {
      std::cerr << "Failed to stop collection" << std::endl;
      return false;
    }
    return true;
  }

  // Get Calculated data from the collection run
  // If log_data is true,
  //  then function will also log calculated data.
  //  if file_name is specified, a file with specified name is used for logging
  //  if fine_name is empty, function will log to std out
  bool GetCalculatedData(bool log_data = false, std::string filename = std::string()) {
    if (configured_device_handle_ == nullptr) {
      std::cerr << "Failed to start collection" << std::endl;
      return false;
    }
    uint32_t metrics_values_count = 0;
    pti_result result = ptiMetricGetCalculatedData(
        configured_device_handle_, configured_group_handle_, nullptr, &metrics_values_count);
    if (result != PTI_SUCCESS || metrics_values_count == 0) {
      std::cerr << "Failed to get required buffer size to dump collected data on specified device"
                << std::endl;
      return false;
    }

    std::vector<pti_value_t> metrics_values_buffer(metrics_values_count);

    result = ptiMetricGetCalculatedData(configured_device_handle_, configured_group_handle_,
                                        metrics_values_buffer.data(), &metrics_values_count);

    if (result != PTI_SUCCESS || metrics_values_count == 0 || metrics_values_buffer.empty()) {
      std::cerr << "Failed to capture collected data" << std::endl;
      return false;
    }

    if (configured_group_handle_ == nullptr ||
        groups_.find(configured_group_handle_) == groups_.end()) {
      std::cerr << "Failed to process collected data" << std::endl;
      return false;
    }

    pti_metrics_group_properties_t collected_group = groups_[configured_group_handle_];

    if (collected_group._metric_properties == nullptr) {
      std::cerr << "Failed to process collected data: no metrics properties found in metric group"
                << std::endl;
      return false;
    }

    std::string group_name = collected_group._name;
    std::string str = "";
    uint32_t buffer_idx = 0;
    uint32_t buffer_idx_max = metrics_values_count - 1;
    uint64_t ts = 0;
    uint32_t metric_count = collected_group._metric_count;
    if (collected_group._type == PTI_METRIC_GROUP_TYPE_TRACE_BASED) {
      metric_count += 2;  // We have two added for start and end timestamps
    }

    std::shared_ptr<spdlog::logger> logger = utils::GetLogStream(log_data, filename);

    float activePercent = 0.0;
    float stallPercent = 0.0;
    float occupancyPercent = 0.0;
    bool busy = false;
    logger->info("{\n\t\"displayTimeUnit\": \"us\",\n\t\"traceEvents\": [");
    while ((buffer_idx + metric_count) <= buffer_idx_max) {
      if (buffer_idx != 0) str += ",";
      str += " {\n\t\t\"args\": {\n";
      activePercent = 0.0;
      stallPercent = 0.0;
      occupancyPercent = 0.0;
      busy = false;
      for (uint32_t i = 0; i < metric_count; i++) {
        std::string metric_name;
        std::string units;
        pti_metric_value_type type;
        if (collected_group._type == PTI_METRIC_GROUP_TYPE_TRACE_BASED) {
          // for traced metric groups, first two values in the buffer are the
          // start and end timestamps
          if (i == 0) {
            metric_name = "StartTimestamp";
            units = "us";
            type = PTI_METRIC_VALUE_TYPE_UINT64;
          } else if (i == 1) {
            metric_name = "StopTimestamp";
            units = "us";
            type = PTI_METRIC_VALUE_TYPE_UINT64;
          } else {
            // Metric descriptions in the metrics properties buffer don't include
            // the start and end timestamps
            metric_name = collected_group._metric_properties[i - 2]._name;
            units = collected_group._metric_properties[i - 2]._units;
            type = collected_group._metric_properties[i - 2]._value_type;
          }
        } else {
          metric_name = collected_group._metric_properties[i]._name;
          units = collected_group._metric_properties[i]._units;
          type = collected_group._metric_properties[i]._value_type;
        }
        if (units == "percent") units = "%";
        if (!units.empty() && units != "(null)") {
          metric_name += "[" + units + "]";
        }
        pti_value_t value = metrics_values_buffer[buffer_idx++];
        if (metric_name.find("QueryBeginTime") != std::string::npos ||
            metric_name.find("StartTimestamp") != std::string::npos) {  // Added for trace groups
          ts = value.ui64;
          continue;
        }
        if (metric_name.find("StopTimestamp") != std::string::npos) {
          value.ui64 = value.ui64 / 1000;  // convert to us
        }
        if (i != 0) str += ",\n";
        str += "\t\t\t\"" + metric_name + "\": " + PrintTypedValue(value, type);

        // data validation
        if (metric_name.find("XVE_STALL") != std::string::npos) {
          stallPercent = static_cast<float>(value.fp32);
        } else if (metric_name.find("XVE_ACTIVE") != std::string::npos) {
          activePercent = static_cast<float>(value.fp32);
        } else if (metric_name.find("OCCUPANCY_ALL") != std::string::npos) {
          occupancyPercent = static_cast<float>(value.fp32);
        } else if (metric_name.find("XVE_BUSY") != std::string::npos) {
          busy = static_cast<bool>(value.ui64);
        }
      }

      // check that STALL % + ACTIVE % ~= OCCUPENCY % when OCCUPENCY %~= 100%+/-0.5%
      if (busy) {
        data_checked_ = true;
        if (occupancyPercent < 100.5 && occupancyPercent > 99.5) {
          if ((activePercent + stallPercent) > 99.5 && (activePercent + stallPercent) < 100.5) {
            std::cout << "PASS: DATA VALID: ";
          } else {
            std::cout << "FAIL: DATA INVALID: ";
            data_valid_ = false;
          }
          std::cout << std::fixed << std::setprecision(2) << "active percent:" << activePercent
                    << "% + stall percent:" << stallPercent
                    << "% ~= occupancy percent:" << occupancyPercent << "%" << std::endl;
        }
      }

      str += "\n\t\t\t},\n";
      str += "\t\t\t\"cat\": \"" + group_name + "\",\n";
      str += "\t\t\t\"name\": \"" + group_name + "\",\n";
      str += "\t\t\t\"ph\": \"C\",\n";
      str += "\t\t\t\"pid\": 0,\n";
      str += "\t\t\t\"tid\": 0,\n";
      str += "\t\t\t\"ts\": " + std::to_string(ts / 1000) + "\n";
      str += "\t\t}";
    }

    logger->info(str);
    logger->info("\n\t]\n}\n");
    return true;
  }

  bool ValidateData() {
    if (data_checked_) {
      std::cout << "Data validity check";
      if (data_valid_) {
        std::cout << " : Success";
      } else {
        std::cout << " : Fail";
      }
      std::cout << std::endl << "--------------------------------------" << std::endl;
    }
    return data_checked_ ? data_valid_ : true;
  }

  bool ValidateDeviceUUID(uint8_t *uuid) {
    if (devices_.find(configured_device_handle_) == devices_.end()) {
      std::cout << "ERROR: can't find configured device" << std::endl;
      return false;
    }

    auto device_uuid = devices_[configured_device_handle_]._uuid;
    for (uint32_t i = 0; i < PTI_MAX_DEVICE_UUID_SIZE; i++) {
      if (device_uuid[i] != uuid[i]) {
        std::cout
            << "Device used for metric data collection and the compute device are not equivalent"
            << "Metric device: " << samples_utils::stringify_uuid(device_uuid, " | UUID: ")
            << "Compute device: " << samples_utils::stringify_uuid(uuid, " | UUID: ") << " : Fail"
            << std::endl
            << "--------------------------------------" << std::endl;
        return false;
      }
    }
    std::cout << "Device used for metric data collection and the compute device are equivalent"
              << samples_utils::stringify_uuid(uuid, " | UUID: ") << " : Success" << std::endl
              << "--------------------------------------" << std::endl;
    return true;
  }

 private:
  std::unordered_map<pti_device_handle_t, pti_device_properties_t> devices_;
  std::unordered_map<pti_metrics_group_handle_t, pti_metrics_group_properties_t> groups_;
  pti_metrics_group_handle_t configured_group_handle_;
  pti_device_handle_t configured_device_handle_;
  bool data_checked_;
  bool data_valid_;

 public:
  inline static auto &MetricsProfilerInstance() {
    static MetricsProfiler metrics_profiler{};
    return metrics_profiler;
  }
};
}  // namespace metrics_utils
#endif
