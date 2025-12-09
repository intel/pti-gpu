//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef LEVELZERO_ZE_EVENT_MANAGERS_H_
#define LEVELZERO_ZE_EVENT_MANAGERS_H_

#include <assert.h>
#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "overhead_kinds.h"
#include "pti/pti_driver_levelzero_api_ids.h"
#include "pti_assert.h"

template <typename T>
class ZeEventView {
 public:
  ZeEventView() : event_(nullptr), event_pool_(nullptr), index_(0) {}

  ZeEventView(ze_event_handle_t event, T* event_pool, uint32_t idx)
      : event_(event), event_pool_(event_pool), index_(idx) {}

  explicit ZeEventView(ze_event_handle_t event) : event_(event), event_pool_(nullptr), index_(0) {}

  ZeEventView(const ZeEventView&) = delete;
  ZeEventView(ZeEventView&& other) noexcept
      : event_(std::exchange(other.event_, nullptr)),
        event_pool_(std::exchange(other.event_pool_, nullptr)),
        index_(std::exchange(other.index_, 0)) {}
  ZeEventView& operator=(const ZeEventView&) = delete;
  ZeEventView& operator=(ZeEventView&& other) noexcept {
    if (this != &other) {
      if (event_) {
        Release();
      }
      event_ = std::exchange(other.event_, nullptr);
      event_pool_ = std::exchange(other.event_pool_, nullptr);
      index_ = std::exchange(other.index_, 0);
    }
    return *this;
  }

  constexpr ze_event_handle_t Get() const { return event_; }
  constexpr uint32_t Idx() const { return index_; }
  constexpr bool Empty() const { return event_ == nullptr; }

  bool Ready() const {
    if (!event_) {
      return false;
    }
    overhead::ScopedOverheadCollector overhead(zeEventQueryStatus_id);
    return zeEventQueryStatus(event_) == ZE_RESULT_SUCCESS;
  }

  void Unlink() { event_pool_ = nullptr; }

  ~ZeEventView() noexcept {
    if (event_) {
      Release();
    }
  }

 private:
  void Release() {
    if (event_pool_) {
      event_pool_->ResetEvent(this);
    }
  }

  ze_event_handle_t event_;
  T* event_pool_;
  uint32_t index_;
};

class ZeEventPool {
 public:
  ZeEventPool(ze_context_handle_t ctx, uint32_t count) : events_(count) {
    assert(ctx != nullptr);
    assert(count != 0);
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, count};

