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
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <atomic>
#include <sstream>
#include <thread>

#include "logger.h"
#include "unimemory.h"
#include "utils.h"
#include "utils_ze.h"
#include "pti_assert.h"
#include "unicontrol.h"
#include <inttypes.h>

constexpr static uint64_t min_dummy_instance_id = 1024 * 1024;	// min dummy instance id if idle sampling is enabled
constexpr static uint32_t max_metric_samples = 32768;

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
  uint64_t host_time_origin_;
  uint64_t device_time_origin_;
  uint64_t device_timer_frequency_;
  uint64_t device_timer_mask_;
  uint64_t metric_time_origin_;
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
    delete logger_;
    if (!log_name_.empty()) {
      std::cerr << "[INFO] Device metrics are stored in " << log_name_ << std::endl;
    }
  }

  ZeMetricProfiler(const ZeMetricProfiler& that) = delete;
  ZeMetricProfiler& operator=(const ZeMetricProfiler& that) = delete;

#ifdef _WIN32
  static void ComputeMetricsQueried(uint32_t pid) {

    std::string metric_group_name = utils::GetEnv("UNITRACE_MetricGroup");
    std::vector<zet_metric_group_handle_t> groups;
    ze_result_t status = ZE_RESULT_SUCCESS;
 
    // get handle of the metric group on each device
    // but initialize L0 first
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
                // no metric group found
                // should not get here
                groups.push_back(nullptr);
              }
            }
            else {
              // unable to get metric groups
              // should get here
              groups.push_back(nullptr);
            }
          }
        }
      }
    }

    // derive computed metrics file name from the log file name if "-o" option is present, otherwise output to stdout
    std::string  log_file_name = utils::GetEnv("UNITRACE_LogFilename");
    Logger *metrics_logger = nullptr;
    std::string computed_metrics_file_name;
    if (log_file_name.empty()) {
      metrics_logger = new Logger(log_file_name);  // output to stdout
    }
    else {
      size_t pos = log_file_name.find_first_of('.');

      if (pos == std::string::npos) {
        computed_metrics_file_name = log_file_name;
      } else {
        computed_metrics_file_name = log_file_name.substr(0, pos);
      }

      computed_metrics_file_name = computed_metrics_file_name + ".metrics." + std::to_string(pid);      // embed the target process id in the final data file

      std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
      if (!rank.empty()) {
        computed_metrics_file_name = computed_metrics_file_name + "." + rank;
      }

      if (pos != std::string::npos) {
        computed_metrics_file_name = computed_metrics_file_name + log_file_name.substr(pos);
      }

      metrics_logger = new Logger(computed_metrics_file_name, true, true);
      if (metrics_logger == nullptr) {
        std::cerr << "[ERROR] Failed to create metric data file" << std::endl;
      }
    }

    // raw metrics data is stored in ".metrics.<pid>.q" in the folder stored in UNITRACE_DataDir
    std::string data_dir = utils::GetEnv("UNITRACE_DataDir");
    std::string metrics_file_name = data_dir + "/.metrics." + std::to_string(pid) + ".q";
    std::ifstream mf(metrics_file_name, std::ios::binary);
    if (!mf) {
      std::cerr << "[ERROR] Could not open the metric data file" << std::endl;
      return;
    }
    int did = -1;
    zet_metric_group_handle_t group = nullptr;
    std::vector<std::string> metric_names;
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
        metrics_logger->Log("\n=== Device #" + std::to_string(did) + " Metrics ===\n");
        std::string header("\nKernel,GlobalInstanceId,SubDeviceId");
        for (auto& metric : metric_names) {
          header += "," + metric;
        }
        header += "\n";
        metrics_logger->Log(header);
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
          std::string str;
          for (uint32_t i = 0; i < num_samples; ++i) {
            str = kname + ",";
            str += std::to_string(instance_id) + ",";
            str += std::to_string(i);
 
            uint32_t size = samples[i];
            const zet_typed_value_t *value = computed_metrics.data() + i * size;
            for (int j = 0; j < size; ++j) {
              str += ",";
              str += PrintTypedValue(value[j]);
            }
            str += "\n";
          }
          str += "\n";
 
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

    if (!log_file_name.empty()) {
      std::cerr << "[INFO] Kernel metrics are stored in " << computed_metrics_file_name << std::endl;
    }
    delete metrics_logger; // close metric data file
  }
