//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// ISO3DFD: Data Parallel C++ Language Basics Using 3D-Finite-Difference-Wave
// Propagation
//
// ISO3DFD is a finite difference stencil kernel for solving the 3D acoustic
// isotropic wave equation. Kernels in this sample are implemented as 16th order
// in space, 2nd order in time scheme without boundary conditions. Using Data
// Parallel C++, the sample can explicitly run on the GPU and/or CPU to
// calculate a result.  If successful, the output will print the device name
// where the SYCL code ran along with the grid computation metrics - flops
// and effective throughput
//
// For comprehensive instructions regarding SYCL Programming, go to
// https://software.intel.com/en-us/oneapi-programming-guide
// and search based on relevant terms noted in the comments.
//
// SYCL material used in this code sample:
//
// SYCL Queues (including device selectors and exception handlers)
// SYCL Custom device selector
// SYCL Buffers and accessors (communicate data between the host and the
// device)
// SYCL Kernels (including parallel_for function and nd-range<3>
// objects)
// Shared Local Memory (SLM) optimizations (SYCL)
// SYCL Basic synchronization (barrier function)
//
#ifdef __clang__
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#include <dpc_common.hpp>

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>

#include <algorithm>
#include <random>
#include <iostream>
#include <string>
#include <mutex>

#include "iso3dfd.h"
#include "device_selector.hpp"
#include "pti/pti_metrics.h"
#include "samples_utils.h"

namespace oneapi {}
using namespace oneapi;

std::mutex global_cout_mtx;

class MetricsProfiler {
 public:

MetricsProfiler(const MetricsProfiler &) = delete;
MetricsProfiler &operator=(const MetricsProfiler &) = delete;
MetricsProfiler(MetricsProfiler &&) = delete;
MetricsProfiler &operator=(MetricsProfiler &&) = delete;

MetricsProfiler() {
  configured_device_handle_ = NULL;
  configured_group_handle_ = NULL;
}

~MetricsProfiler() {
  devices_.clear();
  for (auto it = groups_.begin(); it != groups_.end(); it++) {
    free(it->second._metric_properties);
  }
  groups_.clear();
}

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

inline void SetEnv(const char* name, const char* value) {
  if(name == nullptr || value == nullptr){
    exit(-1);
  }

  int status = 0;
#if defined(_WIN32)
  std::string str = std::string(name) + "=" + value;
  status = _putenv(str.c_str());
#else
  status = setenv(name, value, 1);
#endif
  if (status != 0) {
    exit(-1);
  }
}

pti_device_handle_t GetDevice(uint32_t deviceId) {
  // Step1: discover available supported device count
  uint32_t device_count = 0;
  pti_result result = ptiMetricsGetDevices(NULL, &device_count);
  if (result != PTI_SUCCESS || device_count == 0) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    return NULL; // exit function
  }

  // Step2: get available devices in device_buffer
  std::vector<pti_device_properties_t> device_buffer(device_count);

  result = ptiMetricsGetDevices(device_buffer.data(), &device_count);
  if (result != PTI_SUCCESS || device_buffer.empty()) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    return NULL; // exit function
  }

  pti_device_handle_t device_handle = NULL;
  if(deviceId < device_count) {
    device_handle = device_buffer[deviceId]._handle;
  }

  return device_handle;
}

pti_metrics_group_handle_t FindMetricGroup(pti_device_handle_t HDevice, std::string& groupName, pti_metrics_group_type groupType) {

  if(HDevice == NULL) {
    return NULL;
  }

  uint32_t group_count = 0;
  pti_result result = ptiMetricsGetMetricGroups(HDevice, NULL, &group_count);
  if (result != PTI_SUCCESS || group_count == 0) {
    std::cerr << "Failed to discover metric groups on specified device:" << std::endl;
    return NULL;
  }

  std::vector<pti_metrics_group_properties_t> groups_buffer(group_count);

  result = ptiMetricsGetMetricGroups(HDevice, groups_buffer.data(), &group_count);
  if (result != PTI_SUCCESS || group_count == 0 || groups_buffer.empty()) {
    std::cerr << "Failed to discover metric groups" << std::endl;
    return NULL;
  }

  pti_metrics_group_handle_t group_handle = NULL;
  // Iterate through metric groups
  for (uint32_t i = 0; i < group_count; i++) {
    if (groupName == groups_buffer[i]._name && groups_buffer[i]._type == groupType) {
      group_handle = groups_buffer[i]._handle;
      break;
    }
  }

  return group_handle;
}

