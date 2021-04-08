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

#include <level_zero/ze_api.h>

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
  Correlator() : base_time_(PTI_CLOCK::now()) {}

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

  std::vector<uint64_t> GetKernelId(
      ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    if (kernel_id_map_.count(command_list) > 0) {
      return kernel_id_map_[command_list];
    } else {
      return std::vector<uint64_t>();
    }
  }

  void SetKernelId(uint64_t kernel_id) {
    kernel_id_ = kernel_id;
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

 private:
  TimePoint base_time_;
  std::map<ze_command_list_handle_t, std::vector<uint64_t> > kernel_id_map_;

  static thread_local uint64_t kernel_id_;
};

#endif // PTI_TOOLS_UTILS_CORRELATOR_H_