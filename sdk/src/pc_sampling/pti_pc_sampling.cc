//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti/pti_pc_sampling.h"

#include <algorithm>

#include "pc_sampling/pti_pc_sampling_collector.h"
#include "pc_sampling/pti_pc_sampling_internal.h"

namespace pti::pc_sampling {

void ResetCollectionSession(pti_pc_sampling_handle_t handle) {
  if (handle == nullptr) {
    return;
  }

  ClearProfiledDeviceData(handle);
  handle->collector.reset();
  handle->collected_metric_group_ = nullptr;
  handle->collected_raw_data_.Reset();
  handle->samples_dropped_ = false;
}

}  // namespace pti::pc_sampling

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

  if (handle->state_ != pti::pc_sampling::PcSamplingState::kEnabled) {
    SPDLOG_ERROR(
        "{}: cannot configure PC sampling because handle is not in the initial state for "
        "configuration, "
        "current state is {}",
        __FUNCTION__, pti::pc_sampling::PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED;
  }

  if (devices != nullptr && device_count == 0) {
    SPDLOG_ERROR(
        "{}: Invalid PC sampling configuration parameters passed, devices = {} device_count = {}",
        __FUNCTION__, static_cast<const void*>(devices), device_count);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  // Find out list of supported devices
  handle->configured_devices_.clear();

  if (devices == nullptr) {
    // all supported devices will be profiled if no device filter is provided
    auto supported_devices = pti::pc_sampling::GetAllDevices();
    if (supported_devices.empty()) {
      SPDLOG_ERROR("{}: No supported devices found for PC sampling", __FUNCTION__);
      return PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED;
    }
    const size_t devices_to_copy =
        (std::min)(supported_devices.size(), pti::pc_sampling::kMaxConfiguredDevices);
    handle->configured_devices_.reserve(devices_to_copy);
    std::copy_n(supported_devices.begin(), devices_to_copy,
                std::back_inserter(handle->configured_devices_));
  } else {
    for (size_t i = 0; i < device_count; ++i) {
      if (pti::pc_sampling::IsPCSamplingSupportedDevice(devices[i])) {
        handle->configured_devices_.push_back(devices[i]);
        if (handle->configured_devices_.size() >= pti::pc_sampling::kMaxConfiguredDevices) {
          break;
        }
      } else {
        SPDLOG_WARN("{}: device {} does not support PC sampling and will be ignored", __FUNCTION__,
                    static_cast<const void*>(devices[i]));
      }
    }
  }

  if (handle->configured_devices_.empty()) {
    SPDLOG_ERROR("{}: None of the provided device(s) support PC sampling", __FUNCTION__);
    return PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED;
  }

  handle->sampling_period_ns_ =
      (sampling_period_ns == 0) ? pti::pc_sampling::kDefaultSamplingPeriodNs : sampling_period_ns;
  handle->state_ = pti::pc_sampling::PcSamplingState::kConfigured;
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

  pti::pc_sampling::ResetCollectionSession(handle);

  pti_device_handle_t profiling_device = nullptr;
  const pti_result configured_device_status =
      pti::pc_sampling::GetConfiguredDevice(handle, &profiling_device);
  if (configured_device_status != PTI_SUCCESS) {
    return configured_device_status;
  }

  ze_driver_handle_t driver =
      pti::pc_sampling::PtiPcSamplingHandleStorage::Instance().GetDriver(profiling_device);
  zet_metric_group_handle_t metric_group =
      pti::pc_sampling::PtiPcSamplingHandleStorage::Instance().GetMetricGroup(profiling_device);
  if (driver == nullptr || metric_group == nullptr) {
    SPDLOG_ERROR("{}: configured device {} is missing PC sampling configuration", __FUNCTION__,
                 static_cast<const void*>(profiling_device));
    return PTI_ERROR_INTERNAL;
  }

  handle->collector.reset(new (std::nothrow) pti::pc_sampling::PtiPcSamplingDataCollector(
      profiling_device, driver, metric_group, handle->collected_raw_data_,
      handle->samples_dropped_));
  if (handle->collector == nullptr) {
    SPDLOG_ERROR("{}: collector creation returned null", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  handle->collected_metric_group_ = metric_group;

  const pti_result start_status = handle->collector->Start(handle->sampling_period_ns_);
  if (start_status != PTI_SUCCESS) {
    pti::pc_sampling::ResetCollectionSession(handle);
    return start_status;
  }

  handle->state_ = pti::pc_sampling::PcSamplingState::kStarted;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingStopCollection(pti_pc_sampling_handle_t handle) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state_ == pti::pc_sampling::PcSamplingState::kStopped) {
    SPDLOG_ERROR("{}: PC sampling is already stopped", __FUNCTION__);
    return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED;
  }

  if (handle->state_ != pti::pc_sampling::PcSamplingState::kStarted) {
    SPDLOG_ERROR("{}: cannot stop PC sampling on not running collection, current state is {}",
                 __FUNCTION__, pti::pc_sampling::PcSamplingStateToString(handle->state_));
    return PTI_ERROR_PC_SAMPLING_NOT_STARTED;
  }

  if (handle->collector == nullptr) {
    SPDLOG_ERROR("{}: collector is missing while collection is running", __FUNCTION__);
    return PTI_ERROR_INTERNAL;
  }

  const pti_result stop_status = handle->collector->Stop();
  if (stop_status != PTI_SUCCESS) {
    return stop_status;
  }

  handle->collector.reset();
  handle->state_ = pti::pc_sampling::PcSamplingState::kStopped;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingGetStallReasons(pti_pc_sampling_handle_t handle,
                                        pti_pc_sampling_stall_reason_info_t* reasons,
                                        size_t* reason_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (reason_count == nullptr) {
    SPDLOG_ERROR("{}: reason_count output pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const pti_result stall_reasons_status = pti::pc_sampling::EnsureStallReasons(handle);
  if (stall_reasons_status != PTI_SUCCESS) {
    return stall_reasons_status;
  }

  const size_t total_reasons = handle->stall_reasons_.size();

  if (reasons == nullptr) {
    *reason_count = total_reasons;
    return PTI_SUCCESS;
  }

  if (*reason_count > total_reasons) {
    SPDLOG_WARN(
        "{}: provided reasons buffer holds {} entries but only {} stall reasons exist; "
        "the required count will be returned without populating the output buffer",
        __FUNCTION__, *reason_count, total_reasons);
    *reason_count = total_reasons;
    return PTI_SUCCESS;
  }

  const size_t entries_to_copy = (std::min)(*reason_count, total_reasons);
  *reason_count = entries_to_copy;
  for (size_t i = 0; i < entries_to_copy; ++i) {
    reasons[i]._name = handle->stall_reasons_[i].first;
    reasons[i]._description = handle->stall_reasons_[i].second;
  }

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

  if (handle->state_ != pti::pc_sampling::PcSamplingState::kStopped) {
    SPDLOG_ERROR("{}: profiled devices can be queried only after collection is stopped",
                 __FUNCTION__);
    return PTI_ERROR_PC_SAMPLING_NOT_STOPPED;
  }

  return pti::pc_sampling::FillProfiledDevices(handle, devices, device_count);
}

pti_result ptiPcSamplingGetObservedKernelHandles(pti_pc_sampling_handle_t handle,
                                                 pti_device_handle_t device,
                                                 uint64_t* kernel_handles, size_t* kernel_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (kernel_count == nullptr) {
    SPDLOG_ERROR("{}: kernel_count output pointer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const pti_result device_status =
      pti::pc_sampling::ValidateConfiguredProfiledDevice(handle, device);
  if (device_status != PTI_SUCCESS) {
    return device_status;
  }

  const pti_result aggregate_status = pti::pc_sampling::EnsureAggregatedResults(handle);
  if (aggregate_status != PTI_SUCCESS) {
    return aggregate_status;
  }

  return pti::pc_sampling::FillObservedKernelHandles(handle, kernel_handles, kernel_count);
}

pti_result ptiPcSamplingGetObservedKernelInfo(pti_pc_sampling_handle_t handle,
                                              pti_device_handle_t device, uint64_t kernel_handle,
                                              pti_pc_sampling_kernel_info_t* kernel_info) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (kernel_info == nullptr || kernel_info->_struct_size < sizeof(pti_pc_sampling_kernel_info_t)) {
    SPDLOG_ERROR("{}: kernel_info output is invalid", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const pti_result device_status =
      pti::pc_sampling::ValidateConfiguredProfiledDevice(handle, device);
  if (device_status != PTI_SUCCESS) {
    return device_status;
  }

  const pti_result aggregate_status = pti::pc_sampling::EnsureAggregatedResults(handle);
  if (aggregate_status != PTI_SUCCESS) {
    return aggregate_status;
  }

  return pti::pc_sampling::FillObservedKernelInfo(device, handle, kernel_handle, kernel_info);
}

pti_result ptiPcSamplingGetSamplesPerInstruction(pti_pc_sampling_handle_t handle,
                                                 pti_device_handle_t device, uint64_t kernel_handle,
                                                 pti_pc_sampling_instruction_t* instruction_buffer,
                                                 size_t instruction_buffer_count,
                                                 uint64_t* samples_buffer,
                                                 size_t samples_buffer_count) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (instruction_buffer == nullptr || samples_buffer == nullptr) {
    SPDLOG_ERROR("{}: instruction_buffer or samples_buffer is null", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const pti_result device_status =
      pti::pc_sampling::ValidateConfiguredProfiledDevice(handle, device);
  if (device_status != PTI_SUCCESS) {
    return device_status;
  }

  const pti_result aggregate_status = pti::pc_sampling::EnsureAggregatedResults(handle);
  if (aggregate_status != PTI_SUCCESS) {
    return aggregate_status;
  }

  return pti::pc_sampling::FillSamplesPerInstruction(handle, kernel_handle, instruction_buffer,
                                                     instruction_buffer_count, samples_buffer,
                                                     samples_buffer_count);
}

pti_result ptiPcSamplingGetDeviceStatus(pti_pc_sampling_handle_t handle, pti_device_handle_t device,
                                        pti_pc_sampling_device_status_t* device_status) {
  const pti_result handle_status = pti::pc_sampling::ValidateStoppedCollectionHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  const size_t expected_struct_size = sizeof(pti_pc_sampling_device_status_t);
  if (device == nullptr || device_status == nullptr ||
      device_status->_struct_size != expected_struct_size) {
    SPDLOG_ERROR("{}: device status output is invalid", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const pti_result aggregate_status = pti::pc_sampling::EnsureAggregatedResults(handle);
  if (aggregate_status != PTI_SUCCESS) {
    return aggregate_status;
  }

  const pti_pc_sampling_device_status_t& device_data = handle->device_aggregate_.status;
  if (device_data._device != device) {
    SPDLOG_ERROR("{}: device does not match the profiled PC sampling device", __FUNCTION__);
    return PTI_ERROR_BAD_ARGUMENT;
  }

  *device_status = device_data;
  return PTI_SUCCESS;
}

pti_result ptiPcSamplingDisable(pti_pc_sampling_handle_t handle) {
  pti_result handle_status = pti::pc_sampling::ValidateHandle(handle);
  if (handle_status != PTI_SUCCESS) {
    return handle_status;
  }

  if (handle->state_ != pti::pc_sampling::PcSamplingState::kStopped) {
    SPDLOG_WARN("{}: destroying PC sampling handle while collection state is {}", __FUNCTION__,
                pti::pc_sampling::PcSamplingStateToString(handle->state_));
    // Attempt to stop collection if it's still running before destroying handle
    if (handle->state_ == pti::pc_sampling::PcSamplingState::kStarted) {
      handle_status = ptiPcSamplingStopCollection(handle);
      if (handle_status != PTI_SUCCESS) {
        SPDLOG_WARN("{}: failed to stop collection before destroying handle, status: {:#x}",
                    __FUNCTION__, static_cast<uint32_t>(handle_status));
      }
    }
  }

  pti::pc_sampling::ResetCollectionSession(handle);

  return pti::pc_sampling::PtiPcSamplingHandleStorage::Instance().Destroy(handle);
}
