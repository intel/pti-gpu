//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_PROFILER_H_
#define PTI_TOOLS_ONEPROF_PROFILER_H_

#include <sstream>

#include "logger.h"
#include "metric_collector.h"
#include "prof_options.h"
#include "prof_utils.h"
#include "ze_kernel_collector.h"

class Profiler {
 public:
  static Profiler* Create(const ProfOptions& options) {
    ze_driver_handle_t driver = GetDriver(options.GetDeviceId());
    PTI_ASSERT(driver != nullptr);
    ze_device_handle_t device = GetDevice(options.GetDeviceId());
    PTI_ASSERT(device != nullptr);

    uint32_t sub_device_count = 0;
    ze_result_t status =
      zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (sub_device_count == 0) {
      sub_device_count = 1;
    }

    Profiler* profiler = new Profiler(options, device, sub_device_count);
    PTI_ASSERT(profiler != nullptr);

    MetricCollector* metric_collector = nullptr;
    if (profiler->CheckOption(PROF_RAW_METRICS) ||
        profiler->CheckOption(PROF_KERNEL_METRICS) ||
        profiler->CheckOption(PROF_AGGREGATION)) {
      metric_collector = MetricCollector::Create(
          driver, device, options.GetMetricGroup().c_str(),
          options.GetSamplingInterval());
      if (metric_collector == nullptr) {
        std::cout <<
          "[WARNING] Unable to create metric collector" << std::endl;
        delete profiler;
        return nullptr;
      }
      profiler->metric_collector_ = metric_collector;
    }

    ZeKernelCollector* kernel_collector = nullptr;
    if (profiler->CheckOption(PROF_KERNEL_INTERVALS) ||
        profiler->CheckOption(PROF_KERNEL_METRICS) ||
        profiler->CheckOption(PROF_AGGREGATION)) {
      kernel_collector = ZeKernelCollector::Create(
          &(profiler->correlator_), true);
      if (kernel_collector == nullptr) {
        std::cout <<
          "[WARNING] Unable to create kernel collector" << std::endl;
        delete profiler;
        return nullptr;
      }
      profiler->kernel_collector_ = kernel_collector;
    }

    return profiler;
  }

  ~Profiler() {
    if (metric_collector_ != nullptr) {
      metric_collector_->DisableTracing();
    }
    if (kernel_collector_ != nullptr) {
      kernel_collector_->DisableTracing();
    }

    Report();

    if (metric_collector_ != nullptr) {
      delete metric_collector_;
    }
    if (kernel_collector_ != nullptr) {
      delete kernel_collector_;
    }

    if (!options_.GetLogFileName().empty()) {
      std::cerr << "[INFO] Log was stored to " <<
        options_.GetLogFileName() << std::endl;
    }
  }

  bool CheckOption(unsigned option) {
    return options_.CheckFlag(option);
  }

  Profiler(const Profiler& copy) = delete;
  Profiler& operator=(const Profiler& copy) = delete;

 private:
  Profiler(
      ProfOptions options,
      ze_device_handle_t device,
      uint32_t sub_device_count)
      : options_(options),
        device_(device),
        sub_device_count_(sub_device_count),
        correlator_(options_.GetLogFileName()) {
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(sub_device_count_ > 0);
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

  void Report() {
    correlator_.Log("\n");
    correlator_.Log("=== Profiling Results ===\n");
    correlator_.Log("\n");
    std::stringstream header;
    header << "Total Execution Time: " <<
      correlator_.GetTimestamp() << " ns" << std::endl;
    correlator_.Log(header.str());

    if (metric_collector_ != nullptr &&
        CheckOption(PROF_RAW_METRICS)) {
      for (uint32_t i = 0; i < sub_device_count_; ++i) {
        correlator_.Log("\n");
        correlator_.Log("== Raw Metrics ==\n");
        correlator_.Log("\n");
        ReportRawMetrics(i);
      }
    }

    if (kernel_collector_ != nullptr &&
        CheckOption(PROF_KERNEL_INTERVALS)) {
      correlator_.Log("\n");
      correlator_.Log("== Raw Kernel Intervals ==\n");
      correlator_.Log("\n");
      ReportKernelIntervals();
    }

    if (metric_collector_ != nullptr &&
        kernel_collector_ != nullptr &&
        CheckOption(PROF_KERNEL_METRICS)) {
      correlator_.Log("\n");
      correlator_.Log("== Kernel Metrics ==\n");
      correlator_.Log("\n");
      ReportKernelMetrics();
    }

    if (metric_collector_ != nullptr &&
        kernel_collector_ != nullptr &&
        CheckOption(PROF_AGGREGATION)) {
      correlator_.Log("\n");
      correlator_.Log("== Aggregated Metrics ==\n");
      correlator_.Log("\n");
      ReportAggregatedMetrics();
    }
  }

  void ReportKernelIntervals() {
    PTI_ASSERT(kernel_collector_ != nullptr);

    const ZeKernelIntervalList& interval_list =
      kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      if (kernel_interval.device == device_) {
        std::stringstream stream;
        stream << "Kernel," << kernel_interval.kernel_name << "," << std::endl;
        correlator_.Log(stream.str());

        std::stringstream header;
        header << "SubDeviceId,";
        header << "Start,";
        header << "End,";
        header << std::endl;
        correlator_.Log(header.str());

        for (auto& device_interval : kernel_interval.device_interval_list) {
          std::stringstream line;
          line << device_interval.sub_device_id << ",";
          line << device_interval.start << ",";
          line << device_interval.end << ",";
          line << std::endl;
          correlator_.Log(line.str());
        }

        correlator_.Log("\n");
      }
    }
  }