void PrintAvailableMetrics() {

  // Step1: discover available supported devices
  uint32_t device_count = 0;
  pti_result result = ptiMetricsGetDevices(NULL, &device_count);
  if (result != PTI_SUCCESS || device_count == 0) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    exit(-1); // exit
  }

  std::ofstream out("available_metrics.txt", std::ios::trunc);
  if (!out.is_open()) {
    std::cerr << "Failed to open file stream" << std::endl;
    return; // exit function
  }

  std::vector<pti_device_properties_t> device_buffer(device_count);

  result = ptiMetricsGetDevices(device_buffer.data(), &device_count);
  if (result != PTI_SUCCESS || device_buffer.empty()) {
    std::cerr << "Failed to discover supportted devices" << std::endl;
    exit(-1); // exit
  }

  // Iterate through devices
  for (uint32_t i = 0; i < device_count; i++) {
    devices_[device_buffer[i]._handle] = device_buffer[i];
    out << "\nDEVICE(" << i << ")"
              << "-> handle: " << std::to_string((unsigned long long)(void**)(device_buffer[i]._handle))
              << " | model name: " << device_buffer[i]._model_name
              << " | dbdf: [" << +device_buffer[i]._address._domain
              << ":" << +device_buffer[i]._address._bus
              << ":" << +device_buffer[i]._address._device
              << ":" << +device_buffer[i]._address._function
              << "]";
    std::string uuid = samples_utils::stringify_uuid(device_buffer[i]._uuid, " | UUID: ");
    out << uuid << std::endl;

    // Step2: discover available metric groups
    uint32_t group_count = 0;
    result = ptiMetricsGetMetricGroups(device_buffer[i]._handle, NULL, &group_count);
    if (result != PTI_SUCCESS || group_count == 0) {
      std::cerr << "Failed to discover metric groups on device:" << i << std::endl;
      continue; // Try next device
    }

    std::vector<pti_metrics_group_properties_t> groups_buffer(group_count);

    result = ptiMetricsGetMetricGroups(device_buffer[i]._handle, groups_buffer.data(), &group_count);
    if (result != PTI_SUCCESS || group_count == 0 || groups_buffer.empty()) {
      std::cerr << "Failed to discover metric groups" << std::endl;
      continue; // Try next device
    }

    // Iterate through metric groups
    for (uint32_t j = 0; j < group_count; j++) {
      out << "\t METRIC GROUP(" << j << ")"
              << "-> handle: " << std::to_string((unsigned long long)(void**)(groups_buffer[j]._handle))
              << " | name: " << groups_buffer[j]._name
              << " | description: " << groups_buffer[j]._description << std::endl
              << "\t\t | type: " << groups_buffer[j]._type << "["
              << GetGroupType(groups_buffer[j]._type)
              << "]"
              << " | metric count: " << groups_buffer[j]._metric_count
              << " | domain: " << groups_buffer[j]._domain
              << std::endl;

      // Step3: discover available metrics in metric group
      if (groups_buffer[i]._metric_count == 0) {
        std::cerr << "Failed to discover metrics in metric group:" << j << " on device:" << i << std::endl;
        continue; // Try next metric group
      }

      groups_buffer[j]._metric_properties = (pti_metric_properties_t *)malloc(
        sizeof(pti_metric_properties_t) * groups_buffer[j]._metric_count);

      result = ptiMetricsGetMetricsProperties(groups_buffer[j]._handle, groups_buffer[j]._metric_properties);

      if (result != PTI_SUCCESS || groups_buffer[j]._metric_properties == NULL) {
        std::cerr << "Failed to discover metrics in metric group:" << j << " on device: "<< i <<  std::endl;
        free(groups_buffer[j]._metric_properties);
        continue; // Try next metric group
      }
      groups_[groups_buffer[j]._handle] = groups_buffer[j];
      // Iterate through metrics
      for (uint32_t k = 0; k < groups_buffer[j]._metric_count; k++) {
        out << "\t\t\t METRIC(" << k << ")"
              << "-> handle: " << std::to_string((unsigned long long)(void**)(groups_buffer[j]._metric_properties[k]._handle))
              << " | name: " << groups_buffer[j]._metric_properties[k]._name
              << " | description: " << groups_buffer[j]._metric_properties[k]._description << std::endl
              << "\t\t\t\t | metric type: " << groups_buffer[j]._metric_properties[k]._metric_type << "["
              << GetMetricType(groups_buffer[j]._metric_properties[k]._metric_type)
              << "]"
              << " | value type: " << groups_buffer[j]._metric_properties[k]._value_type << "["
              << GetMetricValueType(groups_buffer[j]._metric_properties[k]._value_type)
              << "]"
              << " | units: " << groups_buffer[j]._metric_properties[k]._units
              << std::endl;
      }
    }
  }
  out.close();
}

