//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_API_PTI_METRICS_SCOPE_BUFFER_H_
#define SRC_API_PTI_METRICS_SCOPE_BUFFER_H_

#include <level_zero/ze_api.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "pti/pti.h"
#include "pti/pti_callback.h"

namespace {

// Key constants limiting sizes of Metrics Scope buffers

// this cap number of records per buffer is to avoid allocating too large buffers
// that might lead to memory allocation failures.
// this makes max buffer ~ 10 MB
constexpr size_t kMaxNumberOfRawRecordsPerCollectionBuffer = 10000;

// Estimate based on metric group properties
// based on observation, the size of each collection is about 700-900 bytes,
// depending on the hardware. Choosing 960 for now, as it is divisible by 64 (cacheline size)
// TODO: ask Level-Zero for the API to query for it

constexpr size_t kEstimatedSizeOfRawRecord = 960;

// Minimum collection buffer size - bigger than estoimated one raw record size,
// so that if it gets bigger - at least one record would fit there
constexpr size_t kMinCollectionBufferSize = 2048;
}  // namespace

// Kernel metric data structure
struct KernelMetricData {
  uint64_t kernel_id = 0;
  std::string kernel_name;
  std::unique_ptr<uint8_t[]> raw_data;
  size_t data_size = 0;
  ze_command_queue_handle_t queue = nullptr;
};

/**
 * @brief PtiMetricsScopeBuffer class that contains actual collected data and manages buffer state
 * Tracks beginning position, current position, and can contain multiple scopes
 */
class PtiMetricsScopeBuffer {
 public:
  PtiMetricsScopeBuffer(size_t capacity, pti_device_handle_t device_handle, uint64_t buffer_id)
      : used_size_(0),
        capacity_(capacity),
        device_handle_(device_handle),
        buffer_id_(buffer_id),
        is_finalized_(false) {
    raw_buffer_ = std::make_unique<uint8_t[]>(capacity);
    if (raw_buffer_) {
      std::memset(raw_buffer_.get(), 0, capacity);
    }

    records_.reserve(64);
  }

  // PtiMetricsScopeBuffer state management
  uint8_t* GetRawBuffer() const { return raw_buffer_.get(); }
  size_t GetUsedSize() const { return used_size_; }

  // PtiMetricsScopeBuffer metadata
  pti_device_handle_t GetDeviceHandle() const { return device_handle_; }
  uint64_t GetBufferId() const { return buffer_id_; }
  size_t GetRecordCount() const { return records_.size(); }
  bool IsFinalized() const { return is_finalized_; }

  // Record management
  bool AddKernelRecord(std::unique_ptr<KernelMetricData> kernel_data) {
    if (!kernel_data) {
      SPDLOG_DEBUG("AddKernelRecord: REJECT null kernel_data");
      return false;
    }
    if (is_finalized_) {
      SPDLOG_DEBUG(
          "AddKernelRecord: REJECT buffer is_finalized, kernel_id={}, buffer_id={}, device={}",
          kernel_data->kernel_id, buffer_id_, static_cast<void*>(device_handle_));
      return false;
    }

    size_t required_space = kernel_data->data_size + sizeof(KernelMetricData);
    if (!HasSpace(required_space)) {
      SPDLOG_DEBUG(
          "AddKernelRecord: REJECT no space (used={}, cap={}, required={}, data_size={}), "
          "kernel_id={}, buffer_id={}",
          used_size_, capacity_, required_space, kernel_data->data_size, kernel_data->kernel_id,
          buffer_id_);
      return false;
    }

    records_.push_back(std::move(kernel_data));
    used_size_ += required_space;
    return true;
  }

  const KernelMetricData* GetRecord(size_t index) const {
    if (index >= records_.size()) {
      return nullptr;
    }
    return records_[index].get();
  }

  // Buffer operations
  bool HasSpace(size_t required_size) const { return (used_size_ + required_size) <= capacity_; }

  void Finalize() { is_finalized_ = true; }

 private:
  std::unique_ptr<uint8_t[]> raw_buffer_;
  size_t used_size_;
  size_t capacity_;
  pti_device_handle_t device_handle_;
  uint64_t buffer_id_;
  bool is_finalized_;

  // Records management
  std::vector<std::unique_ptr<KernelMetricData>> records_;
};

#endif  // SRC_API_PTI_METRICS_SCOPE_BUFFER_H_
