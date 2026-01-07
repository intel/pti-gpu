//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef LEVELZERO_EVENTS_AND_POOLS_OBSERVER_H_
#define LEVELZERO_EVENTS_AND_POOLS_OBSERVER_H_

// Events in PTI are primary objects through which GPU timing information is collected.
// However there could be different types of events in Level-Zero:
// 1) Regular events created by zeEventPoolCreate and zeEventCreate
// 2) Counter-based events, so far also created from special event pools and zeEventCreate
//    but which behave differently vs. regular events
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/EXT_Exp_CounterBasedEventPools.html#counter-based-event-pools-extension
// Moreover, on some platforms (starting from BMG and newer in 2025.3)
// counter-based events are used by default in UR, while regular events still supported

// With that in mind, this file provides API to query events and pools.
// For this purpose it maintains the cache of pools that collector sees when tracing Level-Zero
// application. If Level-Zero loader/layers provide APIs to query event properties/flags directly
// and easy - this code can be removed.

#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include "overhead_kinds.h"
#include "pti/pti_driver_levelzero_api_ids.h"
#include "ze_wrappers.h"

enum class EventPoolType { kUnknown, kRegular, kCounterBased };
struct EventPoolProperties {
  EventPoolType type = EventPoolType::kUnknown;
  ze_event_pool_flags_t flags = 0;
};

class ZeEventPoolsObserver {
 public:
  explicit ZeEventPoolsObserver(Level0Wrapper& wrapper) : wrapper_(wrapper) {}

  bool Add(ze_event_pool_handle_t pool, ze_context_handle_t context, ze_event_pool_flags_t flags,
           EventPoolType pool_type) {
    std::unique_lock lock(event_pool_mutex_);
    SPDLOG_TRACE("In {} pool: {}, context: {}, type: {}", __func__, static_cast<const void*>(pool),
                 static_cast<const void*>(context), static_cast<int>(pool_type));
    bool added = false;
    if (event_pool_properties_.find(pool) == event_pool_properties_.end()) {
      event_pool_properties_[pool] = {context, EventPoolProperties{pool_type, flags}};
      context_to_pools_[context].insert(pool);
      added = true;
    }
    return added;
  }

  void ClearPool(ze_event_pool_handle_t pool) {
    std::unique_lock lock(event_pool_mutex_);
    SPDLOG_TRACE("In {} pool: {}", __func__, static_cast<const void*>(pool));

    auto pool_it = event_pool_properties_.find(pool);
    if (pool_it != event_pool_properties_.end()) {
      ze_context_handle_t context = pool_it->second.first;
      event_pool_properties_.erase(pool_it);

      // Remove from context tracking
      auto ctx_it = context_to_pools_.find(context);
      if (ctx_it != context_to_pools_.end()) {
        ctx_it->second.erase(pool);
        if (ctx_it->second.empty()) {
          context_to_pools_.erase(ctx_it);
        }
      }
    }
  }

  void ClearContext(ze_context_handle_t context) {
    std::unique_lock lock(event_pool_mutex_);
    SPDLOG_TRACE("In {} context: {}", __func__, static_cast<const void*>(context));

    auto ctx_it = context_to_pools_.find(context);
    if (ctx_it != context_to_pools_.end()) {
      // Remove all pools associated with this context
      for (auto pool : ctx_it->second) {
        event_pool_properties_.erase(pool);
      }
      context_to_pools_.erase(ctx_it);
    }
  }

  void ClearAll() {
    std::unique_lock lock(event_pool_mutex_);
    SPDLOG_TRACE("In {} remove all pools", __func__);
    event_pool_properties_.clear();
    context_to_pools_.clear();
  }

  std::optional<EventPoolProperties> GetPoolProperties(ze_event_pool_handle_t pool) {
    // Until counter-based events are created via Event Pool -
    // calling EventPoolGetFlag for not-traced Pool doesn't tell the diff between
    // regular and counter-based events.
    // But when counter-based events created w/o Pool or have a special flag in Pool
    // - asking Pool flags might be helpful and then this fragment could be changed, e.g.
    // wrapper_.w_zeEventPoolGetFlags(pool, &flags);

    std::shared_lock lock(event_pool_mutex_);
    SPDLOG_TRACE("In {} pool: {}", __func__, static_cast<const void*>(pool));
    auto iter = event_pool_properties_.find(pool);
    if (iter != event_pool_properties_.end()) {
      return iter->second.second;
    }
    return std::nullopt;
  }

  std::optional<EventPoolProperties> GetEventProperties(ze_event_handle_t event) {
    ze_event_pool_handle_t pool;
    ze_result_t res = ZE_RESULT_SUCCESS;
    {
      overhead::ScopedOverheadCollector overhead(zeEventGetEventPool_id);
      res = wrapper_.w_zeEventGetEventPool(event, &pool);
    }
    if (res != ZE_RESULT_SUCCESS || pool == nullptr) {
      SPDLOG_DEBUG("In {}, zeEventGetEventPool failed with code {:x}, pool: {}", __func__,
                   static_cast<uint32_t>(res), static_cast<const void*>(pool));
      return std::nullopt;
    }
    return GetPoolProperties(pool);
  }

 private:
  std::unordered_map<ze_event_pool_handle_t, std::pair<ze_context_handle_t, EventPoolProperties>>
      event_pool_properties_;
  std::unordered_map<ze_context_handle_t, std::unordered_set<ze_event_pool_handle_t>>
      context_to_pools_;
  mutable std::shared_mutex event_pool_mutex_;
  Level0Wrapper& wrapper_;
};

#endif  // LEVELZERO_EVENTS_AND_POOLS_OBSERVER_H_
