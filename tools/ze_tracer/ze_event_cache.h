//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ZE_TRACER_ZE_EVENT_CACHE_H_
#define PTI_TOOLS_ZE_TRACER_ZE_EVENT_CACHE_H_

#include <map>
#include <mutex>
#include <vector>

#include "ze_utils.h"

struct ZeEventInfo {
  ze_event_pool_handle_t pool;
  ze_context_handle_t context;
};

class ZeEventCache {
 public:
  ZeEventCache() {}

  ~ZeEventCache() {
    for (auto& value : event_map_) {
      for (auto event : value.second) {
        auto info = event_info_map_.find(event);
        PTI_ASSERT(info != event_info_map_.end());

        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zeEventDestroy(event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(info->second.pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }
    }
  }

  ze_event_handle_t GetEvent(ze_context_handle_t context) {
    PTI_ASSERT(context != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    ze_event_handle_t event = nullptr;

    auto result = event_map_.find(context);
    if (result == event_map_.end()) {
      ze_result_t status = ZE_RESULT_SUCCESS;

      ze_event_pool_desc_t pool_desc = {
          ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
          nullptr,
          ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP |
            ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
          1};
      ze_event_pool_handle_t pool = nullptr;
      status = zeEventPoolCreate(context, &pool_desc, 0, nullptr, &pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      ze_event_desc_t event_desc = {
          ZE_STRUCTURE_TYPE_EVENT_DESC,
          nullptr,
          0,
          ZE_EVENT_SCOPE_FLAG_HOST,
          ZE_EVENT_SCOPE_FLAG_HOST};
      status = zeEventCreate(pool, &event_desc, &event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      PTI_ASSERT(event_info_map_.count(event) == 0);
      event_info_map_[event] = {pool, context};
    } else {
      event = result->second.back();
      result->second.pop_back();
      PTI_ASSERT(zeEventQueryStatus(event) == ZE_RESULT_NOT_READY);
    }

    return event;
  }

  void ResetEvent(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    auto info = event_info_map_.find(event);
    if (info != event_info_map_.end()) {
      ze_result_t status = zeEventHostReset(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void ReleaseEvent(ze_event_handle_t event) {
    PTI_ASSERT(event != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    auto info = event_info_map_.find(event);
    if (info == event_info_map_.end()) {
      return;
    }

    auto result = event_map_.find(info->second.context);
    if (result == event_map_.end()) {
      event_map_[info->second.context] =
        std::vector<ze_event_handle_t>(1, event);
    } else {
      result->second.push_back(event);
    }
  }

  void ReleaseContext(ze_context_handle_t context) {
    PTI_ASSERT(context != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    auto result = event_map_.find(context);
    if (result != event_map_.end()) {
      for (auto event : result->second) {
        auto info = event_info_map_.find(event);
        PTI_ASSERT(info != event_info_map_.end());

        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zeEventDestroy(event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(info->second.pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        event_info_map_.erase(info);
      }

      event_map_.erase(result);
    }
  }

 private:
  std::map<ze_context_handle_t, std::vector<ze_event_handle_t> > event_map_;
  std::map<ze_event_handle_t, ZeEventInfo> event_info_map_;
  std::mutex lock_;
};

#endif // PTI_TOOLS_ZE_TRACER_ZE_EVENT_CACHE_H_