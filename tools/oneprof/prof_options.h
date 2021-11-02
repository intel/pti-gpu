//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_PROF_OPTIONS_H_
#define PTI_TOOLS_ONEPROF_PROF_OPTIONS_H_

#include <sstream>
#include <string>

#include "pti_assert.h"
#include "utils.h"

#define PROF_RAW_METRICS       0
#define PROF_KERNEL_METRICS    1
#define PROF_KERNEL_INTERVALS  2
#define PROF_AGGREGATION       3

class ProfOptions {
 public:
  ProfOptions(
      uint32_t flags,
      uint32_t device_id,
      uint32_t sampling_interval,
      const std::string& metric_group,
      const std::string& log_file,
      const std::string& raw_data_path)
      : flags_(flags),
        device_id_(device_id),
        sampling_interval_(sampling_interval),
        metric_group_(metric_group),
        log_file_(log_file),
        raw_data_path_(raw_data_path) {}

  bool CheckFlag(uint32_t flag) const {
    return (flags_ & (1 << flag));
  }

  uint32_t GetDeviceId() const {
    return device_id_;
  }

  uint32_t GetSamplingInterval() const {
    return sampling_interval_;
  }

  std::string GetMetricGroup() const {
    return metric_group_;
  }

  std::string GetLogFileName() const {
    if (log_file_.empty()) {
      return std::string();
    }

    std::stringstream result;

    size_t pos = log_file_.find_first_of('.');
    if (pos == std::string::npos) {
      result << log_file_;
    } else {
      result << log_file_.substr(0, pos);
    }

    result << "." + std::to_string(utils::GetPid());

    std::string rank = utils::GetEnv("PMI_RANK");
    if (!rank.empty()) {
      result << "." + rank;
    }

    if (pos != std::string::npos) {
      result << log_file_.substr(pos);
    }

    return result.str();
  }

  std::string GetRawDataPath() const {
    return raw_data_path_;
  }

 private:
  uint32_t flags_;
  uint32_t device_id_;
  uint32_t sampling_interval_;
  std::string metric_group_;
  std::string log_file_;
  std::string raw_data_path_;
};

#endif // PTI_TOOLS_ONEPROF_PROF_OPTIONS_H_