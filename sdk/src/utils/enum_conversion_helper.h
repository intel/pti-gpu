//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UTILS_ENUM_CONVERSION_HELPER_
#define UTILS_ENUM_CONVERSION_HELPER_

#include <cstdlib>
#include <optional>

namespace pti::utils {

template <typename E, typename T>
struct EnumContainer {
  E value_;
  T conversion_;
};

template <typename E>
using EnumToString = EnumContainer<E, const char*>;

// Lookup an enum value index from the table evaulated at compile
// time.
template <typename E, typename C>
inline constexpr std::optional<std::size_t> EnumIdx(E my_enum, C container) {
  std::size_t count = 0;
  for (const auto& enum_container : container) {
    if (my_enum == enum_container.value_) {
      return count;
    }
    count++;
  }
  return std::nullopt;
}

// Acknowledging using macros is typically bad. However, I don't know how else
// to do this other than autogenerating code (maybe one day [C++26
// reflections](https://isocpp.org/files/papers/P2996R4.html) will be a thing).
#define PTI_ASSOCIATE_ENUM_MEMBER_TO_STRING(enum_type, enum_value, enum_string) \
  pti::utils::EnumToString<enum_type> { enum_value, enum_string }
#define PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(enum_type, enum_value) \
  PTI_ASSOCIATE_ENUM_MEMBER_TO_STRING(enum_type, enum_value, #enum_value)
#define PTI_ENUM_CONVERSION(enum_type, enum_value, container) \
  case enum_value: {                                          \
    constexpr auto enum_idx = EnumIdx(enum_value, container); \
    static_assert(enum_idx != std::nullopt);                  \
    return container[*enum_idx].conversion_;                  \
  }
}  // namespace pti::utils

#endif  // UTILS_ENUM_CONVERSION_HELPER_
