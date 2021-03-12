//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_ZE_CORRELATOR_H_
#define PTI_SAMPLES_UTILS_ZE_CORRELATOR_H_

#include <chrono>
#include <map>
#include <vector>

#include <level_zero/ze_api.h>

#include "pti_assert.h"

using ZeTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

class ZeCorrelator {
 public:
  ZeCorrelator() : base_time_(std::chrono::steady_clock::now()) {}

  ZeTimePoint GetTimepoint() const {
    return base_time_;
  }

  uint64_t GetTimestamp() const {
    std::chrono::duration<uint64_t, std::nano> timestamp =
      std::chrono::steady_clock::now() - base_time_;
    return timestamp.count();
  }

  uint64_t GetTimeDiff(const ZeTimePoint& time_point) {
    std::chrono::duration<uint64_t, std::nano> duration =
      time_point - base_time_;
    return duration.count();
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
  ZeTimePoint base_time_;
  std::map<ze_command_list_handle_t, std::vector<uint64_t> > kernel_id_map_;

  static thread_local uint64_t kernel_id_;
};

#endif // PTI_SAMPLES_ZE_TRACER_ZE_CORRELATOR_H_