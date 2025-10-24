//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_
#define SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_

#include <memory>
#include <mutex>
#include <vector>

#include "pti_metrics_scope_buffer.h"

/**
 * @brief PtiMetricsScopeBufferHandle class that provides services to scope collection handle
 * Manages a vector of raw collection buffer pointers that associate with a scope collection handle
 */
class PtiMetricsScopeBufferHandle {
 public:
  PtiMetricsScopeBufferHandle() = default;
  ~PtiMetricsScopeBufferHandle() {
    finalized_buffers_.clear();
    current_buffer_.reset();
  }

  // Delete copy constructor and assignment operator
  PtiMetricsScopeBufferHandle(const PtiMetricsScopeBufferHandle&) = delete;
  PtiMetricsScopeBufferHandle& operator=(const PtiMetricsScopeBufferHandle&) = delete;

  PtiMetricsScopeBufferHandle(PtiMetricsScopeBufferHandle&&) = delete;
  PtiMetricsScopeBufferHandle& operator=(PtiMetricsScopeBufferHandle&&) = delete;

  // Buffer management services
  pti_result CreateBuffer(size_t buffer_size, pti_device_handle_t device_handle,
                          uint64_t buffer_id) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t actual_buffer_size = (buffer_size < kMaxBufferSize) ? buffer_size : kMaxBufferSize;

    auto buffer =
        std::make_unique<PtiMetricsScopeBuffer>(actual_buffer_size, device_handle, buffer_id);
    if (!buffer->GetRawBuffer()) {
      return PTI_ERROR_METRICS_SCOPE_OUT_OF_MEMORY;
    }

    current_buffer_ = std::move(buffer);

    return PTI_SUCCESS;
  }

  // Buffer access services
  size_t GetBufferCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t count = finalized_buffers_.size();

    // Add current buffer if it has data
    if (HasCurrentBufferWithDataInternal()) {
      count++;
    }

    return count;
  }

  PtiMetricsScopeBuffer* GetBuffer(size_t index) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // First check finalized buffers
    if (index < finalized_buffers_.size()) {
      return finalized_buffers_[index].get();
    }

    // Check if requesting current buffer
    size_t current_buffer_index = finalized_buffers_.size();
    if (index == current_buffer_index && HasCurrentBufferWithDataInternal()) {
      return current_buffer_.get();
    }

    return nullptr;
  }

  PtiMetricsScopeBuffer* GetCurrentBuffer() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return current_buffer_.get();
  }

  PtiMetricsScopeBuffer* FindBufferByRawPointer(void* raw_buffer) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Check finalized buffers
    for (const auto& buffer : finalized_buffers_) {
      if (buffer->GetRawBuffer() == raw_buffer) {
        return buffer.get();
      }
    }

    // Check current buffer
    if (current_buffer_ && current_buffer_->GetRawBuffer() == raw_buffer) {
      return current_buffer_.get();
    }

    return nullptr;
  }

  // Collection management
  bool HasCurrentBufferWithData() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return current_buffer_ && current_buffer_->GetRecordCount() > 0;
  }

  pti_result FinalizeCurrentBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (current_buffer_ && current_buffer_->GetRecordCount() > 0) {
      // Finalize the buffer directly here
      current_buffer_->Finalize();

      finalized_buffers_.push_back(std::move(current_buffer_));
      current_buffer_.reset();
    }

    return PTI_SUCCESS;
  }

 private:
  std::vector<std::unique_ptr<PtiMetricsScopeBuffer>> finalized_buffers_;
  std::unique_ptr<PtiMetricsScopeBuffer> current_buffer_;
  mutable std::mutex buffer_mutex_;
  static constexpr size_t kMaxBufferSize = 1024 * 1024 * 1024;  // 1GB

  // Internal helper - without lock
  bool HasCurrentBufferWithDataInternal() const {
    return current_buffer_ && current_buffer_->GetRecordCount() > 0;
  }
};

#endif  // SRC_API_PTI_METRICS_SCOPE_BUFFER_HANDLER_H_