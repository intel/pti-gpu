//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_LEVEL_ZERO_METRICS_H_
#define PTI_TOOLS_UNITRACE_LEVEL_ZERO_METRICS_H_


#include <string.h>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <level_zero/layers/zel_tracing_api.h>
#include "ze_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <atomic>
#include <thread>

#include "logger.h"
#include "unimemory.h"
#include "utils.h"
#include "utils_ze.h"
#include "pti_assert.h"
#include "logger_factory.h"
#include "unicontrol.h"
#include "kernel_disassembler.h"
#include "kernel_extractor.h"
#include "source_resolver.h"
#include <inttypes.h>

constexpr static uint64_t min_dummy_instance_id = 1024 * 1024;  // offset of idle-sampling instance ids
constexpr static uint32_t max_metric_samples = 32768;
// Default UNITRACE_SamplingInterval in us; single source for both the numeric and string forms
#define DEFAULT_SAMPLING_INTERVAL_US 50
#define DEFAULT_SAMPLING_INTERVAL_US_STR_(x) #x
#define DEFAULT_SAMPLING_INTERVAL_US_STR_X_(x) DEFAULT_SAMPLING_INTERVAL_US_STR_(x)
#define DEFAULT_SAMPLING_INTERVAL_US_STR DEFAULT_SAMPLING_INTERVAL_US_STR_X_(DEFAULT_SAMPLING_INTERVAL_US)
constexpr static double default_sampling_interval_us = DEFAULT_SAMPLING_INTERVAL_US;

#define MAX_METRIC_BUFFER  (8ULL * 1024ULL * 1024ULL)

inline void PrintDeviceList() {
  if (!InitializeL0()) {
    return;
  }

  auto device_list = GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No Level Zero devices found" << std::endl;
    return;
  }

  for (size_t i = 0; i < device_list.size(); ++i) {
    auto device = device_list[i];
    ze_device_properties_t device_properties{
        ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    auto status = ZE_FUNC(zeDeviceGetProperties)(device, &device_properties);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_pci_ext_properties_t pci_props;
    pci_props.pNext = nullptr;
    pci_props.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;
    status = ZE_FUNC(zeDevicePciGetPropertiesExt)(device, &pci_props);

    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << "Device #" << i << ": [" << std::hex <<
      pci_props.address.domain << ":" <<
      pci_props.address.bus << ":" <<
      pci_props.address.device << "." <<
      pci_props.address.function << "] " <<
      device_properties.name << std::dec << std::endl;
  }
}

inline std::string GetMetricUnits(const char* units) {
  PTI_ASSERT(units != nullptr);

  std::string result = units;
  if (result.find("null") != std::string::npos) {
    result = "";
  } else if (result.find("percent") != std::string::npos) {
    result = "%";
  }

  return result;
}

