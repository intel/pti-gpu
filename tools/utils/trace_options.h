//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_TRACE_OPTIONS_H_
#define PTI_TOOLS_UTILS_TRACE_OPTIONS_H_

#include <sstream>
#include <string>

#include "pti_assert.h"
#include "utils.h"

#define TRACE_CALL_LOGGING           0
#define TRACE_HOST_TIMING            1
#define TRACE_DEVICE_TIMING          2
#define TRACE_KERNEL_SUBMITTING      3
#define TRACE_DEVICE_TIMELINE        4
#define TRACE_CHROME_CALL_LOGGING    5
#define TRACE_CHROME_DEVICE_TIMELINE 6
#define TRACE_CHROME_KERNEL_TIMELINE 7
#define TRACE_CHROME_DEVICE_STAGES   8
#define TRACE_VERBOSE                9
#define TRACE_DEMANGLE               10
#define TRACE_KERNELS_PER_TILE       11
#define TRACE_TID                    12
#define TRACE_PID                    13
#define TRACE_LOG_TO_FILE            14
#define TRACE_CONDITIONAL_COLLECTION 15
#define TRACE_OUTPUT_DIR_PATH        16
#define TRACE_KERNEL_NAME_FILTER     17
#define TRACE_K_NAME_FILTER_FILE     18
#define TRACE_K_NAME_FILTER_IN       19
#define TRACE_K_NAME_FILTER_OUT      20
#define TRACE_CHROME_KERNEL_LOGGING  21
#define TRACE_CHROME_DEVICE_LOGGING  22
#define TRACE_CHROME_SYCL_LOGGING    23
#define TRACE_CHROME_ITT_LOGGING     24
#define TRACE_OPENCL                 25
#define TRACE_CHROME_NO_THREAD_ON_DEVICE	26
#define TRACE_CHROME_NO_ENGINE_ON_DEVICE	27
#define TRACE_METRIC_QUERY           28
#define TRACE_METRIC_STREAM          29
#define TRACE_CCL_SUMMARY_REPORT     30

const char* kChromeTraceFileExt = "json";

class TraceOptions {
 public:
  TraceOptions(uint32_t flags, const std::string& log_file)
      : flags_(flags), log_file_(log_file) {
    if (CheckFlag(TRACE_LOG_TO_FILE)) {
      PTI_ASSERT(!log_file_.empty());
    }
    if (flags_ == 0) {
      flags_ |= (1 << TRACE_HOST_TIMING);
      flags_ |= (1 << TRACE_DEVICE_TIMING);
    }
  }

  bool CheckFlag(uint32_t flag) const {
    return (flags_ & (1 << flag));
  }

  std::string GetLogFileName() const {
    if (!CheckFlag(TRACE_LOG_TO_FILE)) {
      PTI_ASSERT(log_file_.empty());
      return std::string();
    }

    PTI_ASSERT(!log_file_.empty());
    size_t pos = log_file_.find_last_of('.');

    std::stringstream result;
    if (pos == std::string::npos) {
      result << log_file_;
    } else {
      result << log_file_.substr(0, pos);
    }

    result << "." + std::to_string(utils::GetPid());

    std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
    if (!rank.empty()) {
      result << "." + rank;
    }

    if (pos != std::string::npos) {
      result << log_file_.substr(pos);
    }

    return result.str();
  }

  static std::string GetChromeTraceFileName(const char* filename) {
    std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
    if (!rank.empty()) {
      return
        std::string(filename) +
        "." + std::to_string(utils::GetPid()) +
        "." + rank +
        "." + kChromeTraceFileExt;
    }
    return
        std::string(filename) +
        "." + std::to_string(utils::GetPid()) +
        "." + kChromeTraceFileExt;
  }

 private:
  uint32_t flags_;
  std::string log_file_;
};

#endif // PTI_TOOLS_UTILS_TRACE_OPTIONS_H_
