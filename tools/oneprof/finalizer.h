//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_FINALIZER_H_
#define PTI_TOOLS_ONEPROF_FINALIZER_H_

#include "logger.h"
#include "metric_storage.h"
#include "prof_options.h"
#include "prof_utils.h"
#include "result_storage.h"
#include "utils.h"

class Finalizer {
 public:
  static Finalizer* Create(const ProfOptions& options) {
    if (options.GetFlags() == 0) {
      return nullptr;
    }

    std::string filename = options.GetResultFile();
    PTI_ASSERT(!filename.empty());

    ResultReader* reader = ResultReader::Create(filename);
    if (reader == nullptr) {
      std::cerr << "[ERROR] Unable to open " << filename << std::endl;
      return nullptr;
    }

    ResultData* data = reader->Read();
    PTI_ASSERT(data != nullptr);

    delete reader;
    return new Finalizer(data, options);
  }

 public:
  ~Finalizer() {
    PTI_ASSERT(data_ != nullptr);
    delete data_;
  }

  void Report() {
    PTI_ASSERT(data_ != nullptr);

    logger_.Log("\n");
    logger_.Log("=== Profiling Results ===\n");
    logger_.Log("\n");

    std::stringstream header;
    header << "Total Execution Time: " <<
      data_->execution_time << " ns" << std::endl;
    logger_.Log(header.str());

    if (options_.CheckFlag(PROF_KERNEL_INTERVALS)) {
      ReportKernelIntervals();
    }

    if (options_.CheckFlag(PROF_RAW_METRICS) ||
        options_.CheckFlag(PROF_KERNEL_METRICS) ||
        options_.CheckFlag(PROF_AGGREGATION)) {
      bool are_metrics_found = ComputeMetrics();
      if (!are_metrics_found) {
        std::cerr << "[WARNING] No metric results found" << std::endl;
        return;
      }
    }

    std::vector< std::vector<uint64_t> > cache;
    if (options_.CheckFlag(PROF_KERNEL_METRICS) ||
        options_.CheckFlag(PROF_AGGREGATION)) {
      cache = MakeCache();
    }

    if (options_.CheckFlag(PROF_RAW_METRICS)) {
      ReportRawMetrics();
    }

    if (options_.CheckFlag(PROF_KERNEL_METRICS)) {
      ReportKernelMetrics(cache);
    }

    if (options_.CheckFlag(PROF_AGGREGATION)) {
      ReportAggregatedMetrics(cache);
    }
  }

  Finalizer(const Finalizer& copy) = delete;
  Finalizer& operator=(const Finalizer& copy) = delete;

 private:
  static uint32_t GetMetricCount(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return group_props.metricCount;
  }

  static std::vector<std::string> GetMetricList(
      zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    uint32_t metric_count = GetMetricCount(group);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metric_list(metric_count);
    ze_result_t status = zetMetricGet(
        group, &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metric_list.size());

    std::vector<std::string> name_list;
    for (auto metric : metric_list) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = zetMetricGetProperties(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::string units = GetMetricUnits(metric_props.resultUnits);
      std::string name = metric_props.name;
      if (!units.empty()) {
        name += "[" + units + "]";
      }
      name_list.push_back(name);
    }

    return name_list;
  }

