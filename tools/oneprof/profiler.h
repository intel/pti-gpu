//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_PROFILER_H_
#define PTI_TOOLS_ONEPROF_PROFILER_H_

#include <sstream>

#include "logger.h"
#include "metric_query_collector.h"
#include "metric_streamer_collector.h"
#include "prof_options.h"
#include "prof_utils.h"
#include "result_storage.h"

#include "cl_kernel_collector.h"
#include "ze_kernel_collector.h"

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

    if (profiler->CheckOption(PROF_KERNEL_QUERY)) { // Metric Query Mode
      MetricQueryCollector* metric_collector = MetricQueryCollector::Create(
          driver, device,
          options.GetMetricGroup().c_str(),
          options.GetRawDataPath());
      if (metric_collector == nullptr) {
        std::cout <<
          "[WARNING] Unable to create metric collector" << std::endl;
      }
      profiler->metric_query_collector_ = metric_collector;

      if (profiler->metric_query_collector_ == nullptr) {
        delete profiler;
        return nullptr;
      }
    } else { // Metric Streamer Mode
      if (profiler->CheckOption(PROF_RAW_METRICS) ||
          profiler->CheckOption(PROF_KERNEL_METRICS) ||
          profiler->CheckOption(PROF_AGGREGATION)) {
        MetricStreamerCollector* metric_collector =
          MetricStreamerCollector::Create(
            driver, device, options.GetMetricGroup().c_str(),
            options.GetSamplingInterval(), options.GetRawDataPath());
        if (metric_collector == nullptr) {
          std::cout <<
            "[WARNING] Unable to create metric collector" << std::endl;
        }
        profiler->metric_streamer_collector_ = metric_collector;

        if (profiler->metric_streamer_collector_ == nullptr) {
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
    }

    return profiler;
  }

  ~Profiler() {
    if (CheckOption(PROF_KERNEL_QUERY)) { // Metric Query Mode
      PTI_ASSERT(metric_streamer_collector_ == nullptr);
      PTI_ASSERT(ze_kernel_collector_ == nullptr);
      PTI_ASSERT(cl_kernel_collector_ == nullptr);

      if (metric_query_collector_ != nullptr) {
        metric_query_collector_->DisableTracing();
        DumpResultFile();
        delete metric_query_collector_;
      }
    } else { // Metric Streamer Mode
      PTI_ASSERT(metric_query_collector_ == nullptr);

      if (metric_streamer_collector_ != nullptr) {
        metric_streamer_collector_->DisableMetrics();
      }
      if (ze_kernel_collector_ != nullptr) {
        ze_kernel_collector_->DisableTracing();
      }
      if (cl_kernel_collector_ != nullptr) {
        cl_kernel_collector_->DisableTracing();
      }

      DumpResultFile();

      if (metric_streamer_collector_ != nullptr) {
        delete metric_streamer_collector_;
      }
      if (ze_kernel_collector_ != nullptr) {
        delete ze_kernel_collector_;
      }
      if (cl_kernel_collector_ != nullptr) {
        delete cl_kernel_collector_;
      }
    }

    if (CheckOption(PROF_NO_FINALIZE)) {
      std::cerr << "[INFO] No finalization is done, " <<
        "use --finalize option to perform it later" << std::endl;
    } else {
      Finalizer* finalizer = Finalizer::Create(options_);
      if (finalizer != nullptr) {
        finalizer->Report();
        delete finalizer;
      }

      if (!options_.GetLogFileName().empty()) {
        std::cerr << "[INFO] Log was stored to " <<
          options_.GetLogFileName() << std::endl;
      }
    }

    std::cerr << "[INFO] Result file is " <<
        options_.GetResultFile() << std::endl;
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
    SetDeviceProps();
  }

  void SetDeviceProps() {
    ze_device_handle_t device = GetZeDevice(device_id_);
    PTI_ASSERT(device != nullptr);

    auto sub_device_list = utils::ze::GetSubDeviceList(device);
    if (sub_device_list.size() == 0) {
      PTI_ASSERT(sub_device_count_ == 1);
      sub_device_list.push_back(device);
    }

    for (auto sub_device : sub_device_list) {
      DeviceProps device_props{};

      device_props.freq = utils::ze::GetDeviceTimerFrequency(sub_device);
      PTI_ASSERT(device_props.freq > 0);

      device_props.mask = utils::ze::GetMetricTimestampMask(device);
      PTI_ASSERT(device_props.mask > 0);

      device_props_list_.push_back(device_props);
    }
  }

  template <typename RuntimeKernelInterval, typename RuntimeDevice>
  void AddKernelIntervals(
      RuntimeKernelInterval& runtime_kernel_interval_list,
      RuntimeDevice device,
      std::vector<std::string>& kernel_name_list,
      std::vector<KernelInterval>& kernel_interval_list) {
    PTI_ASSERT(device != nullptr);

    for (const auto& runtime_kernel_interval : runtime_kernel_interval_list) {
      if (device != runtime_kernel_interval.device) {
        continue;
      }

      const std::string& name = runtime_kernel_interval.kernel_name;
      size_t kernel_id = kernel_name_list.size();
      for (size_t i = 0; i < kernel_name_list.size(); ++i) {
        if (kernel_name_list[i] == name) {
          kernel_id = i;
          break;
        }
      }

      if (kernel_id == kernel_name_list.size()) {
        kernel_name_list.push_back(name);
      }

      KernelInterval kernel_interval{};
      kernel_interval.kernel_id = kernel_id;

      const auto& runtime_device_interval_list =
        runtime_kernel_interval.device_interval_list;
      for (const auto& runtime_device_interval : runtime_device_interval_list) {
        kernel_interval.device_interval_list.push_back(
            {runtime_device_interval.start,
             runtime_device_interval.end,
             runtime_device_interval.sub_device_id});
      }

      kernel_interval_list.push_back(kernel_interval);
    }
  }

  void DumpResultFile() {
    std::vector<std::string> kernel_name_list;
    std::vector<KernelInterval> kernel_interval_list;

    if (CheckOption(PROF_KERNEL_INTERVALS) ||
        CheckOption(PROF_KERNEL_METRICS) ||
        CheckOption(PROF_AGGREGATION)) {

      if (cl_kernel_collector_ != nullptr) {
        const ClKernelIntervalList& cl_kernel_interval_list =
          cl_kernel_collector_->GetKernelIntervalList();

        std::vector<cl_device_id> device_list =
          utils::cl::GetDeviceList(CL_DEVICE_TYPE_GPU);
        if (!device_list.empty()) {
          PTI_ASSERT(device_id_ < device_list.size());
          AddKernelIntervals(
              cl_kernel_interval_list,
              device_list[device_id_],
              kernel_name_list,
              kernel_interval_list);
        }
      }

      if (ze_kernel_collector_ != nullptr) {
        const ZeKernelIntervalList& ze_kernel_interval_list =
          ze_kernel_collector_->GetKernelIntervalList();

        std::vector<ze_device_handle_t> device_list =
          utils::ze::GetDeviceList();
        if (!device_list.empty()) {
          PTI_ASSERT(device_id_ < device_list.size());
          AddKernelIntervals(
              ze_kernel_interval_list,
              device_list[device_id_],
              kernel_name_list,
              kernel_interval_list);
        }
      }
    }

    if (CheckOption(PROF_KERNEL_QUERY)) {
      PTI_ASSERT(metric_query_collector_ != nullptr);
      kernel_name_list = metric_query_collector_->GetKernels();
    }

    ResultStorage* storage = ResultStorage::Create(
        options_.GetRawDataPath(), utils::GetPid());
        PTI_ASSERT(storage != nullptr);

    ResultData data{
        utils::GetPid(),
        device_id_,
        correlator_.GetTimestamp(),
        options_.GetMetricGroup(),
        device_props_list_,
        kernel_name_list,
        kernel_interval_list};

    storage->Dump(&data);

    delete storage;
  }

 private:
  ProfOptions options_;

  MetricQueryCollector* metric_query_collector_ = nullptr;

  MetricStreamerCollector* metric_streamer_collector_ = nullptr;
  ZeKernelCollector* ze_kernel_collector_ = nullptr;
  ClKernelCollector* cl_kernel_collector_ = nullptr;

  Correlator correlator_;

  uint32_t device_id_ = 0;
  uint32_t sub_device_count_ = 0;

  std::vector<DeviceProps> device_props_list_;
};

#endif // PTI_TOOLS_ONEPROF_PROFILER_H_