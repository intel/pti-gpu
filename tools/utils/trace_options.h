//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_TRACE_OPTIONS_H_
#define PTI_TOOLS_UTILS_TRACE_OPTIONS_H_

#include <string>

#include "pti_assert.h"
#include "utils.h"

#define TRACE_CALL_LOGGING           0
#define TRACE_HOST_TIMING            1
#define TRACE_DEVICE_TIMING          2
#define TRACE_DEVICE_TIMING_VERBOSE  3
#define TRACE_DEVICE_TIMELINE        4
#define TRACE_CHROME_CALL_LOGGING    5
#define TRACE_CHROME_DEVICE_TIMELINE 6
#define TRACE_CHROME_DEVICE_STAGES   7
#define TRACE_TID                    8
#define TRACE_PID                    9
#define TRACE_LOG_TO_FILE            10

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
    size_t pos = log_file_.find_first_of('.');
    if (pos == std::string::npos) {
      return log_file_ + "." + std::to_string(utils::GetPid());
    }
    return log_file_.substr(0, pos + 1) +
      std::to_string(utils::GetPid()) +
      log_file_.substr(pos);
  }

 private:
  uint32_t flags_;
  std::string log_file_;
};

#endif // PTI_TOOLS_UTILS_TRACE_OPTIONS_H_