void ConfigureMetricGroups(std::string &group_name, pti_metrics_group_type group_type) {

  // Basic group info
  uint32_t group_count = 1;

  // Setup env vars
  SetEnv("PTI_LogToFile", "1");
  std::string logfilename(group_name);
  logfilename += "_iso3dfd_pti_metric_log.json";
  SetEnv("PTI_LogFileName", logfilename.c_str());

  // Get a device handle
  configured_device_handle_ = GetDevice(0); // first device
  if (configured_device_handle_ == NULL) {
    std::cerr << "Can't find a supported device" << std::endl;
    exit(-1);
  }

  // Get group handle
  configured_group_handle_ = FindMetricGroup(configured_device_handle_, group_name, group_type);
  pti_metrics_group_handle_t groups[] = {configured_group_handle_};
  if (groups[0] == NULL) {
    std::cerr << "Can't find metric group on specified device" << std::endl;
    exit(-1);
  }

  pti_metrics_group_collection_params_t config_collection_params;
  config_collection_params._struct_size = sizeof(pti_metrics_group_collection_params_t);
  config_collection_params._group_handle = configured_group_handle_;
  config_collection_params._sampling_interval = 1000000; // ns
  config_collection_params._time_aggr_window = 10000000; // ns
  std::vector<pti_metrics_group_collection_params_t> config_collection_params_buffer = {config_collection_params};
  if (ptiMetricsConfigureCollection(configured_device_handle_, config_collection_params_buffer.data(), group_count) != PTI_SUCCESS)
    exit(-1);
}

void StartCollection() {
  if(configured_device_handle_ == NULL || ptiMetricsStartCollection(configured_device_handle_) != PTI_SUCCESS)
    exit(-1);
}

void StartCollectionPaused() {
  if(configured_device_handle_ == NULL || ptiMetricsStartCollectionPaused(configured_device_handle_) != PTI_SUCCESS)
    exit(-1);
}
/* Not implemented yet
void PauseCollection() {
  if(configured_device_handle_ == NULL || ptiMetricsPauseCollection(configured_device_handle_) != PTI_SUCCESS)
    exit(-1);
}
*/
void ResumeCollection() {
  if(configured_device_handle_ == NULL || ptiMetricsResumeCollection(configured_device_handle_) != PTI_SUCCESS)
    exit(-1);
}

void StopCollection() {
  if(configured_device_handle_ == NULL || ptiMetricsStopCollection(configured_device_handle_) != PTI_SUCCESS)
    exit(-1);
}

static std::string PrintTypedValue(const pti_value_t value, pti_metric_value_type type) {
  switch (type) {
    case PTI_METRIC_VALUE_TYPE_UINT32:
      return std::to_string(value.ui32);
    case PTI_METRIC_VALUE_TYPE_UINT64:
      return std::to_string(value.ui64);
    case PTI_METRIC_VALUE_TYPE_FLOAT32:
      return std::to_string(value.fp32);
    case PTI_METRIC_VALUE_TYPE_FLOAT64:
      return std::to_string(value.fp64);
    case PTI_METRIC_VALUE_TYPE_BOOL8:
      return std::to_string(static_cast<uint32_t>(value.b8));
    default:
      break;
  }
  return "UNKNOWN";
}

