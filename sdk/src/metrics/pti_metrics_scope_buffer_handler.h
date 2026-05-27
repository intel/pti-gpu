//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_
#define SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_

#include <spdlog/spdlog.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "pti/pti.h"

/**
 * @brief PtiMetricsScopeBufferHandler manages per-(device, thread) buffer history
 * for a scope collection handle. Each (device, thread) slot is an ordered vector
 * of buffers; the last element may be non-finalized ("current"), all earlier
 * elements are finalized.
 */
template <typename BufferType>
class PtiMetricsScopeBufferHandler {
 public:
  PtiMetricsScopeBufferHandler() = default;

  PtiMetricsScopeBufferHandler(const PtiMetricsScopeBufferHandler&) = delete;
  PtiMetricsScopeBufferHandler& operator=(const PtiMetricsScopeBufferHandler&) = delete;
  PtiMetricsScopeBufferHandler(PtiMetricsScopeBufferHandler&&) = delete;
  PtiMetricsScopeBufferHandler& operator=(PtiMetricsScopeBufferHandler&&) = delete;

  void RegisterDevice(pti_device_handle_t device) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto [it, inserted] = buffers_.try_emplace(device);
    if (!inserted) {
      SPDLOG_DEBUG("Device {} already registered, skipping", static_cast<void*>(device));
      return;
    }
  }

  // Create a buffer for a specific device on the calling thread.
  // - If no current buffer exists for (device, this thread): allocate and push.
  // - If current is empty (no records): discard it, allocate and push.
  // - If current has records and is not finalized: return PTI_WARN_BUFFER_NOT_FINALIZED.
  //   Caller must call FinalizeCurrentBuffer first to rotate.
  pti_result CreateBuffer(size_t buffer_size, pti_device_handle_t device, uint64_t buffer_id) {
    SPDLOG_DEBUG("CreateBuffer: Requesting buffer creation for device {}, thread {}, buffer_id {}",
                 static_cast<void*>(device),
                 std::hash<std::thread::id>{}(std::this_thread::get_id()), buffer_id);
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto device_it = buffers_.find(device);
    if (device_it == buffers_.end()) {
      SPDLOG_DEBUG("CreateBuffer: Attempted to create buffer for unregistered device {}",
                   static_cast<void*>(device));
      return PTI_ERROR_BAD_ARGUMENT;
    }

    auto thread_id = std::this_thread::get_id();
    auto& vec = device_it->second[thread_id];

    if (!vec.empty() && !vec.back()->IsFinalized()) {
      if (vec.back()->GetRecordCount() == 0) {
        SPDLOG_DEBUG("CreateBuffer: Replacing empty current buffer (device {}, thread {})",
                     static_cast<void*>(device), std::hash<std::thread::id>{}(thread_id));
        vec.pop_back();
      } else {
        SPDLOG_DEBUG("CreateBuffer: Current buffer not finalized (device {}, thread {})",
                     static_cast<void*>(device), std::hash<std::thread::id>{}(thread_id));
        return PTI_WARN_BUFFER_NOT_FINALIZED;
      }
    }

    size_t actual_buffer_size = (buffer_size < kMaxBufferSize) ? buffer_size : kMaxBufferSize;
    auto buffer = std::make_unique<BufferType>(actual_buffer_size, device, buffer_id);

    if (!buffer->GetRawBuffer()) {
      return PTI_ERROR_METRICS_SCOPE_OUT_OF_MEMORY;
    }

    vec.push_back(std::move(buffer));
    return PTI_SUCCESS;
  }

  // Total number of buffers (any state) with at least one record.
  size_t GetBufferCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t total = 0;
    for (const auto& [device, thread_map] : buffers_) {
      for (const auto& [thread_id, vec] : thread_map) {
        for (const auto& buf : vec) {
          if (buf->GetRecordCount() > 0) {
            total++;
          }
        }
      }
    }
    return total;
  }

  // Iterate all buffers with data in deterministic device-handle order
  // (within a device, thread-iteration order is unordered_map order).
  BufferType* GetBuffer(size_t global_index) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t current_index = 0;

    for (const auto& [device, thread_map] : buffers_) {
      for (const auto& [thread_id, vec] : thread_map) {
        for (const auto& buf : vec) {
          if (buf->GetRecordCount() == 0) {
            continue;
          }
          if (current_index == global_index) {
            return buf.get();
          }
          current_index++;
        }
      }
    }
    return nullptr;
  }

  // Returns the calling thread's current (non-finalized) buffer for the device,
  // or nullptr if none.
  BufferType* GetCurrentBuffer(pti_device_handle_t device) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto thread_id = std::this_thread::get_id();

    auto device_it = buffers_.find(device);
    if (device_it == buffers_.end()) {
      return nullptr;
    }

    auto thread_it = device_it->second.find(thread_id);
    if (thread_it == device_it->second.end() || thread_it->second.empty()) {
      return nullptr;
    }

    auto& back = thread_it->second.back();

    return back->IsFinalized() ? nullptr : back.get();
  }

  BufferType* FindBufferByRawPointer(void* raw_buffer) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    for (const auto& [device, thread_map] : buffers_) {
      for (const auto& [thread_id, vec] : thread_map) {
        for (const auto& buf : vec) {
          if (buf->GetRawBuffer() == raw_buffer) {
            return buf.get();
          }
        }
      }
    }
    return nullptr;
  }

  // True if the calling thread has a non-finalized current buffer with at
  // least one record for the given device.
  bool HasCurrentBufferWithData(pti_device_handle_t device) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto thread_id = std::this_thread::get_id();
    auto device_it = buffers_.find(device);
    if (device_it == buffers_.end()) {
      return false;
    }
    auto thread_it = device_it->second.find(thread_id);
    if (thread_it == device_it->second.end() || thread_it->second.empty()) {
      return false;
    }
    auto& back = thread_it->second.back();
    return !back->IsFinalized() && back->GetRecordCount() > 0;
  }

  // Finalize the calling thread's current buffer for the device.
  // - If empty (no records): pop it (silent discard).
  // - If non-empty: flip its is_finalized flag in place.
  // - If no current buffer: no-op.
  pti_result FinalizeCurrentBuffer(pti_device_handle_t device) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto thread_id = std::this_thread::get_id();
    auto device_it = buffers_.find(device);
    if (device_it == buffers_.end()) {
      SPDLOG_DEBUG("FinalizeCurrentBuffer: Attempted to finalize buffer for unregistered device {}",
                   static_cast<void*>(device));
      return PTI_SUCCESS;
    }
    auto thread_it = device_it->second.find(thread_id);
    if (thread_it == device_it->second.end() || thread_it->second.empty()) {
      SPDLOG_DEBUG(
          "FinalizeCurrentBuffer: Attempted to finalize buffer for non-existent thread {} on "
          "device {}",
          std::hash<std::thread::id>{}(thread_id), static_cast<void*>(device));
      return PTI_SUCCESS;
    }

    auto& vec = thread_it->second;
    if (!vec.back()->IsFinalized()) {
      if (vec.back()->GetRecordCount() == 0) {
        vec.pop_back();
        SPDLOG_DEBUG(
            "FinalizeCurrentBuffer: Discarded empty current buffer for thread {} on device {}",
            std::hash<std::thread::id>{}(thread_id), static_cast<void*>(device));
      } else {
        vec.back()->Finalize();
        SPDLOG_DEBUG(
            "FinalizeCurrentBuffer: Finalized current buffer for thread {} on device {}, "
            "record count: {}",
            std::hash<std::thread::id>{}(thread_id), static_cast<void*>(device),
            vec.back()->GetRecordCount());
      }
    }
    return PTI_SUCCESS;
  }

  // Finalize current buffers for all devices and all threads (cross-thread sweep).
  // Used at end-of-collection from the API thread.
  pti_result FinalizeAllCurrentBuffers() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    for (auto& [device, thread_map] : buffers_) {
      for (auto& [thread_id, vec] : thread_map) {
        if (vec.empty() || vec.back()->IsFinalized()) {
          continue;
        }
        if (vec.back()->GetRecordCount() == 0) {
          vec.pop_back();
        } else {
          vec.back()->Finalize();
        }
      }
    }
    return PTI_SUCCESS;
  }

 private:
  // Per-(device, thread) buffer history. Outer container is std::map for
  // deterministic iteration order (sorted by device handle), which GetBuffer()
  // relies on for stable global indexing.
  // Note: only the last element of each vector may have IsFinalized() == false
  // (the "current" buffer).
  std::map<pti_device_handle_t,
           std::unordered_map<std::thread::id, std::vector<std::unique_ptr<BufferType>>>>
      buffers_;

  mutable std::mutex buffer_mutex_;
  static constexpr size_t kMaxBufferSize = 1024 * 1024 * 1024;  // 1GB
};

#endif  // SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_
