//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_INTERNAL_H_
#define PTI_PC_SAMPLING_INTERNAL_H_

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <new>
#include <vector>

#include "pti/pti_pc_sampling.h"

constexpr uint32_t kDefaultSamplingPeriodNs = 100'000;  // 100 µs
constexpr size_t kMaxConfiguredDevices = 1;

enum class pti_pc_sampling_state : uint32_t {
  PTI_PC_SAMPLING_INITIALIZED = 0,  // Handle was created but not configured yet.
  PTI_PC_SAMPLING_CONFIGURED = 1,   // Configuration was accepted; collection has not started.
  PTI_PC_SAMPLING_STARTED = 2,      // Collection is currently running.
  PTI_PC_SAMPLING_STOPPED = 3,      // Collection has stopped; query APIs are now valid.
  PTI_PC_SAMPLING_FORCE_UINT32 = 0x7fffffff,
};

PTI_STATIC_ASSERT(sizeof(pti_pc_sampling_state) == sizeof(uint32_t),
                  "pti_pc_sampling_state enum should be equal to size of uint32_t");

struct _pti_pc_sampling_handle_t {
  pti_pc_sampling_state state = pti_pc_sampling_state::PTI_PC_SAMPLING_INITIALIZED;
  uint32_t sampling_period_ns = kDefaultSamplingPeriodNs;
  std::vector<pti_device_handle_t> configured_devices;
};

namespace pti::pc_sampling {

inline const char* PtiPcSamplingStateToString(pti_pc_sampling_state state) {
  switch (state) {
    case pti_pc_sampling_state::PTI_PC_SAMPLING_INITIALIZED:
      return "INITIALIZED";
    case pti_pc_sampling_state::PTI_PC_SAMPLING_CONFIGURED:
      return "CONFIGURED";
    case pti_pc_sampling_state::PTI_PC_SAMPLING_STARTED:
      return "STARTED";
    case pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED:
      return "STOPPED";
    case pti_pc_sampling_state::PTI_PC_SAMPLING_FORCE_UINT32:
      return "FORCE_UINT32";
  }

  return "UNKNOWN";
}

class PtiPcSamplingHandleStorage {
 public:
  static PtiPcSamplingHandleStorage& Instance() {
    static PtiPcSamplingHandleStorage storage;
    return storage;
  }

  bool Contains(pti_pc_sampling_handle_t handle) const {
    return handle_ != nullptr && handle_.get() == handle;
  }

  pti_result Create(pti_pc_sampling_handle_t* handle) {
    if (handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    auto collection_handle = std::make_unique<_pti_pc_sampling_handle_t>();
    if (collection_handle == nullptr) {
      return PTI_ERROR_INTERNAL;
    }

    pti_pc_sampling_handle_t new_handle = collection_handle.get();
    if (handle_ != nullptr) {
      SPDLOG_ERROR("{}: only one PC sampling handle can be enabled at a time", __FUNCTION__);
      return PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED;
    }

    handle_ = std::move(collection_handle);
    *handle = new_handle;
    return PTI_SUCCESS;
  }

  pti_result Destroy(pti_pc_sampling_handle_t handle) {
    if (handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (handle_ == nullptr || handle_.get() != handle) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    handle_.reset();
    return PTI_SUCCESS;
  }

 private:
  std::unique_ptr<_pti_pc_sampling_handle_t> handle_;
};

inline bool IsOurHandle(pti_pc_sampling_handle_t handle) {
  return PtiPcSamplingHandleStorage::Instance().Contains(handle);
}

inline bool IsConfiguredDevice(const pti_pc_sampling_handle_t handle, pti_device_handle_t device) {
  if (handle == nullptr || device == nullptr) {
    return false;
  }

  return std::find(handle->configured_devices.begin(), handle->configured_devices.end(), device) !=
         handle->configured_devices.end();
}

inline pti_result ValidateHandle(const pti_pc_sampling_handle_t handle) {
  if (handle == nullptr) {
    SPDLOG_ERROR("{}: handle is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (!IsOurHandle(handle)) {
    SPDLOG_ERROR("{}: handle is invalid", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_SUCCESS;
}

inline pti_result ValidateConfiguredHandle(const pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state == pti_pc_sampling_state::PTI_PC_SAMPLING_STARTED) {
    SPDLOG_ERROR(
        "{}: cannot start PC sampling because collection is already running, current "
        "state is {}",
        __FUNCTION__, pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_ALREADY_STARTED;
  }

  if (handle->state == pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED) {
    SPDLOG_ERROR(
        "{}: cannot start PC sampling because collection is already stopped, current "
        "state is {}",
        __FUNCTION__, pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_CONFIGURED) {
    SPDLOG_ERROR(
        "{}: cannot start PC sampling unless collection is configured, current state is "
        "{}",
        __FUNCTION__, pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED;
  }

  return PTI_SUCCESS;
}

inline pti_result ValidateStoppedCollectionHandle(const pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED) {
    SPDLOG_ERROR("{}: collection must be stopped, current state is {}", __FUNCTION__,
                 PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_NOT_STOPPED;
  }

  return PTI_SUCCESS;
}

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_INTERNAL_H_