void GetCalculatedData() {
  if(configured_device_handle_ == NULL) {
    exit(-1);
  }
  uint32_t metrics_values_count = 0;
  pti_result result = ptiMetricGetCalculatedData(configured_device_handle_, configured_group_handle_, NULL, &metrics_values_count);
  if (result != PTI_SUCCESS || metrics_values_count == 0) {
    std::cerr << "Failed to get required buffer size to dump collected data on specified device" <<  std::endl;
    exit(-1);
  }

  std::vector<pti_value_t> metrics_values_buffer(metrics_values_count);

  result = ptiMetricGetCalculatedData(configured_device_handle_, configured_group_handle_, metrics_values_buffer.data(), &metrics_values_count);

  if (result != PTI_SUCCESS || metrics_values_count == 0 || metrics_values_buffer.empty()) {
    std::cerr << "Failed to capture collected data" << std::endl;
    exit(-1);
  }

  if (configured_group_handle_ == NULL || groups_.find(configured_group_handle_) == groups_.end()) {
    std::cerr << "Failed to process collected data" << std::endl;
    exit(-1);
  }

  pti_metrics_group_properties_t collected_group = groups_[configured_group_handle_];

  if (collected_group._metric_properties == NULL) {
    std::cerr << "Failed to process collected data: no metrics properties found in metric group" << std::endl;
    exit(-1);
  }

  std::string group_name = collected_group._name;
  std::string str = "";
  uint32_t buffer_idx = 0;
  uint32_t buffer_idx_max = metrics_values_count - 1;
  uint64_t ts = 0;
  uint32_t metric_count = collected_group._metric_count;
  if(collected_group._type == PTI_METRIC_GROUP_TYPE_TRACE_BASED) {
    metric_count += 2; // We have two added for start and end timestamps
  }
  std::string filename = group_name + "_iso3dfd_pti_metric_log_sample.json";
  std::shared_ptr<spdlog::logger> logger;
  try {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    std::random_device rand_dev;    // uniformly-distributed integer random number generator
    std::mt19937 prng(rand_dev());  // pseudorandom number generator
    std::uniform_int_distribution<uint64_t> rand_num(0);  // random number

    logger = std::make_shared<spdlog::logger>("file_logger" + fmt::format("{:x}", rand_num(prng)), file_sink);
    auto format = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, std::string(""));  // disable eol
    logger->set_formatter(std::move(format));
    spdlog::register_logger(logger);
  } catch (const spdlog::spdlog_ex &exception) {
      std::cerr << "Failed to initialize log file: " << exception.what() << std::endl;
  }

  logger->info("{\n\t\"displayTimeUnit\": \"us\",\n\t\"traceEvents\": [");
  while ((buffer_idx + metric_count ) <= buffer_idx_max) {
    if (buffer_idx != 0) str += ",";
    str += " {\n\t\t\"args\": {\n";
    for (uint32_t i = 0 ; i < metric_count; i++) {
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
          metric_name = collected_group._metric_properties[i-2]._name;
          units = collected_group._metric_properties[i-2]._units;
          type = collected_group._metric_properties[i-2]._value_type;
        }
      } else {
        metric_name = collected_group._metric_properties[i]._name;
        units = collected_group._metric_properties[i]._units;
        type = collected_group._metric_properties[i]._value_type;
      }
      if (units == "percent" ) units = "%";
      if (!units.empty() && units != "(null)") {
        metric_name += "[" + units + "]";
      }
      pti_value_t value = metrics_values_buffer[buffer_idx++];
      if (metric_name.find("QueryBeginTime") != std::string::npos ||
          metric_name.find("StartTimestamp") != std::string::npos) { // Added for trace groups
        ts = value.ui64;
        continue;
      }
      if (metric_name.find("StopTimestamp") != std::string::npos) {
        value.ui64 = value.ui64 / 1000; // convert to us
      }
      if (i !=0) str += ",\n";
      str += "\t\t\t\"" + metric_name + "\": " + PrintTypedValue(value, type);
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
/*  std::cout << "Value count: " << metrics_values_count
            << " | buffer idx max: " << buffer_idx_max
            << " | buffer idx: " << buffer_idx
            << std::endl;
*/
  logger->info(str);
  logger->info("\n\t]\n}\n");
}

