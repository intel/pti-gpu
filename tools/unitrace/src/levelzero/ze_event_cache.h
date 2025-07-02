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

#include "ze_loader.h"

#define EVENT_POOL_SIZE	1024

struct ZeEventInfo {
  ze_event_pool_handle_t pool;
  ze_context_handle_t context;
};

class ZeEventCache {
 public:
  ZeEventCache(ze_event_pool_flags_t flags) : flags_(flags) {}

  ZeEventCache(const ZeEventCache& that) = delete;

  ZeEventCache& operator=(const ZeEventCache& that) = delete;

  ~ZeEventCache() {
    bool destroyed = true;
    const std::lock_guard<std::shared_mutex> lock(lock_);

    for (auto& value : event_map_) {
      for (auto event : value.second) {
        ze_result_t status = ZE_RESULT_SUCCESS;
        status = ZE_FUNC(zeEventDestroy)(event);
        if (status != ZE_RESULT_SUCCESS) {
          destroyed = false;
        }
      }
    }
    if (!destroyed) {
#ifndef _WIN32
      // on Windows, it is very possible that L0 has been unloaded or is being unloaded at this point and L0 calls may fail safely
      // so ignore the error
      std::cerr << "[WARNING] Event in event cache is not destroyed" << std::endl;
#endif /* _WIN32 */
    }
    destroyed = true;
    for (auto& value : event_pools_) {
      for (auto pool : value.second) {
        ze_result_t status = ZE_FUNC(zeEventPoolDestroy)(pool);
        if (status != ZE_RESULT_SUCCESS) {
	  destroyed = false;
        }
      }
    }
    if (!destroyed) {
#ifndef _WIN32
      // on Windows, it is very possible that L0 has been unloaded or is being unloaded at this point and L0 calls may fail safely
      // so ignore the error
      std::cerr << "[WARNING] Event pool in event cache is not destroyed" << std::endl;
#endif /* _WIN32 */
    }
  }

  bool QueryEvent(ze_event_handle_t event) {
    if (event == nullptr) {
      return false;
    }

    lock_.lock_shared();
    auto info = event_info_map_.find(event);
    if (info == event_info_map_.end()) {
      return false;
    }
    lock_.unlock_shared();

    return true;
  }


  ze_event_handle_t GetEvent(ze_context_handle_t context) {
    if (context == nullptr) {
      return nullptr;
    }

    ze_event_handle_t event = nullptr;

    const std::lock_guard<std::shared_mutex> lock(lock_);

    auto result = event_map_.find(context);
    if (result == event_map_.end()) {
      result = event_map_.emplace(
          std::make_pair(context, std::vector<ze_event_handle_t>())).first;
    }

    if (result->second.empty()) {
      ze_result_t status = ZE_RESULT_SUCCESS;

      ze_event_pool_flags_t flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | flags_;
      ze_event_pool_desc_t pool_desc = {
          ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
          nullptr, flags, EVENT_POOL_SIZE};
      ze_event_pool_handle_t pool = nullptr;
      status = ZE_FUNC(zeEventPoolCreate)(context, &pool_desc, 0, nullptr, &pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      auto pool_iter = event_pools_.find(context);
      if (pool_iter == event_pools_.end()) {
        pool_iter = event_pools_.emplace(std::make_pair(context, std::vector<ze_event_pool_handle_t>())).first;
      }
      pool_iter->second.push_back(pool);

      for (uint32_t i = 0; i < EVENT_POOL_SIZE; i++) {
        ze_event_desc_t event_desc = {
            ZE_STRUCTURE_TYPE_EVENT_DESC,
            nullptr,
            i,
            ZE_EVENT_SCOPE_FLAG_HOST,
            ZE_EVENT_SCOPE_FLAG_HOST};
        status = ZE_FUNC(zeEventCreate)(pool, &event_desc, &event);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        PTI_ASSERT(event_info_map_.count(event) == 0);
        event_info_map_[event] = context;
	result->second.push_back(event);
      }
    } 

    event = result->second.back();
    result->second.pop_back();

    // TODO: Removing the below assert for now. Reason is that we see
    //       a behavior that if an event has signaled from an immediate
    //       command list, it stays at signaled state even after zeEventHostReset.
    //       This is not causing a real issue since once a new command is appended
    //       using the same event as the signal event, the event state becomes
    //       not ready.
    //PTI_ASSERT(ZE_FUNC(zeEventQueryStatus)(event) == ZE_RESULT_NOT_READY);

    return event;
  }

  void ResetEvent(ze_event_handle_t event) {
    if (event == nullptr) {
      return;
    }

    const std::lock_guard<std::shared_mutex> lock(lock_);

    auto info = event_info_map_.find(event);
    if (info != event_info_map_.end()) {
      ze_result_t status = ZE_FUNC(zeEventHostReset)(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void ReleaseEvent(ze_event_handle_t event) {
    if (event == nullptr) {
      return;
    }

    const std::lock_guard<std::shared_mutex> lock(lock_);

    auto info = event_info_map_.find(event);
    if (info == event_info_map_.end()) {
      return;
    }

    auto result = event_map_.find(info->second);
    PTI_ASSERT(result != event_map_.end());
    if (result != event_map_.end()) {
      ze_result_t status = ZE_FUNC(zeEventHostReset)(event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      result->second.push_back(event);
    }
  }

  void ReleaseContext(ze_context_handle_t context) {
    if (context == nullptr) {
      return;
    }

    const std::lock_guard<std::shared_mutex> lock(lock_);

    // all events in the context should already be released
    auto result = event_map_.find(context);
    if (result != event_map_.end()) {
      auto iter = event_pools_.find(context);
      if (iter != event_pools_.end()) {
        if (result->second.size() == (EVENT_POOL_SIZE * iter->second.size())) {
          for (auto event : result->second) {
            ze_result_t status = ZE_RESULT_SUCCESS;
            status = ZE_FUNC(zeEventDestroy)(event);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            event_info_map_.erase(event);
          }

          event_map_.erase(result);

          for (auto pool: iter->second) {
            ze_result_t status = ZE_RESULT_SUCCESS;
            status = ZE_FUNC(zeEventPoolDestroy)(pool);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          }
          event_pools_.erase(iter);
        }
      }
    }
  }


 private:
  ze_event_pool_flags_t flags_ = 0;
  std::map<ze_context_handle_t, std::vector<ze_event_handle_t> > event_map_;
  std::map<ze_event_handle_t, ze_context_handle_t> event_info_map_;
  std::map<ze_context_handle_t, std::vector<ze_event_pool_handle_t> > event_pools_;
  std::shared_mutex lock_;
};

#endif // PTI_TOOLS_ZE_TRACER_ZE_EVENT_CACHE_H_
