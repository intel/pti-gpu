//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_LOGGER_H_
#define PTI_TOOLS_UTILS_LOGGER_H_

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

#include "pti_assert.h"

class Logger {
 public:
  Logger(const std::string& filename) {
    if (!filename.empty()) {
      file_.open(filename);
      PTI_ASSERT(file_.is_open());
    }
  }

  ~Logger() {
    if (file_.is_open()) {
      file_ << std::flush;
      file_.close();
    }
  }

  void Log(const std::string& text) {
    if (file_.is_open()) {
      const std::lock_guard<std::mutex> lock(lock_);
      // file_ << text << std::flush;
      file_ << text;
    } else {
      std::cerr << text << std::flush;
    }
  }

 private:
  std::mutex lock_;
  std::ofstream file_;
};

#endif  // PTI_TOOLS_UTILS_LOGGER_H_
