//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_CORRELATOR_H_
#define PTI_TOOLS_UTILS_CORRELATOR_H_

#include <chrono>
#include <map>
#include <vector>

#ifdef PTI_LEVEL_ZERO
#include <level_zero/ze_api.h>
#endif // PTI_LEVEL_ZERO

#include "logger.h"
#include "pti_assert.h"

#ifdef CLOCK_HIGH_RESOLUTION
#define PTI_CLOCK std::chrono::high_resolution_clock
#else
#define PTI_CLOCK std::chrono::steady_clock
#endif

using TimePoint = std::chrono::time_point<PTI_CLOCK>;

struct ApiCollectorOptions {
  bool call_tracing;
  bool need_tid;
  bool need_pid;
};

class Correlator {
 public:
  Correlator(const std::string& log_file)
      : logger_(log_file), base_time_(PTI_CLOCK::now()) {}

  void Log(const std::string& text) {
    logger_.Log(text);
  }

  uint64_t GetTimestamp() const {
    std::chrono::duration<uint64_t, std::nano> timestamp =
      PTI_CLOCK::now() - base_time_;
    return timestamp.count();
  }

  uint64_t GetStartPoint() const {
    std::chrono::duration<uint64_t, std::nano> start_point =
      base_time_.time_since_epoch();
    return start_point.count();
  }

  uint64_t GetKernelId() const {
    return kernel_id_;
  }

  void SetKernelId(uint64_t kernel_id) {
    kernel_id_ = kernel_id;
  }

#ifdef PTI_LEVEL_ZERO

  std::vector<uint64_t> GetKernelId(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    if (kernel_id_map_.count(command_list) > 0) {
      return kernel_id_map_[command_list];
    } else {
      return std::vector<uint64_t>();
    }
  }

  void CreateKernelIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(kernel_id_map_.count(command_list) == 0);
    kernel_id_map_[command_list] = std::vector<uint64_t>();
  }

  void RemoveKernelIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(kernel_id_map_.count(command_list) == 1);
    kernel_id_map_.erase(command_list);
  }

  void ResetKernelIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(kernel_id_map_.count(command_list) == 1);
    kernel_id_map_[command_list].clear();
  }

  void AddKernelId(ze_command_list_handle_t command_list, uint64_t kernel_id) {
    PTI_ASSERT(kernel_id_map_.count(command_list) == 1);
    kernel_id_map_[command_list].push_back(kernel_id);
  }

  std::vector<uint64_t> GetCallId(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    if (call_id_map_.count(command_list) > 0) {
      return call_id_map_[command_list];
    } else {
      return std::vector<uint64_t>();
    }
  }

  void CreateCallIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(call_id_map_.count(command_list) == 0);
    call_id_map_[command_list] = std::vector<uint64_t>();
  }

  void RemoveCallIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(call_id_map_.count(command_list) == 1);
    call_id_map_.erase(command_list);
  }

  void ResetCallIdList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(call_id_map_.count(command_list) == 1);
    call_id_map_[command_list].clear();
  }

  void AddCallId(ze_command_list_handle_t command_list, uint64_t call_id) {
    PTI_ASSERT(call_id_map_.count(command_list) == 1);
    call_id_map_[command_list].push_back(call_id);
  }

#endif // PTI_LEVEL_ZERO

 private:
  TimePoint base_time_;
#ifdef PTI_LEVEL_ZERO
  std::map<ze_command_list_handle_t, std::vector<uint64_t> > kernel_id_map_;
  std::map<ze_command_list_handle_t, std::vector<uint64_t> > call_id_map_;
#endif // PTI_LEVEL_ZERO

  Logger logger_;

  static thread_local uint64_t kernel_id_;
};

#endif // PTI_TOOLS_UTILS_CORRELATOR_H_