    ze_result_t result = ZE_RESULT_SUCCESS;
    {
      overhead::ScopedOverheadCollector overhead(zeEventPoolCreate_id);
      result = zeEventPoolCreate(ctx, &event_pool_desc, 0, nullptr, &event_pool_);
    }
    HandleIrrecoverableZeError(result, "zeEventPoolCreate");

    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};

    for (uint32_t i = 0; i < count; ++i) {
      event_desc.index = i;
      ze_event_handle_t event = nullptr;
      {
        overhead::ScopedOverheadCollector overhead(zeEventCreate_id);
        result = zeEventCreate(event_pool_, &event_desc, &event);
      }
      HandleIrrecoverableZeError(result, "zeEventCreate");
      events_[i] = ZeEventView{event, this, i};
    }
  }

  ZeEventPool(const ZeEventPool&) = delete;

  ZeEventPool(ZeEventPool&& other) noexcept = delete;
  ZeEventPool& operator=(const ZeEventPool&) = delete;
  ZeEventPool& operator=(ZeEventPool&& other) noexcept = delete;

  [[nodiscard]] ZeEventView<ZeEventPool> AcquireEvent() {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto event = std::move(events_[current_event_index_]);
    current_event_index_ = (current_event_index_ + 1) % events_.size();
    ++outstanding_events_;
    return event;
  }

  [[nodiscard]] bool IsExhausted() const {
    // The pool is exhausted if the next event to be acquired is not available
    // (i.e., its handle is null because it was moved out and not returned
    // yet).
    const std::lock_guard<std::mutex> lock(mutex_);
    return events_[current_event_index_].Get() == nullptr;
  }

  void ResetEvent(ZeEventView<ZeEventPool>* event) {
    if (event) {
      const std::lock_guard<std::mutex> lock(mutex_);  // avoid race between pool destroy and reset
      if (destroyed_) {
        return;
      }
      auto* ze_event = event->Get();
      if (ze_event) {
        overhead::ScopedOverheadCollector overhead(zeEventHostReset_id);
        auto result = zeEventHostReset(ze_event);
        HandleIrrecoverableZeError(result, "zeEventHostReset");
      }
      events_[event->Idx()] = std::move(*event);
      --outstanding_events_;
    }
  }

  [[nodiscard]] bool IsCompletelyAvailable() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return outstanding_events_ == 0;
  }

  ~ZeEventPool() noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    destroyed_ = true;
    DestroyResources();
  }

 private:
  static void HandleIrrecoverableZeError(ze_result_t result, const char* function_name) {
    if (result != ZE_RESULT_SUCCESS) {
      SPDLOG_CRITICAL("{} irrecoverable error: {:x}", function_name, static_cast<uint32_t>(result));
      PTI_ASSERT(false);  // NOLINT
    }
  }

  static void HandleZeError(ze_result_t result, const char* function_name) {
    if (result != ZE_RESULT_SUCCESS) {
      SPDLOG_INFO("{} failed with result: {:x}", function_name, static_cast<uint32_t>(result));
    }
  }

  void DestroyResources() noexcept {
    // call lock before calling this function
    for (auto& event : events_) {
      auto finished_event = std::move(event);
      finished_event.Unlink();  // prevent deadlock
      if (finished_event.Get()) {
        overhead::ScopedOverheadCollector overhead(zeEventDestroy_id);
        auto result = zeEventDestroy(finished_event.Get());
        HandleZeError(result, "zeEventDestroy");
      }
    }
    if (event_pool_) {
      overhead::ScopedOverheadCollector overhead(zeEventPoolDestroy_id);
      auto result = zeEventPoolDestroy(event_pool_);
      HandleZeError(result, "zeEventPoolDestroy");
      event_pool_ = nullptr;
    }
  }

  mutable std::mutex mutex_;
  std::vector<ZeEventView<ZeEventPool>> events_;
  uint32_t current_event_index_ = 0;
  uint32_t outstanding_events_ = 0;
  bool destroyed_ = false;
  ze_event_pool_handle_t event_pool_ = nullptr;
};

class ZeEventPoolManager {
 public:
  static constexpr uint32_t kDefaultPoolSize = 256U;

  [[nodiscard]] ZeEventView<ZeEventPool> AcquireEvent(ze_context_handle_t context) {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto& pools = event_pools_[context];
    if (pools.empty()) {
      pools.emplace_back(std::make_unique<ZeEventPool>(context, kDefaultPoolSize));
    } else if (pools.back()->IsExhausted()) {
      auto last_unchecked_pool_it = pools.end() - 1;
      auto reusable_pool_it =
          std::find_if(pools.begin(), last_unchecked_pool_it,
                       [](const auto& pool) { return pool->IsCompletelyAvailable(); });
      if (reusable_pool_it != last_unchecked_pool_it) {
        std::iter_swap(reusable_pool_it, last_unchecked_pool_it);
      } else {
        pools.emplace_back(std::make_unique<ZeEventPool>(context, kDefaultPoolSize));
      }
    }
    assert(pools.back()->IsExhausted() == false);
    return pools.back()->AcquireEvent();
  }

  void Clear(ze_context_handle_t context) {
    const std::lock_guard<std::mutex> lock(mutex_);
    event_pools_.erase(context);
  }

 private:
  using PoolStorage = std::vector<std::unique_ptr<ZeEventPool>>;

  mutable std::mutex mutex_;
  std::unordered_map<ze_context_handle_t, PoolStorage> event_pools_;
};

#endif  // LEVELZERO_ZE_EVENT_MANAGERS_H_