inline void PrintMetricList(uint32_t device_id) {
  if (!InitializeL0()) {
    return;
  }

  auto device_list = GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No Level Zero devices found" << std::endl;
    return;
  }

  PTI_ASSERT(device_id < device_list.size());
  ze_device_handle_t device = device_list[device_id];

  uint32_t group_count = 0;
  auto status = ZE_FUNC(zetMetricGroupGet)(device, &group_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || group_count == 0) {
    std::cerr << "[WARNING] No metrics found (status = 0x" << std::hex << status << std::dec << ") group_count = " << group_count << std::endl;
    std::cerr << "[INFO] Please make sure the intel-metrics-discovery and intel-metrics-library packages are installed." << std::endl;
    return;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = ZE_FUNC(zetMetricGroupGet)(device, &group_count, group_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint32_t group_id = 0;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    status = ZE_FUNC(zetMetricGroupGetProperties)(group_list[i], &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    bool is_ebs = (group_props.samplingType &
                   ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    bool is_tbs = (group_props.samplingType &
                   ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    PTI_ASSERT(is_ebs || is_tbs);
    if (is_ebs) {
      continue;
    }

    std::cout << "Group " << group_id << ": " << group_props.name << " (" <<
      group_props.description << ")" << std::endl;
    ++group_id;

    uint32_t metric_count = group_props.metricCount;
    std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
    status = ZE_FUNC(zetMetricGet)(group_list[i], &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == group_props.metricCount);

    for (uint32_t j = 0; j < metric_count; ++j) {
      zet_metric_properties_t metric_props{};
      metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
      status = ZE_FUNC(zetMetricGetProperties)(metric_list[j], &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::cout << "\tMetric " << j << ": "  << metric_props.name;
      std::string units = GetMetricUnits(metric_props.resultUnits);
      if (!units.empty()) {
        std::cout << "[" << units << "]";
      }
      std::cout << " (" << metric_props.description << ") [" <<
        GetResultType(metric_props.resultType) << ", " <<
        GetMetricType(metric_props.metricType) << ", " <<
        group_props.name << "]" << std::endl;
    }
  }
}

inline uint32_t GetMetricId(const std::vector<std::string>& metric_list, const std::string& metric_name) {
  PTI_ASSERT(!metric_list.empty());
  PTI_ASSERT(!metric_name.empty());

  for (size_t i = 0; i < metric_list.size(); ++i) {
    if (metric_list[i].find(metric_name) == 0) {
      return i;
    }
  }

  return metric_list.size();
}

enum ZeProfilerState {
  PROFILER_DISABLED = 0,
  PROFILER_ENABLED = 1
};

struct ZeDeviceDescriptor {
  ze_device_handle_t device_;
  ze_device_handle_t parent_device_;
  uint64_t device_timer_frequency_;
  uint64_t device_timer_mask_;
  uint64_t metric_timer_frequency_;
  uint64_t metric_timer_mask_;
  ze_driver_handle_t driver_;
  ze_context_handle_t context_;
  int32_t device_id_;
  int32_t subdevice_id_;
  int32_t num_sub_devices_;
  zet_metric_group_handle_t metric_group_;
  ze_pci_ext_properties_t pci_properties_;
  std::thread *profiling_thread_;
  std::atomic<ZeProfilerState> profiling_state_;
  std::string metric_file_name_;
  std::ofstream metric_file_stream_;
  bool stall_sampling_;
};

class ZeMetricProfiler {
 public:
  static ZeMetricProfiler* Create(uint32_t app_pid, char *dir, std::string& logfilename, bool idle_sampling, std::string devices_to_sample) {
    ZeMetricProfiler* profiler = new ZeMetricProfiler(app_pid, dir, logfilename, idle_sampling, devices_to_sample);
    UniMemory::ExitIfOutOfMemory((void *)(profiler));

    profiler->StartProfilingMetrics();

    return profiler;
  }

  ~ZeMetricProfiler() {
    StopProfilingMetrics();
    ComputeMetricsSampled();
  }

  ZeMetricProfiler(const ZeMetricProfiler& that) = delete;
  ZeMetricProfiler& operator=(const ZeMetricProfiler& that) = delete;

#ifdef _WIN32
  static void ComputeMetricsQueried(uint32_t pid) {

    std::string metric_group_name = utils::GetEnv("UNITRACE_MetricGroup");
    std::vector<zet_metric_group_handle_t> groups;
    ze_result_t status = ZE_RESULT_SUCCESS;

    // initialize L0, then get the metric group of each device
    if (ZE_FUNC(zeInit)(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) {
      std::cerr << "[ERROR] Failed to initialize Level Zero runtime" << std::endl;
      return;
    }

    uint32_t num_drivers = 0;
    status = ZE_FUNC(zeDriverGet)(&num_drivers, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[ERROR] Unable to get driver" << std::endl;
      return;
    }

    if (num_drivers > 0) {
      std::vector<ze_driver_handle_t> drivers(num_drivers);
      status = ZE_FUNC(zeDriverGet)(&num_drivers, drivers.data());
      if (status != ZE_RESULT_SUCCESS) {
        std::cerr << "[ERROR] Unable to get driver" << std::endl;
        return;
      }

      for (auto driver : drivers) {
        uint32_t num_devices = 0;
        status = ZE_FUNC(zeDeviceGet)(driver, &num_devices, nullptr);
        if (status != ZE_RESULT_SUCCESS) {
          std::cerr << "[WARNING] Unable to get device" << std::endl;
          return;
        }
        if (num_devices) {
          std::vector<ze_device_handle_t> devices(num_devices);
          status = ZE_FUNC(zeDeviceGet)(driver, &num_devices, devices.data());
          if (status != ZE_RESULT_SUCCESS) {
            std::cerr << "[WARNING] Unable to get device" << std::endl;
            return;
          }
          for (auto device : devices) {
            uint32_t num_groups = 0;
            status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, nullptr);
            if (status != ZE_RESULT_SUCCESS) {
              std::cerr << "[ERROR] Unable to get metric group" << std::endl;
              return;
            }
            if (num_groups > 0) {
              std::vector<zet_metric_group_handle_t> grps(num_groups, nullptr);
              status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, grps.data());
              if (status != ZE_RESULT_SUCCESS) {
                std::cerr << "[ERROR] Unable to get metric group" << std::endl;
                return;
              }

              uint32_t k;
              for (k = 0; k < num_groups; ++k) {
                zet_metric_group_properties_t group_props{};
                group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                status = ZE_FUNC(zetMetricGroupGetProperties)(grps[k], &group_props);
                if (status != ZE_RESULT_SUCCESS) {
                  std::cerr << "[ERROR] Unable to get metric group properties" << std::endl;
                  return;
                }

                if ((strcmp(group_props.name, metric_group_name.c_str()) == 0) && (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED)) {
                  groups.push_back(grps[k]);
                  break;
                }
              }
              if (k == num_groups) {
                // no metric group found; should not happen
                groups.push_back(nullptr);
              }
            }
            else {
              // unable to get metric groups; should not happen
              groups.push_back(nullptr);
            }
          }
        }
      }
    }

    LoggerFactory* logger_factory = LoggerFactory::Create();
    std::pair<std::string, std::string> metrics_files = logger_factory->SearchRawMetricFiles(pid);
    std::string metrics_file_name = metrics_files.first;
    std::shared_ptr<Logger> metrics_logger = std::make_shared<Logger>(metrics_files.second);
    if (metrics_logger == nullptr) {
      std::cerr << "[ERROR] Failed to create metric data file" << std::endl;
    }

    // raw metrics are in <UNITRACE_DataDir>/.metrics.<pid>.q
    std::ifstream mf(metrics_file_name, std::ios::binary);
    if (!mf) {
      std::cerr << "[ERROR] Could not open the metric data file" << std::endl;
      return;
    }
    int did = -1;
    zet_metric_group_handle_t group = nullptr;
    std::vector<std::string> metric_names;
    std::string counter_header;
    while (!mf.eof()) {
      int32_t device;
      size_t kname_size;
      std::string kname;
      uint64_t instance_id;
      uint64_t data_size;
      std::vector<uint8_t> metrics_data;

      mf.read(reinterpret_cast<char *>(&device), sizeof(int32_t));
      if (mf.gcount() != sizeof(int32_t)) {
        break;
      }
      mf.read(reinterpret_cast<char *>(&kname_size), sizeof(size_t));
      if (mf.gcount() != sizeof(size_t)) {
        break;
      }
      if (kname.size() < kname_size) {
        kname.resize(kname_size);
      }
      mf.read(&kname[0], kname_size);
      if (mf.gcount() != kname_size) {
        break;
      }
      mf.read(reinterpret_cast<char *>(&instance_id), sizeof(uint64_t));
      if (mf.gcount() != sizeof(uint64_t)) {
        break;
      }
      mf.read(reinterpret_cast<char *>(&data_size), sizeof(uint64_t));
      if (mf.gcount() != sizeof(uint64_t)) {
        break;
      }
      if (metrics_data.size() < data_size) {
        metrics_data.resize(data_size);
      }
      mf.read(reinterpret_cast<char *>(metrics_data.data()), data_size);
      if (mf.gcount() != data_size) {
        break;
      }

      if (device != did) {
        did = device;
        group = groups[device];
        metric_names = GetMetricNames(groups[device]);
        PTI_ASSERT(!metric_names.empty());

        if (logger_factory->IsLegacy()) {
           metrics_logger->Log("\n=== Device #" + std::to_string(did) + " Metrics ===\n");
        }
       
        // Query metrics are written as a self-contained section per kernel-instance, the
        // shape stall and time-based sampling also use:
        //   Kernel, GlobalInstanceId              <- section label, naming the identity
        //   "<name>", <instance id>                  the section carries
        //   SubDeviceId, <counter names...>       <- column header: the sub-device, then
        //   <sub-device>, <counter values...>        all metrics in metric_names order;
        //                                            one row per sub-device
        // The counter column header is the same for every section of a device, so build
        // it once per device change.
        counter_header = "SubDeviceId";
        for (size_t i = 0; i < metric_names.size(); i++) {
          counter_header += ", " + metric_names[i];
        }
        counter_header += "\n";
      }

      // compute metrics
      uint32_t num_samples = 0;
      uint32_t num_metrics = 0;
      ze_result_t status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
        group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        data_size, metrics_data.data(), &num_samples, &num_metrics,
        nullptr, nullptr);

      if ((status == ZE_RESULT_SUCCESS) && (num_samples > 0) && (num_metrics > 0)) {
        std::vector<uint32_t> samples(num_samples);
        std::vector<zet_typed_value_t> computed_metrics(num_metrics);

        status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
          group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
          data_size, metrics_data.data(), &num_samples, &num_metrics,
          samples.data(), computed_metrics.data());

        if (status == ZE_RESULT_SUCCESS) {
          // The leading newline puts a blank line before every section.
          std::string str = "\nKernel, GlobalInstanceId\n" + kname + ", " +
                            std::to_string(instance_id) + "\n" + counter_header;
          for (uint32_t i = 0; i < num_samples; ++i) {
            uint32_t size = samples[i];

            // A sample per sub-device, i being the sub-device
            str += std::to_string(i);
            const zet_typed_value_t *value = computed_metrics.data() + i * size;
            for (uint32_t j = 0; j < size; ++j) {
              str += ", " + PrintTypedValue(value[j]);
            }
            str += "\n";
          }
          metrics_logger->Log(str);
        }
        else {
          std::cerr << "[WARNING] Not able to calculate metrics" << std::endl;
        }
      }
      else {
        std::cerr << "[WARNING] Not able to calculate metrics" << std::endl;
      }
    }

    if (logger_factory->IsLegacy() && !metrics_logger->GetLogFileName().empty()) {
      std::cerr << "[INFO] Kernel metrics are stored in " << metrics_logger->GetLogFileName() << std::endl;
    }
  }
#endif /* _WIN32 */

 private:
  std::set<int> devices_to_sample_;

  ZeMetricProfiler(uint32_t app_pid, char *dir, std::string& logfile, bool idle_sampling, std::string &devices_to_sample) {
    data_dir_name_ = std::string(dir);
    
    idle_sampling_ = idle_sampling;
    logger_factory_ = LoggerFactory::Create(app_pid);
    
    GetZeDevicesStringToSet(devices_to_sample_, devices_to_sample);

    EnumerateDevices(app_pid, dir);
  }

  void EnumerateDevices(uint32_t app_pid, char *dir) {

    std::string metric_group = utils::GetEnv("UNITRACE_MetricGroup");
    std::string output_dir = utils::GetEnv("UNITRACE_TraceOutputDir");

    bool stall_sampling = false;
    if (metric_group == "EuStallSampling") {
      stall_sampling = true;
    }

    int32_t global_dev_cnt = -1;
    auto drivers = GetDriverList();
    for (auto driver : drivers) {
      ze_context_handle_t context = nullptr;
      ze_context_desc_t cdesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

      auto status = ZE_FUNC(zeContextCreate)(driver, &cdesc, &context);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      metric_contexts_.push_back(context);

      auto devices = GetDeviceList(driver);
      for (auto device : devices) {
        global_dev_cnt++;

        // Skip devices not found in the list, whenever the list is given
        if (!devices_to_sample_.empty()) {
          if (devices_to_sample_.find(global_dev_cnt) == devices_to_sample_.end()) {
            continue;
          }
        }

        auto sub_devices = GetSubDeviceList(device);

        ZeDeviceDescriptor *desc = new ZeDeviceDescriptor;
        UniMemory::ExitIfOutOfMemory((void *)(desc));

        desc->stall_sampling_ = stall_sampling;

        desc->device_ = device;
        desc->device_id_ = global_dev_cnt;
        desc->parent_device_ = nullptr;
        desc->subdevice_id_ = -1;     // not a subdevice
        desc->num_sub_devices_ = sub_devices.size();

        ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2, };
        ze_result_t status = ZE_FUNC(zeDeviceGetProperties)(device, &props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        PTI_ASSERT(props.timerResolution != 0);
        PTI_ASSERT(props.kernelTimestampValidBits != 0);

        desc->device_timer_frequency_ = props.timerResolution;
        desc->device_timer_mask_ = (props.kernelTimestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)() : ((1ull << props.kernelTimestampValidBits) - 1ull);

        ze_pci_ext_properties_t pci_device_properties;
        status = ZE_FUNC(zeDevicePciGetPropertiesExt)(device, &pci_device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        desc->pci_properties_ = pci_device_properties;

        zet_metric_group_handle_t group = FindMetricGroup (device, metric_group, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
        if (group == nullptr) {
          std::cerr << "[WARNING] Metric group " << metric_group << " is not found on device : " << desc->device_id_ << std::endl;
          continue;
        }

        zet_metric_group_properties_t metric_group_prop;
        metric_group_prop.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        metric_group_prop.pNext = nullptr;

        zet_metric_global_timestamps_resolution_exp_t metrics_ts_prop;
        metrics_ts_prop.stype = ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP;
        metrics_ts_prop.pNext = nullptr;
        metric_group_prop.pNext = &metrics_ts_prop;
        status = ZE_FUNC(zetMetricGroupGetProperties)(group, &metric_group_prop);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        PTI_ASSERT(metrics_ts_prop.timerResolution != 0);
        PTI_ASSERT(metrics_ts_prop.timestampValidBits != 0);

        desc->metric_timer_frequency_ = metrics_ts_prop.timerResolution;
        // special handling for certain older devices
        uint32_t device_gen = (props.deviceId & 0xFF00);
        if (device_gen == 0x0B00) {
          if (desc->metric_timer_frequency_ > desc->device_timer_frequency_) {
            desc->metric_timer_frequency_ = desc->device_timer_frequency_;
          }

          if (metrics_ts_prop.timestampValidBits == props.kernelTimestampValidBits) {
            metrics_ts_prop.timestampValidBits = metrics_ts_prop.timestampValidBits - 1;
          }
        }
        desc->metric_timer_mask_ = (metrics_ts_prop.timestampValidBits == 64) ? (std::numeric_limits<uint64_t>::max)() : ((1ull << metrics_ts_prop.timestampValidBits) - 1ull);

        desc->driver_ = driver;
        desc->context_ = context;
        desc->metric_group_ = group;

        desc->profiling_thread_ = nullptr;
        desc->profiling_state_.store(PROFILER_DISABLED, std::memory_order_release);

        desc->metric_file_name_ = logger_factory_->GenerateLogFileName(LOGGER_TYPE_METRICS_SAMPLING_TEMP, global_dev_cnt);

        desc->metric_file_stream_ = std::ofstream(desc->metric_file_name_, std::ios::out | std::ios::trunc | std::ios::binary);

        device_descriptors_.insert({device, desc});

        for (size_t j = 0; j < sub_devices.size(); j++) {
          ZeDeviceDescriptor *sub_desc = new ZeDeviceDescriptor;
          UniMemory::ExitIfOutOfMemory((void *)(sub_desc));

          sub_desc->stall_sampling_ = stall_sampling;

          sub_desc->device_ = sub_devices[j];
          sub_desc->device_id_ = global_dev_cnt;           // subdevice
          sub_desc->parent_device_ = device;
          sub_desc->subdevice_id_ = j;                     // a subdevice
          sub_desc->num_sub_devices_ = 0;

          sub_desc->driver_ = driver;
          sub_desc->context_ = context;
          sub_desc->metric_group_ = group;

          sub_desc->device_timer_frequency_ = desc->device_timer_frequency_;
          sub_desc->device_timer_mask_ = desc->device_timer_mask_;

          sub_desc->metric_timer_frequency_ = desc->metric_timer_frequency_;
          sub_desc->metric_timer_mask_ = desc->metric_timer_mask_;

          ze_pci_ext_properties_t pci_device_properties;
          ze_result_t status = ZE_FUNC(zeDevicePciGetPropertiesExt)(sub_devices[j], &pci_device_properties);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);

          sub_desc->pci_properties_ = pci_device_properties;

          sub_desc->driver_ = driver;
          sub_desc->context_ = context;
          sub_desc->metric_group_ = group;

          sub_desc->profiling_thread_ = nullptr;
          sub_desc->profiling_state_.store(PROFILER_DISABLED, std::memory_order_release);

#if 0
          sub_desc->metric_file_name_ = std::string(dir) + "/." + std::to_string(global_dev_cnt) + "." + std::to_string(j) + "." + metric_group + "." + std::to_string(app_pid) + ".t";
          sub_desc->metric_file_stream_ = std::ofstream(sub_desc->metric_file_name_, std::ios::out | std::ios::trunc | std::ios::binary);
#endif /* 0 */

          device_descriptors_.insert({sub_devices[j], sub_desc});
        } // subdevices list
      } // devices list
    } // drivers list
    if (device_descriptors_.empty()) {
      std::cerr << "[ERROR] No supported device found for the metric group " << metric_group << std::endl;;
      exit(-1);
    }
  }

  int GetDeviceId(ze_device_handle_t sub_device) const {
    if (auto it = device_descriptors_.find(sub_device); it != device_descriptors_.end()) {
      return it->second->device_id_;
    }
    return -1;
  }

  int GetSubDeviceId(ze_device_handle_t sub_device) const {
    if (auto it = device_descriptors_.find(sub_device); it != device_descriptors_.end()) {
      return it->second->subdevice_id_;
    }
    return -1;
  }

  ze_device_handle_t GetParentDevice(ze_device_handle_t sub_device) const {
    if (auto it = device_descriptors_.find(sub_device); it != device_descriptors_.end()) {
      return it->second->parent_device_;
    }
    return nullptr;
  }

  void StartProfilingMetrics(void) {

    for (auto [handle, device] : device_descriptors_) {
      if (device->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      device->profiling_thread_ = new std::thread(MetricProfilingThread, device);
      while (device->profiling_state_.load(std::memory_order_acquire) != PROFILER_ENABLED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  void StopProfilingMetrics() {
    for (auto [handle, device] : device_descriptors_) {
      if (device->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      PTI_ASSERT(device->profiling_thread_ != nullptr);
      PTI_ASSERT(device->profiling_state_ == PROFILER_ENABLED);
      device->profiling_state_.store(PROFILER_DISABLED, std::memory_order_release);
      device->profiling_thread_->join();
      delete device->profiling_thread_;
      device->profiling_thread_ = nullptr;
      device->metric_file_stream_.close();
    }
  }

  struct ZeKernelInfo {
    int32_t subdevice_id;
    uint64_t global_instance_id;
    uint64_t metric_start;
    uint64_t metric_end;
    std::string kernel_name;
  };

  static bool CompareInterval(ZeKernelInfo& iv1, ZeKernelInfo& iv2) {
    return (iv1.metric_start < iv2.metric_start);
  }

  // 0: mangled name, which names the kernel's ".text.<mangled name>" section
  // 1: kernel command id
  // 2: code size (extent) of the section, 0 until the section is found
  // 3: assembly listing
  // 4: source locations of the instructions
  // 5: view of the kernel's bytes in ModuleProps::native_binary_
  // 6: defs and uses read from the listing
  // 7: IGA refused to disassemble the kernel, so it is not retried
  // 2 and 5 are filled when the search first reaches the kernel, 3, 4 and 6 when a sample
  // is first attributed to it.
  using KernelProps = std::tuple<std::string, uint64_t, uint64_t, std::string,
                                 std::vector<SourceLocation>, KernelBinaryView, DefsAndUses,
                                 bool>;

  // native_binary_ is read once, when a kernel of the module first needs it, and shared by
  // all its kernels. native_binary_loaded_ records that it was read, since a missing binary
  // also reads back empty.
  struct ModuleProps {
    bool native_binary_loaded_ = false;
    std::vector<uint8_t> native_binary_;
    std::map<uint64_t, KernelProps> kernels_;
  };

  // One device's modules, keyed by (module id, pid): ids are only unique within a process,
  // and the ranks of an MPI run share the data directory.
  using DeviceModuleProps = std::map<std::pair<uint32_t, uint32_t>, ModuleProps>;

  // Reads every .kprops file of the device (one per traced process) into modules. A
  // duplicate base address within one (module, pid) keeps the first record.
  //
  // The collectors (ze_collector.h at zeKernelCreate, cl_collector.h) write one record per
  // kernel to .kprops.<device_id>.<pid>.txt, and one binary per module. A record holds only
  // what the application alone knows: mangled name, base address, command id and module
  // id. Everything else is derived here from the module binary. The files are
  // intermediates under UNITRACE_DataDir, removed at exit, so they carry no version.
  DeviceModuleProps ReadModuleAndKernelProps(int32_t device_id) const {
    // One line per field, in the order the collectors write them.
    enum Field {
      kName,
      kBaseAddr,
      kKernelId,
      kModuleId,
      kFieldCount
    };

    DeviceModuleProps props;
    // enumerate all kernel property files
    std::vector<std::string> props_file_paths = logger_factory_->SearchFilesByType(LOGGER_TYPE_KPROPS, device_id);
    for (const auto& props_file: props_file_paths) {
      // kernel properties file path: <data_dir>/.kprops.<device_id>.<pid>.txt
      std::ifstream kpf = std::ifstream(props_file);
      if (!kpf.is_open()) {
        continue;
      }

      // The pid of the process that wrote the file, from its name (logger_factory.h).
      const uint32_t pid = LoggerFactory::GetFileNamePid(props_file);

      // A truncated last record (abnormal exit) is dropped; the ones before it are kept.
      for (;;) {
        std::string field[kFieldCount];
        bool complete = true;
        for (std::string& line : field) {
          if (!std::getline(kpf, line)) {
            complete = false;
            break;
          }
        }
        if (!complete) {
          break;
        }

        // Strip the quotes the writer put around the name.
        std::string& kname = field[kName];
        if (kname.size() >= 2 && kname.front() == '"' && kname.back() == '"') {
          kname = kname.substr(1, kname.size() - 2);
        }

        // strtoull: an address needs 64 bits, and long is 32-bit on Windows.
        const uint64_t base_addr = std::strtoull(field[kBaseAddr].c_str(), nullptr, 0);
        const uint64_t kernel_id = std::strtoull(field[kKernelId].c_str(), nullptr, 0);
        const uint32_t mod =
            static_cast<uint32_t>(std::strtoul(field[kModuleId].c_str(), nullptr, 0));

        // Module 0 means the collector has no binary for the kernel (e.g. an OpenCL kernel,
        // or a module created before tracing started). No sample can be attributed to it,
        // so it is dropped.
        if (mod == 0) {
          std::cerr << "[ERROR] No module binary for kernel "
                    << utils::Demangle(kname.c_str()) << " of process " << pid
                    << ": its stall samples cannot be reported" << std::endl;
          continue;
        }

        // Everything but the name and id is filled in from the module binary during the
        // walk, for the kernels samples land in.
        props[{mod, pid}].kernels_.insert(
            {base_addr, std::make_tuple(std::move(kname), kernel_id, uint64_t{0}, std::string(),
                                        std::vector<SourceLocation>(), KernelBinaryView(),
                                        DefsAndUses(), false)});
      }
      kpf.close();
    }

    return props;
  }

  // The module's binary, read from the data directory on first use and cached in the
  // entry. Empty if the file is missing or truncated (an abnormal exit).
  const std::vector<uint8_t>& GetModuleBinary(
      DeviceModuleProps::value_type& entry) const {
    ModuleProps& module = entry.second;
    if (module.native_binary_loaded_) {
      return module.native_binary_;
    }

    // Named as WriteOutModuleNativeBinary() (utils_ze.h) wrote it, with the traced
    // process's pid.
    const std::string path =
        data_dir_name_ + "/" +
        logger_factory_->GetModuleBinaryFileName(/* pid = */ entry.first.second,
                                                 /* module_id = */ entry.first.first);
    if (!ReadBackModuleNativeBinary(path, module.native_binary_)) {
      // Discard a partial read.
      module.native_binary_.clear();
    }

    // Set even on failure, so a missing binary is not looked for again.
    module.native_binary_loaded_ = true;
    return module.native_binary_;
  }

  // A CSV string cell (RFC 4180): quoted, with embedded quotes doubled. Every string cell
  // needs it, since instruction text and demangled names contain commas.
  static std::string Quote(const std::string& value) {
    std::string cell("\"");
    for (const char c : value) {
      if (c == '"') {
        cell += '"';
      }
      cell += c;
    }
    cell += '"';
    return cell;
  }

  // The Source cell: "<file>:<line>[:<column>]", or "NA" for no location, no file or
  // line 0 (no statement). Unquoted; the caller applies Quote().
  static std::string FormatSourceLocation(const SourceLocation* location) {
    if (location == nullptr || location->file_path_.empty() || location->line_ == 0) {
      return "NA";
    }

    std::string cell = location->file_path_ + ":" + std::to_string(location->line_);
    if (location->column_ != 0) {
      cell += ":" + std::to_string(location->column_);
    }
    return cell;
  }

  // The Dependency cell: the instructions producing the values this one reads, each as
  // "<offset> (<source>)[: <instruction>]", joined by " | ", or "NA" if none. Live-ins and
  // defs with no instruction offset are skipped. Consumers split on " (", ")" and ": ".
  // Unquoted; the caller applies Quote().
  std::string FormatDependencies(const DefsAndUses& defs,
                                 const std::vector<SourceLocation>& locations,
                                 const UsedBy* instruction) const {
    if (instruction == nullptr) {
      return "NA";
    }

    std::string cell;
    for (const DefinedBy& def : instruction->defs_) {
      if (def.live_in_ || def.offset_ == kNoInstructionOffset) {
        continue;
      }
      if (!cell.empty()) {
        cell += " | ";
      }
      cell += HexOffset(def.offset_) + " (" +
              FormatSourceLocation(FindSourceLocation(locations, def.offset_)) + ")";

      // The producer's text, if the listing has a line for it.
      const UsedBy* producer = defs.FindByOffset(def.offset_);
      if (producer != nullptr && !producer->text_.empty()) {
        cell += ": " + producer->text_;
      }
    }
    return cell.empty() ? "NA" : cell;
  }

  // Writes <device>.<pid>.<kernel id>.asm to disasm_dir: the IGA listing, the source
  // locations and the defs of one disassembled kernel. A plain stream, since a per-kernel
  // file does not fit a Logger's (type, device) naming.
  bool WriteOutKernelAssembly(const std::string& disasm_dir, int32_t device_id, uint32_t pid,
                              uint64_t kernel_id, const std::string& kname,
                              const std::string& disassembly,
                              const std::vector<SourceLocation>& locations,
                              const KernelBinaryView& isa, const DefsAndUses& defs) const {
    const std::string path =
        logger_factory_->GetDisasmFileName(disasm_dir, device_id, pid, kernel_id, "asm");
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
      std::cerr << "[ERROR] Failed to open file for writing: " << path << std::endl;
      return false;
    }

    file << "// Kernel: " << utils::Demangle(kname.c_str()) << "\n";
    file << "// Mangled name: " << kname << "\n";
    file << "// Device #" << device_id << ", pid " << pid << ", kernel id " << kernel_id
         << "\n";
    file << "// ISA: " << isa.size_ << " bytes of section " << isa.section_name_ << "\n\n";

    // Empty when IGA refused the kernel, which it has already said.
    file << "// Assembly\n";
    if (disassembly.empty()) {
      file << "// None\n";
    } else {
      file << disassembly;
      if (disassembly.back() != '\n') {
        file << "\n";
      }
    }

    // By kernel offset, matching the report's Offset column. None without -g.
    file << "\n// Source locations, by offset in the kernel:\n";
    if (locations.empty()) {
      file << "// None\n";
    }
    for (const SourceLocation& location : locations) {
      // Line 0 (no statement) prints as NA; a column of 0 is omitted.
      file << "// " << std::hex << location.range_offset_ << "-"
           << (location.range_offset_ + location.range_size_) << std::dec << " "
           << (location.file_path_.empty() ? "NA" : location.file_path_) << ":";
      if (location.line_ == 0) {
        file << "NA";
      } else {
        file << location.line_;
      }
      if (location.column_ != 0) {
        file << ":" << location.column_;
      }
      if (!location.is_statement_) {
        file << " [not a statement]";
      }
      file << "\n";
    }

    // "<offset> #<id> <- <offset> #<id>{<registers>}, ...": each instruction, then the
    // producers of the values it reads. A live-in is "IN", a producer with no offset "NA".
    // The listing's own def comments show the same; this is what the tool parsed.
    file << "\n// Defs, by offset in the kernel:\n";
    if (defs.Empty()) {
      file << "// None\n";
    }
    for (const UsedBy& instruction : defs.instructions_) {
      file << "// " << std::hex << instruction.offset_ << std::dec << " #" << instruction.id_;
      const char* separator = " <- ";
      for (const DefinedBy& def : instruction.defs_) {
        file << separator;
        separator = ", ";
        if (def.live_in_) {
          file << "IN";
        } else if (def.offset_ == kNoInstructionOffset) {
          file << "NA #" << def.id_;
        } else {
          file << std::hex << def.offset_ << std::dec << " #" << def.id_;
        }
        file << "{" << def.registers_ << "}";
      }
      file << "\n";
    }

    // Checked after close(), which flushes, so write errors such as a full disk show.
    file.close();
    if (!file.good()) {
      std::cerr << "[ERROR] Error writing disassembly to: " << path << std::endl;
      return false;
    }

    return true;
  }

  // The stall-sampling half of ComputeMetricsSampled: correlate one device's EU stall
  // samples with the kernels the collector recorded and write the report. raw_metrics is
  // the caller's buffer, reused across devices. Returns false when the run cannot be
  // continued at all, as opposed to this device having nothing to report.
  bool ComputeStallMetrics(const ZeDeviceDescriptor* device,
                           const std::shared_ptr<Logger>& metric_logger,
                           uint8_t* raw_metrics) {
    DeviceModuleProps modules = ReadModuleAndKernelProps(device->device_id_);
    if (modules.size() == 0) {
      return true;
    }

    std::vector<std::string> metric_list;
    metric_list = GetMetricList(device->metric_group_);
    PTI_ASSERT(!metric_list.empty());

    uint32_t ip_idx = GetMetricId(metric_list, "IP");
    if (ip_idx >= metric_list.size()) {
      // no IP metric
      return true;
    }

    constexpr uint32_t max_num_of_stall_types = 16;  // should be enough for all current platforms

    if ((metric_list.size() - 1) > max_num_of_stall_types) { // metric_list.size() includes IP
      std::cerr << "[ERROR] Number of stall types exceeds supported limit of " << max_num_of_stall_types << std::endl;
      return false;
    }

    std::ifstream inf = std::ifstream(device->metric_file_name_, std::ios::in | std::ios::binary);
    if (!inf.is_open()) {
      return true;
    }

    std::map<uint64_t, std::array<uint64_t, max_num_of_stall_types>> eustalls;

    while (!inf.eof()) {
      // each segment is its size in bytes, then the data
      uint64_t data_size;
      inf.read(reinterpret_cast<char *>(&data_size), sizeof(data_size));
      if (inf.eof()) {
        // no more segments
        break;
      }
      if (inf.gcount() != sizeof(data_size)) {
        std::cerr << "[WARNING] Intermediate metrics file is invalid. Cannot find the size of the next data segment. Output likely to be incomplete." << std::endl;
        break;
      }
      if (data_size > MAX_METRIC_BUFFER) {
        std::cerr << "[WARNING] Intermediate metrics file is invalid. Next chunk cannot be larger than the allocated buffer. Output likely to be incomplete." << std::endl;
        break;
      }
      inf.read(reinterpret_cast<char *>(raw_metrics), data_size);
      int raw_size = inf.gcount();
      if (raw_size < data_size) {
        std::cerr << "[WARNING] Intermediate metrics file is incomplete. Expecting " << data_size << " bytes but only " << raw_size << " bytes were found. Output likely to be incomplete." << std::endl;
        break;
      }
      if (raw_size > 0) {
        uint32_t num_samples = 0;
        uint32_t num_metrics = 0;
        auto status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
          device->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
          raw_size, raw_metrics, &num_samples, &num_metrics,
          nullptr, nullptr);
        if ((status != ZE_RESULT_SUCCESS) || (num_samples == 0) || (num_metrics == 0)) {
          std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
          continue;
        }

        std::vector<uint32_t> samples(num_samples);
        std::vector<zet_typed_value_t> metrics(num_metrics);

        status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
          device->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
          raw_size, raw_metrics, &num_samples, &num_metrics,
          samples.data(), metrics.data());

        if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
          std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
          continue;
        }

        const zet_typed_value_t *value = metrics.data();
        for (uint32_t i = 0; i < num_samples; ++i) {
          std::string str;

          uint32_t size = samples[i];
          for (uint32_t j = 0; j < size; j += metric_list.size()) {
            uint64_t ip;
            ip = (value[j + 0].value.ui64 << 3);
            if (ip == 0) {
              continue;
            }

            std::array<uint64_t, max_num_of_stall_types> stall;

            // value[j] is the IP; the (metric_list.size() - 1) stall counts follow
            for (uint32_t k = 0; k <  (metric_list.size() - 1); k++) {
              stall[k] = value[j + k + 1].value.ui64;
            }
            if (auto eit = eustalls.find(ip); eit != eustalls.end()) {
              for (uint32_t k = 0; k <  (metric_list.size() - 1); k++) {
                eit->second[k] += stall[k];
              }
            }
            else {
              eustalls.insert({ip, std::move(stall)});
            }
          }
          value += samples[i];
        }
      }
    }
    inf.close();

    if (eustalls.size() == 0) {
      return true;
    }

    if (logger_factory_->IsLegacy()) {
      std::string banner("\n=== Device #");
      banner += std::to_string(device->device_id_) + " Metrics ===\n";
      metric_logger->Log(banner);
    }

    // The column header repeated in every kernel section. metric_list[0] is the IP
    // (reported as Offset); the rest are the stall types, in row order.
    std::string column_header("Source,Offset,Instruction,Dependency");
    for (size_t i = 1; i < metric_list.size(); i++) {
      column_header += "," + metric_list[i];
    }
    column_header += "\n";

    // Fetched on the first .asm file, since fetching it creates the directory.
    std::string disasm_dir;

    // The kernel the walk is on and its module. No kernel is current while current_module
    // is modules.end(); current_kernel is only valid otherwise.
    DeviceModuleProps::iterator current_module = modules.end();
    std::map<uint64_t, KernelProps>::reverse_iterator current_kernel;

    // The last source location found in the current kernel's locations, or null.
    const SourceLocation* current_source_location = nullptr;

    for (auto [ip, stall] : eustalls) {
      // Samples come in ascending IP order, so most fall in the current kernel. Search only
      // when there is none or the IP is outside it; an IP below the base wraps and fails
      // the unsigned test.
      if ((current_module == modules.end()) ||
          ((ip - current_kernel->first) >= std::get<2>(current_kernel->second))) {
        bool attributed = false;

        // Modules interleave in the address space, so every module is searched; the first
        // kernel containing the IP wins.
        for (auto mod = modules.begin(); mod != modules.end(); ++mod) {
          std::map<uint64_t, KernelProps>& kernels = mod->second.kernels_;

          // Highest base address first: within a module, kernels do not overlap, so the
          // nearest kernel at or below the IP is the only candidate.
          for (auto it = kernels.rbegin(); it != kernels.rend(); ++it) {
            const uint64_t base_addr = it->first;
            KernelProps& props = it->second;

            if (base_addr > ip) {
              continue;
            }

            // The extent comes from the kernel's section in the module binary. If there is
            // no binary or no section it stays 0, the kernel matches no sample, and the
            // lookup is repeated each time the search reaches it.
            if (!std::get<2>(props)) {
              const std::vector<uint8_t>& binary = GetModuleBinary(*mod);
              if (!binary.empty() &&
                  GetKernelBinary(binary.data(), binary.size(), std::get<0>(props),
                                  std::get<5>(props))) {
                std::get<2>(props) = std::get<5>(props).Size();
              }
            }

            // Past this kernel's end: try the rest. An extent of 0 matches nothing, so a
            // found kernel always has its ISA.
            if ((ip - base_addr) >= std::get<2>(props)) {
              continue;
            }

            // Found: make it current, and drop the previous kernel's source location.
            current_module = mod;
            current_kernel = it;
            current_source_location = nullptr;

            attributed = true;
            break;
          }

          if (attributed) {
            break;
          }
        }

        // No row for a sample in no known kernel.
        if (!attributed) {
          continue;
        }

        KernelProps& props = current_kernel->second;

        // First time this kernel is found: resolve its source and disassemble it, unless
        // IGA already refused.
        if (std::get<3>(props).empty() && !std::get<7>(props)) {
          // Source locations from the module's line information; none without -g. Needs
          // no disassembly, so a kernel IGA refuses still has them.
          const std::vector<uint8_t>& binary = GetModuleBinary(*current_module);
          if (!ResolveAllKernelInstructions(binary.data(), binary.size(),
                                            std::get<0>(props), std::get<4>(props))) {
            std::get<4>(props).clear();
          }

          // With defs (each instruction names the instructions producing its inputs),
          // without register-level deps; see DisassembleKernelInstructions() for costs.
          if (DisassembleKernelInstructions(std::get<5>(props), std::get<3>(props),
                                            /* with_deps = */ false,
                                            /* with_defs = */ true)) {
            // Parse the listing's defs into records looked up by offset.
            ConstructDefsAndUses(std::get<3>(props), std::get<6>(props));
          } else {
            // Drop any partial listing and mark the kernel so it is not retried. Its rows
            // report source and counts only.
            std::get<3>(props).clear();
            std::get<7>(props) = true;
            std::cerr << "[ERROR] Unable to disassemble kernel "
                      << utils::Demangle(std::get<0>(props).c_str()) << " of process "
                      << current_module->first.second << std::endl;
          }

          // Write the kernel's .asm file, creating the directory on the first one.
          if (disasm_dir.empty()) {
            disasm_dir = logger_factory_->GetDisasmDirPath();
          }
          WriteOutKernelAssembly(disasm_dir, device->device_id_,
                                 current_module->first.second,
                                 std::get<1>(props), std::get<0>(props),
                                 std::get<3>(props), std::get<4>(props),
                                 std::get<5>(props), std::get<6>(props));
        }

        // Open a self-contained section for the new kernel: its demangled name, pid and
        // kernel id (which also name its .asm file), then the column header.
        std::string header("\nKernel,Pid,KernelId\n");
        header += Quote(utils::Demangle(std::get<0>(props).c_str())) + "," +
                  std::to_string(current_module->first.second) + "," +
                  std::to_string(std::get<1>(props)) + "\n";
        header += column_header;
        metric_logger->Log(header);
      }

      const uint64_t base_addr = current_kernel->first;
      KernelProps& props = current_kernel->second;

      const uint64_t offset = ip - base_addr;

      // Null for a kernel that could not be disassembled; the row still has its source
      // and counts.
      const UsedBy* instruction = std::get<6>(props).FindByOffset(offset);
      const std::string text =
          (instruction != nullptr && !instruction->text_.empty()) ? instruction->text_
                                                                 : "unknown";

      // Reuse the current source location while it covers the offset, else look it up.
      if (current_source_location == nullptr ||
          offset - current_source_location->range_offset_ >=
              current_source_location->range_size_) {
        current_source_location = FindSourceLocation(std::get<4>(props), offset);
      }

      std::string row = Quote(FormatSourceLocation(current_source_location)) + "," +
                        HexOffset(offset) + "," + Quote(text) + "," +
                        Quote(FormatDependencies(std::get<6>(props), std::get<4>(props),
                                                 instruction));

      // One count per stall type, in header order.
      for (size_t k = 0; k < (metric_list.size() - 1); k++) {
        row += "," + std::to_string(stall[k]);
      }
      row += "\n";
      metric_logger->Log(row);
    }

    return true;
  }

  // The time-based half of ComputeMetricsSampled: line one device's metric samples up
  // against the intervals the kernels ran in and write the report. raw_metrics is the
  // caller's buffer, reused across devices.
  void ComputeTimedMetrics(const ZeDeviceDescriptor* device,
                           const std::shared_ptr<Logger>& metric_logger,
                           uint8_t* raw_metrics) {
    std::vector<ZeKernelInfo> kinfo;
    uint64_t max_global_instance_id = 0;
    double device_ns_per_cycle = (static_cast<double>(NSEC_IN_SEC) / static_cast<double>(device->device_timer_frequency_));
    double metric_ns_per_cycle = (static_cast<double>(NSEC_IN_SEC) / static_cast<double>(device->metric_timer_frequency_));

    // enumerate all kernel time files
    std::vector<std::string> ktime_file_paths = logger_factory_->SearchFilesByType(LOGGER_TYPE_KTIME, device->device_id_);
    for (const auto& ktime_path: ktime_file_paths) {
      std::ifstream kf = std::ifstream(ktime_path);
      if (!kf.is_open()) {
        continue;
      }

      while (!kf.eof()) {
        ZeKernelInfo info;
        std::string line;

        std::getline(kf, line);
        if (kf.eof()) {
          break;
        }
        info.subdevice_id = std::strtol(line.c_str(), nullptr, 0);
        line.clear();
        std::getline(kf, line);
        if (kf.eof()) {
          break;
        }
        info.global_instance_id = std::strtoll(line.c_str(), nullptr, 0);
        if (info.global_instance_id > max_global_instance_id) {
          max_global_instance_id = info.global_instance_id;
        }
        line.clear();

        std::getline(kf, line);
        if (kf.eof()) {
          break;
        }
        info.metric_start = std::strtoll(line.c_str(), nullptr, 0);
        line.clear();

        std::getline(kf, line);
        if (kf.eof()) {
          break;
        }
        info.metric_end = std::strtoll(line.c_str(), nullptr, 0);

        uint64_t kstart;
        uint64_t kend;
        uint64_t elapsed;

        kstart = info.metric_start & device->device_timer_mask_;
        kstart = static_cast<double>(kstart) * device_ns_per_cycle;

        kend = info.metric_end & device->device_timer_mask_;
        kend = static_cast<double>(kend) * device_ns_per_cycle;

        while (kend <= kstart) {
          kend += static_cast<double>(device->device_timer_mask_ + 1UL) * device_ns_per_cycle;
        }
        elapsed = kend - kstart;

        info.metric_start = info.metric_start & device->metric_timer_mask_;
        info.metric_start = static_cast<double>(info.metric_start) * metric_ns_per_cycle;

        info.metric_end = info.metric_start + elapsed;

        std::getline(kf, info.kernel_name);
        if ((info.metric_start != 0) && (info.metric_end != 0) && (!info.kernel_name.empty())) {
          kinfo.push_back(std::move(info));
        }
      }
      kf.close();
    }

    if (kinfo.empty()) {
      return;
    }
    std::sort(kinfo.begin(), kinfo.end(), CompareInterval);

    auto metric_list = GetMetricList(device->metric_group_);
    PTI_ASSERT(!metric_list.empty());

    uint32_t ts_idx = GetMetricId(metric_list, "QueryBeginTime");
    if (ts_idx >= metric_list.size()) {
      // no QueryBeginTime metric
      return;
    }

    //TODO: handle subdevices in case of implicit scaling
    uint64_t time_span_between_clock_resets = static_cast<double>(device->metric_timer_mask_ + 1ull) * metric_ns_per_cycle;

    std::ifstream inf = std::ifstream(device->metric_file_name_, std::ios::in | std::ios::binary);
    if (!inf.is_open()) {
      return;
    }

    std::string header;
    if (logger_factory_->IsLegacy()) {
      header = "\n=== Device #";
      header += std::to_string(device->device_id_) + " Metrics ===\n";
      metric_logger->Log(header);
    }

    // Time-based sampling is written as a self-contained section per kernel-instance:
    //   Kernel, GlobalInstanceId          <- section label (distinguishes this format
    //   "<name>", <global_instance_id>       from the stall one, "Kernel,Pid,KernelId")
    //   <counter names...>                <- column header: all metrics, in metric_list order
    //   <counter values...>               <- one row per time sample
    // The counter column header is the same for every section, so build it once.
    std::string counter_header;
    for (size_t i = 0; i < metric_list.size(); i++) {
      if (i != 0) {
        counter_header += ", ";
      }
      counter_header += metric_list[i];
    }
    counter_header += "\n";

    uint64_t current_instance = (std::numeric_limits<uint64_t>::max)();
    auto WriteOutInstanceHeader = [&metric_logger, &counter_header]
                                  (const std::string& kernel_name, uint64_t id) {
      // The leading newline puts a blank line before every instance.
      metric_logger->Log("\nKernel, GlobalInstanceId\n" + kernel_name + ", " +
                         std::to_string(id) + "\n");
      metric_logger->Log(counter_header);
    };

    uint64_t dummy_global_instance_id = max_global_instance_id + min_dummy_instance_id;  // for idle samples

    uint64_t cur_sampling_ts = 0;
    auto kit = kinfo.begin();
    while (!inf.eof() && kit != kinfo.end()) {
      // each segment is its size in bytes, then the data
      uint64_t data_size;
      inf.read(reinterpret_cast<char *>(&data_size), sizeof(data_size));
      if (inf.eof()) {
        // no more segments
        break;
      }
      if (inf.gcount() != sizeof(data_size)) {
        std::cerr << "[WARNING] Intermediate metrics file is invalid. Cannot find the size of the next data segment. Output likely to be incomplete." << std::endl;
        break;
      }
      if (data_size > MAX_METRIC_BUFFER) {
        std::cerr << "[WARNING] Intermediate metrics file is invalid. Next chunk cannot be larger than the allocated buffer. Output likely to be incomplete." << std::endl;
        break;
      }
      inf.read(reinterpret_cast<char *>(raw_metrics), data_size);
      size_t raw_size = inf.gcount();
      if (raw_size < data_size) {
        std::cerr << "[WARNING] Intermediate metrics file is incomplete. Expecting " << data_size << " bytes but only " << raw_size << " bytes were found. Output likely to be incomplete." << std::endl;
        break;
      }
      if (raw_size > 0) {
        uint32_t num_samples = 0;
        uint32_t num_metrics = 0;
        auto status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
          device->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
          raw_size, raw_metrics, &num_samples, &num_metrics,
          nullptr, nullptr);
        if ((status != ZE_RESULT_SUCCESS) || (num_samples == 0) || (num_metrics == 0)) {
          std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
          continue;
        }

        std::vector<uint32_t> samples(num_samples);
        std::vector<zet_typed_value_t> metrics(num_metrics);

        status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
          device->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
          raw_size, raw_metrics, &num_samples, &num_metrics,
          samples.data(), metrics.data());
        if ((status != ZE_RESULT_SUCCESS) && (status != ZE_RESULT_WARNING_DROPPED_DATA)) {
          std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
          continue;
        }

        const zet_typed_value_t *value = metrics.data();
        for (uint32_t i = 0; i < num_samples; ++i) {
          uint32_t size = samples[i];

          for (uint32_t j = 0; j < (size / metric_list.size()); ++j) {
            std::string str;
            const zet_typed_value_t *v = value + j * metric_list.size();
            uint64_t ts = v[ts_idx].value.ui64;
            if (cur_sampling_ts != 0) {
              while (cur_sampling_ts >= ts) { // clock overflow
                  ts += time_span_between_clock_resets;
              }
            }
            cur_sampling_ts = ts;
            if ((ts >= kit->metric_start) && (ts <= kit->metric_end)) {
              if (current_instance != kit->global_instance_id) {
                WriteOutInstanceHeader(kit->kernel_name, kit->global_instance_id);
                current_instance = kit->global_instance_id;
              }

              for (size_t k = 0; k < metric_list.size(); k++) {
                if (k != 0) {
                  str += ", ";
                }
                if (k == ts_idx) {
                  str += std::to_string(ts);
                }
                else{
                  str += PrintTypedValue(v[k]);
                }
              }
              str += "\n";
              metric_logger->Log(str);
            }
            else {
              if (ts > kit->metric_end) {
                kit++;  // move to next kernel
                dummy_global_instance_id++;
                if (kit == kinfo.end()) {
                  break;  // we are done
                }
              }
              else { // ts < kit->metric_start
                // does not belong to any kernel/command
                if (idle_sampling_) {
                  // one idle section per gap: dummy_global_instance_id advances with kit,
                  // so each gap preceding a different kernel becomes its own section
                  if (current_instance != dummy_global_instance_id) {
                    auto sz = kit->kernel_name.size();
                    std::string idle_name;
                    if (sz > 2) {
                      idle_name = "\"NoKernel(Before " + kit->kernel_name.substr(1, sz - 2)  + ")\"";
                    }
                    else {
                      idle_name = "\"NoKernel\"";
                    }
                    WriteOutInstanceHeader(idle_name, dummy_global_instance_id);
                    current_instance = dummy_global_instance_id;
                  }

                  // sample row: counter values only
                  for (size_t k = 0; k < metric_list.size(); k++) {
                    if (k != 0) {
                      str += ", ";
                    }
                    if (k == ts_idx) {
                      str += std::to_string(ts);
                    }
                    else{
                      str += PrintTypedValue(v[k]);
                    }
                  }
                  str += "\n";
                  metric_logger->Log(str);
                }
              }
            }
          }
          value += samples[i];
        }
      }
    }
    inf.close();
  }

  void ComputeMetricsSampled() {
    // The .kprops/.ktime files and metric blobs read below are written by the child
    // processes when their tracing stops. The caller must make sure they are ready.
    auto *raw_metrics = static_cast<uint8_t*>(malloc(sizeof(uint8_t)*MAX_METRIC_BUFFER));
    UniMemory::ExitIfOutOfMemory((void *)raw_metrics);

    std::shared_ptr<Logger> metric_logger = nullptr;

    for (auto [handle, device] : device_descriptors_) {
      if (device->parent_device_ != nullptr) {
        // subdevice
        continue;
      }

      metric_logger = logger_factory_->GetDeviceLogger(LOGGER_TYPE_METRICS_SAMPLING, device->device_id_, true, true);
      if (device->stall_sampling_) {
        if (!ComputeStallMetrics(device, metric_logger, raw_metrics)) {
          free(raw_metrics);
          return;
        }
      }
      else {
        ComputeTimedMetrics(device, metric_logger, raw_metrics);
      }
      metric_logger->Flush();
    }
    free (raw_metrics);
    if (logger_factory_->IsLegacy() && metric_logger && !metric_logger->GetLogFileName().empty()) {
      std::cerr << "[INFO] Device metrics are stored in " << metric_logger->GetLogFileName() << std::endl;
    }
  }

  static std::vector<std::string> GetMetricNames(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    uint32_t metric_count = GetMetricCount(group);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metrics(metric_count);
    ze_result_t status = ZE_FUNC(zetMetricGet)(group, &metric_count, metrics.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metrics.size());

    std::vector<std::string> names;
    for (auto metric : metrics) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = ZE_FUNC(zetMetricGetProperties)(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::string units = GetMetricUnits(metric_props.resultUnits);
      std::string name = metric_props.name;
      if (!units.empty()) {
        name += "[" + units + "]";
      }
      names.push_back(std::move(name));
    }

    return names;
  }

 private:

    static std::string PrintTypedValue(const zet_typed_value_t& typed_value) {
    switch (typed_value.type) {
      case ZET_VALUE_TYPE_UINT32:
        return std::to_string(typed_value.value.ui32);
      case ZET_VALUE_TYPE_UINT64:
        return std::to_string(typed_value.value.ui64);
      case ZET_VALUE_TYPE_FLOAT32:
        return std::to_string(typed_value.value.fp32);
      case ZET_VALUE_TYPE_FLOAT64:
        return std::to_string(typed_value.value.fp64);
      case ZET_VALUE_TYPE_BOOL8:
        return std::to_string(static_cast<uint32_t>(typed_value.value.b8));
      default:
        PTI_ASSERT(0);
        break;
    }
    return "";  // in case of error returns empty string.
  }

  inline static std::string GetMetricUnits(const char* units) {
    PTI_ASSERT(units != nullptr);

    std::string result = units;
    if (result.find("null") != std::string::npos) {
      result = "";
    } else if (result.find("percent") != std::string::npos) {
      result = "%";
    }

    return result;
  }

  static uint32_t GetMetricCount(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = ZE_FUNC(zetMetricGroupGetProperties)(group, &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return group_props.metricCount;
  }

  static std::vector<std::string> GetMetricList(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    uint32_t metric_count = GetMetricCount(group);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metric_list(metric_count);
    ze_result_t status = ZE_FUNC(zetMetricGet)(group, &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metric_list.size());

    std::vector<std::string> name_list;
    for (auto metric : metric_list) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = ZE_FUNC(zetMetricGetProperties)(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::string units = GetMetricUnits(metric_props.resultUnits);
      std::string name = metric_props.name;
      if (!units.empty()) {
        name += "[" + units + "]";
      }
      name_list.push_back(std::move(name));
    }

    return name_list;
  }

  static uint64_t ReadMetrics(zet_metric_streamer_handle_t streamer, uint8_t* storage, size_t ssize) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      size_t data_size = ssize;
      status = ZE_FUNC(zetMetricStreamerReadData)(streamer, UINT32_MAX, &data_size, storage);
      if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
          std::cerr << "[WARNING] Metric samples dropped." << std::endl;
      }
      else if (status != ZE_RESULT_SUCCESS) {
          std::cerr << "[ERROR] zetMetricStreamerReadData failed with error code: "
              << static_cast<std::size_t>(status) << std::endl;
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }
      return data_size;
  }

  static uint64_t EventBasedReadMetrics(ze_event_handle_t event, zet_metric_streamer_handle_t  streamer, uint8_t *storage, size_t ssize) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    //status = ZE_FUNC(zeEventHostSynchronize)(event, 0);
    status = ZE_FUNC(zeEventQueryStatus)(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);
    if (status == ZE_RESULT_SUCCESS) {
      status = ZE_FUNC(zeEventHostReset)(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
    else {
      // if (status == ZE_RESULT_NOT_READY)
      return 0;
    }

    return ReadMetrics(streamer, storage, ssize);
  }

  static void MetricProfilingThread(ZeDeviceDescriptor *desc) {

    ze_result_t status = ZE_RESULT_SUCCESS;

    ze_context_handle_t context = desc->context_;
    ze_device_handle_t device = desc->device_;
    zet_metric_group_handle_t group = desc->metric_group_;

    status = ZE_FUNC(zetContextActivateMetricGroups)(context, device, 1, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_pool_handle_t event_pool = nullptr;
    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
    status = ZE_FUNC(zeEventPoolCreate)(context, &event_pool_desc, 1, &device, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_handle_t event = nullptr;
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    status = ZE_FUNC(zeEventCreate)(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    zet_metric_streamer_handle_t streamer = nullptr;
    // UNITRACE_SamplingInterval is in us and may be fractional (e.g. "0.5")
    double interval_us = default_sampling_interval_us;
    try {
      interval_us = std::stod(utils::GetEnv("UNITRACE_SamplingInterval"));
    } catch (...) {
      std::cerr << "[WARNING] Invalid sampling interval \"" << utils::GetEnv("UNITRACE_SamplingInterval") << "\", using default " << default_sampling_interval_us << " us" << std::endl;
    }
    if (!(interval_us > 0.0) || interval_us * 1000.0 > double(UINT32_MAX)) {
      std::cerr << "[WARNING] Sampling interval " << interval_us << " us is out of range, using default " << default_sampling_interval_us << " us" << std::endl;
      interval_us = default_sampling_interval_us;
    }
    uint32_t interval = static_cast<uint32_t>(std::llround(interval_us * 1000.0));  // convert us to ns
    if (interval == 0) {
      interval = 1;
    }

    // Work around a driver bug: zetMetricStreamerOpen fails if the sample count is out of
    // the HW-supported range, so retry with fewer samples.
    uint32_t metric_sample_sz = max_metric_samples;
    while (1) {
      zet_metric_streamer_desc_t streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr, metric_sample_sz, interval};
      status = ZE_FUNC(zetMetricStreamerOpen)(context, device, group, &streamer_desc, event, &streamer);
      if (status == ZE_RESULT_SUCCESS) {
        break;
      } else {
        metric_sample_sz = metric_sample_sz >> 2;
        if (metric_sample_sz < 1024) {
          break;
        }
      }
    }
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to open metric streamer for sampling (status = 0x" << std::hex << status << std::dec << "). The sampling interval might be too small or another sampling instance is active." << std::endl;
#ifndef _WIN32
      std::cerr << "[INFO] Please also make sure /proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid is set to 0." << std::endl;
#endif /* _WIN32 */

      status = ZE_FUNC(zeEventDestroy)(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      status = ZE_FUNC(zeEventPoolDestroy)(event_pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      // set state to enabled to let the parent thread continue
      desc->profiling_state_.store(PROFILER_ENABLED, std::memory_order_release);
      return;
    }

    std::vector<std::string> metrics_list;
    metrics_list = GetMetricList(group);
    PTI_ASSERT(!metrics_list.empty());

    auto *raw_metrics = static_cast<uint8_t*>(malloc(sizeof(uint8_t)*MAX_METRIC_BUFFER));
    UniMemory::ExitIfOutOfMemory((void *)raw_metrics);

    auto dump_metrics = [](uint8_t *buffer, uint64_t size, std::ofstream *f) -> bool {
      // write the size in bytes, then the data
      if (f->good()) {
        f->write(reinterpret_cast<char*>(&size), sizeof(size));
      }
      if (f->good()) {
        f->write(reinterpret_cast<char*>(buffer), size);
      }
      return f->good();
    };

    desc->profiling_state_.store(PROFILER_ENABLED, std::memory_order_release);
    while (desc->profiling_state_.load(std::memory_order_acquire) != PROFILER_DISABLED) {
      auto size = EventBasedReadMetrics(event, streamer, raw_metrics, MAX_METRIC_BUFFER);
      if (size > 0) {
        // dump the data to the intermediate file
        if (UniController::IsMetricSamplingEnabled() && !dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
          std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
          break;
        }
      }
    }

    // flush the remaining metrics
    auto size = ReadMetrics(streamer, raw_metrics, MAX_METRIC_BUFFER);
    while (size > 0) {
      if (UniController::IsMetricSamplingEnabled() && !dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
        std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
        break;
      }
      //
      // If less data than requested was read, we can assume that we have reached the end of the stream and
      // can exit the loop however, it might be that less data was read because there was no space available in the buffer
      // and not because we reached the end of the stream. To be safe, we will read again and if we get less than half of the
      // buffer size, we will assume that we have reached the end of the stream.
      //
      if (size < MAX_METRIC_BUFFER / 2)
        break;
      size = ReadMetrics(streamer, raw_metrics, MAX_METRIC_BUFFER);
    }
    free (raw_metrics);

    status = ZE_FUNC(zetMetricStreamerClose)(streamer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = ZE_FUNC(zeEventDestroy)(event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = ZE_FUNC(zeEventPoolDestroy)(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = ZE_FUNC(zetContextActivateMetricGroups)(context, device, 0, &group);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

 private: // Data

  std::vector<ze_context_handle_t> metric_contexts_;
  std::map<ze_device_handle_t, ZeDeviceDescriptor *> device_descriptors_;
  std::string data_dir_name_;
  LoggerFactory* logger_factory_;
  bool idle_sampling_;
};

#endif // PTI_TOOLS_UNITRACE_LEVEL_ZERO_METRICS_H_
