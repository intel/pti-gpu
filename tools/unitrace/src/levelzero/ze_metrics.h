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
#include "unitrace_ze_utils.h"
#include "pti_assert.h"
#include <inttypes.h>

constexpr static uint64_t min_dummy_instance_id = 1024 * 1024;	// min dummy instance id if idle sampling is enabled
constexpr static uint32_t max_metric_samples = 32768;

#define MAX_METRIC_BUFFER  (8ULL * 1024ULL * 1024ULL)

inline void PrintDeviceList() {
  ze_result_t status = ZE_FUNC(zeInit)(ZE_INIT_FLAG_GPU_ONLY);
  if (status != ZE_RESULT_SUCCESS) {
    std::cerr << "[ERROR] Failed to initialize Level Zero runtime" << std::endl;
#ifndef _WIN32
    std::cerr << "[INFO] Please ensure that either /proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid is set to 0." << std::endl;
#endif /* _WIN32 */
    return;
  }
  
  std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No devices found" << std::endl;
    return;
  }

  for (size_t i = 0; i < device_list.size(); ++i) {
    ze_device_properties_t device_properties{
        ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    status = ZE_FUNC(zeDeviceGetProperties)(device_list[i], &device_properties);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_pci_ext_properties_t pci_props{ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES, };
    status = ZE_FUNC(zeDevicePciGetPropertiesExt)(device_list[i], &pci_props);
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
  ze_result_t status = ZE_FUNC(zeInit)(ZE_INIT_FLAG_GPU_ONLY);
  if (status != ZE_RESULT_SUCCESS) {
    std::cerr << "[ERROR] Failed to initialize Level Zero runtime" << std::endl;
#ifndef _WIN32
    std::cerr << "[INFO] Please ensure that either /proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid is set to 0." << std::endl;
#endif /* _WIN32 */
    return;
  }

  std::vector<ze_device_handle_t> device_list = utils::ze::GetDeviceList();
  if (device_list.empty()) {
    std::cout << "[WARNING] No devices found" << std::endl;
    return;
  }

  PTI_ASSERT(device_id < device_list.size());
  ze_device_handle_t device = device_list[device_id];

  uint32_t group_count = 0;
  status = ZE_FUNC(zetMetricGroupGet)(device, &group_count, nullptr);
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
        utils::ze::GetResultType(metric_props.resultType) << ", " <<
        utils::ze::GetMetricType(metric_props.metricType) << ", " <<
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
  int32_t parent_device_id_;
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
  static ZeMetricProfiler* Create(char *dir, std::string& logfilename, bool idle_sampling) {
    ZeMetricProfiler* profiler = new ZeMetricProfiler(dir, logfilename, idle_sampling);
    UniMemory::ExitIfOutOfMemory((void *)(profiler));

    profiler->StartProfilingMetrics();

    return profiler;
  }

  ~ZeMetricProfiler() {
    StopProfilingMetrics();
    ComputeMetrics();
    delete logger_;
    if (!log_name_.empty()) {
      std::cerr << "[INFO] Device metrics are stored in " << log_name_ << std::endl;
    }
  }

  ZeMetricProfiler(const ZeMetricProfiler& that) = delete;
  ZeMetricProfiler& operator=(const ZeMetricProfiler& that) = delete;

 private:
  ZeMetricProfiler(char *dir, std::string& logfile, bool idle_sampling) {
    data_dir_name_ = std::string(dir);
    if (!logfile.empty()) {
      size_t pos = logfile.find_first_of('.');

      std::string filename;
      if (pos == std::string::npos) {
        filename = logfile;
      } else {
        filename = logfile.substr(0, pos);
      }

      filename = filename + ".metrics." + std::to_string(utils::GetPid());

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
     
    EnumerateDevices(dir);
  }

  void EnumerateDevices(char *dir) {

    std::string metric_group = utils::GetEnv("UNITRACE_MetricGroup");
    std::string output_dir = utils::GetEnv("UNITRACE_TraceOutputDir");

    bool stall_sampling = false;
    if (metric_group == "EuStallSampling") {
      stall_sampling = true;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t num_drivers = 0;
    status = ZE_FUNC(zeDriverGet)(&num_drivers, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (num_drivers > 0) {
      int32_t did = 0;
      std::vector<ze_driver_handle_t> drivers(num_drivers);
      status = ZE_FUNC(zeDriverGet)(&num_drivers, drivers.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      for (auto driver : drivers) {
        ze_context_handle_t context = nullptr;
        ze_context_desc_t cdesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

        status = ZE_FUNC(zeContextCreate)(driver, &cdesc, &context);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        metric_contexts_.push_back(context);

        uint32_t num_devices = 0;
        status = ZE_FUNC(zeDeviceGet)(driver, &num_devices, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        if (num_devices > 0) {
          std::vector<ze_device_handle_t> devices(num_devices);
          status = ZE_FUNC(zeDeviceGet)(driver, &num_devices, devices.data());
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          for (auto device : devices) {
            uint32_t num_sub_devices = 0;
            status = ZE_FUNC(zeDeviceGetSubDevices)(device, &num_sub_devices, nullptr);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      
            ZeDeviceDescriptor *desc = new ZeDeviceDescriptor;
            UniMemory::ExitIfOutOfMemory((void *)(desc));

            desc->stall_sampling_ = stall_sampling;

            desc->device_ = device;
            desc->device_id_ = did;
            desc->parent_device_id_ = -1;	// no parent device
            desc->parent_device_ = nullptr;
            desc->subdevice_id_ = -1;		// not a subdevice
            desc->num_sub_devices_ = num_sub_devices;

            desc->device_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(device);
            desc->device_timer_mask_ = utils::ze::GetDeviceTimestampMask(device);
            desc->metric_timer_frequency_ = utils::ze::GetMetricTimerFrequency(device);
            desc->metric_timer_mask_ = utils::ze::GetMetricTimestampMask(device);

            ze_pci_ext_properties_t pci_device_properties;
            ze_result_t status = ZE_FUNC(zeDevicePciGetPropertiesExt)(device, &pci_device_properties);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            desc->pci_properties_ = pci_device_properties;

            zet_metric_group_handle_t group = nullptr;
            uint32_t num_groups = 0;
            status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, nullptr);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            if (num_groups > 0) {
              std::vector<zet_metric_group_handle_t> groups(num_groups, nullptr);
              status = ZE_FUNC(zetMetricGroupGet)(device, &num_groups, groups.data());
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);

              for (uint32_t k = 0; k < num_groups; ++k) {
                zet_metric_group_properties_t group_props{};
                group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                status = ZE_FUNC(zetMetricGroupGetProperties)(groups[k], &group_props);
                PTI_ASSERT(status == ZE_RESULT_SUCCESS);

                if ((strcmp(group_props.name, metric_group.c_str()) == 0) && (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED)) {
                  group = groups[k];
                  break;
                }
              }
            }

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

            desc->metric_file_name_ = std::string(dir) + "/." + std::to_string(did) + "." + metric_group + "." + std::to_string(utils::GetPid()) + ".t";

            desc->metric_file_stream_ = std::ofstream(desc->metric_file_name_, std::ios::out | std::ios::trunc | std::ios::binary);

            device_descriptors_.insert({device, desc});

            if (num_sub_devices > 0) {
              std::vector<ze_device_handle_t> sub_devices(num_sub_devices);

              status = ZE_FUNC(zeDeviceGetSubDevices)(device, &num_sub_devices, sub_devices.data());
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);

              for (int j = 0; j < num_sub_devices; j++) {
                ZeDeviceDescriptor *sub_desc = new ZeDeviceDescriptor;
                UniMemory::ExitIfOutOfMemory((void *)(sub_desc));
  
                sub_desc->stall_sampling_ = stall_sampling;

                sub_desc->device_ = sub_devices[j];
                sub_desc->device_id_ = did;		// subdevice
                sub_desc->parent_device_id_ = did;	// parent device
                sub_desc->parent_device_ = device;
                sub_desc->subdevice_id_ = j;		// a subdevice
                sub_desc->num_sub_devices_ = 0;

                sub_desc->driver_ = driver;
                sub_desc->context_ = context;
                sub_desc->metric_group_ = group;

                sub_desc->device_timer_frequency_ = utils::ze::GetDeviceTimerFrequency(sub_devices[j]);
                sub_desc->device_timer_mask_ = utils::ze::GetDeviceTimestampMask(sub_devices[j]);
                sub_desc->metric_timer_frequency_ = utils::ze::GetMetricTimerFrequency(sub_devices[j]);
                sub_desc->metric_timer_mask_ = utils::ze::GetMetricTimestampMask(sub_devices[j]);
  
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
                sub_desc->metric_file_name_ = std::string(dir) + "/." + std::to_string(did) + "." + std::to_string(j) + "." + metric_group + "." + std::to_string(utils::GetPid()) + ".t";
                sub_desc->metric_file_stream_ = std::ofstream(sub_desc->metric_file_name_, std::ios::out | std::ios::trunc | std::ios::binary);
#endif /* 0 */

                device_descriptors_.insert({sub_devices[j], sub_desc});
              }
            }
            did++;
          }
        }
      }
    }
  }

  int GetDeviceId(ze_device_handle_t sub_device) const {
    auto it = device_descriptors_.find(sub_device);
    if (it != device_descriptors_.end()) {
      return it->second->device_id_;
    }
    return -1;
  }

  int GetSubDeviceId(ze_device_handle_t sub_device) const {
    auto it = device_descriptors_.find(sub_device);
    if (it != device_descriptors_.end()) {
      return it->second->subdevice_id_;
    }
    return -1;
  }

  ze_device_handle_t GetParentDevice(ze_device_handle_t sub_device) const {
    auto it = device_descriptors_.find(sub_device);
    if (it != device_descriptors_.end()) {
      return it->second->parent_device_;
    }
    return nullptr;
  }

  void StartProfilingMetrics(void) {

    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
      it->second->profiling_thread_ = new std::thread(MetricProfilingThread, it->second);
      while (it->second->profiling_state_.load(std::memory_order_acquire) != PROFILER_ENABLED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  void StopProfilingMetrics() {
   
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice 
        continue;
      }
      PTI_ASSERT(it->second->profiling_thread_ != nullptr);
      PTI_ASSERT(it->second->profiling_state_ == PROFILER_ENABLED);
      it->second->profiling_state_.store(PROFILER_DISABLED, std::memory_order_release);
      it->second->profiling_thread_->join();
      delete it->second->profiling_thread_;
      it->second->profiling_thread_ = nullptr;
      it->second->metric_file_stream_.close();
    }
  }

  struct ZeKernelInfo {
    int32_t subdevice_id;
    uint64_t global_instance_id;
    uint64_t metric_start;
    uint64_t metric_end;
    std::string kernel_name;
  };

 static  bool CompareInterval(ZeKernelInfo& iv1, ZeKernelInfo& iv2) {
    return (iv1.metric_start < iv2.metric_start);
  }

  void ComputeMetrics() {
    auto *raw_metrics = static_cast<uint8_t*>(malloc(sizeof(uint8_t)*MAX_METRIC_BUFFER));
    UniMemory::ExitIfOutOfMemory((void *)raw_metrics);
  
    for (auto it = device_descriptors_.begin(); it != device_descriptors_.end(); ++it) {
      if (it->second->parent_device_ != nullptr) {
        // subdevice
        continue;
      }
 
      if (it->second->stall_sampling_) {
        std::map<uint64_t, std::pair<std::string, size_t>> kprops;
        int max_kname_size = 0;
        // enumerate all kernel property files
        for (const auto& e: CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir_name_))) {
          // kernel properties file path: <data_dir>/.kprops.<device_id>.<pid>.txt
          if (e.path().filename().string().find(".kprops." + std::to_string(it->second->device_id_)) == 0) {
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
       
        struct EuStalls {
          uint64_t active_;
          uint64_t control_;
          uint64_t pipe_;
          uint64_t send_;
          uint64_t dist_;
          uint64_t sbid_;
          uint64_t sync_;
          uint64_t insfetch_;
          uint64_t other_;
        };

        std::map<uint64_t, EuStalls> eustalls;

        std::vector<std::string> metric_list;
        metric_list = GetMetricList(it->second->metric_group_);
        PTI_ASSERT(!metric_list.empty());
  
        uint32_t ip_idx = GetMetricId(metric_list, "IP");
        if (ip_idx >= metric_list.size()) {
          // no IP metric 
          continue;
        }
  
        std::ifstream inf = std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
        
        if (!inf.is_open()) {
          continue;
        }

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
            ze_result_t status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
              it->second->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
              raw_size, raw_metrics, &num_samples, &num_metrics,
              nullptr, nullptr);
            if ((status != ZE_RESULT_SUCCESS) || (num_samples == 0) || (num_metrics == 0)) {
              std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
              continue;
            }

            std::vector<uint32_t> samples(num_samples);
            std::vector<zet_typed_value_t> metrics(num_metrics);

            status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
              it->second->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
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
                  
              for (int j = 0; j < size; j += metric_list.size()) {
                uint64_t ip;
                ip = (value[j + 0].value.ui64 << 3);
                if (ip == 0) {
                  continue;
                }
                EuStalls stall;
                stall.active_ = value[j + 1].value.ui64;
                stall.control_ = value[j + 2].value.ui64;
                stall.pipe_ = value[j + 3].value.ui64;
                stall.send_ = value[j + 4].value.ui64;
                stall.dist_ = value[j + 5].value.ui64;
                stall.sbid_ = value[j + 6].value.ui64;
                stall.sync_ = value[j + 7].value.ui64;
                stall.insfetch_ = value[j + 8].value.ui64;
                stall.other_ = value[j + 9].value.ui64;
                auto eit = eustalls.find(ip);
                if (eit != eustalls.end()) {
                  eit->second.active_ += stall.active_;
                  eit->second.control_ += stall.control_;
                  eit->second.pipe_ += stall.pipe_;
                  eit->second.send_ += stall.send_;
                  eit->second.dist_ += stall.dist_;
                  eit->second.sbid_ += stall.sbid_;
                  eit->second.sync_ += stall.sync_;
                  eit->second.insfetch_ += stall.insfetch_;
                  eit->second.other_ += stall.other_;
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

        header += std::to_string(it->second->device_id_) + " Metrics ===\n\n";
        int field_sizes[11];
        field_sizes[0] = max_kname_size; 
        field_sizes[1] = std::max(sizeof("0x00000000") - 1, metric_list[0].size());
        header += std::string(std::max(int(field_sizes[0] + 1 - sizeof("Kernel")), 0), ' ') + "Kernel, ";
        logger_->Log(header);	// in case the kernel name is too long
        header = std::string(std::max(int(field_sizes[1] - metric_list[0].length()), 0), ' ') + metric_list[0];
        for (int i = 1; i <  metric_list.size(); i++) {
          field_sizes[i + 1] = metric_list[i].size();
          header += ", " + metric_list[i];
        }
        
        header += "\n";
        
        logger_->Log(header);

        for (auto it = eustalls.begin(); it != eustalls.end(); it++) {
          for (auto rit = kprops.crbegin(); rit != kprops.crend(); ++rit) {
            if ((rit->first <= it->first) && ((it->first - rit->first) < rit->second.second)) {
              std::string line;

              char offset[128];
              snprintf(offset, sizeof(offset), "%" PRIx64, (it->first - rit->first));
              line = std::string(std::max(int(field_sizes[0] - rit->second.first.length()), 0), ' ') +
                     rit->second.first + ", ";
              logger_->Log(line);
              line = std::string(std::max(int(field_sizes[1] - std::string(offset).length()), 0), ' ') +
                     std::string(offset) + ", " +
                     std::string(std::max(int(field_sizes[2] - std::to_string(it->second.active_).length()), 0), ' ') +
                     std::to_string(it->second.active_) + ", " +
                     std::string(std::max(int(field_sizes[3] - std::to_string(it->second.control_).length()), 0), ' ') +
                     std::to_string(it->second.control_) + ", " +
                     std::string(std::max(int(field_sizes[4] - std::to_string(it->second.pipe_).length()), 0), ' ') +
                     std::to_string(it->second.pipe_) + ", " +
                     std::string(std::max(int(field_sizes[5] - std::to_string(it->second.send_).length()), 0), ' ') +
                     std::to_string(it->second.send_) + ", " +
                     std::string(std::max(int(field_sizes[6] - std::to_string(it->second.dist_).length()), 0), ' ') +
                     std::to_string(it->second.dist_) + ", " +
                     std::string(std::max(int(field_sizes[7] - std::to_string(it->second.sbid_).length()), 0), ' ') +
                     std::to_string(it->second.sbid_) + ", " +
                     std::string(std::max(int(field_sizes[8] - std::to_string(it->second.sync_).length()), 0), ' ') +
                     std::to_string(it->second.sync_) + ", " +
                     std::string(std::max(int(field_sizes[9] - std::to_string(it->second.insfetch_).length()), 0), ' ') +
                     std::to_string(it->second.insfetch_) + ", " +
                     std::string(std::max(int(field_sizes[10] - std::to_string(it->second.other_).length()), 0), ' ') +
                     std::to_string(it->second.other_) + "\n";
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
          if (e.path().filename().string().find(".ktime." + std::to_string(it->second->device_id_)) == 0) {
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

        std::vector<std::string> metric_list;
        metric_list = GetMetricList(it->second->metric_group_);
        PTI_ASSERT(!metric_list.empty());
  
        uint32_t ts_idx = GetMetricId(metric_list, "QueryBeginTime");
        if (ts_idx >= metric_list.size()) {
          // no QueryBeginTime metric 
          continue;
        }
  
        //TODO: handle subdevices in case of implicit scaling
        uint64_t time_span_between_clock_resets = (it->second->metric_timer_mask_ + 1ull) * static_cast<uint64_t>(NSEC_IN_SEC) / it->second->metric_timer_frequency_;
  
        std::ifstream inf = std::ifstream(it->second->metric_file_name_, std::ios::in | std::ios::binary);
        if (!inf.is_open()) {
          continue;
        }

        std::string header("\n=== Device #");

        header += std::to_string(it->second->device_id_) + " Metrics ===\n";
        logger_->Log(header);

        header = "\nKernel, GlobalInstanceId";
        for (int i = 0; i <  metric_list.size(); i++) {
          header += ", " + metric_list[i];
        }
        
        header += "\n";
        
        logger_->Log(header);

        uint64_t dummy_global_instance_id = max_global_instance_id + min_dummy_instance_id;  // for samples that do not belong to any kernel

        uint64_t cur_sampling_ts = 0;
        auto kit = kinfo.begin();
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
            ze_result_t status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
              it->second->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
              raw_size, raw_metrics, &num_samples, &num_metrics,
              nullptr, nullptr);
            if ((status != ZE_RESULT_SUCCESS) || (num_samples == 0) || (num_metrics == 0)) {
              std::cerr << "[WARNING] Unable to calculate metrics (status = 0x" << std::hex << status << std::dec << ") num_samples = " << num_samples << " num_metrics = " << num_metrics << std::endl;
              continue;
            }

            std::vector<uint32_t> samples(num_samples);
            std::vector<zet_typed_value_t> metrics(num_metrics);
  
            status = ZE_FUNC(zetMetricGroupCalculateMultipleMetricValuesExp)(
              it->second->metric_group_, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
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

              for (int j = 0; j < (size / metric_list.size()); ++j) {
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
                  for (int k = 0; k < metric_list.size(); k++) {
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
                      for (int k = 0; k < metric_list.size(); k++) {
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
        if (!dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
          std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
          break;
        }
      }
    }

    // Flush the remaining metrics after the profiler has stopped
    auto size = ReadMetrics(streamer, raw_metrics, MAX_METRIC_BUFFER);
    while (size > 0) {
      if (!dump_metrics (raw_metrics, size, &desc->metric_file_stream_)) {
        std::cerr << "[ERROR] Failed to write to sampling metrics file " << desc->metric_file_name_ << std::endl;
        break;
      }
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