#endif /* _WIN32 */

 private:
  std::set<int> devices_to_sample_;

  ZeMetricProfiler(uint32_t app_pid, char *dir, std::string& logfile, bool idle_sampling, std::string &devices_to_sample) {
    data_dir_name_ = std::string(dir);
    if (!logfile.empty()) {
      size_t pos = logfile.find_first_of('.');

      std::string filename;
      if (pos == std::string::npos) {
        filename = logfile;
      } else {
        filename = logfile.substr(0, pos);
      }

      filename = filename + ".metrics." + std::to_string(app_pid);

      std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
      if (!rank.empty()) {
        filename = filename + "." + rank;
      }

      if (pos != std::string::npos) {
        filename = filename + logfile.substr(pos);
      }

      log_name_ = std::move(filename);
    }
    else {
      log_name_ = logfile;
    }

    idle_sampling_ = idle_sampling;

    logger_ = new Logger(log_name_, true, true);
    
    if (devices_to_sample.length() > 0) {
      auto list_devices_str = utils::SplitString (devices_to_sample, ',');
      for (const auto &s : list_devices_str) {
        if (!s.empty()) {
          devices_to_sample_.insert (std::stoi(s.c_str()));
        }
      }
    }

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

        desc->device_timer_frequency_ = GetDeviceTimerFrequency(device);
        desc->device_timer_mask_ = GetDeviceTimestampMask(device);
        desc->metric_timer_frequency_ = GetMetricTimerFrequency(device);
        desc->metric_timer_mask_ = GetMetricTimestampMask(device);

        ze_pci_ext_properties_t pci_device_properties;
        ze_result_t status = ZE_FUNC(zeDevicePciGetPropertiesExt)(device, &pci_device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        desc->pci_properties_ = pci_device_properties;

        zet_metric_group_handle_t group = FindMetricGroup (device, metric_group, ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
        if (group == nullptr) {
          std::cerr << "[ERROR] Invalid metric group " << metric_group << std::endl;
          exit(-1);
        }

        desc->driver_ = driver;
        desc->context_ = context;
        desc->metric_group_ = group;

        uint64_t host_time;
        uint64_t ticks;
        uint64_t device_time;
        uint64_t metric_time;

        ZE_FUNC(zeDeviceGetGlobalTimestamps)(device, &host_time, &ticks);

        device_time = ticks & desc->device_timer_mask_;
        device_time = device_time * NSEC_IN_SEC / desc->device_timer_frequency_;

        metric_time = ticks & desc->metric_timer_mask_;
        metric_time = metric_time * NSEC_IN_SEC / desc->metric_timer_frequency_;

        desc->host_time_origin_ = host_time;
        desc->device_time_origin_ = device_time;
        desc->metric_time_origin_ = metric_time;

        desc->profiling_thread_ = nullptr;
        desc->profiling_state_.store(PROFILER_DISABLED, std::memory_order_release);

        desc->metric_file_name_ = std::string(dir) + "/." + std::to_string(global_dev_cnt) + "." + metric_group + "." + std::to_string(utils::GetPid()) + ".t";

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

          sub_desc->device_timer_frequency_ = GetDeviceTimerFrequency(sub_devices[j]);
          sub_desc->device_timer_mask_ = GetDeviceTimestampMask(sub_devices[j]);
          sub_desc->metric_timer_frequency_ = GetMetricTimerFrequency(sub_devices[j]);
          sub_desc->metric_timer_mask_ = GetMetricTimestampMask(sub_devices[j]);

          ze_pci_ext_properties_t pci_device_properties;
          ze_result_t status = ZE_FUNC(zeDevicePciGetPropertiesExt)(sub_devices[j], &pci_device_properties);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          
          sub_desc->pci_properties_ = pci_device_properties;

          uint64_t ticks;
          uint64_t host_time;
          uint64_t device_time;
          uint64_t metric_time;

          ZE_FUNC(zeDeviceGetGlobalTimestamps)(sub_devices[j], &host_time, &ticks);
          device_time = ticks & sub_desc->device_timer_mask_;
          device_time = device_time * NSEC_IN_SEC / sub_desc->device_timer_frequency_;

          metric_time = ticks & sub_desc->metric_timer_mask_;
          metric_time = metric_time * NSEC_IN_SEC / sub_desc->metric_timer_frequency_;

          sub_desc->host_time_origin_ = host_time;
          sub_desc->device_time_origin_ = device_time;
          sub_desc->metric_time_origin_ = metric_time;

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

  void ComputeMetricsSampled() {
    auto *raw_metrics = static_cast<uint8_t*>(malloc(sizeof(uint8_t)*MAX_METRIC_BUFFER));
    UniMemory::ExitIfOutOfMemory((void *)raw_metrics);
    
    for (auto [handle, device] : device_descriptors_) {
      if (device->parent_device_ != nullptr) {
        // subdevice
        continue;
      }

      if (device->stall_sampling_) {
        std::map<uint64_t, std::pair<std::string, size_t>> kprops;
        size_t max_kname_size = 0;
        // enumerate all kernel property files
        for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir_name_))) {
          // kernel properties file path: <data_dir>/.kprops.<device_id>.<pid>.txt
          if (e.path().filename().string().find(".kprops." + std::to_string(device->device_id_)) == 0) {
            std::ifstream kpf = std::ifstream(e.path());
            if (!kpf.is_open()) {
              continue;
            }

            while (!kpf.eof()) {
              std::string kname;
              std::string line;
              size_t size;
              uint64_t base_addr;

              std::getline(kpf, kname);
              if (kpf.eof()) {
                break;
              }

              std::getline(kpf, line);
              if (kpf.eof()) {
                break;
              }
              base_addr = std::strtol(line.c_str(), nullptr, 0);

              line.clear();
              std::getline(kpf, line);
              size = std::strtol(line.c_str(), nullptr, 0);

              if (kname.size() > max_kname_size) {
                max_kname_size = kname.size();
              }
              kprops.insert({base_addr, {std::move(kname), size}});
            }
            kpf.close();
          }
        }
        if (kprops.size() == 0) {
          continue;
        }

        std::vector<std::string> metric_list;
        metric_list = GetMetricList(device->metric_group_);
        PTI_ASSERT(!metric_list.empty());

        uint32_t ip_idx = GetMetricId(metric_list, "IP");
        if (ip_idx >= metric_list.size()) {
          // no IP metric
          continue;
        }

        constexpr uint32_t max_num_of_stall_types = 16;	// should be enough for all current platforms

        if ((metric_list.size() - 1) > max_num_of_stall_types) { // metric_list.size() includes IP
          std::cerr << "[ERROR] Number of stall types exceeds supported limit of " << max_num_of_stall_types << std::endl;
          return;
        }

        std::ifstream inf = std::ifstream(device->metric_file_name_, std::ios::in | std::ios::binary);
        if (!inf.is_open()) {
          continue;
        }

        std::map<uint64_t, std::array<uint64_t, max_num_of_stall_types>> eustalls;

        while (!inf.eof()) {
          // Read metric data in two stages, first actual size (in bytes), followed by actual metrics
          uint64_t data_size;
          inf.read(reinterpret_cast<char *>(&data_size), sizeof(data_size));
          if (inf.eof()) {
            // If we reached EOF, we can stop processing
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

                // IP address is already processed. (metric_list.size() - 1) is the number of types of stall
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
          continue;
        }

        std::string header("\n=== Device #");

        header += std::to_string(device->device_id_) + " Metrics ===\n\n";
        int field_sizes[11];
        field_sizes[0] = max_kname_size;
        field_sizes[1] = std::max(sizeof("0x00000000") - 1, metric_list[0].size());
        header += std::string(std::max(int(field_sizes[0] + 1 - sizeof("Kernel")), 0), ' ') + "Kernel, ";
        logger_->Log(header);  // in case the kernel name is too long
        header = std::string(std::max(int(field_sizes[1] - metric_list[0].length()), 0), ' ') + metric_list[0];
        for (size_t i = 1; i <  metric_list.size(); i++) {
          field_sizes[i + 1] = metric_list[i].size();
          header += ", " + metric_list[i];
        }

        header += "\n";
        logger_->Log(header);

        for (auto [ip, stall] : eustalls) {
          for (auto rit = kprops.crbegin(); rit != kprops.crend(); ++rit) {
            if ((rit->first <= ip) && ((ip - rit->first) < rit->second.second)) {
              std::string line;

              char offset[128];
              snprintf(offset, sizeof(offset), "%" PRIx64, (ip - rit->first));
              line = std::string(std::max(int(field_sizes[0] - rit->second.first.length()), 0), ' ') +
                     rit->second.first + ", ";
              logger_->Log(line);
              line = std::string(std::max(int(field_sizes[1] - std::string(offset).length()), 0), ' ') +
                     std::string(offset) + ", ";
              // the last stall type is processed after this loop
              for (uint32_t k = 0; k < (metric_list.size() - 2); k++) {
                line += std::string(std::max(int(field_sizes[k + 2] - std::to_string(stall[k]).length()), 0), ' ') +
                        std::to_string(stall[k]) + ", ";
              }
              // do not forget the last stall type
              line += std::string(std::max(int(field_sizes[metric_list.size() - 2] - std::to_string(stall[metric_list.size() - 2]).length()), 0), ' ') +
                      std::to_string(stall[metric_list.size() - 2]) + "\n";
              logger_->Log(line);
              break;
            }
          }
        }
      }
      else {
        std::vector<ZeKernelInfo> kinfo;
        uint64_t max_global_instance_id = 0;
        // enumerate all kernel time files
        for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir_name_))) {
          // kernel properties file path: <data_dir>/.ktime.<device_id>.<pid>.txt
          if (e.path().filename().string().find(".ktime." + std::to_string(device->device_id_)) == 0) {
            std::ifstream kf = std::ifstream(e.path());
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

              info.metric_start = info.metric_start & device->metric_timer_mask_;
              info.metric_start = info.metric_start * (NSEC_IN_SEC / device->metric_timer_frequency_);

              info.metric_end = info.metric_end & device->metric_timer_mask_;
              info.metric_end = info.metric_end * (NSEC_IN_SEC / device->metric_timer_frequency_);

              if (info.metric_end < info.metric_start) {
                // clock wraps around
                info.metric_end += (device->metric_timer_mask_ + 1UL) * (NSEC_IN_SEC / device->metric_timer_frequency_);
              }
              std::getline(kf, info.kernel_name);
              if ((info.metric_start != 0) && (info.metric_end != 0) && (!info.kernel_name.empty())) {
                kinfo.push_back(std::move(info));
              }
            }
            kf.close();
          }
        }

        if (kinfo.empty()) {
          continue;
        }
        std::sort(kinfo.begin(), kinfo.end(), CompareInterval);

        auto metric_list = GetMetricList(device->metric_group_);
        PTI_ASSERT(!metric_list.empty());

        uint32_t ts_idx = GetMetricId(metric_list, "QueryBeginTime");
        if (ts_idx >= metric_list.size()) {
          // no QueryBeginTime metric
          continue;
        }

        //TODO: handle subdevices in case of implicit scaling
        uint64_t time_span_between_clock_resets = (device->metric_timer_mask_ + 1ull) * (static_cast<uint64_t>(NSEC_IN_SEC) / device->metric_timer_frequency_);

        std::ifstream inf = std::ifstream(device->metric_file_name_, std::ios::in | std::ios::binary);
        if (!inf.is_open()) {
          continue;
        }

        std::string header("\n=== Device #");

        header += std::to_string(device->device_id_) + " Metrics ===\n";
        logger_->Log(header);

        header = "\nKernel, GlobalInstanceId";
        for (size_t i = 0; i <  metric_list.size(); i++) {
          header += ", " + metric_list[i];
        }

        header += "\n";
        logger_->Log(header);

        uint64_t dummy_global_instance_id = max_global_instance_id + min_dummy_instance_id;  // for samples that do not belong to any kernel

        uint64_t cur_sampling_ts = 0;
        auto kit = kinfo.begin();
        while (!inf.eof() && kit != kinfo.end()) {
          // Read metric data in two stages, first actual size (in bytes), followed by actual metrics
          uint64_t data_size;
          inf.read(reinterpret_cast<char *>(&data_size), sizeof(data_size));
          if (inf.eof()) {
            // If we reached EOF, we can stop processing
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
            bool kernelsampled = false;
            bool idle = false;	// is the sample collected while the device is idle?
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
                  if (idle) {
                    logger_->Log("\n");	// separate from samples that do not belong to any kernels/commands
                    idle = false;
                  }

                  // belong to this kernel
                  kernelsampled = true;
                  str = kit->kernel_name + ", ";
                  logger_->Log(str);	// in case the kernel name is too long
                  str = std::to_string(kit->global_instance_id);
                  for (size_t k = 0; k < metric_list.size(); k++) {
                    str += ", ";
                    if (k == ts_idx) {
                      str += std::to_string(ts);
                    }
                    else{
                      str += PrintTypedValue(v[k]);
                    }
                  }
                  str += "\n";
                  logger_->Log(str);
                }
                else {
                  if (ts > kit->metric_end) {
                    if (kernelsampled) {
                      logger_->Log("\n");
                      kernelsampled = false;	// reset for next kernel
                    }
                    kit++;	// move to next kernel
                    dummy_global_instance_id++;
                    if (kit == kinfo.end()) {
                      break;	// we are done
                    }
                  }
                  else { // ts < kit->metric_start
                    // does not belong to any kernel/command
                    if (idle_sampling_) {
                      auto sz = kit->kernel_name.size();
                      if (sz > 2) {
                        str = "\"NoKernel(Before " + kit->kernel_name.substr(1, sz - 2)  + ")\", ";
                      }
                      else {
                        str = "\"NoKernel\", ";
                      }
                      logger_->Log(str);	// in case the kernel name is too long
                      str = std::to_string(dummy_global_instance_id);
                      for (size_t k = 0; k < metric_list.size(); k++) {
                        str += ", ";
                        if (k == ts_idx) {
                          str += std::to_string(ts);
                        }
                        else{
                          str += PrintTypedValue(v[k]);
                        }
                      }
                      str += "\n";
                      logger_->Log(str);
                      idle = true;
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
    }
    free (raw_metrics);
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
    uint32_t interval = std::stoi(utils::GetEnv("UNITRACE_SamplingInterval")) * 1000;	// convert us to ns

    zet_metric_streamer_desc_t streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC, nullptr, max_metric_samples, interval};
    status = ZE_FUNC(zetMetricStreamerOpen)(context, device, group, &streamer_desc, event, &streamer);
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
      // Write metric data in two stages, first actual size (in bytes), followed by actual metrics
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
        // If we have data, dump it to the intermediate file
        if (UniController::IsCollectionEnabled() && !dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
          std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
          break;
        }
      }
    }

    // Flush the remaining metrics after the profiler has stopped
    auto size = ReadMetrics(streamer, raw_metrics, MAX_METRIC_BUFFER);
    while (size > 0) {
      if (UniController::IsCollectionEnabled() && !dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
        std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
        break;
      }
      if (size < MAX_METRIC_BUFFER)
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
  Logger* logger_;
  std::string log_name_;
  bool idle_sampling_;
};

#endif // PTI_TOOLS_UNITRACE_LEVEL_ZERO_METRICS_H_

