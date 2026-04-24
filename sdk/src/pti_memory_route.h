//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_MEMORY_ROUTE_
#define PTI_MEMORY_ROUTE_

#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

#include "pti/pti_view.h"

/**
 * \internal
 * @brief Representation of a memory copy route.
 */
struct PtiMemoryCommandRoute {
  pti_view_memory_type src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
  pti_view_memory_type dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY;
  bool is_peer_2_peer = false;

  // Intentional - compiler warns if developer adds a new memory type but forgets to update this
  // function.
  [[nodiscard]] static constexpr std::size_t GetIdx(pti_view_memory_type mem_type) {
    static_assert(static_cast<std::size_t>(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY) == 0,
                  "Expected MEMORY to be 0. Enum values must not change");
    static_assert(static_cast<std::size_t>(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST) == 1,
                  "Expected HOST to be 1. Enum values must not change");
    static_assert(static_cast<std::size_t>(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE) == 2,
                  "Expected DEVICE to be 2. Enum values must not change");
    static_assert(static_cast<std::size_t>(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED) == 3,
                  "Expected SHARED to be 3. Enum values must not change");
    switch (mem_type) {
      case pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY:
      case pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST:
      case pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE:
      case pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED:
        return static_cast<std::size_t>(mem_type);
    }
    return 0;
  }

  static char GetChar(pti_view_memory_type type) {
    constexpr static std::array kCharTable = {'M', 'H', 'D', 'S'};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return kCharTable[GetIdx(type)];
  }

  std::string GetCompactStringForTypes() const {
    return {GetChar(src_type), '2', GetChar(dst_type)};
  }

  std::string_view GetCompactStringForP2P() const { return is_peer_2_peer ? " - P2P" : ""; }

  pti_view_memcpy_type GetMemcpyType() const {
    using Mct = pti_view_memcpy_type;
    // clang-format off
    constexpr static std::array kMemcpyTypeTable = {
        //                            M                                 H                                D                                 S
        /* M */ std::array{Mct::PTI_VIEW_MEMCPY_TYPE_M2M, Mct::PTI_VIEW_MEMCPY_TYPE_M2H, Mct::PTI_VIEW_MEMCPY_TYPE_M2D, Mct::PTI_VIEW_MEMCPY_TYPE_M2S},
        /* H */ std::array{Mct::PTI_VIEW_MEMCPY_TYPE_H2M, Mct::PTI_VIEW_MEMCPY_TYPE_H2H, Mct::PTI_VIEW_MEMCPY_TYPE_H2D, Mct::PTI_VIEW_MEMCPY_TYPE_H2S},
        /* D */ std::array{Mct::PTI_VIEW_MEMCPY_TYPE_D2M, Mct::PTI_VIEW_MEMCPY_TYPE_D2H, Mct::PTI_VIEW_MEMCPY_TYPE_D2D, Mct::PTI_VIEW_MEMCPY_TYPE_D2S},
        /* S */ std::array{Mct::PTI_VIEW_MEMCPY_TYPE_S2M, Mct::PTI_VIEW_MEMCPY_TYPE_S2H, Mct::PTI_VIEW_MEMCPY_TYPE_S2D, Mct::PTI_VIEW_MEMCPY_TYPE_S2S}};
    // clang-format on

    // PTI sets the enum, so no need for runtime check.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return kMemcpyTypeTable[GetIdx(src_type)][GetIdx(dst_type)];
  }

  /// @internal
  /// @brief Returns true if the source memory should be considered the 'main' device
  bool IsMainDeviceSrc() const {
    if (src_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE &&
        dst_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE) {
      return false;
    }
    return src_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE ||
           src_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
  }
};

#endif  // PTI_MEMORY_ROUTE_
