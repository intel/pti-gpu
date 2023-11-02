//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_VIEW_HANDLER_H_
#define SRC_API_VIEW_HANDLER_H_

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include "common.h"
#include "default_buffer_callbacks.h"
#include "pti_view.h"
#include "spdlog/spdlog.h"

#if defined(PTI_TRACE_SYCL)
#include "sycl_collector.h"
#endif

#include "overhead_kinds.h"
#include "unikernel.h"
#include "view_buffer.h"
#include "view_record_info.h"
#include "ze_collector.h"

using AskForBufferEvent = std::function<void(unsigned char**, size_t*)>;
using ReturnBufferEvent = std::function<void(unsigned char*, size_t, size_t)>;
using ViewInsert = std::function<void(void*, const ZeKernelCommandExecutionRecord&)>;

inline void MemCopyEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void MemFillEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void KernelEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void SyclRuntimeEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void OverheadCollectionEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void ZeChromeKernelStagesCallback(void* data,
                                         std::vector<ZeKernelCommandExecutionRecord>& kcexecrec);

inline void SyclRuntimeViewCallback(void* data, ZeKernelCommandExecutionRecord& rec);
inline void OverheadCollectionCallback(void* data, ZeKernelCommandExecutionRecord& rec);

struct ViewData {
  const char* fn_name = "";
  ViewInsert callback;
};

inline const std::vector<ViewData>& GetViewNameAndCallback(pti_view_kind view) {
  // clang-format off
  static const std::map<pti_view_kind, std::vector<ViewData>> view_data_map =
      {
        {PTI_VIEW_DEVICE_GPU_KERNEL,
          {
            ViewData{"KernelEvent", KernelEvent}
          }
        },
        {PTI_VIEW_SYCL_RUNTIME_CALLS,
          {
            ViewData{"SyclRuntimeEvent", SyclRuntimeEvent}
          }
        },
        {PTI_VIEW_COLLECTION_OVERHEAD,
          {
            ViewData{"OverheadCollectionEvent", OverheadCollectionEvent}
          }
        },
        {PTI_VIEW_DEVICE_GPU_MEM_COPY,
          {
            ViewData{"zeCommandListAppendMemoryCopy", MemCopyEvent},
          }
        },
        {PTI_VIEW_DEVICE_GPU_MEM_FILL,
          {
            ViewData{"zeCommandListAppendMemoryFill", MemFillEvent}
          }
        },
      };
  // clang-format on
  const auto result = view_data_map.find(view);
  if (result == view_data_map.end()) {
    throw std::out_of_range("No view record handling routine in table");
  }
  return result->second;
}

constexpr auto kDefaultBufferQueueDepth = 50UL;
static std::atomic<bool> external_collection_enabled = false;

struct PtiViewRecordHandler {
 public:
  using ViewBuffer = pti::view::utilities::ViewBuffer;
  using ViewBufferQueue = pti::view::utilities::ViewBufferQueue;
  using ViewBufferTable = pti::view::utilities::ViewBufferTable<std::thread::id>;
  using ViewEventTable = pti::view::utilities::ThreadSafeHashTable<std::string, ViewInsert>;
  using KernelNameStorageQueue =
      pti::view::utilities::ViewRecordBufferQueue<std::unique_ptr<std::string>>;

  PtiViewRecordHandler()
      : get_new_buffer_(pti::view::defaults::DefaultBufferAllocation),
        deliver_buffer_(pti::view::defaults::DefaultRecordParser),
        buffer_consumer_([this]() { return BufferConsumer(); }) {
    // Set queue depth based on number of hardware threads supported.
    constexpr auto kBufQueueDepthMult = 2UL;
    const auto threads_supported = std::thread::hardware_concurrency();
    if (threads_supported) {
      buffer_queue_.SetBufferDepth(kBufQueueDepthMult * threads_supported);
    }

    if (!collector_) {
      CollectorOptions collector_options;
      collector_options.kernel_tracing = true;
      collector_ = ZeCollector::Create(collector_options, ZeChromeKernelStagesCallback, nullptr,
                                       nullptr, nullptr);
      overhead::SetOverheadCallback(OverheadCollectionCallback);
    }
  }

