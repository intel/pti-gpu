//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti/pti_pc_sampling.h"

#include <algorithm>

#include "pc_sampling/pti_pc_sampling_internal.h"

pti_result ptiPcSamplingEnable(pti_pc_sampling_handle_t* handle) {
  return pti::pc_sampling::PtiPcSamplingHandleStorage::Instance().Create(handle);
}

pti_result ptiPcSamplingConfigure(pti_pc_sampling_handle_t handle,
                                  const pti_device_handle_t* devices, size_t device_count,
                                  uint32_t sampling_period_ns) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_INITIALIZED) {
    SPDLOG_ERROR(
        "{}: cannot configure PC sampling because handle is not in the initial state for "
        "configuration, "
        "current state is {}",
        __FUNCTION__, pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED;
  }

  if (devices != nullptr || device_count != 0) {
    SPDLOG_ERROR(
        "{}: device-filtered PC sampling configuration is not implemented; "
        "pass devices == nullptr and device_count == 0",
        __FUNCTION__);
    return PTI_ERROR_NOT_IMPLEMENTED;
  }

  handle->configured_devices.clear();
  handle->sampling_period_ns =
      (sampling_period_ns == 0) ? kDefaultSamplingPeriodNs : sampling_period_ns;
  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_CONFIGURED;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingQueryCollectionBufferSize(pti_pc_sampling_handle_t, size_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingSetCollectionBufferSize(pti_pc_sampling_handle_t, size_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingStartCollection(pti_pc_sampling_handle_t handle) {
  const pti_result configured_status = pti::pc_sampling::ValidateConfiguredHandle(handle);
  if (configured_status != PTI_SUCCESS) {
    return configured_status;
  }

  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_STARTED;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingStopCollection(pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state == pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED) {
    SPDLOG_ERROR("{}: PC sampling is already stopped", __FUNCTION__);
    return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_STARTED) {
    SPDLOG_ERROR("{}: cannot stop PC sampling on not running collection, current state is {}",
                 __FUNCTION__, pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
    return PTI_ERROR_PC_SAMPLING_NOT_STARTED;
  }

  handle->state = pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingGetStallReasons(pti_pc_sampling_handle_t handle,
                                        pti_pc_sampling_stall_reason_info_t*,
                                        size_t* reason_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (reason_count == nullptr) {
    SPDLOG_ERROR("{}: reason_count output pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  *reason_count = 0;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingGetProfiledDevices(pti_pc_sampling_handle_t handle,
                                           pti_device_handle_t* devices, size_t* device_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (device_count == nullptr) {
    SPDLOG_ERROR("{}: device_count output pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED) {
    SPDLOG_ERROR("{}: profiled devices can be queried only after collection is stopped",
                 __FUNCTION__);
    return PTI_ERROR_PC_SAMPLING_NOT_STOPPED;
  }

  const size_t configured_device_count = handle->configured_devices.size();
  if (devices != nullptr && configured_device_count != 0 && *device_count != 0) {
    const size_t copied_device_count = std::min(*device_count, configured_device_count);
    std::copy_n(handle->configured_devices.begin(), copied_device_count, devices);
  }
  *device_count = configured_device_count;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingGetObservedKernelHandles(pti_pc_sampling_handle_t handle,
                                                 pti_device_handle_t device, uint64_t*,
                                                 size_t* kernel_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (kernel_count == nullptr) {
    SPDLOG_ERROR("{}: kernel_count output pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (!pti::pc_sampling::IsConfiguredDevice(handle, device)) {
    SPDLOG_ERROR("{}: device does not match the configured PC sampling device", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  *kernel_count = 0;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingGetObservedKernelInfo(pti_pc_sampling_handle_t handle,
                                              pti_device_handle_t device, uint64_t,
                                              pti_pc_sampling_kernel_info_t* kernel_info) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (kernel_info == nullptr || kernel_info->_struct_size < sizeof(pti_pc_sampling_kernel_info_t)) {
    SPDLOG_ERROR("{}: kernel_info output is invalid", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (!pti::pc_sampling::IsConfiguredDevice(handle, device)) {
    SPDLOG_ERROR("{}: device does not match the configured PC sampling device", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetSamplesPerInstruction(pti_pc_sampling_handle_t handle,
                                                 pti_device_handle_t device, uint64_t,
                                                 pti_pc_sampling_instruction_t*, size_t, uint64_t*,
                                                 size_t) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (!pti::pc_sampling::IsConfiguredDevice(handle, device)) {
    SPDLOG_ERROR("{}: device does not match the configured PC sampling device", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetDeviceStatus(pti_pc_sampling_handle_t, pti_device_handle_t,
                                        pti_pc_sampling_device_status_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingDisable(pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state != pti_pc_sampling_state::PTI_PC_SAMPLING_STOPPED) {
    SPDLOG_WARN("{}: destroying PC sampling handle while collection state is {}", __FUNCTION__,
                pti::pc_sampling::PtiPcSamplingStateToString(handle->state));
  }

  return pti::pc_sampling::PtiPcSamplingHandleStorage::Instance().Destroy(handle);
}
