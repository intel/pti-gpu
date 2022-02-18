//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_RESULT_STORAGE_H_
#define PTI_TOOLS_ONEPROF_RESULT_STORAGE_H_

#include <fstream>
#include <vector>

#include "ze_utils.h"

struct DeviceProps {
  uint64_t freq;
  uint64_t mask;
};

struct DeviceInterval {
  uint64_t start;
  uint64_t end;
  uint32_t sub_device_id;
};

struct KernelInterval {
  std::string kernel_name;
  std::vector<DeviceInterval> device_interval_list;
};

struct ResultData {
  uint32_t pid;
  uint32_t device_id;
  uint64_t execution_time;
  std::vector<DeviceProps> device_props_list;
  std::vector<KernelInterval> kernel_interval_list;
  std::string metric_group;
};

class ResultStorage {
 public:
  static std::string GetResultFileName(const std::string& path, uint32_t pid) {
    std::string filename =
      std::string("result.") + std::to_string(pid) + ".bin";
    if (!path.empty()) {
      filename = path + "/" + filename;
    }
    return filename;
  }

  static ResultStorage* Create(const std::string& path, uint32_t pid) {
    std::string filename = GetResultFileName(path, pid);
    PTI_ASSERT(!filename.empty());

    ResultStorage* storage = new ResultStorage(filename);
    PTI_ASSERT(storage != nullptr);

    if (!storage->file_.is_open()) {
      delete storage;
      return nullptr;
    }

    return storage;
  }

  ~ResultStorage() {
    file_.close();
  }

  void Dump(const ResultData* data) {
    PTI_ASSERT(file_.is_open());

    file_.write( // PID
        reinterpret_cast<const char*>(&data->pid),
        sizeof(uint32_t));

    file_.write( // Device ID
        reinterpret_cast<const char*>(&data->device_id),
        sizeof(uint32_t));

    file_.write( // Execution Time
        reinterpret_cast<const char*>(&data->execution_time),
        sizeof(uint64_t));

    DumpDeviceProps(data->device_props_list);
    DumpMetricGroup(data->metric_group);
    DumpKernelIntervals(data->kernel_interval_list);
  }

 private:
  ResultStorage(const std::string& filename) 
      : file_(filename, std::ios::out | std::ios::binary) {}

  void DumpDeviceProps(const std::vector<DeviceProps>& device_props_list) {
    PTI_ASSERT(file_.is_open());

    size_t device_props_list_size = device_props_list.size();
    file_.write( // Device Properties List Size
        reinterpret_cast<const char*>(&device_props_list_size),
        sizeof(size_t));

    for (size_t i = 0; i < device_props_list_size; ++i) {
      const DeviceProps& device_props = device_props_list[i];
      file_.write( // Frequency
          reinterpret_cast<const char*>(&device_props.freq),
          sizeof(uint64_t));
      file_.write( // Mask
          reinterpret_cast<const char*>(&device_props.mask),
          sizeof(uint64_t));
    }
  }

  void DumpMetricGroup(const std::string& metric_group) {
    PTI_ASSERT(!metric_group.empty());
    PTI_ASSERT(file_.is_open());

    size_t metric_group_size = metric_group.size();
    file_.write( // Metic Group Name Size
        reinterpret_cast<const char*>(&metric_group_size),
        sizeof(size_t));

    file_.write( // Metric Group Name
        metric_group.c_str(),
        metric_group_size * sizeof(char));
  }

  void DumpKernelIntervals(
      const std::vector<KernelInterval>& kernel_interval_list) {
    PTI_ASSERT(file_.is_open());

    size_t kernel_interval_list_size = kernel_interval_list.size();
    file_.write( // Kernel Interval List Size
        reinterpret_cast<const char*>(&kernel_interval_list_size),
        sizeof(size_t));

    for (const auto& kernel_interval : kernel_interval_list) {
      const std::string& kernel_name = kernel_interval.kernel_name;

      size_t kernel_name_size = kernel_name.size();
      PTI_ASSERT(kernel_name_size > 0);
      file_.write( // Kernel Name Size
          reinterpret_cast<const char*>(&kernel_name_size),
          sizeof(size_t));

      file_.write( // Kernel Name
          kernel_name.c_str(),
          kernel_name_size * sizeof(char));

      const auto& device_interval_list =
        kernel_interval.device_interval_list;
      size_t device_interval_list_size = device_interval_list.size();
      file_.write( // Device Interval List Size
          reinterpret_cast<const char*>(&device_interval_list_size),
          sizeof(size_t));

      for (const auto& device_interval : device_interval_list) {
        file_.write( // Start Time
            reinterpret_cast<const char*>(&device_interval.start),
            sizeof(uint64_t));
        file_.write( // End Time
            reinterpret_cast<const char*>(&device_interval.end),
            sizeof(uint64_t));
        file_.write( // Sub-device ID
            reinterpret_cast<const char*>(&device_interval.sub_device_id),
            sizeof(uint32_t));
      }
    }
  }