  PtiViewRecordHandler(const PtiViewRecordHandler&) = delete;
  PtiViewRecordHandler& operator=(const PtiViewRecordHandler&) = delete;
  PtiViewRecordHandler(PtiViewRecordHandler&&) = delete;
  PtiViewRecordHandler& operator=(PtiViewRecordHandler&&) = delete;

  virtual ~PtiViewRecordHandler() {
    overhead::overhead_collection_enabled = false;
    DisableTracing();
    collector_->DisableTracing();
    delete collector_;
    stop_consumer_thread_ = true;
    buffer_queue_.ResetBufferDepth();
    buffer_queue_.Push(ViewBuffer{});  // Stop consumer
    if (buffer_consumer_.joinable()) {
      buffer_consumer_.join();
    }
  }

  void BufferConsumer() {
    while (!stop_consumer_thread_) {
      auto consume_buffer = buffer_queue_.Pop();
      if (!consume_buffer.IsNull()) {
        DeliverBuffer(std::move(consume_buffer));
      }
    }
  }

  inline pti_result FlushBuffers() {
    view_buffers_.ForEach(
        [this](const auto&, auto&& buffer) { buffer_queue_.Push(std::move(buffer)); });

    buffer_queue_.WaitUntilEmptyOr(stop_consumer_thread_);

    return PTI_SUCCESS;
  }