void ValidateDeviceUUID(uint8_t* uuid)
{
  if (devices_.find(configured_device_handle_) == devices_.end()) {
    std::cout << "ERROR: can't find configured device"
               << std::endl;
    return;
  }

  auto device_uuid = devices_[configured_device_handle_]._uuid;
  for (uint32_t i = 0; i < PTI_MAX_DEVICE_UUID_SIZE; i++) {
    if(device_uuid[i] != uuid[i]) {
      std::cout << "Device used for metric data collection and the compute device are not equivalent"
                << "Metric device: " << samples_utils::stringify_uuid(device_uuid, " | UUID: ")
                << "Compute device: " << samples_utils::stringify_uuid(uuid, " | UUID: ")
                << " : Fail"
                << std::endl
                << "--------------------------------------"
                << std::endl;
      return;
    }
  }
  std::cout << "Device used for metric data collection and the compute device are equivalent"
            << samples_utils::stringify_uuid(uuid, " | UUID: ")
            << " : Success"
            << std::endl
            << "--------------------------------------"
            << std::endl;
  return;
}

private:
 std::unordered_map<pti_device_handle_t, pti_device_properties_t> devices_;
 std::unordered_map<pti_metrics_group_handle_t, pti_metrics_group_properties_t> groups_;
 pti_metrics_group_handle_t configured_group_handle_;
 pti_device_handle_t configured_device_handle_;
};

/*
 * Host-Code
 * Function used for initialization
 */
void Initialize(float* ptr_prev, float* ptr_next, float* ptr_vel, size_t n1,
                size_t n2, size_t n3) {
  {
    const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
    std::cout << "Initializing ... \n";
  }
  size_t dim2 = n2 * n1;

  for (size_t i = 0; i < n3; i++) {
    for (size_t j = 0; j < n2; j++) {
      size_t offset = i * dim2 + j * n1;
#pragma omp simd
      for (size_t k = 0; k < n1; k++) {
        ptr_prev[offset + k] = 0.0f;
        ptr_next[offset + k] = 0.0f;
        ptr_vel[offset + k] =
            2250000.0f * dt * dt;  // Integration of the v*v and dt*dt
      }
    }
  }
  // Add a source to initial wavefield as an initial condition
  float val = 1.f;
  for (int s = 5; s >= 0; s--) {
    for (size_t i = n3 / 2 - s; i < n3 / 2 + s; i++) {
      for (size_t j = n2 / 4 - s; j < n2 / 4 + s; j++) {
        size_t offset = i * dim2 + j * n1;
        for (size_t k = n1 / 4 - s; k < n1 / 4 + s; k++) {
          ptr_prev[offset + k] = val;
        }
      }
    }
    val *= 10;
  }
}

/*
 * Host-Code
 * OpenMP implementation for single iteration of iso3dfd kernel.
 * This function is used as reference implementation for verification and
 * also to compare performance of OpenMP and SYCL on CPU
 * Additional Details:
 * https://software.intel.com/en-us/articles/eight-optimizations-for-3-dimensional-finite-difference-3dfd-code-with-an-isotropic-iso
 */
