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
      PTI_ASSERT(file_.is_open());
    }
    lazy_flush_ = lazy_flush;
    lock_free_ = lock_free;
  }

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

 private:
  std::mutex lock_;
  std::ofstream file_;
  bool lazy_flush_;
  bool lock_free_;	// caller deal with concurrency?
};

#endif // PTI_TOOLS_UTILS_LOGGER_H_
