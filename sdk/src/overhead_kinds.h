//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_OVERHEAD_H_
#define PTI_TOOLS_OVERHEAD_H_

/*
 * Overhead collection methods:  These are called from collectors when they make
 * runtime api calls in order to estimate cost of making the api calls.   The
 * overhead captured is trickled into the buffer stream via buffer callback -
 * ocallback.
 */
#include <atomic>
#include <type_traits>

#include "pti/pti_driver_levelzero_api_ids.h"
#include "unikernel.h"
#include "utils.h"

namespace overhead {

enum class OverheadRuntimeType {
  kSycl = 0,
  kL0,
};

typedef void (*OnZeOverheadFinishCallback)(void* data, ZeKernelCommandExecutionRecord& kcexec);

#ifdef PTI_OVERHEAD_TRACKING_ENABLED
extern std::atomic<bool> overhead_collection_enabled;
extern OnZeOverheadFinishCallback
    ocallback_;  // Overhead callback registered for any overhead records that need
                 // to be captured and sent to buffer.

#else
inline std::atomic<bool> overhead_collection_enabled{false};
inline OnZeOverheadFinishCallback ocallback_{nullptr};
#endif

inline thread_local uint64_t init_ref_count = 0;

inline static void SetOverheadCallback(OnZeOverheadFinishCallback callback) {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  ocallback_ = callback;
#else
  (void)callback;
#endif
}

//
// TODO -- change the init to take in a param of type ovehead_kind and use that
// -- instead of assuming KIND_TIME only.
//
// Mark or up ref count for this function to include in overhead time
//   Overhead is captured for any runtime calls made that have a chance to
//   affect device times.
//

inline void Init() {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  if (!overhead_collection_enabled) {
    return;
  }
  if (map_overhead_per_kind.empty()) {
    pti_view_record_overhead overhead_rec = pti_view_record_overhead();
    overhead_rec._overhead_kind = pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME;
    overhead_rec._view_kind._view_kind = pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD;
    overhead_rec._overhead_start_timestamp_ns = 0;
    overhead_rec._overhead_end_timestamp_ns = 0;
    overhead_rec._overhead_count = 0;
    map_overhead_per_kind[{pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME}] = overhead_rec;
  }

  uint64_t tid = PidTidInfo::Get().tid;
  uint64_t start_time_ns = utils::GetTime();
  auto overhead_it =
      map_overhead_per_kind.find({pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME});
  init_ref_count++;
  if (overhead_it != map_overhead_per_kind.cend()) {
    if (overhead_it->second._overhead_start_timestamp_ns != 0) {
      return;
    }
    overhead_it->second._overhead_start_timestamp_ns = start_time_ns;
  } else {
    pti_view_record_overhead overhead_rec = pti_view_record_overhead();
    overhead_rec._view_kind._view_kind = pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD;
    overhead_rec._overhead_kind = pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME;
    overhead_rec._overhead_start_timestamp_ns = start_time_ns;
    overhead_rec._overhead_thread_id = tid;
    map_overhead_per_kind[{pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME}] = overhead_rec;
  }
#endif
}

inline void ResetRecord() {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  auto overhead_it =
      map_overhead_per_kind.find({pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME});
  if (overhead_it != map_overhead_per_kind.cend()) {
    overhead_it->second._overhead_duration_ns = 0;
    overhead_it->second._overhead_start_timestamp_ns = 0;
    overhead_it->second._overhead_end_timestamp_ns = 0;
    overhead_it->second._overhead_count = 0;
    PTI_ASSERT(init_ref_count == 0);
  }
#endif
}

inline void FiniLevel0(OverheadRuntimeType runtime_type,
                       [[maybe_unused]] pti_api_id_driver_levelzero api_id) {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  if (!overhead_collection_enabled) {
    return;
  }

  // Init not called, nothing to do.
  if (init_ref_count == 0) {
    return;
  }

  if (init_ref_count > 1) {  // we are not done if there is more than 1 ref
    init_ref_count--;        // count for this object per thread basis.
    return;
  }

  init_ref_count--;

  uint64_t end_time_ns = utils::GetTime();
  auto overhead_it =
      map_overhead_per_kind.find({pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME});

  if (overhead_it != map_overhead_per_kind.cend()) {
    // Note: it could be surprising that duration is zero - but this can happen,
    // especially on Windows where PTI at the moment uses QueryPerformanceCounter
    // which has a coarse granularity of ~ 100ns.
    uint64_t duration = end_time_ns - overhead_it->second._overhead_start_timestamp_ns;
    overhead_it->second._overhead_duration_ns += duration;
    if (runtime_type == OverheadRuntimeType::kSycl) {
      overhead_it->second._overhead_count += 1;
    }
    if (runtime_type == OverheadRuntimeType::kL0) {
      overhead_it->second._overhead_count += 1;
    }
    overhead_it->second._overhead_end_timestamp_ns = end_time_ns;
    overhead_it->second._overhead_thread_id = PidTidInfo::Get().tid;
    overhead_it->second._api_id = api_id;  // Turn this
    // back on if we need to propagate api_name to user.
    if ((runtime_type == OverheadRuntimeType::kL0) && (ocallback_ != nullptr)) {
      ocallback_(&overhead_it->second, overhead_data);
    }
    ResetRecord();
  }
#else
  (void)runtime_type;
  (void)api_id;
#endif
}

inline void FiniSycl(OverheadRuntimeType runtime_type) {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  // Init not called, nothing to do.
  if (init_ref_count == 0) {
    return;
  }

  if (init_ref_count > 1) {
    init_ref_count--;
    return;
  }
  init_ref_count--;

  uint64_t end_time_ns = utils::GetTime();
  auto overhead_it =
      map_overhead_per_kind.find({pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME});
  if (overhead_it != map_overhead_per_kind.cend()) {
    uint64_t duration = end_time_ns - overhead_it->second._overhead_start_timestamp_ns;
    overhead_it->second._overhead_duration_ns += duration;
    if (runtime_type == OverheadRuntimeType::kSycl) {
      overhead_it->second._overhead_count += 1;
    }
    if (runtime_type == OverheadRuntimeType::kL0) {
      overhead_it->second._overhead_count += 1;
    }
    overhead_it->second._overhead_end_timestamp_ns = end_time_ns;
    overhead_it->second._overhead_thread_id = utils::GetTid();
    if ((runtime_type == OverheadRuntimeType::kSycl) && (ocallback_ != nullptr)) {
      ocallback_(&overhead_it->second, overhead_data);
    }
    ResetRecord();
  }
#else
  (void)runtime_type;
#endif
}

template <typename E>
class ScopedOverheadCollector {
 public:
  constexpr explicit ScopedOverheadCollector(E api_id) : identifier_(api_id) {
    static_assert(std::is_enum_v<E>, "Must be overhead enum type");
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
    Init();
#endif
  }

  ScopedOverheadCollector(const ScopedOverheadCollector&) = delete;
  ScopedOverheadCollector& operator=(const ScopedOverheadCollector&) = delete;
  ScopedOverheadCollector(ScopedOverheadCollector&&) = delete;
  ScopedOverheadCollector& operator=(ScopedOverheadCollector&&) = delete;

  ~ScopedOverheadCollector() noexcept {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
    if constexpr (std::is_same_v<E, pti_api_id_driver_levelzero>) {
      FiniLevel0(OverheadRuntimeType::kL0, identifier_);
    }
    // Remove and add SYCL/other runtimes here as needed.
    static_assert(std::is_same_v<E, pti_api_id_driver_levelzero>, "Unsupported overhead enum type");
#endif
  }

 private:
  E identifier_;
};

}  // namespace overhead

static inline void overhead_fini(pti_api_id_driver_levelzero api_id) {
#ifdef PTI_OVERHEAD_TRACKING_ENABLED
  overhead::FiniLevel0(overhead::OverheadRuntimeType::kL0, api_id);
#else
  (void)api_id;
#endif
}

#endif  // PTI_TOOLS_OVERHEAD_H_
