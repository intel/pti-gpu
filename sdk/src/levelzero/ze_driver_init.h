//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef LEVEL_ZERO_ZE_DRIVER_INIT_H_
#define LEVEL_ZERO_ZE_DRIVER_INIT_H_

#include <level_zero/ze_api.h>

#include <vector>

class ZeDriverInit {
 public:
  ZeDriverInit();

  bool Success() const;

  const std::vector<ze_driver_handle_t>& Drivers() const;

  std::vector<ze_driver_handle_t>& Drivers();

 private:
  bool InitDrivers();
  void CollectLegacyDrivers();
  void InitSysmanDrivers();

  bool init_success_ = false;
  std::vector<ze_driver_handle_t> drivers_;
};

#endif  // LEVEL_ZERO_ZE_DRIVER_INIT_H_
