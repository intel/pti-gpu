//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_VIEW_HANDLER_H_
#define SRC_API_VIEW_HANDLER_H_

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "consumer_thread.h"
#include "default_buffer_callbacks.h"
#include "pti/pti_view.h"

#if defined(PTI_TRACE_SYCL)
#include "sycl_collector.h"
#endif

#include "overhead_kinds.h"
#include "unikernel.h"
#include "utils.h"
#include "view_buffer.h"
#include "view_record_info.h"
#include "ze_collector.h"

using AskForBufferEvent = std::function<void(unsigned char**, size_t*)>;
using ReturnBufferEvent = std::function<void(unsigned char*, size_t, size_t)>;

using ViewInsert = std::function<void(void*, const ZeKernelCommandExecutionRecord&)>;

inline void MemCopyEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

inline void MemCopyP2PEvent(void* data, const ZeKernelCommandExecutionRecord& rec);

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
            ViewData{"zeCommandListAppendMemoryCopy", MemCopyEvent}
          }
        },
        {PTI_VIEW_DEVICE_GPU_MEM_FILL,
          {
            ViewData{"zeCommandListAppendMemoryFill", MemFillEvent}
          }
        },
        {PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P,
          {
            ViewData{"zeCommandListAppendMemoryCopyP2P", MemCopyP2PEvent},
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

inline static std::atomic<bool> external_collection_enabled = false;

struct PtiViewRecordHandler {
 public:
  using ViewBuffer = pti::view::utilities::ViewBuffer;
  using ViewBufferQueue = pti::view::utilities::ViewBufferQueue;
  using ViewBufferTable = pti::view::utilities::ViewBufferTable<uint32_t>;
  using ViewEventTable = pti::view::utilities::GuardedUnorderedMap<std::string, ViewInsert>;
  using KernelNameStorageQueue =
      pti::view::utilities::ViewRecordBufferQueue<std::unique_ptr<std::string>>;

  PtiViewRecordHandler()
      : get_new_buffer_(pti::view::defaults::DefaultBufferAllocation),
        deliver_buffer_(pti::view::defaults::DefaultRecordParser),
        user_provided_ts_func_ptr_(utils::GetRealTime) {
    // initially set logging level to warn
    // need to use warnings very carefully, only when absolutely necessary
    // as on Windows encountered it is INFO (taken from compiler define) by default (?)
    spdlog::set_level(spdlog::level::warn);
    // Read Logging level required
    // set environment variable PTILOG_LEVEL=<level>, where level=TRACE/DEBUG/INFO..
    // Logs appear only when PTI_ENABLE_LOGGING=ON => SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    std::string env_string = utils::GetEnv("PTILOG_LEVEL");
    if (!env_string.empty()) {
      spdlog::cfg::helpers::load_levels(env_string);
    }
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    spdlog::set_pattern("[%H:%M][%^-%l-%$]%P:%t %s:%# %v");

    if (!collector_) {
      CollectorOptions collector_options{};
      collector_options.kernel_tracing = true;
      collector_ =
          ZeCollector::Create(&state_, collector_options, ZeChromeKernelStagesCallback, nullptr);
      overhead::SetOverheadCallback(OverheadCollectionCallback);
      // Get timevalue in nanoseconds for frequency of sync between clock sources
      // (clock_monotonic_raw and by default clock_realtime)
      //   Default is 1millisecond --- we allow any value closely bounded by 1second to
      //   1microsecond.
      std::string env_string = utils::GetEnv("PTI_CONV_CLOCK_SYNC_TIME_NS");
      if (!env_string.empty()) {
        try {
          int64_t env_value = std::stoi(env_string);
          if (env_value >= NSEC_IN_USEC &&
              env_value <= NSEC_IN_SEC)     // are we within 1micro to 1sec bounds?
            sync_clocks_every = env_value;  // reset it.

        } catch (std::invalid_argument const& /*ex*/) {
          sync_clocks_every = kDefaultSyncTime;  // default conversion sync time -- 1 ms.
        } catch (std::out_of_range const& /*ex*/) {
          sync_clocks_every = kDefaultSyncTime;  // default conversion sync time -- 1 ms.
        }
      }
      timestamp_of_last_ts_shift_ = utils::GetTime();  // CLOCK_MONOTONIC_RAW or equivalent
      SPDLOG_INFO("\tClock Sync time (ns) set at: {}", sync_clocks_every);
      ts_shift_ = utils::ConversionFactorMonotonicRawToUnknownClock(user_provided_ts_func_ptr_);
    }
  }

  PtiViewRecordHandler(const PtiViewRecordHandler&) = delete;
  PtiViewRecordHandler& operator=(const PtiViewRecordHandler&) = delete;
  PtiViewRecordHandler(PtiViewRecordHandler&&) = delete;
  PtiViewRecordHandler& operator=(PtiViewRecordHandler&&) = delete;

  virtual ~PtiViewRecordHandler() {
    try {
      overhead::overhead_collection_enabled = false;
      if (collector_) {
        collector_->DisableTracer();
      }
      DisableTracing();
    } catch ([[maybe_unused]] const std::exception& e) {
      SPDLOG_ERROR("Exception caught in {}: {}", __FUNCTION__, e.what());
    } catch (...) {
      SPDLOG_ERROR("Unknown Exception in {}", __FUNCTION__);
    }
  }

  inline pti_result FlushBuffers() {
    auto result = consumer_.Push([this]() mutable {
      view_buffers_.ForEach([this](const auto&, auto&& buffer) {
        if (!buffer.IsNull()) {
          DeliverBuffer(std::move(buffer));
        }
      });
    });

    result.wait();

    return PTI_SUCCESS;
  }

  template <typename T>
  inline void InsertRecord(const T& view_record, uint32_t thread_id) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "One can only insert trivially copyable types into the "
                  "ViewBuffer (view records)");
    const std::lock_guard<std::mutex> lock(insert_record_mtx_);
    auto& buffer = view_buffers_[thread_id];

    if (buffer.IsNull()) {
      RequestNewBuffer(buffer);
    }

    buffer.Insert(view_record);
    static_assert(SizeOfLargestViewRecord() != 0, "Largest record not avaiable on compile time");
    if (buffer.FreeBytes() >= SizeOfLargestViewRecord()) {
      // There's space to insert more records. No need for swap.
      return;
    }
    consumer_.PushAndForget([this, buffer = std::move(buffer)]() mutable {
      if (!buffer.IsNull()) {
        DeliverBuffer(std::move(buffer));
      }
    });
  }

  inline pti_result RegisterTimestampCallback(pti_fptr_get_timestamp get_timestamp) {
    if (!get_timestamp) return pti_result::PTI_ERROR_BAD_ARGUMENT;
    const std::lock_guard<std::mutex> lock(timestamp_api_mtx_);
    user_provided_ts_func_ptr_ = get_timestamp;
    timestamp_of_last_ts_shift_ = utils::GetTime();  // CLOCK_MONOTONIC_RAW or equivalent
    ts_shift_ = utils::ConversionFactorMonotonicRawToUnknownClock(user_provided_ts_func_ptr_);
    return pti_result::PTI_SUCCESS;
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

    uint32_t tid = utils::GetTid();
    auto buffer_to_replace = view_buffers_.TryTakeElement(tid);

    if (buffer_to_replace) {
      DeliverBuffer(std::move(*buffer_to_replace));
    }

    view_buffers_[tid].Refresh(raw_buffer, raw_buffer_size);
    callbacks_set_ = true;

    return result;
  }

  inline pti_result Enable(pti_view_kind type) {
    if (!callbacks_set_) {
      return pti_result::PTI_ERROR_NO_CALLBACKS_SET;
    }
    auto result = pti_result::PTI_SUCCESS;
    bool collection_enabled = collection_enabled_;
    bool l0_collection_type = ((type == pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P));

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
      collection_enabled = true;
      if (l0_collection_type) {
        auto it = map_view_kind_enabled.find(type);
        if (it == map_view_kind_enabled.cend() || map_view_kind_enabled[type] == false) {
          map_view_kind_enabled[type] = true;
          collector_->EnableTracing();
        }
      }
    }

    collection_enabled_ = collection_enabled;

    if (!collection_enabled_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    try {
      if (type != pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION) {
        for (const auto& view_types : GetViewNameAndCallback(type)) {
          view_event_map_.Add(view_types.fn_name, view_types.callback);
        }
      }
    } catch (const std::out_of_range&) {
      result = pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    return result;
  }

  inline pti_result Disable(pti_view_kind type) {
    pti_result result = pti_result::PTI_SUCCESS;
    bool l0_collection_type = ((type == pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY) ||
                               (type == pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P));

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
    if (collector_) {
      if (l0_collection_type) {
        auto it = map_view_kind_enabled.find(type);
        if (it != map_view_kind_enabled.cend() && map_view_kind_enabled[type] == true) {
          map_view_kind_enabled[type] = false;
          collector_->DisableTracing();
        }
      }
    }

    try {
      if (type != pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION) {
        for (const auto& view_types : GetViewNameAndCallback(type)) {
          view_event_map_.Erase(view_types.fn_name);
        }
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
    SPDLOG_TRACE("In {}, ext_id: {}, ext_kind: {}", __FUNCTION__, external_id,
                 static_cast<uint32_t>(external_kind));

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
      SPDLOG_TRACE("In {}, ext_id: {} ext_kind: {}", __FUNCTION__, ext_record._external_id,
                   static_cast<uint32_t>(external_kind));
      if (p_external_id != nullptr) {
        *p_external_id = ext_record._external_id;
      }
      it->second.pop();
      if (!it->second.size()) {
        map_ext_corrid_vectors.erase(it);
      }
    } else {
      SPDLOG_TRACE("In {}, External ID Queue is empty", __FUNCTION__);
      result = pti_result::PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY;
    }
    return result;
  }

  inline void operator()(const std::string& key, void* data,
                         const ZeKernelCommandExecutionRecord& rec) {
    auto view_event_callback = view_event_map_.TryFindElement(key);
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

  inline pti_result GetState() { return state_; }
  inline void SetState(pti_result new_state) { state_ = new_state; }

  inline pti_result GPULocalAvailable() {
    if (collector_) {
      if (collector_->IsIntrospectionCapable() && collector_->IsDynamicTracingCapable()) {
        return pti_result::PTI_SUCCESS;
      } else {
        return pti_result::PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED;
      }
    }
    return pti_result::PTI_ERROR_INTERNAL;
  }
  inline uint64_t GetUserTimestamp() { return (*user_provided_ts_func_ptr_.load())(); }

  inline int64_t GetTimeShift() {
    const std::lock_guard<std::mutex> lock(timestamp_api_mtx_);

    uint64_t now = utils::GetTime();  // CLOCK_MONOTONIC_RAW or equivalent
    if ((now - timestamp_of_last_ts_shift_) > sync_clocks_every) {
      timestamp_of_last_ts_shift_ = utils::GetTime();  // CLOCK_MONOTONIC_RAW or equivalent
      ts_shift_ = utils::ConversionFactorMonotonicRawToUnknownClock(user_provided_ts_func_ptr_);
    }
    return ts_shift_;
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
    // if (collector_) {
    // collector_->DisableTracer();
    //}
    collection_enabled_ = false;
  }
  std::unique_ptr<ZeCollector> collector_ = nullptr;
  std::atomic<bool> collection_enabled_ = false;
  // Internal PTI state.
  // If abnornal situation happens - this variable will be set the corresponding value
  std::atomic<pti_result> state_ = pti_result::PTI_SUCCESS;
  std::atomic<bool> callbacks_set_ = false;
  AskForBufferEvent get_new_buffer_;
  ReturnBufferEvent deliver_buffer_;
  mutable std::mutex get_new_buffer_mtx_;
  mutable std::mutex deliver_buffer_mtx_;
  mutable std::mutex timestamp_api_mtx_;
  mutable std::mutex insert_record_mtx_;  // protecting writing to buffers, as different threads
                                          // might be writing to the same buffer

  ViewEventTable view_event_map_;
  KernelNameStorageQueue kernel_name_storage_;
  ViewBufferTable view_buffers_;
  pti::view::BufferConsumer consumer_ = {};  // Starts thread
  std::atomic<pti_fptr_get_timestamp> user_provided_ts_func_ptr_ = nullptr;
  int64_t ts_shift_ = 0;  // conversion factor for switching from default clock to user provided
                          // one(defaults to monotonic raw)
  uint64_t timestamp_of_last_ts_shift_ = 0;  // every 1 second we recalculate time_shift_
  inline static constexpr auto kDefaultSyncTime = 1000000ULL;
  uint64_t sync_clocks_every =
      kDefaultSyncTime;  // time in nanoseconds, sync every millisecond by default --- this can be
                         // overridden by the env variable PTI_CONV_CLOCK_SYNC_TIME_NS.
};

// Required to access buffer from ze_collector callbacks
inline static auto& Instance() {
  static PtiViewRecordHandler data_container{};
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

inline void SetMemFillType(pti_view_record_memory_fill& mem_record,
                           const ZeKernelCommandExecutionRecord& rec) {
  SPDLOG_TRACE("In {}, memory route: {}", __FUNCTION__, rec.route_.StringifyTypesCompact());
  mem_record._mem_type = rec.route_.dst_type;
}

template <typename T>
inline void SetMemCopyType(T& mem_record, const ZeKernelCommandExecutionRecord& rec) {
  mem_record._memcpy_type = rec.route_.GetMemcpyType();
  mem_record._mem_src = rec.route_.src_type;
  mem_record._mem_dst = rec.route_.dst_type;
}

inline void GetDeviceId(char* buf, const ze_pci_ext_properties_t& pci_prop_) {
  // determined by pti_view_record_kernel _pci_address
  constexpr auto kMaxDeviceIdLength = PTI_MAX_PCI_ADDRESS_SIZE;
  std::snprintf(buf, kMaxDeviceIdLength, "%x:%x:%x.%x", pci_prop_.address.domain,
                pci_prop_.address.bus, pci_prop_.address.device, pci_prop_.address.function);
}

inline void GenerateExternalCorrelationRecords(const ZeKernelCommandExecutionRecord& rec) {
  for (auto it = map_ext_corrid_vectors.cbegin(); it != map_ext_corrid_vectors.cend(); it++) {
    pti_view_record_external_correlation ext_record = it->second.top();
    ext_record._correlation_id = rec.cid_;
    ext_record._view_kind._view_kind = pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION;
    SPDLOG_TRACE("In {}, ext_id: {}, ext_kind: {}, corr_id: {}", __FUNCTION__,
                 ext_record._external_id, static_cast<uint32_t>(ext_record._external_kind),
                 ext_record._correlation_id);
    Instance().InsertRecord(ext_record, rec.tid_);
  }
}

inline uint64_t ApplyTimeShift(uint64_t timestamp, int64_t time_shift) {
  uint64_t out_ts = 0;
  try {
    if (time_shift < 0) {
      if (timestamp < static_cast<uint64_t>(-time_shift)) {  // underflow?
        SPDLOG_WARN("Timestamp underflow detected when shifting domains: TS: {}, time_shift: {}",
                    timestamp, time_shift);
        throw std::out_of_range("Timestamp underflow detected");
      }
      out_ts = timestamp - static_cast<uint64_t>(-time_shift);
    } else {
      if ((UINT64_MAX - timestamp) < static_cast<uint64_t>(time_shift)) {  // overflow?
        SPDLOG_WARN("Timestamp overflow detected when shifting domains: TS: {}, time_shift: {}",
                    timestamp, time_shift);
        throw std::out_of_range("Timestamp overflow detected");
      };
      out_ts = timestamp + static_cast<uint64_t>(time_shift);
    }
  } catch (const std::out_of_range&) {
    Instance().SetState(pti_result::PTI_ERROR_BAD_TIMESTAMP);
  }
  return out_ts;
}

template <typename T>
inline void SetMemCpyIds(T& record, const ZeKernelCommandExecutionRecord& rec) {
  if (rec.device_ != nullptr) {
    GetDeviceId(record._pci_address, rec.pci_prop_);
    std::copy_n(rec.src_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._device_uuid);
    SetMemCopyType<T>(record, rec);
    return;
  } else if (rec.dst_device_ != nullptr)
    GetDeviceId(record._pci_address, rec.dst_pci_prop_);
  else
    memset(record._pci_address, 0, PTI_MAX_PCI_ADDRESS_SIZE);

  std::copy_n(rec.dst_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._device_uuid);
  SetMemCopyType<T>(record, rec);
}

template <typename T>
inline void SetMemCpyIdsP2P(T& record, const ZeKernelCommandExecutionRecord& rec) {
  if (rec.device_ != nullptr)
    GetDeviceId(record._src_pci_address, rec.pci_prop_);
  else
    memset(record._src_pci_address, 0, PTI_MAX_PCI_ADDRESS_SIZE);
  if (rec.dst_device_ != nullptr)
    GetDeviceId(record._dst_pci_address, rec.dst_pci_prop_);
  else
    memset(record._dst_pci_address, 0, PTI_MAX_PCI_ADDRESS_SIZE);

  std::copy_n(rec.src_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._src_uuid);
  std::copy_n(rec.dst_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._dst_uuid);
  SetMemCopyType<T>(record, rec);
}

template <typename T>
inline auto DoCommonMemCopy(bool p2p, const ZeKernelCommandExecutionRecord& rec) {
  T record;
  utils::Zeroize(record);

  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY;
  if (p2p) record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P;

  int64_t ts_shift = Instance().GetTimeShift();

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._sycl_queue_id = rec.sycl_queue_id_;
  record._context_handle = rec.context_;
  record._bytes = rec.bytes_xfered_;

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._mem_op_id = rec.cid_;
  record._correlation_id = rec.cid_;

  return record;
}

inline void MemCopyP2PEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  bool p2p = true;
  pti_view_record_memory_copy_p2p record =
      DoCommonMemCopy<pti_view_record_memory_copy_p2p>(p2p, rec);
  SetMemCpyIdsP2P(record, rec);
  Instance().InsertRecord(record, record._thread_id);
}

inline void MemCopyEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  bool p2p = false;
  pti_view_record_memory_copy record = DoCommonMemCopy<pti_view_record_memory_copy>(p2p, rec);
  SetMemCpyIds(record, rec);
  Instance().InsertRecord(record, record._thread_id);
}

inline void MemFillEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_memory_fill record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL;

  int64_t ts_shift = Instance().GetTimeShift();

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._context_handle = rec.context_;
  record._bytes = rec.bytes_xfered_;
  record._value_for_set = rec.value_set_;

  GetDeviceId(record._pci_address, rec.pci_prop_);
  std::copy_n(rec.src_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._device_uuid);
  SetMemFillType(record, rec);

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._mem_op_id = rec.cid_;
  record._correlation_id = rec.cid_;
  Instance().InsertRecord(record, record._thread_id);
}

inline void OverheadCollectionEvent(void* data, const ZeKernelCommandExecutionRecord& /*rec*/) {
  int64_t ts_shift = Instance().GetTimeShift();
  pti_view_record_overhead* ohRec = reinterpret_cast<pti_view_record_overhead*>(data);
  ohRec->_overhead_start_timestamp_ns =
      ApplyTimeShift(ohRec->_overhead_start_timestamp_ns, ts_shift);
  ohRec->_overhead_end_timestamp_ns = ApplyTimeShift(ohRec->_overhead_end_timestamp_ns, ts_shift);
  Instance().InsertRecord(*ohRec, ohRec->_overhead_thread_id);
}

inline void SyclRuntimeEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_sycl_runtime record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS;

  int64_t ts_shift = Instance().GetTimeShift();

  if (external_collection_enabled) {
    GenerateExternalCorrelationRecords(rec);
  }

  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._thread_id = rec.tid_;
  record._process_id = rec.pid_;
  record._correlation_id = rec.cid_;
  record._name = rec.sycl_func_name_;
  SPDLOG_TRACE("In {}, name: {}, corr_id: {}", __FUNCTION__, record._name, record._correlation_id);
  Instance().InsertRecord(record, record._thread_id);
}

