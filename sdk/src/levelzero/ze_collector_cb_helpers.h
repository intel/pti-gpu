//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef ZE_COLLECTOR_CB_HELPERS_H_
#define ZE_COLLECTOR_CB_HELPERS_H_

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

#include "pti/pti_callback.h"
#include "unikernel.h"

#define CB_SUBSCRIBER_HANDLE_INVALID nullptr

struct pair_hash {
  std::size_t operator()(const std::pair<pti_backend_ctx_t, pti_device_handle_t>& p) const {
    uintptr_t a1 = reinterpret_cast<uintptr_t>(p.first);
    uintptr_t a2 = reinterpret_cast<uintptr_t>(p.second);
    auto h1 = std::hash<uintptr_t>{}(a1);
    auto h2 = std::hash<uintptr_t>{}(a2);
    // Combine the two hashes
    return h1 ^ (h2 << 1);
  }
};

// Not copy Records but store their pointers
using ExecRecordsMap =
    std::unordered_map<std::pair<pti_backend_ctx_t, pti_device_handle_t>,
                       std::vector<const ZeKernelCommandExecutionRecord*>, pair_hash>;

#define INVALID_KERNEL_HANDLE 0

struct CbDomainProperties {
  pti_callback_domain domain;
  // serve for ENTER/EXIT to API, THREAD_START/THREAD_END to THREAD or other "events" that
  // come in a pair
  bool is_enter_enabled;
  bool is_exit_enabled;
};

struct _pti_callback_subscriber {
  _pti_callback_subscriber(pti_callback_function callback, void* user_data)
      : callback_(callback), user_data_(user_data), instance_user_data_(nullptr) {}

  _pti_callback_subscriber(const _pti_callback_subscriber&) = delete;
  _pti_callback_subscriber& operator=(const _pti_callback_subscriber&) = delete;
  _pti_callback_subscriber(_pti_callback_subscriber&&) = delete;
  _pti_callback_subscriber& operator=(_pti_callback_subscriber&&) = delete;
  pti_callback_function callback_ = nullptr;
  std::unordered_map<pti_callback_domain, CbDomainProperties> domains_;
  mutable void* user_data_ = nullptr;
  mutable void* instance_user_data_ = nullptr;
};

class ZeCollectorCBSubscriber {
 public:
  ZeCollectorCBSubscriber() : subscriber_info_(nullptr, nullptr) {}

  ZeCollectorCBSubscriber(const ZeCollectorCBSubscriber&) = delete;
  ZeCollectorCBSubscriber& operator=(const ZeCollectorCBSubscriber&) = delete;
  ZeCollectorCBSubscriber(ZeCollectorCBSubscriber&&) = delete;
  ZeCollectorCBSubscriber& operator=(ZeCollectorCBSubscriber&&) = delete;

  // Virtual destructor for proper cleanup in derived classes
  virtual ~ZeCollectorCBSubscriber() = default;

  _pti_callback_subscriber* GetHandle() { return &subscriber_info_; }