void Iso3dfdIteration(float* ptr_next_base, float* ptr_prev_base,
                      float* ptr_vel_base, float* coeff, const size_t n1,
                      const size_t n2, const size_t n3, const size_t n1_block,
                      const size_t n2_block, const size_t n3_block) {
  size_t dimn1n2 = n1 * n2;
  size_t n3End = n3 - kHalfLength;
  size_t n2End = n2 - kHalfLength;
  size_t n1End = n1 - kHalfLength;

#pragma omp parallel default(shared)
#pragma omp for schedule(static) collapse(3)
  for (size_t bz = kHalfLength; bz < n3End;
       bz += n3_block) {  // start of cache blocking
    for (size_t by = kHalfLength; by < n2End; by += n2_block) {
      for (size_t bx = kHalfLength; bx < n1End; bx += n1_block) {
        size_t izEnd = (std::min)(bz + n3_block, n3End);
        size_t iyEnd = (std::min)(by + n2_block, n2End);
        size_t ixEnd = (std::min)(n1_block, n1End - bx);
        for (size_t iz = bz; iz < izEnd; iz++) {  // start of inner iterations
          for (size_t iy = by; iy < iyEnd; iy++) {
            float* ptr_next = ptr_next_base + iz * dimn1n2 + iy * n1 + bx;
            float* ptr_prev = ptr_prev_base + iz * dimn1n2 + iy * n1 + bx;
            float* ptr_vel = ptr_vel_base + iz * dimn1n2 + iy * n1 + bx;
#pragma omp simd
            for (size_t ix = 0; ix < ixEnd; ix++) {
              float value = 0.0;
              value += ptr_prev[ix] * coeff[0];
#pragma unroll(kHalfLength)
              for (unsigned int ir = 1; ir <= kHalfLength; ir++) {
                value += coeff[ir] *
                         ((ptr_prev[ix + ir] + ptr_prev[ix - ir]) +
                          (ptr_prev[ix + ir * n1] + ptr_prev[ix - ir * n1]) +
                          (ptr_prev[ix + ir * dimn1n2] +
                           ptr_prev[ix - ir * dimn1n2]));
              }
              ptr_next[ix] =
                  2.0f * ptr_prev[ix] - ptr_next[ix] + value * ptr_vel[ix];
            }
          }
        }  // end of inner iterations
      }
    }
  }  // end of cache blocking
}

/*
 * Host-Code
 * Driver function for ISO3DFD OpenMP code
 * Uses ptr_next and ptr_prev as ping-pong buffers to achieve
 * accelerated wave propogation
 */
void Iso3dfd(float* ptr_next, float* ptr_prev, float* ptr_vel, float* coeff,
             const size_t n1, const size_t n2, const size_t n3,
             const unsigned int nreps, const size_t n1_block,
             const size_t n2_block, const size_t n3_block) {
  for (unsigned int it = 0; it < nreps; it += 1) {
    Iso3dfdIteration(ptr_next, ptr_prev, ptr_vel, coeff, n1, n2, n3, n1_block,
                     n2_block, n3_block);

    // here's where boundary conditions and halo exchanges happen
    // Swap previous & next between iterations
    it++;
    if (it < nreps)
      Iso3dfdIteration(ptr_prev, ptr_next, ptr_vel, coeff, n1, n2, n3, n1_block,
                       n2_block, n3_block);
  }  // time loop
}

/*
 * Host-Code
 * Main function to drive the sample application
 */