inline void KernelEvent(void* /*data*/, const ZeKernelCommandExecutionRecord& rec) {
  pti_view_record_kernel record;
  record._view_kind._view_kind = pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL;

  // Note: no need to call  GenerateExternalCorrelationRecords(rec)
  // as there records go only with runtime API records and not with GPU kernels, memory ops..

  int64_t ts_shift = Instance().GetTimeShift();

  record._append_timestamp = ApplyTimeShift(rec.append_time_, ts_shift);
  record._start_timestamp = ApplyTimeShift(rec.start_time_, ts_shift);
  record._end_timestamp = ApplyTimeShift(rec.end_time_, ts_shift);
  record._submit_timestamp = ApplyTimeShift(rec.submit_time_, ts_shift);
  record._queue_handle = rec.queue_;
  record._context_handle = rec.context_;

  GetDeviceId(record._pci_address, rec.pci_prop_);
  std::copy_n(rec.src_device_uuid, PTI_MAX_DEVICE_UUID_SIZE, record._device_uuid);

  // We're storing it in a kernel map so this shouldn't go out of scope
  record._name = Instance().InsertKernel(rec.name_);
  record._thread_id = rec.tid_;
  record._kernel_id = rec.kid_;
  record._correlation_id = rec.cid_;
  record._source_file_name = Instance().InsertKernel(rec.source_file_name_);
  record._source_line_number =
      rec.source_line_number_ != UINT32_MAX ? rec.source_line_number_ : 0ULL;
  record._sycl_node_id = rec.sycl_node_id_;
  record._sycl_queue_id = rec.sycl_queue_id_;
  record._sycl_invocation_id = rec.sycl_invocation_id_;
  record._sycl_enqk_begin_timestamp = ApplyTimeShift(rec.sycl_enqk_begin_time_, ts_shift);
  record._sycl_task_begin_timestamp = ApplyTimeShift(rec.sycl_task_begin_time_, ts_shift);

  Instance().InsertRecord(record, record._thread_id);
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
    if ((rec.name_.find("P2P)") != std::string::npos) &&
        (rec.name_.find("zeCommandListAppendMemoryCopy") != std::string::npos)) {
      Instance()("zeCommandListAppendMemoryCopyP2P", data, rec);
    } else if (rec.name_.find("zeCommandListAppendMemoryCopy") != std::string::npos) {
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
