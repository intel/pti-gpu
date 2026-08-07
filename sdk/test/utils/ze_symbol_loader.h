//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef TEST_UTILS_ZE_SYMBOL_LOADER_H_
#define TEST_UTILS_ZE_SYMBOL_LOADER_H_

#include <level_zero/ze_api.h>

#include <exception>

#include "utils/library_loader.h"

namespace pti::test::utils::level_zero {

#if defined(_WIN32)
inline constexpr const char* const kZeLoaderName = "ze_loader.dll";
#else
inline constexpr const char* const kZeLoaderName = "libze_loader.so.1";
#endif

class ZeSymbolLoader {
 public:
  [[nodiscard]] bool Loaded() const { return loaded_; }

  template <typename ZeSym>
  [[nodiscard]] ZeSym Get(const char* name) {
    return Loaded() ? loader_.GetSymbol<ZeSym>(name) : nullptr;
  }

  ZeSymbolLoader() {
    try {
      loader_ = LibraryLoader{kZeLoaderName};
      loaded_ = true;
    } catch (const std::exception&) {
      loaded_ = false;
    }
  }

 private:
  LibraryLoader loader_;
  bool loaded_ = false;
};

}  // namespace pti::test::utils::level_zero

#endif  // TEST_UTILS_ZE_SYMBOL_LOADER_H_
