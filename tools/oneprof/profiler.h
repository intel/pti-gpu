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
#include "cl_kernel_collector.h"
#include "ze_kernel_collector.h"

struct SyncPoint {
  uint64_t host;
  uint64_t device;
  uint64_t freq;
  uint64_t mask;
};

class Profiler {
 public:
  static Profiler* Create(const ProfOptions& options) {
    ze_driver_handle_t driver = GetZeDriver(options.GetDeviceId());
    PTI_ASSERT(driver != nullptr);
    ze_device_handle_t device = GetZeDevice(options.GetDeviceId());
    PTI_ASSERT(device != nullptr);

    uint32_t sub_device_count = 0;
    ze_result_t status =
      zeDeviceGetSubDevices(device, &sub_device_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (sub_device_count == 0) {
      sub_device_count = 1;
    }

    Profiler* profiler = new Profiler(
        options, options.GetDeviceId(), sub_device_count);
    PTI_ASSERT(profiler != nullptr);

    if (profiler->CheckOption(PROF_RAW_METRICS) ||
        profiler->CheckOption(PROF_KERNEL_METRICS) ||
        profiler->CheckOption(PROF_AGGREGATION)) {
      MetricCollector* metric_collector = MetricCollector::Create(
          driver, device, options.GetMetricGroup().c_str(),
          options.GetSamplingInterval(), options.GetRawDataPath());
      if (metric_collector == nullptr) {
        std::cout <<
          "[WARNING] Unable to create metric collector" << std::endl;
      }
      profiler->metric_collector_ = metric_collector;

      if (profiler->metric_collector_ == nullptr) {
        delete profiler;
        return nullptr;
      }
    }

    if (profiler->CheckOption(PROF_KERNEL_INTERVALS) ||
        profiler->CheckOption(PROF_KERNEL_METRICS) ||
        profiler->CheckOption(PROF_AGGREGATION)) {

      KernelCollectorOptions kernel_options;
      kernel_options.verbose = true;

      ZeKernelCollector* ze_kernel_collector = ZeKernelCollector::Create(
          &(profiler->correlator_), kernel_options);
      if (ze_kernel_collector == nullptr) {
        std::cout <<
          "[WARNING] Unable to create Level Zero kernel collector" <<
          std::endl;
      }
      profiler->ze_kernel_collector_ = ze_kernel_collector;

      ClKernelCollector* cl_kernel_collector = nullptr;
      cl_device_id device = GetClDevice(options.GetDeviceId());
      if (device == nullptr) {
        std::cout <<
          "[WARNING] Unable to find target OpenCL device" << std::endl;
      } else {
        cl_kernel_collector = ClKernelCollector::Create(
            device, &(profiler->correlator_), kernel_options);
        if (cl_kernel_collector == nullptr) {
          std::cout <<
            "[WARNING] Unable to create OpenCL kernel collector" <<
            std::endl;
        }
      }
      profiler->cl_kernel_collector_ = cl_kernel_collector;

      if (profiler->ze_kernel_collector_ == nullptr &&
          profiler->cl_kernel_collector_ == nullptr) {
        delete profiler;
        return nullptr;
      }
    }

    return profiler;
  }

  ~Profiler() {
    if (metric_collector_ != nullptr) {
      metric_collector_->DisableCollection();
    }
    if (ze_kernel_collector_ != nullptr) {
      ze_kernel_collector_->DisableTracing();
    }
    if (cl_kernel_collector_ != nullptr) {
      cl_kernel_collector_->DisableTracing();
    }

    Report();

    if (metric_collector_ != nullptr) {
      delete metric_collector_;
    }
    if (ze_kernel_collector_ != nullptr) {
      delete ze_kernel_collector_;
    }
    if (cl_kernel_collector_ != nullptr) {
      delete cl_kernel_collector_;
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
      uint32_t device_id,
      uint32_t sub_device_count)
      : options_(options),
        device_id_(device_id),
        sub_device_count_(sub_device_count),
        correlator_(options_.GetLogFileName(), false) {
    PTI_ASSERT(sub_device_count_ > 0);
    SetSyncPoint();
  }

  void SetSyncPoint() {
    ze_device_handle_t device = GetZeDevice(device_id_);
    PTI_ASSERT(device != nullptr);

    auto sub_device_list = utils::ze::GetSubDeviceList(device);
    if (sub_device_list.size() == 0) {
      PTI_ASSERT(sub_device_count_ == 1);
      sub_device_list.push_back(device);
    }

    for (auto sub_device : sub_device_list) {
      SyncPoint sync_point{0, 0, 0};

      sync_point.freq = utils::ze::GetDeviceTimerFrequency(sub_device);
      PTI_ASSERT(sync_point.freq > 0);

      sync_point.mask = utils::ze::GetMetricTimestampMask(device);
      PTI_ASSERT(sync_point.mask > 0);

      utils::ze::GetMetricTimestamps(
          sub_device, &sync_point.host, &sync_point.device);
      sync_point.device &= sync_point.mask;
      sync_point.device = sync_point.device *
        static_cast<uint64_t>(NSEC_IN_SEC) / sync_point.freq;
      sync_point.host = correlator_.GetTimestamp(sync_point.host);

      sync_point_.push_back(sync_point);
    }
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

  uint64_t GetHostTime(
      const zet_typed_value_t* report,
      size_t time_id,
      SyncPoint& sync_point) const {
    PTI_ASSERT(report != nullptr);

    PTI_ASSERT(report[time_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t device_time = report[time_id].value.ui64;

    if (device_time < sync_point.device) { // Timer Overflow
      uint64_t max_time = (sync_point.mask + 1ull) *
        static_cast<uint64_t>(NSEC_IN_SEC) / sync_point.freq;
      uint64_t shift = max_time + device_time - sync_point.device;
      sync_point.device = device_time;
      sync_point.host += shift;
    }

    PTI_ASSERT(sync_point.device <= device_time);
    return
      sync_point.host + (device_time - sync_point.device);
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
      correlator_.Log("\n");
      correlator_.Log("== Raw Metrics ==\n");
      correlator_.Log("\n");
      for (uint32_t i = 0; i < sub_device_count_; ++i) {
        ReportRawMetrics(i);
        correlator_.Log("\n");
      }
    }

    if (CheckOption(PROF_KERNEL_INTERVALS)) {
      if (ze_kernel_collector_ != nullptr) {
        if (!ze_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Raw Kernel Intervals (Level Zero) ==\n");
          correlator_.Log("\n");
          ReportZeKernelIntervals();
        }
      }
      if (cl_kernel_collector_ != nullptr) {
        if (!cl_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Raw Kernel Intervals (OpenCL) ==\n");
          correlator_.Log("\n");
          ReportClKernelIntervals();
        }
      }
    }

    if (metric_collector_ != nullptr &&
        CheckOption(PROF_KERNEL_METRICS)) {
      if (ze_kernel_collector_ != nullptr) {
        if (!ze_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Kernel Metrics (Level Zero) ==\n");
          correlator_.Log("\n");
          ReportZeKernelMetrics();
        }
      }
      if (cl_kernel_collector_ != nullptr) {
        if (!cl_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Kernel Metrics (OpenCL) ==\n");
          correlator_.Log("\n");
          ReportClKernelMetrics();
        }
      }
    }

    if (metric_collector_ != nullptr &&
        CheckOption(PROF_AGGREGATION)) {
      if (ze_kernel_collector_ != nullptr) {
        if (!ze_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Aggregated Metrics (Level Zero) ==\n");
          correlator_.Log("\n");
          ReportZeAggregatedMetrics();
        }
      }
      if (cl_kernel_collector_ != nullptr) {
        if (!cl_kernel_collector_->GetKernelIntervalList().empty()) {
          correlator_.Log("\n");
          correlator_.Log("== Aggregated Metrics (OpenCL) ==\n");
          correlator_.Log("\n");
          ReportClAggregatedMetrics();
        }
      }
    }
  }

  template <typename KernelInterval>
  void ReportKernelInterval(const KernelInterval& interval) {
    std::stringstream stream;
    stream << "Kernel," << interval.kernel_name << "," << std::endl;
    correlator_.Log(stream.str());

    std::stringstream header;
    header << "SubDeviceId,";
    header << "Start,";
    header << "End,";
    header << std::endl;
    correlator_.Log(header.str());

    for (auto& device_interval : interval.device_interval_list) {
      std::stringstream line;
      line << device_interval.sub_device_id << ",";
      line << device_interval.start << ",";
      line << device_interval.end << ",";
      line << std::endl;
      correlator_.Log(line.str());
    }

    correlator_.Log("\n");
  }

  void ReportClKernelIntervals() {
    PTI_ASSERT(cl_kernel_collector_ != nullptr);

    std::vector<cl_device_id> device_list =
      utils::cl::GetDeviceList(CL_DEVICE_TYPE_GPU);
    if (device_list.empty()) {
      return;
    }

    const ClKernelIntervalList& interval_list =
      cl_kernel_collector_->GetKernelIntervalList();
    if (interval_list.empty()) {
      return;
    }

    for (auto& interval : interval_list) {
      if (device_list[device_id_] != interval.device) {
        continue;
      }

      ReportKernelInterval(interval);
    }
  }

  void ReportZeKernelIntervals() {
    PTI_ASSERT(ze_kernel_collector_ != nullptr);

    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList();
    if (device_list.empty()) {
      return;
    }

    const ZeKernelIntervalList& interval_list =
      ze_kernel_collector_->GetKernelIntervalList();
    if (interval_list.empty()) {
      return;
    }

    for (auto& kernel_interval : interval_list) {
      if (device_list[device_id_] != kernel_interval.device) {
        continue;
      }

      ReportKernelInterval(kernel_interval);
    }
  }

  void ReportRawMetrics(uint32_t sub_device_id) {
    PTI_ASSERT(sub_device_id < sub_device_count_);
    PTI_ASSERT(metric_collector_ != nullptr);

    PTI_ASSERT(sub_device_id < sync_point_.size());
    SyncPoint sync_point = sync_point_[sub_device_id];

    uint32_t report_size = metric_collector_->GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    std::vector<std::string> metric_list =
      metric_collector_->GetMetricList(sub_device_id);
    PTI_ASSERT(!metric_list.empty());
    PTI_ASSERT(metric_list.size() == report_size);

    size_t report_time_id = GetMetricId(metric_list, "QueryBeginTime");
    PTI_ASSERT(report_time_id < metric_list.size());

    std::stringstream header;
    header << "SubDeviceId,";
    header << "HostTimestamp,";
    for (auto& metric : metric_list) {
      header << metric << ",";
    }
    header << std::endl;
    correlator_.Log(header.str());

    metric_collector_->ResetReportReader();
    while (true) {
      uint32_t report_chunk_size = 0;
      zet_typed_value_t* report_chunk =
        metric_collector_->GetReportChunk(sub_device_id, &report_chunk_size);
      if (report_chunk == nullptr) {
        break;
      }

      uint32_t report_count = report_chunk_size / report_size;
      PTI_ASSERT(report_count * report_size == report_chunk_size);

      for (int i = 0; i < report_count; ++i) {
        std::stringstream line;
        line << sub_device_id << ",";

        const zet_typed_value_t* report = report_chunk + i * report_size;
        uint64_t host_time = GetHostTime(report, report_time_id, sync_point);
        line << host_time << ",";

        for (int j = 0; j < report_size; ++j) {
          PrintTypedValue(line, report[j]);
          line << ",";
        }
        line << std::endl;
        correlator_.Log(line.str());
      }

      delete[] report_chunk;
    }
  }

  std::vector<zet_typed_value_t> GetMetricInterval(
      uint64_t start, uint64_t end,
      uint32_t sub_device_id, uint32_t report_time_id) const {
    PTI_ASSERT(start < end);
    PTI_ASSERT(sub_device_id < sub_device_count_);
    PTI_ASSERT(metric_collector_ != nullptr);

    PTI_ASSERT(sub_device_id < sync_point_.size());
    SyncPoint sync_point = sync_point_[sub_device_id];

    std::vector<zet_typed_value_t> target_list;
    uint32_t report_size = metric_collector_->GetReportSize(sub_device_id);
    PTI_ASSERT(report_size > 0);

    metric_collector_->ResetReportReader();
    while (true) {
      uint32_t report_chunk_size = 0;
      zet_typed_value_t* report_chunk =
        metric_collector_->GetReportChunk(sub_device_id, &report_chunk_size);
      if (report_chunk == nullptr) {
        break;
      }

      uint32_t report_count = report_chunk_size / report_size;
      PTI_ASSERT(report_count * report_size == report_chunk_size);

      { // Skip late reports
        const zet_typed_value_t* first_report = report_chunk;
        uint64_t first_time =
          GetHostTime(first_report, report_time_id, sync_point);
        if (first_time > end) {
          break;
        }
      }

      { // Skip early reports
        SyncPoint temp_sync_point = sync_point;
        const zet_typed_value_t* last_report =
          report_chunk + (report_count - 1) * report_size;
        uint64_t last_time =
          GetHostTime(last_report, report_time_id, temp_sync_point);
        if (last_time < start) {
          sync_point = temp_sync_point;
          continue;
        }
      }

      for (int i = 0; i < report_count; ++i) {
        const zet_typed_value_t* report =
          report_chunk + i * report_size;
        uint64_t host_time = GetHostTime(report, report_time_id, sync_point);
        if (host_time >= start && host_time <= end) {
          zet_typed_value_t host_timestamp{
              ZET_VALUE_TYPE_UINT64,
              {.ui64 = host_time}};
          target_list.push_back(host_timestamp);
          for (int j = 0; j < report_size; ++j) {
            target_list.push_back(report[j]);
          }
        }
      }

      delete[] report_chunk;
    }

    return target_list;
  }

  template <typename KernelInterval>
  void ReportKernelMetrics(const KernelInterval& interval) {
    std::stringstream stream;
    stream << "Kernel," << interval.kernel_name << "," << std::endl;
    correlator_.Log(stream.str());
    for (auto& device_interval : interval.device_interval_list) {
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
      report_size += 1; // Host Timestamp
      uint32_t report_count = report_list.size() / report_size;
      PTI_ASSERT(report_count * report_size == report_list.size());

      if (report_count > 0) {
        std::stringstream header;
        header << "SubDeviceId,";
        header << "HostTimestamp,";
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

  void ReportZeKernelMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(ze_kernel_collector_ != nullptr);

    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList();
    if (device_list.empty()) {
      return;
    }

    const ZeKernelIntervalList& interval_list =
      ze_kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      if (device_list[device_id_] != kernel_interval.device) {
        continue;
      }

      ReportKernelMetrics(kernel_interval);
    }
  }

  void ReportClKernelMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(cl_kernel_collector_ != nullptr);

    std::vector<cl_device_id> device_list =
      utils::cl::GetDeviceList(CL_DEVICE_TYPE_GPU);
    if (device_list.empty()) {
      return;
    }

    const ClKernelIntervalList& interval_list =
      cl_kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      if (device_list[device_id_] != kernel_interval.device) {
        continue;
      }

      ReportKernelMetrics(kernel_interval);
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
    report_size += 1; // Host Timestamp
    uint32_t report_count = report_list.size() / report_size;
    PTI_ASSERT(report_count * report_size == report_list.size());
    if (report_count == 0) {
      return std::vector<zet_typed_value_t>();
    }

    uint64_t total_clocks = 0;
    for (uint32_t i = 0; i < report_count; ++i) {
      const zet_typed_value_t* report = report_list.data() + i * report_size;
      PTI_ASSERT(report[gpu_clocks_id + 1].type == ZET_VALUE_TYPE_UINT64);
      total_clocks += report[gpu_clocks_id + 1].value.ui64;
    }

    std::vector<zet_typed_value_t> aggregated_report(report_size);
    aggregated_report[0] = report_list[0]; // Host Timestamp

    for (uint32_t i = 0; i < metric_list.size(); ++i) {
      if (metric_list[i] == "GpuTime") {
        aggregated_report[i + 1] = ComputeTotalValue(
            i + 1, report_list, report_size);
        continue;
      }
      if (metric_list[i] == "AvgGpuCoreFrequencyMHz") {
        aggregated_report[i + 1] = ComputeAverageValue(
            i + 1, report_list, report_size, total_clocks, gpu_clocks_id + 1);
        continue;
      }
      if (metric_list[i] == "ReportReason") {
        aggregated_report[i + 1] = report_list.data()[i + 1];
        continue;
      }
      switch (metric_type_list[i]) {
        case ZET_METRIC_TYPE_DURATION:
        case ZET_METRIC_TYPE_RATIO:
          aggregated_report[i + 1] = ComputeAverageValue(
              i + 1, report_list, report_size,
              total_clocks, gpu_clocks_id + 1);
          break;
        case ZET_METRIC_TYPE_THROUGHPUT:
        case ZET_METRIC_TYPE_EVENT:
          aggregated_report[i + 1] = ComputeTotalValue(
              i + 1, report_list, report_size);
          break;
        case ZET_METRIC_TYPE_TIMESTAMP:
        case ZET_METRIC_TYPE_RAW:
          aggregated_report[i + 1] = report_list.data()[i + 1];
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

  template <typename KernelInterval>
  void ReportAggregatedMetrics(const KernelInterval& interval) {
    std::stringstream stream;
    stream << "Kernel," << interval.kernel_name << "," << std::endl;
    correlator_.Log(stream.str());
    for (auto& device_interval : interval.device_interval_list) {
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
      report_size += 1; // Host Timestamp
      uint32_t report_count = report_list.size() / report_size;
      PTI_ASSERT(report_count * report_size == report_list.size());

      if (report_count > 0) {
        std::stringstream header;
        header << "SubDeviceId,";
        header << "HostTimestamp,";
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

  void ReportZeAggregatedMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(ze_kernel_collector_ != nullptr);

    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList();
    if (device_list.empty()) {
      return;
    }

    const ZeKernelIntervalList& interval_list =
      ze_kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      if (device_list[device_id_] != kernel_interval.device) {
        continue;
      }

      ReportAggregatedMetrics(kernel_interval);
    }
  }

  void ReportClAggregatedMetrics() {
    PTI_ASSERT(metric_collector_ != nullptr);
    PTI_ASSERT(cl_kernel_collector_ != nullptr);

    std::vector<cl_device_id> device_list =
      utils::cl::GetDeviceList(CL_DEVICE_TYPE_GPU);
    if (device_list.empty()) {
      return;
    }

    const ClKernelIntervalList& interval_list =
      cl_kernel_collector_->GetKernelIntervalList();
    for (auto& kernel_interval : interval_list) {
      if (device_list[device_id_] != kernel_interval.device) {
        continue;
      }

      ReportAggregatedMetrics(kernel_interval);
    }
  }

 private:
  ProfOptions options_;
  MetricCollector* metric_collector_ = nullptr;
  ZeKernelCollector* ze_kernel_collector_ = nullptr;
  ClKernelCollector* cl_kernel_collector_ = nullptr;
  Correlator correlator_;

  uint32_t device_id_ = 0;
  uint32_t sub_device_count_ = 0;

  std::vector<SyncPoint> sync_point_;
};

#endif // PTI_TOOLS_ONEPROF_PROFILER_H_