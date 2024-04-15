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

#include "unikernel.h"
#include "utils.h"

namespace overhead {

// TODO: redo this approach to enable/disable state tracking.
inline static std::atomic<bool> overhead_collection_enabled = false;

inline constexpr auto kOhThreshold =
    1.00;  // 1ns threshhold by default -- TODO -- make this setAttributable

enum class OverheadRuntimeType {
  OVERHEAD_RUNTIME_TYPE_SYCL = 0,
  OVERHEAD_RUNTIME_TYPE_L0,
};

typedef void (*OnZeOverheadFinishCallback)(void* data, ZeKernelCommandExecutionRecord& kcexec);
inline OnZeOverheadFinishCallback ocallback_ =
    nullptr;  // Overhead callback registered for any overhead records that need
              // to be captured and sent to buffer.

inline thread_local uint64_t init_ref_count = 0;
inline static void SetOverheadCallback(OnZeOverheadFinishCallback callback) {
  ocallback_ = callback;
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

  uint64_t tid = utils::GetTid();
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
}

inline void ResetRecord() {
  auto overhead_it =
      map_overhead_per_kind.find({pti_view_overhead_kind::PTI_VIEW_OVERHEAD_KIND_TIME});
  if (overhead_it != map_overhead_per_kind.cend()) {
    overhead_it->second._overhead_duration_ns = 0;
    overhead_it->second._overhead_start_timestamp_ns = 0;
    overhead_it->second._overhead_end_timestamp_ns = 0;
    overhead_it->second._overhead_count = 0;
    PTI_ASSERT(init_ref_count == 0);
  }
}

inline void FiniLevel0(OverheadRuntimeType runtime_type,
                       [[maybe_unused]] const char* api_func_name) {
  if (!overhead_collection_enabled) {
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
    uint64_t duration = end_time_ns - overhead_it->second._overhead_start_timestamp_ns;
    overhead_it->second._overhead_duration_ns += duration;
    if (runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_SYCL) {
      overhead_it->second._overhead_count += 1;
    }
    if (runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0) {
      overhead_it->second._overhead_count += 1;
    }
    if ((overhead_it->second._overhead_duration_ns / kOhThreshold) > 1) {
      overhead_it->second._overhead_end_timestamp_ns = end_time_ns;
      overhead_it->second._overhead_thread_id = utils::GetTid();
      // overhead_it->second._overhead_api_name = api_func_name; // Turn this
      // back on if we need to propagate api_name to user.
      if ((runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0) &&
          (ocallback_ != nullptr)) {
        ocallback_(&overhead_it->second, overhead_data);
      }
      ResetRecord();
    }
  }
}

inline void FiniSycl(OverheadRuntimeType runtime_type) {
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
    if (runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_SYCL) {
      overhead_it->second._overhead_count += 1;
    }
    if (runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0) {
      overhead_it->second._overhead_count += 1;
    }
    if ((overhead_it->second._overhead_duration_ns / kOhThreshold) > 1) {
      overhead_it->second._overhead_end_timestamp_ns = end_time_ns;
      overhead_it->second._overhead_thread_id = utils::GetTid();
      if ((runtime_type == OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_SYCL) &&
          (ocallback_ != nullptr)) {
        ocallback_(&overhead_it->second, overhead_data);
      }
      ResetRecord();
    }
  }
}

}  // namespace overhead

static inline void overhead_fini(std::string o_api_string) {
  // std::string o_api_string = l0_api;
  overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                       o_api_string.c_str());
}

#endif  // PTI_TOOLS_OVERHEAD_H_
