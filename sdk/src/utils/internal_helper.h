//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef INTERNAL_HELPER_H_
#define INTERNAL_HELPER_H_
#include <type_traits>

#include "pti_view.h"

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper functions to detect wrong value converted to specific enum
/// type
template <typename IntType, typename EnumType, typename EnumVal>
bool is_valid(IntType val, EnumVal V) {
  static_assert(std::is_enum<EnumVal>::value);
  static_assert(std::is_same<EnumType, EnumVal>::value);
  return static_cast<IntType>(V) == val;
};

template <typename IntType, typename EnumType, typename EnumVal, typename... Next>
bool is_valid(IntType val, EnumVal V, Next... oth) {
  static_assert(std::is_enum<EnumVal>::value);
  static_assert(std::is_same<EnumType, EnumVal>::value);

  return static_cast<IntType>(V) == val || is_valid<IntType, EnumType, Next...>(val, oth...);
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Checks is the provided value v belongs to pti_view_kind enums
bool IsPtiViewKindEnum(int v) {
  return is_valid<int, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind,
                  pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind>(
      v, pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL, pti_view_kind::PTI_VIEW_DEVICE_CPU_KERNEL,
      pti_view_kind::PTI_VIEW_LEVEL_ZERO_CALLS, pti_view_kind::PTI_VIEW_OPENCL_CALLS,
      pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD, pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS,
      pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY,
      pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P);
}
#endif  // INTERNAL_HELPER_H_
