//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_API_PTI_METRICS_SCOPE_BUFFER_H_
#define SRC_API_PTI_METRICS_SCOPE_BUFFER_H_

#include <cstdlib>
#include <cstring>

#include "pti/pti.h"
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
struct kernel_metric_data {
  uint64_t kernel_id;
  char* kernel_name;
  uint64_t start_time;
  uint64_t end_time;
  std::unique_ptr<uint8_t[]> raw_data;
  size_t data_size;
  ze_context_handle_t context;
  ze_command_queue_handle_t queue;
  pti_backend_command_list_type submit_type;

  kernel_metric_data() = default;

  // Constructor
  kernel_metric_data(kernel_metric_data&& other) noexcept
      : kernel_id(other.kernel_id),
        kernel_name(other.kernel_name),  // Transfer ownership
        start_time(other.start_time),
        end_time(other.end_time),
        raw_data(std::move(other.raw_data)),
        data_size(other.data_size),
        context(other.context),
        queue(other.queue),
        submit_type(other.submit_type) {
    other.kernel_name = nullptr;  // Clear source pointer
  }

  // Assignment Operator Overload
  kernel_metric_data& operator=(kernel_metric_data&& other) noexcept {
    if (this != &other) {
      // Clean up existing kernel_name
      delete[] kernel_name;

      kernel_id = other.kernel_id;
      kernel_name = other.kernel_name;  // Transfer ownership
      start_time = other.start_time;
      end_time = other.end_time;
      raw_data = std::move(other.raw_data);
      data_size = other.data_size;
      context = other.context;
      queue = other.queue;
      submit_type = other.submit_type;

      other.kernel_name = nullptr;  // Clear source pointer
    }
    return *this;
  }

  // Destructor to handle kernel_name cleanup
  ~kernel_metric_data() { delete[] kernel_name; }

  // Delete copy constructor and assignment to prevent accidental copying
  kernel_metric_data(const kernel_metric_data&) = delete;
  kernel_metric_data& operator=(const kernel_metric_data&) = delete;
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

  ~PtiMetricsScopeBuffer() = default;

  // Delete copy constructor and assignment operator
  PtiMetricsScopeBuffer(const PtiMetricsScopeBuffer&) = delete;
  PtiMetricsScopeBuffer& operator=(const PtiMetricsScopeBuffer&) = delete;

  // PtiMetricsScopeBuffer state management
  uint8_t* GetRawBuffer() const { return raw_buffer_.get(); }
  size_t GetUsedSize() const { return used_size_; }
  size_t GetCapacity() const { return capacity_; }
  size_t GetAvailableSpace() const { return capacity_ - used_size_; }

  // Position tracking
  uint8_t* GetBeginning() const { return raw_buffer_.get(); }
  uint8_t* GetCurrentPosition() const { return raw_buffer_.get() + used_size_; }

  // PtiMetricsScopeBuffer metadata
  pti_device_handle_t GetDeviceHandle() const { return device_handle_; }
  uint64_t GetBufferId() const { return buffer_id_; }
  size_t GetRecordCount() const { return records_.size(); }
  bool IsFinalized() const { return is_finalized_; }

  // Record management
  bool AddKernelRecord(std::unique_ptr<kernel_metric_data> kernel_data) {
    if (!kernel_data || is_finalized_) {
      return false;
    }

    size_t required_space = kernel_data->data_size + sizeof(kernel_metric_data);
    if (!HasSpace(required_space)) {
      return false;
    }

    records_.push_back(std::move(kernel_data));
    used_size_ += required_space;
    return true;
  }

  const kernel_metric_data* GetRecord(size_t index) const {
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
  std::vector<std::unique_ptr<kernel_metric_data>> records_;
};

#endif  // SRC_API_PTI_METRICS_SCOPE_BUFFER_H_