  void ReportRawMetrics(uint32_t sub_device_id) {
    PTI_ASSERT(sub_device_id < sub_device_count_);
    PTI_ASSERT(metric_collector_ != nullptr);

    uint32_t report_size = metric_collector_->GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    std::vector<std::string> metric_list =
      metric_collector_->GetMetricList(sub_device_id);
    PTI_ASSERT(!metric_list.empty());
    PTI_ASSERT(metric_list.size() == report_size);

    std::stringstream header;
    header << "SubDeviceId,";
    for (auto& metric : metric_list) {
      header << metric << ",";
    }
    header << std::endl;
    correlator_.Log(header.str());

    metric_collector_->ResetReportReader();
    while (true) {
      std::vector<zet_typed_value_t> report_chunk =
        metric_collector_->GetReportChunk(sub_device_id);
      if (report_chunk.empty()) {
        break;
      }

      uint32_t report_count = report_chunk.size() / report_size;
      PTI_ASSERT(report_count * report_size == report_chunk.size());

      for (int i = 0; i < report_count; ++i) {
        std::stringstream line;
        line << sub_device_id << ",";
        const zet_typed_value_t* report =
          report_chunk.data() + i * report_size;
        for (int j = 0; j < report_size; ++j) {
          PrintTypedValue(line, report[j]);
          line << ",";
        }
        line << std::endl;
        correlator_.Log(line.str());
      }
    }
  }

  std::vector<zet_typed_value_t> GetMetricInterval(
      uint64_t start, uint64_t end,
      uint32_t sub_device_id, uint32_t report_time_id) const {
    PTI_ASSERT(start < end);
    PTI_ASSERT(sub_device_id < sub_device_count_);
    PTI_ASSERT(metric_collector_ != nullptr);

    std::vector<zet_typed_value_t> target_list;
    uint32_t report_size = metric_collector_->GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    metric_collector_->ResetReportReader();
    while (true) {
      std::vector<zet_typed_value_t> report_chunk =
        metric_collector_->GetReportChunk(sub_device_id);
      if (report_chunk.empty()) {
        break;
      }

      uint32_t report_count = report_chunk.size() / report_size;
      PTI_ASSERT(report_count * report_size == report_chunk.size());

      const zet_typed_value_t* first_report = report_chunk.data();
      PTI_ASSERT(first_report[report_time_id].type == ZET_VALUE_TYPE_UINT64);
      if (first_report[report_time_id].value.ui64 > end) {
        continue;
      }

      const zet_typed_value_t* last_report =
        report_chunk.data() + (report_count - 1) * report_size;
      PTI_ASSERT(last_report[report_time_id].type == ZET_VALUE_TYPE_UINT64);
      if (last_report[report_time_id].value.ui64 < start) {
        continue;
      }

      for (int i = 0; i < report_count; ++i) {
        const zet_typed_value_t* report =
          report_chunk.data() + i * report_size;
        zet_typed_value_t report_time = report[report_time_id];
        PTI_ASSERT(report_time.type == ZET_VALUE_TYPE_UINT64);
        if (report_time.value.ui64 >= start && report_time.value.ui64 <= end) {
          for (int j = 0; j < report_size; ++j) {
            target_list.push_back(report[j]);
          }
        }
      }
    }

    return target_list;
  }

  static size_t GetMetricId(
      const std::vector<std::string>& metric_list,
      const std::string& metric_name) {
    for (size_t i = 0; i < metric_list.size(); ++i) {
      if (metric_list[i] == metric_name) {
        return i;
      }
    }
    return metric_list.size();
  }

  void ReportKernelMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(kernel_collector_ != nullptr);