  template <typename T>
  inline void InsertRecord(const T& view_record) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "One can only insert trivially copyable types into the "
                  "ViewBuffer (view records)");
    auto& buffer = view_buffers_[std::this_thread::get_id()];

    if (buffer.IsNull()) {
      RequestNewBuffer(buffer);
    }

    buffer.Insert(view_record);
    static_assert(SizeOfLargestViewRecord() != 0, "Largest record not avaiable on compile time");
    if (buffer.FreeBytes() >= SizeOfLargestViewRecord()) {
      // There's space to insert more records. No need for swap.
      return;
    }

    buffer_queue_.Push(std::move(buffer));
  }

  inline pti_result RegisterBufferCallbacks(AskForBufferEvent&& get_new_buf,
                                            ReturnBufferEvent&& return_new_buf) {
    pti_result result = pti_result::PTI_ERROR_BAD_ARGUMENT;
    auto get_new_buffer = std::move(get_new_buf);
    auto deliver_buffer = std::move(return_new_buf);
    if (!get_new_buffer || !deliver_buffer) {
      // Keep using default callbacks
      return result;
    }

    unsigned char* raw_buffer = nullptr;
    std::size_t raw_buffer_size = 0;
    get_new_buffer(&raw_buffer, &raw_buffer_size);
    if (raw_buffer_size < SizeOfLargestViewRecord() || !raw_buffer) {
      // Keep using default callbacks
      result = pti_result::PTI_ERROR_BAD_ARGUMENT;
      deliver_buffer(raw_buffer, raw_buffer_size, 0);
    } else {
      // User callback is fine, keep memory they gave us
      result = pti_result::PTI_SUCCESS;
    }

    if (result == pti_result::PTI_SUCCESS) {
      // Use user-defined callbacks
      {
        std::lock_guard<std::mutex> cb_lock(get_new_buffer_mtx_);
        get_new_buffer_ = std::move(get_new_buffer);
      }
      {
        std::lock_guard<std::mutex> cb_lock(deliver_buffer_mtx_);
        deliver_buffer_ = std::move(deliver_buffer);
      }
    } else {
      get_new_buffer_(&raw_buffer, &raw_buffer_size);
    }
    auto* buffer_to_replace = view_buffers_.TryFindElement(std::this_thread::get_id());

    if (buffer_to_replace) {
      DeliverBuffer(std::move(*buffer_to_replace));
    }

    view_buffers_[std::this_thread::get_id()].Refresh(raw_buffer, raw_buffer_size);
    callbacks_set_ = true;

    return result;
  }

  inline pti_result Enable(pti_view_kind type) {
    if (!callbacks_set_) return pti_result::PTI_ERROR_NO_CALLBACKS_SET;
    auto result = pti_result::PTI_SUCCESS;
    bool collection_enabled = collection_enabled_;

    //
    // TBD --- implement and remove the checks for below pti_view_kinds
    //
    if ((type == pti_view_kind::PTI_VIEW_DEVICE_CPU_KERNEL) ||
        (type == pti_view_kind::PTI_VIEW_LEVEL_ZERO_CALLS) ||
        (type == pti_view_kind::PTI_VIEW_OPENCL_CALLS)) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (type == pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD) {
      overhead::overhead_collection_enabled = true;
    }

    if (type == pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION) {
      external_collection_enabled = true;
    }

    if (type == pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS) {
#if defined(PTI_TRACE_SYCL)
      if (!view_event_map_.TryFindElement("SyclRuntimeEvent")) {
        SyclCollector::Instance().SetCallback(SyclRuntimeViewCallback);
        SyclCollector::Instance().EnableTracing();
        collection_enabled = true;
      }
#else
      SPDLOG_DEBUG(
          "Sycl tracing activated, but the library has not been compiled with "
          "-DPTI_TRACE_SYCL");
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
#endif
    }

    if (collector_) {
      collector_->EnableTracer();
      collection_enabled = true;
    }

    collection_enabled_ = collection_enabled;

    if (!collection_enabled_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    try {
      if (type != pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION) {
        for (const auto& view_types : GetViewNameAndCallback(type)) {
          view_event_map_[view_types.fn_name] = view_types.callback;
        }
      }
    } catch (const std::out_of_range&) {
      result = pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    return result;
  }

  inline pti_result Disable(pti_view_kind type) {
    pti_result result = pti_result::PTI_SUCCESS;
    if (type == pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD) {
      overhead::overhead_collection_enabled = false;
    }
    if (type == pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION) {
      external_collection_enabled = false;
    }
    if (type == pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS) {
#if defined(PTI_TRACE_SYCL)
      SyclCollector::Instance().DisableTracing();
#endif
    }
    if (type == pti_view_kind::PTI_VIEW_INVALID) {
      return pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    try {
      for (const auto& view_types : GetViewNameAndCallback(type)) {
        view_event_map_.Erase(view_types.fn_name);
      }
    } catch (const std::out_of_range&) {
      result = pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    if (view_event_map_.Empty()) {
      DisableTracing();
    }
    return result;
  }

  inline pti_result PushExternalKindId(pti_view_external_kind external_kind, uint64_t external_id) {
    pti_result result = pti_result::PTI_SUCCESS;

    pti_view_record_external_correlation ext_corr_rec = pti_view_record_external_correlation();
    ext_corr_rec._external_id = external_id;
    ext_corr_rec._external_kind = external_kind;
    auto it = map_ext_corrid_vectors.find({external_kind});
    if (it != map_ext_corrid_vectors.cend()) {
      it->second.push(ext_corr_rec);
    } else {
      map_ext_corrid_vectors[{external_kind}].push(ext_corr_rec);
    }

    return result;
  }

  inline pti_result PopExternalKindId(pti_view_external_kind external_kind,
                                      uint64_t* p_external_id) {
    pti_result result = pti_result::PTI_SUCCESS;
    auto it = map_ext_corrid_vectors.find({external_kind});
    if (it != map_ext_corrid_vectors.cend()) {
      pti_view_record_external_correlation ext_record = it->second.top();
      if (p_external_id != nullptr) *p_external_id = ext_record._external_id;
      it->second.pop();
      if (!it->second.size()) {
        map_ext_corrid_vectors.erase(it);
      }
    } else {
      result = pti_result::PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY;
    }
    return result;
  }

  inline void operator()(const std::string& key, void* data,
                         const ZeKernelCommandExecutionRecord& rec) {
    auto* view_event_callback = view_event_map_.TryFindElement(key);
    if (view_event_callback) {
      (*view_event_callback)(data, rec);
    }
  }

  inline const char* InsertKernel(const std::string& name) {
    auto kernel_name = std::make_unique<std::string>(name);
    const auto* kernel_name_str = kernel_name->c_str();
    kernel_name_storage_.Push(std::move(kernel_name));
    return kernel_name_str;
  }

 private:
  inline void RequestNewBuffer(pti::view::utilities::ViewBuffer& buffer) {
    unsigned char* raw_buffer = nullptr;
    std::size_t buffer_size = 0;
    {
      std::lock_guard<std::mutex> cb_lock(get_new_buffer_mtx_);
      get_new_buffer_(&raw_buffer, &buffer_size);
    }
    buffer.Refresh(raw_buffer, buffer_size);
  }

  inline void DeliverBuffer(pti::view::utilities::ViewBuffer&& buffer) {
    auto buffer_to_deliver = std::move(buffer);
    {
      std::lock_guard<std::mutex> cb_lock(deliver_buffer_mtx_);
      if (buffer_to_deliver.GetBuffer()) {
        deliver_buffer_(buffer_to_deliver.GetBuffer(), buffer_to_deliver.GetBufferSize(),
                        buffer_to_deliver.GetValidBytes());
      }
    }
  }

  inline void DisableTracing() {
#if defined(PTI_TRACE_SYCL)
    SyclCollector::Instance().DisableTracing();
#endif
    if (collector_) {
      collector_->DisableTracer();
    }
    collection_enabled_ = false;
  }
  ZeCollector* collector_ = nullptr;
  std::atomic<bool> flush_operation_ = false;
  std::atomic<bool> stop_consumer_thread_ = false;
  std::atomic<bool> collection_enabled_ = false;
  std::atomic<bool> callbacks_set_ = false;
  ViewBufferQueue buffer_queue_ = ViewBufferQueue{kDefaultBufferQueueDepth};
  AskForBufferEvent get_new_buffer_;
  ReturnBufferEvent deliver_buffer_;
  mutable std::mutex get_new_buffer_mtx_;
  mutable std::mutex deliver_buffer_mtx_;
  ViewEventTable view_event_map_;
  KernelNameStorageQueue kernel_name_storage_;
  ViewBufferTable view_buffers_;
  std::thread buffer_consumer_;
};

// Required to access buffer from ze_collector callbacks
inline static auto& Instance() {
  static PtiViewRecordHandler data_container;
  return data_container;
}

inline pti_result GetNextRecord(uint8_t* buffer, size_t valid_bytes,
                                pti_view_record_base** record) {
  if (!record) {
    return pti_result::PTI_ERROR_BAD_ARGUMENT;
  }

  pti::view::utilities::ViewBuffer view_buffer(buffer, valid_bytes, valid_bytes);

  if (view_buffer.IsNull() || !view_buffer.GetValidBytes()) {
    return pti_result::PTI_STATUS_END_OF_BUFFER;
  }

  auto* current_record = *record;

  // User passed a nullptr for the record. Give them the first record.
  if (!current_record) {
    *record = view_buffer.Peek<pti_view_record_base>();
    return pti_result::PTI_SUCCESS;
  }

  auto next_element_loc = GetViewSize(current_record->_view_kind);

  // Found invalid record
  if (next_element_loc == SIZE_MAX) {
    return pti_result::PTI_ERROR_BAD_ARGUMENT;
  }

  auto* next_element_ptr = view_buffer.Peek(*record, next_element_loc);

  if (!next_element_ptr) {
    return pti_result::PTI_STATUS_END_OF_BUFFER;
  }

  *record = next_element_ptr;

  return pti_result::PTI_SUCCESS;
}

//
// TODO -- can we save on the string compare cost by optimizing at the source in
// L0 collector?
//
inline void SetMemFillType(pti_view_record_memory_fill& mem_record,
                           const ZeKernelCommandExecutionRecord& rec) {
  std::size_t found_pos = rec.name_.find_last_of("(");
  std::string tmp_str = rec.name_.substr(found_pos);
  if (tmp_str == "(M)") {
    mem_record._mem_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    return;
  }
  if (tmp_str == "(H)") {
    mem_record._mem_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    return;
  }
  if (tmp_str == "(D)") {
    mem_record._mem_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    return;
  }
  if (tmp_str == "(S)") {
    mem_record._mem_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    return;
  }
}

//
// TODO -- can we save on the string compare cost by optimizing at the source in
// L0 collector?
//
inline void SetMemCopyType(pti_view_record_memory_copy& mem_record,
                           const ZeKernelCommandExecutionRecord& rec) {
  std::size_t found_pos = rec.name_.find_last_of("(");
  std::string tmp_str = rec.name_.substr(found_pos, 4);
  tmp_str.push_back(')');
  if (tmp_str == "(M2M)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2M;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    return;
  }
  if (tmp_str == "(M2H)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2H;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    return;
  }
  if (tmp_str == "(M2D)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2D;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    return;
  }
  if (tmp_str == "(M2S)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2S;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    return;
  }
  if (tmp_str == "(H2M)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_H2M;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    return;
  }
  if (tmp_str == "(H2H)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_H2H;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    return;
  }
  if (tmp_str == "(H2D)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_H2D;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    return;
  }
  if (tmp_str == "(H2S)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_H2S;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    return;
  }
  if (tmp_str == "(D2M)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2M;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    return;
  }
  if (tmp_str == "(D2H)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2H;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    return;
  }
  if (tmp_str == "(D2D)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2D;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    return;
  }
  if (tmp_str == "(D2S)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2S;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    return;
  }
  if (tmp_str == "(S2M)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_S2M;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
    return;
  }
  if (tmp_str == "(S2H)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_S2H;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
    return;
  }
  if (tmp_str == "(S2D)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_S2D;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
    return;
  }
  if (tmp_str == "(S2S)") {
    mem_record._memcpy_type = pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_S2S;
    mem_record._mem_src = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    mem_record._mem_dst = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
    return;
  }
}

inline void GetDeviceId(char* buf, const ZeKernelCommandExecutionRecord& rec) {
  // determined by pti_view_record_kernel _pci_address
  constexpr auto kMaxDeviceIdLength = 16;
  std::snprintf(buf, kMaxDeviceIdLength, "%x:%x:%x.%x", rec.pci_prop_.address.domain,
                rec.pci_prop_.address.bus, rec.pci_prop_.address.device,
                rec.pci_prop_.address.function);
}

inline void GenerateExternalCorrelationRecords(const ZeKernelCommandExecutionRecord& rec) {
  for (auto it = map_ext_corrid_vectors.cbegin(); it != map_ext_corrid_vectors.cend(); it++) {
    // for ( pti_view_record_external_correlation ext_record : it->second ) {
    pti_view_record_external_correlation ext_record = it->second.top();
    ext_record._correlation_id = rec.cid_;
    ext_record._view_kind._view_kind = pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION;
    Instance().InsertRecord(ext_record);
    //}
  }
}

inline uint64_t ApplyTimeShift(uint64_t timestamp, int64_t time_shift) {
  uint64_t out_ts = 0;
  if (time_shift < 0) {
    PTI_ASSERT(timestamp >= static_cast<uint64_t>(-time_shift));  // underflow?
    out_ts = timestamp - static_cast<uint64_t>(-time_shift);
  } else {
    PTI_ASSERT((UINT64_MAX - timestamp) >= static_cast<uint64_t>(time_shift));  // overflow?
    out_ts = timestamp + static_cast<uint64_t>(time_shift);
  }
  return out_ts;
}

inline void MemCopyEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_memory_copy record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY;

  int64_t ts_shift = utils::ConvertionFactorMonotonicRawToReal();

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._device_handle = rec.device_;
  record._context_handle = rec.context_;
  record._bytes_copied = rec.bytes_xfered_;

  GetDeviceId(record._pci_address, rec);
  SetMemCopyType(record, rec);

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._mem_op_id = rec.cid_;
  record._correlation_id = rec.cid_;
  Instance().InsertRecord(record);
}

inline void MemFillEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_memory_fill record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL;

  int64_t ts_shift = utils::ConvertionFactorMonotonicRawToReal();

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._device_handle = rec.device_;
  record._context_handle = rec.context_;
  record._bytes = rec.bytes_xfered_;
  record._value_for_set = rec.value_set_;

  GetDeviceId(record._pci_address, rec);
  SetMemFillType(record, rec);

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._mem_op_id = rec.cid_;
  record._correlation_id = rec.cid_;
  Instance().InsertRecord(record);
}

inline void OverheadCollectionEvent(void* data, const ZeKernelCommandExecutionRecord& /*rec*/) {
  int64_t ts_shift = utils::ConvertionFactorMonotonicRawToReal();
  pti_view_record_overhead* ohRec = reinterpret_cast<pti_view_record_overhead*>(data);
  ohRec->_overhead_start_timestamp_ns =
      ApplyTimeShift(ohRec->_overhead_start_timestamp_ns, ts_shift);
  ohRec->_overhead_end_timestamp_ns = ApplyTimeShift(ohRec->_overhead_end_timestamp_ns, ts_shift);
  // ohRec->_overhead_api_name =
  // Instance().InsertKernel(ohRec->_overhead_api_name);
  Instance().InsertRecord(*ohRec);
}

inline void SyclRuntimeEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_sycl_runtime record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS;

  int64_t ts_shift = utils::ConvertionFactorMonotonicRawToReal();

  if (external_collection_enabled) {
    GenerateExternalCorrelationRecords(rec);
  }

  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._thread_id = rec.tid_;
  record._process_id = rec.pid_;
  record._correlation_id = rec.cid_;
  record._name = rec.sycl_func_name_;
  Instance().InsertRecord(record);
}