  static std::vector<zet_metric_type_t> GetMetricTypeList(
      zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;

    uint32_t metric_count = GetMetricCount(group);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metric_list(metric_count);
    status = zetMetricGet(group, &metric_count, metric_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metric_list.size());

    std::vector<zet_metric_type_t> type_list;
    for (auto metric : metric_list) {
      zet_metric_properties_t metric_props{
          ZET_STRUCTURE_TYPE_METRIC_PROPERTIES, };
      status = zetMetricGetProperties(metric, &metric_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      type_list.push_back(metric_props.metricType);
    }

    return type_list;
  }

  static size_t GetMetricId(
      const std::vector<std::string>& metric_list,
      const std::string& metric_name) {
    PTI_ASSERT(!metric_list.empty());
    PTI_ASSERT(!metric_name.empty());

    for (size_t i = 0; i < metric_list.size(); ++i) {
      if (metric_list[i].find(metric_name) == 0) {
        return i;
      }
    }

    return metric_list.size();
  }

  static std::vector<zet_typed_value_t> GetMetricInterval(
      const std::vector<uint64_t>& cache,
      MetricReader* reader,
      zet_metric_group_handle_t group,
      uint64_t start,
      uint64_t end,
      uint32_t sub_device_id) {
    PTI_ASSERT(reader != nullptr);
    PTI_ASSERT(group != nullptr);
    PTI_ASSERT(start < end);

    size_t start_index = utils::LowerBound(cache, start);
    PTI_ASSERT(start_index >= 0 && start_index <= cache.size());
    size_t end_index = utils::UpperBound(cache, end);
    PTI_ASSERT(end_index >= 0 && end_index <= cache.size());
    PTI_ASSERT(start_index <= end_index);

    if (start_index == end_index) {
      return std::vector<zet_typed_value_t>();
    }

    uint32_t report_size = GetMetricCount(group);
    PTI_ASSERT(report_size > 0);

    size_t report_size_in_bytes = report_size * sizeof(zet_typed_value_t);
    size_t start_byte = start_index * report_size_in_bytes;
    size_t size = (end_index - start_index) * report_size_in_bytes;

    size_t report_count = end_index - start_index;
    std::vector<zet_typed_value_t> target_list(report_count * report_size);
    reader->Read(
        sub_device_id, start_byte, size,
        reinterpret_cast<char*>(target_list.data()));
    return target_list;
  }

  static zet_typed_value_t ComputeTotalValue(
      uint32_t metric_id,
      const std::vector<zet_typed_value_t>& report_list,
      uint32_t report_size) {
    PTI_ASSERT(report_list.size() > 0);
    PTI_ASSERT(report_size > 0);
    PTI_ASSERT(metric_id < report_size);

    uint32_t report_count = report_list.size() / report_size;
    PTI_ASSERT(report_count * report_size == report_list.size());

    const zet_typed_value_t* report = report_list.data();
    switch (report[metric_id].type) {
      case ZET_VALUE_TYPE_UINT32: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_UINT64;
        total.value.ui64 = 0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_UINT32);
          total.value.ui64 += value.value.ui32;
        }
        return total;
      }
      case ZET_VALUE_TYPE_UINT64: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_UINT64;
        total.value.ui64 = 0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_UINT64);
          total.value.ui64 += value.value.ui64;
        }
        return total;
      }
      case ZET_VALUE_TYPE_FLOAT32: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_FLOAT64;
        total.value.fp64 = 0.0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_FLOAT32);
          total.value.fp64 += value.value.fp32;
        }
        return total;
      }
      case ZET_VALUE_TYPE_FLOAT64: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_FLOAT64;
        total.value.fp64 = 0.0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_FLOAT64);
          total.value.fp64 += value.value.fp64;
        }
        return total;
      }
      default: {
        PTI_ASSERT(0);
        break;
      }
    }

    return zet_typed_value_t();
  }

  static zet_typed_value_t ComputeAverageValue(
      uint32_t metric_id,
      const std::vector<zet_typed_value_t>& report_list,
      uint32_t report_size,
      uint64_t total_clocks,
      uint32_t gpu_clocks_id) {
    PTI_ASSERT(report_list.size() > 0);
    PTI_ASSERT(report_size > 0);
    PTI_ASSERT(metric_id < report_size);
    PTI_ASSERT(gpu_clocks_id < report_size);
    PTI_ASSERT(total_clocks > 0);

    uint32_t report_count = report_list.size() / report_size;
    PTI_ASSERT(report_count * report_size == report_list.size());

    const zet_typed_value_t* report = report_list.data();
    switch (report[metric_id].type) {
      case ZET_VALUE_TYPE_UINT32: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_UINT64;
        total.value.ui64 = 0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_UINT32);
          zet_typed_value_t clocks = report[gpu_clocks_id];
          PTI_ASSERT(clocks.type == ZET_VALUE_TYPE_UINT64);
          total.value.ui64 += value.value.ui32 * clocks.value.ui64;
        }
        total.value.ui64 /= total_clocks;
        return total;
      }
      case ZET_VALUE_TYPE_UINT64: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_UINT64;
        total.value.ui64 = 0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_UINT64);
          zet_typed_value_t clocks = report[gpu_clocks_id];
          PTI_ASSERT(clocks.type == ZET_VALUE_TYPE_UINT64);
          total.value.ui64 += value.value.ui64 * clocks.value.ui64;
        }
        total.value.ui64 /= total_clocks;
        return total;
      }
      case ZET_VALUE_TYPE_FLOAT32: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_FLOAT64;
        total.value.fp64 = 0.0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_FLOAT32);
          zet_typed_value_t clocks = report[gpu_clocks_id];
          PTI_ASSERT(clocks.type == ZET_VALUE_TYPE_UINT64);
          total.value.fp64 += value.value.fp32 * clocks.value.ui64;
        }
        total.value.fp64 /= total_clocks;
        return total;
      }
      case ZET_VALUE_TYPE_FLOAT64: {
        zet_typed_value_t total;
        total.type = ZET_VALUE_TYPE_FLOAT64;
        total.value.fp64 = 0.0;

        for (int j = 0; j < report_count; ++j) {
          report = report_list.data() + j * report_size;
          zet_typed_value_t value = report[metric_id];
          PTI_ASSERT(value.type == ZET_VALUE_TYPE_FLOAT64);
          zet_typed_value_t clocks = report[gpu_clocks_id];
          PTI_ASSERT(clocks.type == ZET_VALUE_TYPE_UINT64);
          total.value.fp64 += value.value.fp64 * clocks.value.ui64;
        }
        total.value.fp64 /= total_clocks;
        return total;
      }
      default: {
        PTI_ASSERT(0);
        break;
      }
    }

    return zet_typed_value_t();
  }

  static std::vector<zet_typed_value_t> GetAggregatedMetrics(
      const std::vector<uint64_t>& cache,
      MetricReader* reader,
      zet_metric_group_handle_t group,
      uint64_t start,
      uint64_t end,
      uint32_t sub_device_id,
      uint32_t gpu_clocks_id) {
    PTI_ASSERT(reader != nullptr);
    PTI_ASSERT(group != nullptr);
    PTI_ASSERT(start < end);

    uint32_t report_size = GetMetricCount(group);
    PTI_ASSERT(report_size > 0);

    std::vector<std::string> metric_list = GetMetricList(group);
    PTI_ASSERT(metric_list.size() == report_size);

    std::vector<zet_metric_type_t> metric_type_list = GetMetricTypeList(group);
    PTI_ASSERT(metric_type_list.size() == report_size);

    std::vector<zet_typed_value_t> report_list = GetMetricInterval(
        cache, reader, group, start, end, sub_device_id);
    uint32_t report_count = report_list.size() / report_size;
    PTI_ASSERT(report_count * report_size == report_list.size());
    if (report_count == 0) {
      return std::vector<zet_typed_value_t>();
    }

    uint64_t total_clocks = 0;
    for (uint32_t i = 0; i < report_count; ++i) {
      const zet_typed_value_t* report = report_list.data() + i * report_size;
      PTI_ASSERT(report[gpu_clocks_id].type == ZET_VALUE_TYPE_UINT64);
      total_clocks += report[gpu_clocks_id].value.ui64;
    }

    std::vector<zet_typed_value_t> aggregated_report(report_size);

    for (uint32_t i = 0; i < metric_list.size(); ++i) {
      if (metric_list[i] == "GpuTime") {
        aggregated_report[i] = ComputeTotalValue(i, report_list, report_size);
        continue;
      }
      if (metric_list[i] == "AvgGpuCoreFrequencyMHz") {
        aggregated_report[i] = ComputeAverageValue(
            i, report_list, report_size, total_clocks, gpu_clocks_id);
        continue;
      }
      if (metric_list[i] == "ReportReason") {
        aggregated_report[i] = report_list.data()[i];
        continue;
      }
      switch (metric_type_list[i]) {
        case ZET_METRIC_TYPE_DURATION:
        case ZET_METRIC_TYPE_RATIO:
          aggregated_report[i] = ComputeAverageValue(
              i, report_list, report_size, total_clocks, gpu_clocks_id);
          break;
        case ZET_METRIC_TYPE_THROUGHPUT:
        case ZET_METRIC_TYPE_EVENT:
          aggregated_report[i] = ComputeTotalValue(
              i, report_list, report_size);
          break;
        case ZET_METRIC_TYPE_TIMESTAMP:
        case ZET_METRIC_TYPE_RAW:
          aggregated_report[i] = report_list.data()[i];
          break;
        case ZET_METRIC_TYPE_EVENT_WITH_RANGE:
        case ZET_METRIC_TYPE_FLAG:
          break;
        default:
          PTI_ASSERT(0);
          break;
      }
    }

    return aggregated_report;
  }

  static void PrintTypedValue(
      std::stringstream& stream,
      const zet_typed_value_t& typed_value) {
    switch (typed_value.type) {
      case ZET_VALUE_TYPE_UINT32:
        stream << typed_value.value.ui32;
        break;
      case ZET_VALUE_TYPE_UINT64:
        stream << typed_value.value.ui64;
        break;
      case ZET_VALUE_TYPE_FLOAT32:
        stream << typed_value.value.fp32;
        break;
      case ZET_VALUE_TYPE_FLOAT64:
        stream << typed_value.value.fp64;
        break;
      case ZET_VALUE_TYPE_BOOL8:
        stream << static_cast<uint32_t>(typed_value.value.b8);
        break;
      default:
        PTI_ASSERT(0);
        break;
    }
  }

 private:
  Finalizer(ResultData* data, ProfOptions options)
      : data_(data), options_(options),
        logger_(options.GetLogFileName(data->pid)) {}

  uint64_t ProcessReportChunk(
      std::vector<zet_typed_value_t>& report_chunk,
      zet_metric_group_handle_t group,
      const DeviceProps& props,
      uint64_t base_time) {
    PTI_ASSERT(!report_chunk.empty());
    PTI_ASSERT(group != nullptr);
    PTI_ASSERT(data_ != nullptr);

    uint32_t report_size = GetMetricCount(group);
    uint32_t report_count = report_chunk.size() / report_size;
    PTI_ASSERT(report_count * report_size == report_chunk.size());

    std::vector<std::string> metric_list = GetMetricList(group);
    PTI_ASSERT(!metric_list.empty());
    PTI_ASSERT(metric_list.size() == report_size);

    size_t time_id = GetMetricId(metric_list, "QueryBeginTime");
    PTI_ASSERT(time_id < metric_list.size());

    uint64_t max_time = (props.mask + 1ull) *
        static_cast<uint64_t>(NSEC_IN_SEC) / props.freq;

    uint64_t prev_time = base_time, shift = 0;
    for (uint32_t i = 0; i < report_count; ++i) {
      zet_typed_value_t* report = report_chunk.data() + i * report_size;

      PTI_ASSERT(report[time_id].type == ZET_VALUE_TYPE_UINT64);
      uint64_t time = report[time_id].value.ui64;
      time += shift;

      if (time < prev_time) { // Timer Overflow
        while (time < prev_time) {
          time += max_time;
          shift += max_time;
        }
      }

      report[time_id].value.ui64 = time;
      prev_time = time;
    }

    return prev_time;
  }

  bool ComputeMetrics() {
    ze_result_t status = ZE_RESULT_SUCCESS;

    PTI_ASSERT(data_ != nullptr);

    ze_device_handle_t device = GetZeDevice(data_->device_id);
    PTI_ASSERT(device != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::vector<zet_metric_group_handle_t> metric_group_list;
    for (auto sub_device : sub_device_list) {
      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device, data_->metric_group,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      PTI_ASSERT(group != nullptr);
      metric_group_list.push_back(group);
    }
    PTI_ASSERT(metric_group_list.size() == sub_device_list.size());

    std::string filename = options_.GetResultFile();
    PTI_ASSERT(!filename.empty());

    std::string path = utils::GetFilePath(filename);

    MetricReader* computed_reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    if (computed_reader != nullptr) {
      delete computed_reader;
      return true;
    }

    MetricReader* reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "raw", path);
    if (reader == nullptr) {
      return false;
    }

    MetricStorage* storage = MetricStorage::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    PTI_ASSERT(storage != nullptr);

    for (size_t i = 0; i < sub_device_list.size(); ++i) {
      uint64_t current_timestamp = 0;
      const DeviceProps& props = data_->device_props_list[i];

      while (true) {
        PTI_ASSERT(sub_device_list.size() == metric_group_list.size());
        PTI_ASSERT(!metric_group_list.empty());

        uint32_t metric_data_size = MAX_BUFFER_SIZE;
        uint8_t* metric_data = reader->ReadChunk(metric_data_size, i);
        if (metric_data == nullptr) {
          break;
        }

        uint32_t value_count = 0;
        status = zetMetricGroupCalculateMetricValues(
            metric_group_list[i],
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            metric_data_size, metric_data, &value_count, nullptr);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        PTI_ASSERT(value_count > 0);

        std::vector<zet_typed_value_t> report_chunk(value_count);
        status = zetMetricGroupCalculateMetricValues(
            metric_group_list[i],
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            metric_data_size, metric_data,
            &value_count, report_chunk.data());
        PTI_ASSERT(value_count > 0);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        report_chunk.resize(value_count);

        current_timestamp = ProcessReportChunk(
          report_chunk, metric_group_list[i], props, current_timestamp);

        storage->Dump(
            reinterpret_cast<uint8_t*>(report_chunk.data()),
            report_chunk.size() * sizeof(zet_typed_value_t), i);

        delete[] metric_data;
      }
    }

    delete reader;
    delete storage;

    return true;
  }

  std::vector< std::vector<uint64_t> > MakeCache() {
    PTI_ASSERT(data_ != nullptr);

    ze_device_handle_t device = GetZeDevice(data_->device_id);
    PTI_ASSERT(device != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::string filename = options_.GetResultFile();
    PTI_ASSERT(!filename.empty());

    std::string path = utils::GetFilePath(filename);

    MetricReader* reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    PTI_ASSERT(reader != nullptr);

    std::vector< std::vector<uint64_t> > cache(sub_device_list.size());
    for (size_t i = 0; i < sub_device_list.size(); ++i) {
      std::vector<uint64_t>& sub_device_cache = cache[i];

      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device_list[i], data_->metric_group,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      PTI_ASSERT(group != nullptr);

      uint32_t report_size = GetMetricCount(group);
      PTI_ASSERT(report_size > 0);

      std::vector<std::string> metric_list = GetMetricList(group);
      PTI_ASSERT(!metric_list.empty());
      PTI_ASSERT(metric_list.size() == report_size);

      size_t time_id = GetMetricId(metric_list, "QueryBeginTime");
      PTI_ASSERT(time_id < metric_list.size());

      while (true) {
        uint32_t report_size_in_bytes = report_size * sizeof(zet_typed_value_t);
        uint32_t report_chunk_size = report_size_in_bytes * MAX_REPORT_COUNT;
        zet_typed_value_t* report_chunk = reinterpret_cast<zet_typed_value_t*>(
            reader->ReadChunk(report_chunk_size, i));
        if (report_chunk == nullptr) {
          break;
        }

        uint32_t report_count = report_chunk_size / report_size_in_bytes;
        PTI_ASSERT(report_count * report_size_in_bytes == report_chunk_size);

        const zet_typed_value_t* report = report_chunk;
        PTI_ASSERT(report[time_id].type == ZET_VALUE_TYPE_UINT64);
        sub_device_cache.push_back(report[time_id].value.ui64);

        for (int j = 1; j < report_count; ++j) {
          report = report_chunk + j * report_size;
          PTI_ASSERT(report[time_id].type == ZET_VALUE_TYPE_UINT64);
          PTI_ASSERT(sub_device_cache.back() <= report[time_id].value.ui64);
          sub_device_cache.push_back(report[time_id].value.ui64);
        }

        delete[] report_chunk;
      }
    }

    delete reader;

    return cache;
  }

  void ReportKernelIntervals() {
    PTI_ASSERT(data_ != nullptr);

    logger_.Log("\n");
    logger_.Log("== Raw Kernel Intervals ==\n");
    logger_.Log("\n");

    for (const auto& kernel_interval : data_->kernel_interval_list) {
      std::stringstream header;
      header << "Kernel,";
      header << "SubDeviceId,";
      header << "Time[ns],";
      header << "Start[ns],";
      header << "End[ns],";
      header << std::endl;
      logger_.Log(header.str());

      const auto& device_interval_list = kernel_interval.device_interval_list;
      for (const auto& device_interval : device_interval_list) {
        PTI_ASSERT(device_interval.start <= device_interval.end);
        uint64_t time = device_interval.end - device_interval.start;
        std::stringstream line;
        line << kernel_interval.kernel_name << ",";
        line << device_interval.sub_device_id << ",";
        line << time << ",";
        line << device_interval.start << ",";
        line << device_interval.end << ",";
        line << std::endl;
        logger_.Log(line.str());
      }

      logger_.Log("\n");
    }
  }

  void ReportRawMetrics() {
    PTI_ASSERT(data_ != nullptr);

    logger_.Log("\n");
    logger_.Log("== Raw Metrics ==\n");
    logger_.Log("\n");

    ze_device_handle_t device = GetZeDevice(data_->device_id);
    PTI_ASSERT(device != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::string filename = options_.GetResultFile();
    PTI_ASSERT(!filename.empty());

    std::string path = utils::GetFilePath(filename);

    MetricReader* reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    PTI_ASSERT(reader != nullptr);

    for (size_t i = 0; i < sub_device_list.size(); ++i) {
      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device_list[i], data_->metric_group,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      PTI_ASSERT(group != nullptr);

      uint32_t report_size = GetMetricCount(group);
      PTI_ASSERT(report_size > 0);

      std::vector<std::string> metric_list = GetMetricList(group);
      PTI_ASSERT(!metric_list.empty());
      PTI_ASSERT(metric_list.size() == report_size);

      std::stringstream header;
      header << "SubDeviceId,";
      for (auto& metric : metric_list) {
        header << metric << ",";
      }
      header << std::endl;
      logger_.Log(header.str());

      reader->Reset();
      while (true) {
        uint32_t report_size_in_bytes = report_size * sizeof(zet_typed_value_t);
        uint32_t report_chunk_size = report_size_in_bytes * MAX_REPORT_COUNT;
        zet_typed_value_t* report_chunk = reinterpret_cast<zet_typed_value_t*>(
            reader->ReadChunk(report_chunk_size, i));
        if (report_chunk == nullptr) {
          break;
        }

        uint32_t report_count = report_chunk_size / report_size_in_bytes;
        PTI_ASSERT(report_count * report_size_in_bytes == report_chunk_size);

        for (int j = 0; j < report_count; ++j) {
          const zet_typed_value_t* report = report_chunk + j * report_size;

          std::stringstream line;
          line << i << ",";

          for (int k = 0; k < report_size; ++k) {
            PrintTypedValue(line, report[k]);
            line << ",";
          }
          line << std::endl;
          logger_.Log(line.str());
        }

        delete[] report_chunk;
      }

      logger_.Log("\n");
    }

    delete reader;
  }

  void ReportKernelMetrics(const std::vector< std::vector<uint64_t> >& cache) {
    PTI_ASSERT(data_ != nullptr);

    logger_.Log("\n");
    logger_.Log("== Kernel Metrics ==\n");
    logger_.Log("\n");

    ze_device_handle_t device = GetZeDevice(data_->device_id);
    PTI_ASSERT(device != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::vector<zet_metric_group_handle_t> metric_group_list;
    for (auto sub_device : sub_device_list) {
      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device, data_->metric_group,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      PTI_ASSERT(group != nullptr);
      metric_group_list.push_back(group);
    }

    std::string filename = options_.GetResultFile();
    PTI_ASSERT(!filename.empty());

    std::string path = utils::GetFilePath(filename);

    MetricReader* reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    PTI_ASSERT(reader != nullptr);

    for (const auto& kernel_interval : data_->kernel_interval_list) {
      const auto& device_interval_list = kernel_interval.device_interval_list;
      for (const auto& device_interval : device_interval_list) {
        uint32_t sub_device_id = device_interval.sub_device_id;
        PTI_ASSERT(sub_device_id < sub_device_list.size());

        uint32_t report_size = GetMetricCount(
            metric_group_list[sub_device_id]);
        PTI_ASSERT(report_size > 0);

        std::vector<std::string> metric_list = GetMetricList(
            metric_group_list[sub_device_id]);
        PTI_ASSERT(!metric_list.empty());
        PTI_ASSERT(metric_list.size() == report_size);

        std::vector<zet_typed_value_t> report_list = GetMetricInterval(
          cache[sub_device_id], reader, metric_group_list[sub_device_id],
          device_interval.start, device_interval.end, sub_device_id);
        uint32_t report_count = report_list.size() / report_size;
        PTI_ASSERT(report_count * report_size == report_list.size());

        if (report_count > 0) {
          std::stringstream header;
          header << "Kernel,";
          header << "SubDeviceId,";
          for (auto& metric : metric_list) {
            header << metric << ",";
          }
          header << std::endl;
          logger_.Log(header.str());
        }

        for (int i = 0; i < report_count; ++i) {
          std::stringstream line;
          line << kernel_interval.kernel_name << ",";
          line << device_interval.sub_device_id << ",";
          const zet_typed_value_t* report =
            report_list.data() + i * report_size;
          for (int j = 0; j < report_size; ++j) {
            PrintTypedValue(line, report[j]);
            line << ",";
          }
          line << std::endl;
          logger_.Log(line.str());
        }
      }

      logger_.Log("\n");
    }

    delete reader;
  }

  void ReportAggregatedMetrics(const std::vector< std::vector<uint64_t> >& cache) {
    PTI_ASSERT(data_ != nullptr);

    logger_.Log("\n");
    logger_.Log("== Aggregated Kernel Metrics ==\n");
    logger_.Log("\n");

    ze_device_handle_t device = GetZeDevice(data_->device_id);
    PTI_ASSERT(device != nullptr);

    std::vector<ze_device_handle_t> sub_device_list =
      utils::ze::GetSubDeviceList(device);
    if (sub_device_list.empty()) {
      sub_device_list.push_back(device);
    }

    std::vector<zet_metric_group_handle_t> metric_group_list;
    for (auto sub_device : sub_device_list) {
      zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
          sub_device, data_->metric_group,
          ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
      PTI_ASSERT(group != nullptr);
      metric_group_list.push_back(group);
    }

    std::string filename = options_.GetResultFile();
    PTI_ASSERT(!filename.empty());

    std::string path = utils::GetFilePath(filename);

    MetricReader* reader = MetricReader::Create(
        sub_device_list.size(), data_->pid, "bin", path);
    PTI_ASSERT(reader != nullptr);

    for (const auto& kernel_interval : data_->kernel_interval_list) {
      const auto& device_interval_list = kernel_interval.device_interval_list;
      for (const auto& device_interval : device_interval_list) {
        uint32_t sub_device_id = device_interval.sub_device_id;
        PTI_ASSERT(sub_device_id < sub_device_list.size());

        uint32_t report_size = GetMetricCount(
            metric_group_list[sub_device_id]);
        PTI_ASSERT(report_size > 0);

        std::vector<std::string> metric_list = GetMetricList(
            metric_group_list[sub_device_id]);
        PTI_ASSERT(!metric_list.empty());
        PTI_ASSERT(metric_list.size() == report_size);

        size_t report_time_id = GetMetricId(metric_list, "QueryBeginTime");
        PTI_ASSERT(report_time_id < metric_list.size());

        size_t gpu_clocks_id = GetMetricId(metric_list, "GpuCoreClocks");
        PTI_ASSERT(gpu_clocks_id < metric_list.size());

        std::vector<zet_typed_value_t> report_list = GetAggregatedMetrics(
          cache[sub_device_id], reader, metric_group_list[sub_device_id],
          device_interval.start, device_interval.end,
          sub_device_id, gpu_clocks_id);
        uint32_t report_count = report_list.size() / report_size;
        PTI_ASSERT(report_count * report_size == report_list.size());

        if (report_count > 0) {
          std::stringstream header;
          header << "Kernel,";
          header << "SubDeviceId,";
          header << "KernelTime[ns],";
          for (auto& metric : metric_list) {
            header << metric << ",";
          }
          header << std::endl;
          logger_.Log(header.str());
        }

        for (int i = 0; i < report_count; ++i) {
          PTI_ASSERT(device_interval.start <= device_interval.end);
          uint64_t kernel_time = device_interval.end - device_interval.start;

          std::stringstream line;
          line << kernel_interval.kernel_name << ",";
          line << device_interval.sub_device_id << ",";
          line << kernel_time << ",";
          const zet_typed_value_t* report =
            report_list.data() + i * report_size;
          for (int j = 0; j < report_size; ++j) {
            PrintTypedValue(line, report[j]);
            line << ",";
          }
          line << std::endl;
          logger_.Log(line.str());
        }
      }

      logger_.Log("\n");
    }

    delete reader;
  }

 private:
  ResultData* data_ = nullptr;
  ProfOptions options_;
  Logger logger_;
};

#endif // PTI_TOOLS_ONEPROF_FINALIZER_H_