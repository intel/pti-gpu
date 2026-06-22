//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef LEVEL_ZERO_ZE_DRIVER_INIT_H_
#define LEVEL_ZERO_ZE_DRIVER_INIT_H_

#include <level_zero/ze_api.h>

#include <optional>
#include <unordered_map>
#include <vector>

#include "ze_extensions.h"

class ZeDriverInit {
 public:
  ZeDriverInit();

  bool Success() const;

  const std::vector<ze_driver_handle_t>& Drivers() const;

  std::vector<ze_driver_handle_t>& Drivers();

  template <typename T>
  std::optional<T> GetExtension(ze_driver_handle_t driver) const {
    auto ext_it = driver_extension_fns_.find(driver);
    if (ext_it == driver_extension_fns_.end()) {
      return std::nullopt;
    }
    return std::get<std::optional<T>>(ext_it->second.extensions);
  }

 private:
  bool InitDrivers();
  void CollectLegacyDrivers();
  void InitSysmanDrivers();
  void CollectExtensions(const std::vector<ze_driver_handle_t>& drivers);

  bool init_success_ = false;
  std::vector<ze_driver_handle_t> drivers_;
  std::unordered_map<ze_driver_handle_t, ZeExts> driver_extension_fns_;
};

#endif  // LEVEL_ZERO_ZE_DRIVER_INIT_H_
