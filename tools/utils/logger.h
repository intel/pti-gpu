//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_LOGGER_H_
#define PTI_TOOLS_UTILS_LOGGER_H_

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>

#include "pti_assert.h"

class Logger {
 public:
  Logger(const std::string& filename, bool lazy_flush = false, bool lock_free = false) {
    if (!filename.empty()) {
      file_.open(filename);
      if (!(file_.is_open())) {
        std::cerr << "[ERROR] Failed to open file " << filename << " for writing. Do you have the right permission?" << std::endl;
        exit(-1);
      }
    }
    lazy_flush_ = lazy_flush;
    lock_free_ = lock_free;
    log_file_name_ = filename;
  }

  Logger(const Logger& that) = delete;

  ~Logger() {
    if (file_.is_open()) {
      file_ << std::flush;
      file_.close();
    }
  }

  void Log(const std::string& text) {
    if (file_.is_open()) {
      if (lock_free_) {
        file_ << text;
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
      else {
        const std::lock_guard<std::mutex> lock(lock_);
        file_ << text;
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
    } else {
      std::cerr << text;
      if (!lazy_flush_) {
        std::cerr << std::flush;
      }
    }
  }

  void Flush() {
    if (file_.is_open()) {
      if (lock_free_) {
        file_ << std::flush;
      }
      else {
        const std::lock_guard<std::mutex> lock(lock_);
        file_ << std::flush;
      }
    } else {
      std::cerr << std::flush;
    }
  }

  std::string GetLogFileName() const {
    return log_file_name_;
  }

  std::iostream::pos_type GetLogFilePosition() {
    return file_.tellp();
  }

 private:
  std::string log_file_name_;
  std::mutex lock_;
  std::ofstream file_;
  bool lazy_flush_;
  bool lock_free_;	// caller deal with concurrency?
};

#endif // PTI_TOOLS_UTILS_LOGGER_H_