inline void KernelEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_kernel record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL;

  int64_t ts_shift = utils::ConvertionFactorMonotonicRawToReal();

  if (external_collection_enabled) {
    GenerateExternalCorrelationRecords(rec);
  }

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._device_handle = rec.device_;
  record._context_handle = rec.context_;

  GetDeviceId(record._pci_address, rec);

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._kernel_id = rec.kid_;
  record._correlation_id = rec.cid_;
  record._source_file_name = Instance().InsertKernel(rec.source_file_name_);
  record._source_line_number =
      rec.source_line_number_ != UINT32_MAX ? rec.source_line_number_ : 0ULL;
  record._sycl_node_id = rec.sycl_node_id_;
  record._sycl_invocation_id = rec.sycl_invocation_id_;
  record._sycl_enqk_begin_timestamp = ApplyTimeShift(rec.sycl_enqk_begin_time_, ts_shift);
  record._sycl_task_begin_timestamp = ApplyTimeShift(rec.sycl_task_begin_time_, ts_shift);

  Instance().InsertRecord(record);
}

inline void SyclRuntimeViewCallback(void* data, ZeKernelCommandExecutionRecord& rec) {
  Instance()("SyclRuntimeEvent", data, rec);
}

inline void OverheadCollectionCallback(void* data, ZeKernelCommandExecutionRecord& rec) {
  Instance()("OverheadCollectionEvent", data, rec);
}

inline void ZeChromeKernelStagesCallback(void* data,
                                         std::vector<ZeKernelCommandExecutionRecord>& kcexecrec) {
  for (const auto& rec : kcexecrec) {
    if (rec.name_.find("zeCommandListAppendMemoryCopy") != std::string::npos) {
      Instance()("zeCommandListAppendMemoryCopy", data, rec);
    } else if (rec.name_.find("zeCommandListAppendMemoryFill") != std::string::npos) {
      Instance()("zeCommandListAppendMemoryFill", data, rec);
    } else if (rec.name_.find("zeCommandListAppendBarrier") != std::string::npos) {
      // no-op for now
    } else {
      Instance()("KernelEvent", data, rec);
    }
  }
}

#endif  // SRC_API_VIEW_HANDLER_H_
