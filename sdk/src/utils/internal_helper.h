//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef INTERNAL_HELPER_H_
#define INTERNAL_HELPER_H_
#include <type_traits>

#include "pti/pti_view.h"

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper functions to detect wrong value converted to specific enum
/// type
template <typename IntType, typename EnumType, typename EnumVal>
bool IsValid(IntType val, EnumVal V) {
  static_assert(std::is_enum<EnumVal>::value);
  static_assert(std::is_same<EnumType, EnumVal>::value);
  return static_cast<IntType>(V) == val;
}

template <typename IntType, typename EnumType, typename EnumVal, typename... Next>
bool IsValid(IntType val, EnumVal V, Next... oth) {
  static_assert(std::is_enum<EnumVal>::value);
  static_assert(std::is_same<EnumType, EnumVal>::value);

  return static_cast<IntType>(V) == val || IsValid<IntType, EnumType, Next...>(val, oth...);
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Checks is the provided value v belongs to pti_view_kind enums
bool IsPtiViewKindEnum(int v) {
  return IsValid<int, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind,
                 pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind, pti_view_kind>(
      v, pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL, pti_view_kind::PTI_VIEW_DEVICE_CPU_KERNEL,
      pti_view_kind::PTI_VIEW_DRIVER_API, pti_view_kind::PTI_VIEW_RESERVED,
      pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD, pti_view_kind::PTI_VIEW_RUNTIME_API,
      pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY,
      pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P);
}
#endif  // INTERNAL_HELPER_H_