    const ZeKernelIntervalList& interval_list =
      kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      std::stringstream stream;
      stream << "Kernel," << kernel_interval.kernel_name << "," << std::endl;
      correlator_.Log(stream.str());
      for (auto& device_interval : kernel_interval.device_interval_list) {
        uint32_t report_size =
          metric_collector_->GetReportSize(device_interval.sub_device_id);
        PTI_ASSERT(report_size > 0);

        std::vector<std::string> metric_list =
          metric_collector_->GetMetricList(device_interval.sub_device_id);
        PTI_ASSERT(!metric_list.empty());
        PTI_ASSERT(metric_list.size() == report_size);

        size_t report_time_id = GetMetricId(metric_list, "QueryBeginTime");
        PTI_ASSERT(report_time_id < metric_list.size());

        std::vector<zet_typed_value_t> report_list = GetMetricInterval(
            device_interval.start, device_interval.end,
            device_interval.sub_device_id, report_time_id);
        uint32_t report_count = report_list.size() / report_size;
        PTI_ASSERT(report_count * report_size == report_list.size());

        if (report_count > 0) {
          std::stringstream header;
          header << "SubDeviceId,";
          for (auto& metric : metric_list) {
            header << metric << ",";
          }
          header << std::endl;
          correlator_.Log(header.str());
        }

        for (int i = 0; i < report_count; ++i) {
          std::stringstream line;
          line << device_interval.sub_device_id << ",";
          const zet_typed_value_t* report =
            report_list.data() + i * report_size;
          for (int j = 0; j < report_size; ++j) {
            PrintTypedValue(line, report[j]);
            line << ",";
          }
          line << std::endl;
          correlator_.Log(line.str());
        }
      }
      correlator_.Log("\n");
    }
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

  std::vector<zet_typed_value_t> GetAggregatedMetrics(
      uint64_t start, uint64_t end,
      uint32_t sub_device_id,
      uint32_t report_time_id,
      uint32_t gpu_clocks_id) const {
    PTI_ASSERT(start < end);
    PTI_ASSERT(sub_device_id < sub_device_count_);

    uint32_t report_size =
      metric_collector_->GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    std::vector<std::string> metric_list =
      metric_collector_->GetMetricList(sub_device_id);
    PTI_ASSERT(metric_list.size() == report_size);

    std::vector<zet_metric_type_t> metric_type_list =
      metric_collector_->GetMetricTypeList(sub_device_id);
    PTI_ASSERT(metric_type_list.size() == report_size);

    std::vector<zet_typed_value_t> report_list =
      GetMetricInterval(start, end, sub_device_id, report_time_id);
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

    for (uint32_t i = 0; i < report_size; ++i) {
      if (metric_list[i] == "GpuTime") {
        aggregated_report[i] = ComputeTotalValue(
            i, report_list, report_size);
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
          aggregated_report[i] =
            ComputeAverageValue(
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

  void ReportAggregatedMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(kernel_collector_ != nullptr);

    const ZeKernelIntervalList& interval_list =
      kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      std::stringstream stream;
      stream << "Kernel," << kernel_interval.kernel_name << "," << std::endl;
      correlator_.Log(stream.str());
      for (auto& device_interval : kernel_interval.device_interval_list) {
        uint32_t report_size =
          metric_collector_->GetReportSize(device_interval.sub_device_id);
        PTI_ASSERT(report_size > 0);

        std::vector<std::string> metric_list =
          metric_collector_->GetMetricList(device_interval.sub_device_id);
        PTI_ASSERT(!metric_list.empty());
        PTI_ASSERT(metric_list.size() == report_size);

        size_t report_time_id = GetMetricId(metric_list, "QueryBeginTime");
        PTI_ASSERT(report_time_id < metric_list.size());

        size_t gpu_clocks_id = GetMetricId(metric_list, "GpuCoreClocks");
        PTI_ASSERT(gpu_clocks_id < metric_list.size());

        std::vector<zet_typed_value_t> report_list = GetAggregatedMetrics(
            device_interval.start, device_interval.end,
            device_interval.sub_device_id, report_time_id, gpu_clocks_id);
        uint32_t report_count = report_list.size() / report_size;
        PTI_ASSERT(report_count * report_size == report_list.size());

        if (report_count > 0) {
          std::stringstream header;
          header << "SubDeviceId,";
          for (auto& metric : metric_list) {
            header << metric << ",";
          }
          header << std::endl;
          correlator_.Log(header.str());
        }

        for (int i = 0; i < report_count; ++i) {
          std::stringstream line;
          line << device_interval.sub_device_id << ",";
          const zet_typed_value_t* report =
            report_list.data() + i * report_size;
          for (int j = 0; j < report_size; ++j) {
            PrintTypedValue(line, report[j]);
            line << ",";
          }
          line << std::endl;
          correlator_.Log(line.str());
        }
      }
      correlator_.Log("\n");
    }
  }

 private:
  ProfOptions options_;
  MetricCollector* metric_collector_ = nullptr;
  ZeKernelCollector* kernel_collector_ = nullptr;
  Correlator correlator_;

  ze_device_handle_t device_ = nullptr;
  uint32_t sub_device_count_ = 0;
};

#endif // PTI_TOOLS_ONEPROF_PROFILER_H_