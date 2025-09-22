//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef ZE_COLLECTOR_CB_HELPERS_H_
#define ZE_COLLECTOR_CB_HELPERS_H_

#include <spdlog/spdlog.h>

#include <atomic>
#include <unordered_map>

#include "pti/pti_sync_callback.h"

using SubscriberId = uint32_t;
#define INVALID_KERNEL_HANDLE 0

struct CbDomainProperties {
  pti_callback_domain domain;
  // serve for ENTER/EXIT to API, THREAD_START/THREAD_END to THREAD or other "events" that
  // come in a pair
  bool is_enter_enabled;
  bool is_exit_enabled;
};

class ZeCollectorCallbackSubscriber {
 private:
  static inline std::atomic<SubscriberId> next_id_{1};

  static SubscriberId GenerateUniqueId() { return next_id_.fetch_add(1); }

 public:
  ZeCollectorCallbackSubscriber()
      : id_(GenerateUniqueId()), callback_(nullptr), user_data_(nullptr) {}

  ZeCollectorCallbackSubscriber(const ZeCollectorCallbackSubscriber&) = delete;
  ZeCollectorCallbackSubscriber& operator=(const ZeCollectorCallbackSubscriber&) = delete;
  ZeCollectorCallbackSubscriber(ZeCollectorCallbackSubscriber&&) = delete;
  ZeCollectorCallbackSubscriber& operator=(ZeCollectorCallbackSubscriber&&) = delete;

  // Virtual destructor for proper cleanup in derived classes
  virtual ~ZeCollectorCallbackSubscriber() = default;

  SubscriberId GetId() const { return id_; }

  pti_result EnableDomain(pti_callback_domain domain, uint32_t enter_cb, uint32_t exit_cb) {
    // for now only few specific domains supported
    if (domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED &&
        domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
      return PTI_ERROR_NOT_IMPLEMENTED;
    }
    if (enter_cb == 0 && exit_cb == 0) {
      return PTI_ERROR_BAD_ARGUMENT;  // at least one should be non-zero
    }
    if (enabled_domains_.find(domain) != enabled_domains_.end()) {
      enabled_domains_[domain].is_enter_enabled = static_cast<bool>(enter_cb);
      enabled_domains_[domain].is_exit_enabled = static_cast<bool>(exit_cb);
      // Domain already enabled, return success
      return PTI_SUCCESS;
    }
    CbDomainProperties properties = {domain, static_cast<bool>(enter_cb),
                                     static_cast<bool>(exit_cb)};
    enabled_domains_[domain] = properties;
    return PTI_SUCCESS;
  }

  pti_result DisableDomain(pti_callback_domain domain) {
    auto it = enabled_domains_.find(domain);
    if (it != enabled_domains_.end()) {
      enabled_domains_.erase(it);
      return PTI_SUCCESS;
    }
    return PTI_ERROR_BAD_ARGUMENT;  // Domain not found
  }

  pti_result DisableAllDomains() {
    SPDLOG_TRACE("In {}", __func__);
    enabled_domains_.clear();
    return PTI_SUCCESS;
  }

  pti_result SetCallback(pti_callback_function callback) {
    if (callback == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    callback_ = callback;
    return PTI_SUCCESS;
  }

  pti_callback_function GetCallback() const { return callback_; }

  bool IsActive() const { return !enabled_domains_.empty() && callback_ != nullptr; }

  void* GetUserData() const { return user_data_; }

  void SetUserData(void* user_data) { user_data_ = user_data; }

  void** GetPtrForInstanceUserData() { return &instance_user_data_; }

  pti_result Clean() {
    callback_ = nullptr;
    user_data_ = nullptr;
    return DisableAllDomains();
  }

  bool IsEnabled(pti_callback_domain domain, uint32_t cb_type) const {
    auto it = enabled_domains_.find(domain);
    if (it != enabled_domains_.end()) {
      if (cb_type == PTI_CB_PHASE_API_ENTER) {
        return it->second.is_enter_enabled;
      } else if (cb_type == PTI_CB_PHASE_API_EXIT) {
        return it->second.is_exit_enabled;
      }
    }
    return false;
  }

  void InvokeCallbackGPUOperationCompletion(const ZeKernelCommandExecutionRecord& record,
                                            uint32_t l0_api_id) {
    if (callback_ != nullptr) {
      pti_gpu_op_details gpu_op_details = {
          ._operation_kind = GetGPUOperationKind(record.command_type_),
          ._operation_id = record.kid_,
          // temp, until modules & kernels in them supported
          ._kernel_handle = INVALID_KERNEL_HANDLE,
          ._name = record.name_.c_str()};
      pti_device_handle_t device_handle = record.device_;
      if (record.command_type_ == KernelCommandType::kMemory && device_handle == nullptr) {
        device_handle = record.dst_device_;  // for memcpy use dst device
      }
      pti_callback_gpu_op_data callback_data = {
          ._domain = PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
          ._cmd_list_properties = PTI_BACKEND_COMMAND_LIST_TYPE_UNKNOWN,
          ._cmd_list_handle = nullptr,
          ._queue_handle = record.queue_,
          ._device_handle = device_handle,
          ._phase = PTI_CB_PHASE_API_EXIT,
          ._return_code = 0,
          ._correlation_id = record.cid_,
          ._operation_count = 1,
          ._operation_details = &gpu_op_details};
      callback_(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, PTI_API_GROUP_LEVELZERO, l0_api_id,
                record.context_, &callback_data, GetUserData(), GetPtrForInstanceUserData());
    }
  }

  static pti_gpu_operation_kind GetGPUOperationKind(const KernelCommandType& type) {
    // Map KernelCommandType to pti_gpu_operation_kind
    // TODO: consider to use the same enum, make it external and then this funcion not needed
    switch (type) {
      case KernelCommandType::kKernel:
        return PTI_GPU_OPERATION_KIND_KERNEL;
      case KernelCommandType::kMemory:
        return PTI_GPU_OPERATION_KIND_MEMORY;
      case KernelCommandType::kCommand:
        return PTI_GPU_OPERATION_KIND_OTHER;
      default:
        return PTI_GPU_OPERATION_KIND_INVALID;
    }
  }

 private:
  SubscriberId id_;
  pti_callback_function callback_ = nullptr;
  std::unordered_map<pti_callback_domain, CbDomainProperties> enabled_domains_;
  void* user_data_ = nullptr;
  void* instance_user_data_ = nullptr;
};

#endif
