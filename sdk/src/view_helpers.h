//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_VIEW_HELPERS_H_
#define SRC_VIEW_HELPERS_H_

#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "pti/pti_view.h"
#include "view_record_info.h"

struct ViewState {
  std::atomic<pti_result> state_{pti_result::PTI_SUCCESS};
  void SetState(pti_result s) { state_.store(s); }
  pti_result GetState() const { return state_.load(); }
};

inline ViewState& ViewStateInstance() {
  static ViewState inst;
  return inst;
}

inline std::array<std::atomic<bool>, kSizeOfViewRecordTable> list_of_api_view_state{};

inline void SetApiViewState(pti_view_kind view_kind, bool enabled) {
  if (!IsPtiViewKindValid(view_kind)) {
    SPDLOG_WARN("Invalid or reserved view kind: {}", static_cast<uint32_t>(view_kind));
    return;
  }
  list_of_api_view_state[static_cast<uint32_t>(view_kind)].store(enabled,
                                                                 std::memory_order_release);
}

inline bool GetApiViewState(pti_view_kind view_kind) {
  if (!IsPtiViewKindValid(view_kind)) {
    SPDLOG_WARN("Invalid or reserved view kind: {}", static_cast<uint32_t>(view_kind));
    return false;
  }
  return list_of_api_view_state[static_cast<uint32_t>(view_kind)].load(std::memory_order_acquire);
}

inline bool IsAnyViewEnabled() {
  for (const auto& state : list_of_api_view_state) {
    if (state.load(std::memory_order_acquire)) {
      return true;
    }
  }
  return false;
}

inline uint64_t ApplyTimeShift(uint64_t timestamp, int64_t time_shift) {
  uint64_t out_ts = 0;
  if (timestamp == 0) {
    // this could happen if some collections disabled so not collected timestamps
    // (e.g. task_enqueue.. ) might be zeros
    SPDLOG_DEBUG("Timestamp is 0 when shifting time domains: TS: {}, time_shift: {}", timestamp,
                 time_shift);
    return 0;
  }
  try {
    if (time_shift < 0) {
      if (timestamp < static_cast<uint64_t>(-time_shift)) {  // underflow?
        SPDLOG_WARN(
            "Timestamp underflow detected when shifting time domains: TS: {}, "
            "time_shift: {}",
            timestamp, time_shift);
        throw std::out_of_range("Timestamp underflow detected");
      }
      out_ts = timestamp - static_cast<uint64_t>(-time_shift);
    } else {
      if ((UINT64_MAX - timestamp) < static_cast<uint64_t>(time_shift)) {  // overflow?
        SPDLOG_WARN(
            "Timestamp overflow detected when shifting time domains: TS: {}, "
            "time_shift: {}",
            timestamp, time_shift);
        throw std::out_of_range("Timestamp overflow detected");
      }
      out_ts = timestamp + static_cast<uint64_t>(time_shift);
    }
  } catch (const std::out_of_range&) {
    ViewStateInstance().SetState(pti_result::PTI_ERROR_BAD_TIMESTAMP);
  }
  return out_ts;
}

#endif  // SRC_VIEW_HELPERS_H_