  pti_result EnableDomain(pti_callback_domain domain, uint32_t enter_cb, uint32_t exit_cb) {
    // for now only few specific domains supported
    if (domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED &&
        domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
      return PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (enter_cb == 0 && exit_cb == 0 && domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
      return PTI_ERROR_BAD_ARGUMENT;  // at least one should be non-zero
    }
    if (subscriber_info_.domains_.find(domain) != subscriber_info_.domains_.end()) {
      subscriber_info_.domains_[domain].is_enter_enabled = static_cast<bool>(enter_cb);
      subscriber_info_.domains_[domain].is_exit_enabled = static_cast<bool>(exit_cb);
      // Domain already enabled, return success
      return PTI_SUCCESS;
    }
    CbDomainProperties properties = {domain, static_cast<bool>(enter_cb),
                                     static_cast<bool>(exit_cb)};
    subscriber_info_.domains_[domain] = properties;
    return PTI_SUCCESS;
  }

  pti_result DisableDomain(pti_callback_domain domain) {
    auto it = subscriber_info_.domains_.find(domain);
    if (it != subscriber_info_.domains_.end()) {
      subscriber_info_.domains_.erase(it);
      return PTI_SUCCESS;
    }
    return PTI_ERROR_BAD_ARGUMENT;  // Domain not found
  }

  pti_result DisableAllDomains() {
    SPDLOG_TRACE("In {}", __func__);
    subscriber_info_.domains_.clear();
    return PTI_SUCCESS;
  }

  pti_result SetCallback(pti_callback_function callback) {
    if (callback == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }
    subscriber_info_.callback_ = callback;
    return PTI_SUCCESS;
  }

  pti_callback_function GetCallback() const { return subscriber_info_.callback_; }

  bool IsActive() const {
    return !subscriber_info_.domains_.empty() && subscriber_info_.callback_ != nullptr;
  }

  void* GetUserData() const { return subscriber_info_.user_data_; }

  void SetUserData(void* user_data) { subscriber_info_.user_data_ = user_data; }

  void** GetPtrForInstanceUserData() const { return &(subscriber_info_.instance_user_data_); }

  pti_result Clean() {
    subscriber_info_.callback_ = nullptr;
    subscriber_info_.user_data_ = nullptr;
    return DisableAllDomains();
  }

  bool IsEnabled(pti_callback_domain domain, uint32_t cb_type) const {
    auto it = subscriber_info_.domains_.find(domain);
    if (it != subscriber_info_.domains_.end()) {
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
    if (subscriber_info_.callback_ != nullptr) {
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
      subscriber_info_.callback_(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
                                 PTI_API_GROUP_LEVELZERO, l0_api_id, record.context_,
                                 &callback_data, GetUserData(), GetPtrForInstanceUserData());
    }
  }

  static void MapRecordsByContextAndDevice(
      const std::vector<ZeKernelCommandExecutionRecord>& records, ExecRecordsMap& record_map) {
    for (const auto& record : records) {
      auto key = std::make_pair(record.context_, record.device_);
      record_map[key].push_back(&record);
    }
  }

  static void MakeGPUOpDetailsArray(
      const std::vector<const ZeKernelCommandExecutionRecord*>& records,
      std::vector<pti_gpu_op_details>& op_details) {
    PTI_ASSERT(records.size() == op_details.size());

    for (size_t i = 0; i < records.size(); ++i) {
      op_details[i]._operation_kind = GetGPUOperationKind(records[i]->command_type_);
      op_details[i]._operation_id = records[i]->kid_;
      // temp, until modules & kernels in them supported
      op_details[i]._kernel_handle = INVALID_KERNEL_HANDLE;
      op_details[i]._name = records[i]->name_.c_str();
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
  _pti_callback_subscriber subscriber_info_;
};

class SubscribersCollection {
 public:
  SubscribersCollection() {}
  SubscribersCollection(const SubscribersCollection&) = delete;
  SubscribersCollection& operator=(const SubscribersCollection&) = delete;
  SubscribersCollection(SubscribersCollection&&) = delete;
  SubscribersCollection& operator=(SubscribersCollection&&) = delete;

  ~SubscribersCollection() = default;

  pti_callback_subscriber_handle AddExternalSubscriber(
      std::unique_ptr<ZeCollectorCBSubscriber> subscriber) {
    const std::lock_guard<std::shared_mutex> lock(lock_);
    bool is_internal = false;
    return AddSubscriber(std::move(subscriber), is_internal);
  }

  pti_callback_subscriber_handle AddInternalSubscriber(
      std::unique_ptr<ZeCollectorCBSubscriber> subscriber) {
    const std::lock_guard<std::shared_mutex> lock(lock_);
    bool is_internal = true;
    return AddSubscriber(std::move(subscriber), is_internal);
  }

  bool RemoveExternalSubscriber(pti_callback_subscriber_handle handle) {
    const std::lock_guard<std::shared_mutex> lock(lock_);
    return RemoveSubscriber(handle);
  }

  bool RemoveInternalSubscriber(pti_callback_subscriber_handle handle) {
    const std::lock_guard<std::shared_mutex> lock(lock_);
    return RemoveSubscriber(handle);
  }

  const ZeCollectorCBSubscriber* GetSubscriber(pti_callback_subscriber_handle handle) const {
    const std::shared_lock<std::shared_mutex> lock(lock_);
    auto it = subscribers_map_.find(handle);
    if (it != subscribers_map_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  std::vector<pti_callback_subscriber_handle> GetAllSubscriberHandles() const {
    std::shared_lock<std::shared_mutex> lock(lock_);
    std::vector<pti_callback_subscriber_handle> handles;
    handles.reserve(subscribers_map_.size());
    for (const auto& [handle, subscriber] : subscribers_map_) {
      handles.push_back(handle);
    }
    return handles;
  }

  bool IsAnySubscriberActive() const {
    std::shared_lock<std::shared_mutex> lock(lock_);
    for (const auto& kv : subscribers_map_) {
      if (kv.second && kv.second->IsActive()) {
        return true;
      }
    }
    return false;
  }

  pti_result EnableCallbackDomain(pti_callback_subscriber_handle handle, pti_callback_domain domain,
                                  uint32_t enter_cb, uint32_t exit_cb) {
    std::lock_guard<std::shared_mutex> lock(lock_);
    if (subscribers_map_.find(handle) != subscribers_map_.end()) {
      auto subscriber = subscribers_map_[handle].get();
      PTI_ASSERT(subscriber != nullptr);
      return subscriber->EnableDomain(domain, enter_cb, exit_cb);
    }
    return pti_result::PTI_ERROR_BAD_ARGUMENT;  // Subscriber not found
  }

  pti_result DisableCallbackDomain(pti_callback_subscriber_handle handle,
                                   pti_callback_domain domain) {
    std::lock_guard<std::shared_mutex> lock(lock_);
    if (subscribers_map_.find(handle) != subscribers_map_.end()) {
      auto subscriber = subscribers_map_[handle].get();
      PTI_ASSERT(subscriber != nullptr);
      return subscriber->DisableDomain(domain);
    }
    return pti_result::PTI_ERROR_BAD_ARGUMENT;  // Subscriber not found
  }

  pti_result DisableAllCallbackDomains(pti_callback_subscriber_handle handle) {
    std::lock_guard<std::shared_mutex> lock(lock_);
    if (subscribers_map_.find(handle) != subscribers_map_.end()) {
      auto subscriber = subscribers_map_[handle].get();
      PTI_ASSERT(subscriber != nullptr);
      return subscriber->DisableAllDomains();
    }
    return pti_result::PTI_ERROR_BAD_ARGUMENT;  // Subscriber not found
  }

  size_t GetSubscriberCount() const {
    const std::shared_lock<std::shared_mutex> lock(lock_);
    return subscribers_map_.size();
  }

  auto begin() { return subscribers_list_.begin(); }
  auto end() { return subscribers_list_.end(); }

 private:
  pti_callback_subscriber_handle AddSubscriber(std::unique_ptr<ZeCollectorCBSubscriber> subscriber,
                                               bool is_internal) {
    // lock in a caller
    if (subscriber == nullptr) {
      return CB_SUBSCRIBER_HANDLE_INVALID;
    }
    auto handle = subscriber->GetHandle();
    auto it = std::find(subscribers_list_.begin(), subscribers_list_.end(), handle);
    if (it != subscribers_list_.end()) {
      // something wrong, subscriber already in the list
      PTI_ASSERT(subscribers_map_.find(handle) != subscribers_map_.end());
      return CB_SUBSCRIBER_HANDLE_INVALID;  // Subscriber already exists
    }
    PTI_ASSERT(subscribers_map_.find(handle) == subscribers_map_.end());
    if (is_internal) {
      subscribers_list_.push_back(handle);
    } else {
      subscribers_list_.push_front(handle);
    }
    subscribers_map_.insert({handle, std::move(subscriber)});
    return handle;
  }

  bool RemoveSubscriber(pti_callback_subscriber_handle handle) {
    // lock in a caller
    auto it = std::find(subscribers_list_.begin(), subscribers_list_.end(), handle);
    if (it != subscribers_list_.end()) {
      PTI_ASSERT(subscribers_map_.find(handle) != subscribers_map_.end());
      subscribers_list_.erase(it);
      subscribers_map_.erase(handle);
      return true;
    }
    PTI_ASSERT(subscribers_map_.find(handle) == subscribers_map_.end());
    return false;
  }

  //  PTI subscribers
  // list ensures subscribers will be called in an order they were added
  std::list<pti_callback_subscriber_handle> subscribers_list_;

  std::unordered_map<pti_callback_subscriber_handle, std::unique_ptr<ZeCollectorCBSubscriber>>
      subscribers_map_;
  mutable std::shared_mutex lock_;
};

#endif