 private:
  std::ofstream file_;
};

class ResultReader {
 public:
  static ResultReader* Create(const std::string& filename) {
    ResultReader* reader = new ResultReader(filename);
    PTI_ASSERT(reader != nullptr);

    if (!reader->file_.is_open()) {
      delete reader;
      return nullptr;
    }

    return reader;
  }

  ~ResultReader() {
    file_.close();
  }

  ResultData* Read() {
    PTI_ASSERT(file_.is_open());

    ResultData* data = new ResultData{};
    PTI_ASSERT(data != nullptr);

    file_.read( // PID
        reinterpret_cast<char*>(&data->pid),
        sizeof(uint32_t));

    file_.read( // Device ID
        reinterpret_cast<char*>(&data->device_id),
        sizeof(uint32_t));

    file_.read( // Execution Time
        reinterpret_cast<char*>(&data->execution_time),
        sizeof(uint64_t));

    data->device_props_list = ReadDeviceProps();
    data->metric_group = ReadMetricGroup();
    data->kernel_interval_list = ReadKernelIntervals();

    return data;
  }

 private:
  ResultReader(const std::string& filename) 
      : file_(filename, std::ios::in | std::ios::binary) {}

  std::vector<DeviceProps> ReadDeviceProps() {
    PTI_ASSERT(file_.is_open());

    size_t device_props_list_size = 0;
    file_.read( // Device Properties List Size
        reinterpret_cast<char*>(&device_props_list_size),
        sizeof(size_t));

    std::vector<DeviceProps> device_props_list;
    for (size_t i = 0; i < device_props_list_size; ++i) {
      DeviceProps device_props{};
      file_.read( // Frequency
          reinterpret_cast<char*>(&device_props.freq),
          sizeof(uint64_t));
      file_.read( // Mask
          reinterpret_cast<char*>(&device_props.mask),
          sizeof(uint64_t));
      device_props_list.push_back(device_props);
    }

    return device_props_list;
  }

  std::string ReadMetricGroup() {
    PTI_ASSERT(file_.is_open());

    size_t metric_group_size = 0;
    file_.read( // Metic Group Name Size
        reinterpret_cast<char*>(&metric_group_size),
        sizeof(size_t));

    std::vector<char> metric_group(metric_group_size);
    file_.read( // Metric Group Name
        metric_group.data(),
        metric_group_size * sizeof(char));

    return std::string(metric_group.begin(), metric_group.end());
  }

  std::vector<KernelInterval> ReadKernelIntervals() {
    PTI_ASSERT(file_.is_open());

    size_t kernel_interval_list_size = 0;
    file_.read( // Kernel Interval List Size
        reinterpret_cast<char*>(&kernel_interval_list_size),
        sizeof(size_t));

    std::vector<KernelInterval> kernel_interval_list;
    for (size_t i = 0; i < kernel_interval_list_size; ++i) {
      KernelInterval kernel_interval{};

      size_t kernel_name_size = 0;
      file_.read( // Kernel Name Size
          reinterpret_cast<char*>(&kernel_name_size),
          sizeof(size_t));

      std::vector<char> kernel_name(kernel_name_size);
      file_.read( // Kernel Name
          kernel_name.data(),
          kernel_name_size * sizeof(char));

      kernel_interval.kernel_name =
        std::string(kernel_name.begin(), kernel_name.end());

      size_t device_interval_list_size = 0;
      file_.read( // Device Interval List Size
          reinterpret_cast<char*>(&device_interval_list_size),
          sizeof(size_t));

      for (size_t j = 0; j < device_interval_list_size; ++j) {
        DeviceInterval device_interval{};

        file_.read( // Start Time
            reinterpret_cast<char*>(&device_interval.start),
            sizeof(uint64_t));
        file_.read( // End Time
            reinterpret_cast<char*>(&device_interval.end),
            sizeof(uint64_t));
        file_.read( // Sub-device ID
            reinterpret_cast<char*>(&device_interval.sub_device_id),
            sizeof(uint32_t));

        kernel_interval.device_interval_list.push_back(device_interval);
      }

      kernel_interval_list.push_back(kernel_interval);
    }

    return kernel_interval_list;
  }

 private:
  std::ifstream file_;
};

#endif // PTI_TOOLS_ONEPROF_RESULT_STORAGE_H_