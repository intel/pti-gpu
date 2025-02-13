//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_VIEW_RECORD_INFO_
#define SRC_VIEW_RECORD_INFO_

#include <array>
#include <cstddef>

#include "pti/pti_view.h"

inline constexpr auto kReserved = 0;
inline constexpr auto kLastViewRecordEnumValue = PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P;
inline constexpr auto kSizeOfViewRecordTable = kLastViewRecordEnumValue + 1;

// kViewSizeLookUpTable
//
// Table of view record enum identifiers to their size. The spot in the
// array corresponds to the underlying value of a member of the
// pti_view_kind enum.
//
// clang-format off
inline constexpr std::array<std::size_t, kSizeOfViewRecordTable> kViewSizeLookupTable{
    kReserved,                                        // PTI_VIEW_INVALID
    sizeof(pti_view_record_kernel),                   // PTI_VIEW_DEVICE_GPU_KERNEL
    kReserved,                                        // PTI_VIEW_DEVICE_CPU_KERNEL
    sizeof(pti_view_record_api),                      // PTI_VIEW_LEVEL_ZERO_CALLS
    kReserved,                                        // PTI_VIEW_OPENCL_CALLS
    sizeof(pti_view_record_overhead),                 // PTI_VIEW_COLLECTION_OVERHEAD
    sizeof(pti_view_record_api),                      // PTI_VIEW_SYCL_RUNTIME_CALLS
    sizeof(pti_view_record_external_correlation),     // PTI_VIEW_EXTERNAL_CORRELATION
    sizeof(pti_view_record_memory_copy),              // PTI_VIEW_DEVICE_GPU_MEM_COPY
    sizeof(pti_view_record_memory_fill),              // PTI_VIEW_DEVICE_GPU_MEM_FILL
    sizeof(pti_view_record_memory_copy_p2p),          // PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P
};
// clang-format on

// SizeOfLargestViewRecord()
//
// Calculated at compile time (since we know all the records and their sizes)
//
// @return size of largest view record
inline constexpr auto SizeOfLargestViewRecord() {
  auto largest_record_size = kViewSizeLookupTable.front();
  for (const auto& record_size : kViewSizeLookupTable) {
    if (largest_record_size < record_size) {
      largest_record_size = record_size;
    }
  }
  return largest_record_size;
}

// GetViewSize()
//
// Convert pti_view_kind enum to actual size of record.
//
// @param view_type onpti_view_kind enum value
// @return size of record corresponding to view_type
inline auto GetViewSize(pti_view_kind view_type) {
  const auto view_type_index = static_cast<std::size_t>(view_type);
  if (view_type_index >= std::size(kViewSizeLookupTable)) {
    return SIZE_MAX;
  }

  const auto view_size = kViewSizeLookupTable[view_type_index];
  if (view_size == kReserved) {
    return SIZE_MAX;
  }

  return view_size;
}

#endif  // SRC_VIEW_RECORD_INFO_
