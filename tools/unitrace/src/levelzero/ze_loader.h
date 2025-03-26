//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef PTI_TOOLS_UNITRACE_LEVEL_ZERO_LEADER_H_
#define PTI_TOOLS_UNITRACE_LEVEL_ZERO_LEADER_H_

#include <iostream>
#include <memory>

#include "utils/library_loader.h"
#include <level_zero/layers/zel_tracing_register_cb.h>
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <level_zero/zet_api.h>
#include <level_zero/layers/zel_tracing_api.h>

class ZeLoader {
 public:
  static auto& get() {
    static ZeLoader instance{};
    return instance;
  }

  void Unload() { lib_.reset(nullptr); }

  bool Loaded() const { return lib_ != nullptr; }

 private:
  ZeLoader() {
    #if defined(_WIN32)
    static constexpr const char* const kLevelZeroLoaderName = "ze_loader.dll";
    #else
    static constexpr const char* const kLevelZeroLoaderName = "libze_loader.so.1";
    #endif

    try {
      lib_ = std::make_unique<LibraryLoader>(kLevelZeroLoaderName);
    } catch (const std::exception& e) {
      std::cerr << "[ERROR] Failed to load " << kLevelZeroLoaderName
                << " Reason: " << e.what() << std::endl;
      lib_ = nullptr;
      return;
    }

    init();
  }

  std::unique_ptr<LibraryLoader> lib_ = nullptr;

  #define LEVEL_ZERO_LOADER_GET_SYMBOL(X) \
    this->X##_ = lib_->GetSymbol<decltype(&X)>(#X)  // NOLINT
  #include <l0_loader.gen>               // Auto-generated callbacks
  #undef LEVEL_ZERO_LOADER_GET_SYMBOL
};

#define ZE_FUNC(X) ZeLoader::get().X##_
#define ZE_HAVE_FUNC(X) (ZeLoader::get().X##_ != nullptr)

#endif  // PTI_TOOLS_UNITRACE_LEVEL_ZERO_LEADER_H_