int main(int argc, char* argv[]) {
  // Arrays used to update the wavefield
  float* prev_base;
  float* next_base;
  // Array to store wave velocity
  float* vel_base;
  // Array to store results for comparison
  float* temp;

  bool sycl = true;
  bool omp = true;
  bool error = false;
  bool is_gpu = false;
  bool is_cpu = false;

  size_t n1, n2, n3;
  size_t n1_block, n2_block, n3_block;
  unsigned int num_iterations;
  MetricsProfiler* metrics_profiler = new MetricsProfiler();
  metrics_profiler->PrintAvailableMetrics();

  // TIME metric groups
  std::string group_name = /*"GpuOffload"*/ "ComputeBasic" /* "MemProfile" "DataportProfile" "L1ProfileReads" "L1ProfileSlmBankConflicts" "L1ProfileWrites" */;

  // TRACE metric groups
  //std::string group_name = "tpcs_utilization_and_bw" /* "nic_stms" "dcore0_bmons_bw"*/;
  pti_metrics_group_type group_type = /*PTI_METRIC_GROUP_TYPE_TRACE_BASED*/ PTI_METRIC_GROUP_TYPE_TIME_BASED;

  metrics_profiler->ConfigureMetricGroups(group_name, group_type);
  metrics_profiler->StartCollectionPaused();

  auto dev = sycl::device(sycl::gpu_selector_v);
  uint8_t uuid[16];
  bool sycl_device_has_uuid = false;

  // Read Input Parameters
  try {
    n1 = std::stoi(argv[1]) + (2 * kHalfLength);
    n2 = std::stoi(argv[2]) + (2 * kHalfLength);
    n3 = std::stoi(argv[3]) + (2 * kHalfLength);
    n1_block = std::stoi(argv[4]);
    n2_block = std::stoi(argv[5]);
    n3_block = std::stoi(argv[6]);
    num_iterations = std::stoi(argv[7]);
  }

  catch (...) {
    Usage(argv[0]);
    return 1;
  }

  // Read optional arguments to select version and device
  for (auto arg = 8; arg < argc; arg++) {
    std::string arg_value = argv[arg];
    std::transform(arg_value.begin(), arg_value.end(), arg_value.begin(), ::tolower);

    if (arg_value == "omp") {
      omp = true;
      sycl = false;
    } else if (arg_value == "sycl") {
      omp = false;
      sycl = true;
    } else if (arg_value == "gpu") {
      is_gpu = true;
      is_cpu = false;
    } else if (arg_value == "cpu") {
      is_cpu = true;
      is_gpu = false;
    } else {
      Usage(argv[0]);
      return 1;
    }
  }

  // Validate input sizes for the grid and block dimensions
  if (CheckGridDimension(n1 - 2 * kHalfLength, n2 - 2 * kHalfLength,
                         n3 - 2 * kHalfLength, n1_block, n2_block, n3_block)) {
    Usage(argv[0]);
    return 1;
  }

  // Compute the total size of grid
  size_t nsize = n1 * n2 * n3;

  try {
    prev_base = new float[nsize];
    next_base = new float[nsize];
    vel_base = new float[nsize];
  } catch (const std::bad_alloc& e) {
    std::cerr << "Error: While attempting to allocate space for grid, caught exception: " << e.what() <<"."  <<  '\n';
    return 1;
  }

  // Compute coefficients to be used in wavefield update
  float coeff[kHalfLength + 1] = {-3.0548446,   +1.7777778,     -3.1111111e-1,
                                  +7.572087e-2, -1.76767677e-2, +3.480962e-3,
                                  -5.180005e-4, +5.074287e-5,   -2.42812e-6};

  // Apply the DX DY and DZ to coefficients
  coeff[0] = (3.0f * coeff[0]) / (dxyz * dxyz);
  for (unsigned int i = 1; i <= kHalfLength; i++) {
    coeff[i] = coeff[i] / (dxyz * dxyz);
  }

  {
    const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
    std::cout << "Grid Sizes: " << n1 - 2 * kHalfLength << " "
              << n2 - 2 * kHalfLength << " " << n3 - 2 * kHalfLength << "\n";
    std::cout << "Memory Usage: " << ((3 * nsize * sizeof(float)) / (1024 * 1024))
              << " MB\n";
  }

  // Check if running OpenMP OR Serial version on CPU
  if (omp) {
    {
      const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
#if defined(_OPENMP)
      std::cout << " ***** Running OpenMP variant *****\n";
#else
      std::cout << " ***** Running C++ Serial variant *****\n";
#endif
    }

    // Initialize arrays and introduce initial conditions (source)
    Initialize(prev_base, next_base, vel_base, n1, n2, n3);

    // Start timer
    dpc_common::TimeInterval t_ser;
    // Invoke the driver function to perform 3D wave propogation
    // using OpenMP/Serial version
    Iso3dfd(next_base, prev_base, vel_base, coeff, n1, n2, n3, num_iterations,
            n1_block, n2_block, n3_block);

    // End timer
    PrintStats(t_ser.Elapsed() * 1e3, n1, n2, n3, num_iterations);
  }

  // Check if running both OpenMP/Serial and SYCL version
  // Keeping a copy of output buffer from OpenMP version
  // for comparison
  if (omp && sycl) {
    temp = new float[nsize];
    if (num_iterations % 2)
      memcpy(temp, next_base, nsize * sizeof(float));
    else
      memcpy(temp, prev_base, nsize * sizeof(float));
  }

  // Check if running SYCL version
  if (sycl) {
    try {
      {
        const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
        std::cout << " ***** Running SYCL variant *****\n";
      }
      // Initialize arrays and introduce initial conditions (source)
      Initialize(prev_base, next_base, vel_base, n1, n2, n3);

      sycl::device dev;
      // using the correct sycl device selector
      if (is_gpu) {
        dev = sycl::device(sycl::gpu_selector_v);
      } else if (is_cpu) {
        dev = sycl::device(sycl::cpu_selector_v);
      } else {
        std::cerr << "Using the default sycl device selector";
        dev = sycl::device(sycl::default_selector_v);
      }

      // Create a device queue using SYCL class queue with the
      // device selector
      queue q(dev);
      auto device = q.get_device();
      if (device.has(aspect::ext_intel_device_info_uuid)) {
        auto sycl_uuid = device.get_info<ext::intel::info::device::uuid>();
        for (int i = 0; i < 16; i++) {
          uuid[i] = static_cast<uint8_t>(sycl_uuid[i]);
        }
        sycl_device_has_uuid = true;
      } else {
        sycl_device_has_uuid = false;
      }

      // Validate if the block sizes selected are
      // within range for the selected SYCL device
      if (CheckBlockDimension(q, n1_block, n2_block)) {
        Usage(argv[0]);
        return 1;
      }

      // Start timer
      dpc_common::TimeInterval t_dpc;

      // Collection started in paused mode, resume
      metrics_profiler->ResumeCollection();

      // Invoke the driver function to perform 3D wave propogation
      // using SYCL version on the selected device
      Iso3dfdDevice(q, next_base, prev_base, vel_base, coeff, n1, n2, n3,
                    n1_block, n2_block, n3_block, n3 - kHalfLength,
                    num_iterations);
      // Wait for the commands to complete. Enforce synchronization on the command
      // queue
      q.wait_and_throw();

      // End timer
      PrintStats(t_dpc.Elapsed() * 1e3, n1, n2, n3, num_iterations);
    } catch (const sycl::exception &e) {
      std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
      std::cerr << "\tError code: " << e.code().value()
                << "\n\tCategory: " << e.category().name()
                << "\n\tMessage: " << e.code().message() << '\n';
    } catch (const std::exception &e) {
      std::cerr << "Error: Exception caught " << e.what() << '\n';
    } catch (...) {
      std::cerr << "Error: Unknown exception caught." << '\n';
    }
  }

  // If running both OpenMP/Serial and SYCL version
  // Comparing results
  if (omp && sycl) {
    if (num_iterations % 2) {
      error = WithinEpsilon(next_base, temp, n1, n2, n3, kHalfLength, 0, 0.1f);
    } else {
      error = WithinEpsilon(prev_base, temp, n1, n2, n3, kHalfLength, 0, 0.1f);
    }
    if (error) {
      std::cerr << "Final wavefields from SYCL device and CPU are not "
                << "equivalent: Fail\n";
    } else {
      const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
      std::cout << "Final wavefields from SYCL device and CPU are equivalent:"
                << " Success\n";
    }
    {
      const std::lock_guard<std::mutex> cout_lock(global_cout_mtx);
      std::cout << "--------------------------------------\n";
    }
    delete[] temp;
  }

  delete[] prev_base;
  delete[] next_base;
  delete[] vel_base;

  metrics_profiler->StopCollection();
  metrics_profiler->GetCalculatedData();

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // TIME metric groups
  group_name = /*"GpuOffload"*/ /*"ComputeBasic"  "MemProfile" "DataportProfile"*/ "L1ProfileReads" /* "L1ProfileSlmBankConflicts" "L1ProfileWrites"*/ ;

  // TRACE metric groups
  //group_name = "tpcs_utilization_and_bw" /* "nic_stms" "dcore0_bmons_bw"*/;
  group_type = /*PTI_METRIC_GROUP_TYPE_TRACE_BASED*/ PTI_METRIC_GROUP_TYPE_TIME_BASED;
  metrics_profiler->ConfigureMetricGroups(group_name, group_type);
  metrics_profiler->StartCollectionPaused();

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  metrics_profiler->ResumeCollection();

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  metrics_profiler->StopCollection();
  metrics_profiler->GetCalculatedData();

  if (sycl_device_has_uuid) {
    metrics_profiler->ValidateDeviceUUID(uuid);
  }

  delete(metrics_profiler);

  return error ? 1 : 0;
}
