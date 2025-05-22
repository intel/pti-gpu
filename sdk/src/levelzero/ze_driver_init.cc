//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "ze_driver_init.h"

#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <vector>

#include "lz_api_tracing_api_loader.h"
#include "overhead_kinds.h"
#include "pti_assert.h"
#include "utils.h"
#include "ze_utils.h"

namespace {
// The Level Zero Loader version with a fully functional zeInitDrivers.
// Versions prior to this one have bugs (or don't have it at all).
// zeInitDrivers is the preferred method for initializing drivers from this point on.
constexpr zel_version_t kProperLoaderVersionForZeInitDrivers = {1, 19, 2};
constexpr uint32_t kBmgIpVersion = 0x05004000;

// Environment variable that determines whether to call zesInit.
//
// This is needed to allow users to disable zesInit if they want or if oneAPI changes the default
// behavior in the future, we can still have a way to fix it.
//
// PTI_SYSMAN_ZESINIT=0 not not call zesInit.
// PTI_SYSMAN_ZESINIT=1 call zesInit.
// unset - default to oneAPI behavior, calling it on BMG and later platforms.
constexpr const char* const kCallZesInitEnv = "PTI_SYSMAN_ZESINIT";

std::optional<bool> GetZesInitEnv() {
  auto env_value = utils::GetEnv(kCallZesInitEnv);
  if (env_value.empty()) {
    return std::nullopt;
  }
  if (env_value == "1") {
    return true;
  }
  if (env_value == "0") {
    return false;
  }
  return std::nullopt;
}

inline bool operator>=(const zel_version_t& left, const zel_version_t& right) {
  bool same_major_version = left.major == right.major;
  return (same_major_version && left.minor > right.minor) ||
         (same_major_version && left.minor == right.minor && left.patch >= right.patch) ||
         (left.major > right.major);
}

ze_result_t ZeInitDrivers(uint32_t* driver_count, ze_driver_handle_t* drivers,
                          ze_init_driver_type_desc_t* desc) {
  if (pti::PtiLzTracerLoader::Instance().zeInitDrivers_) {
    return pti::PtiLzTracerLoader::Instance().zeInitDrivers_(driver_count, drivers, desc);
  }
  return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool ProperLoaderforZeInitDrivers() {
  auto loader_version = utils::ze::GetLoaderVersion();
  if (!loader_version) {
    return false;
  }
  SPDLOG_DEBUG("Loader version: {}.{}.{}.", loader_version->major, loader_version->minor,
               loader_version->patch);
  return *loader_version >= kProperLoaderVersionForZeInitDrivers;
}

void CheckLegacyDriverVersion(const std::vector<ze_driver_handle_t>& drivers) {
  auto version = utils::ze::GetVersion(drivers);
  SPDLOG_INFO("Driver version major: {}, minor: {}", ZE_MAJOR_VERSION(version),
              ZE_MINOR_VERSION(version));
  PTI_ASSERT(ZE_MAJOR_VERSION(version) > 1 ||
             (ZE_MAJOR_VERSION(version) == 1 && ZE_MINOR_VERSION(version) >= 3));
}

bool InitLegacyDrivers() {
  overhead::Init();
  ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  overhead_fini("zeInit");
  bool result = (status == ZE_RESULT_SUCCESS);
  if (!result) {
    SPDLOG_WARN("zeInit returned: {}.", static_cast<uint32_t>(status));
    return false;
  }
  return result;
}
}  // namespace

ZeDriverInit::ZeDriverInit() : init_success_(InitLegacyDrivers()) {
  CollectLegacyDrivers();
  CheckLegacyDriverVersion(drivers_);
  if (ProperLoaderforZeInitDrivers()) {
    // If legacy driver initialization failed, the current way. Don't immediately fail.
    if (InitDrivers()) {
      init_success_ = true;
    }
  }
  if (init_success_) {
    InitSysmanDrivers();
  }
}

bool ZeDriverInit::Success() const { return init_success_; }

const std::vector<ze_driver_handle_t>& ZeDriverInit::Drivers() const { return drivers_; }

std::vector<ze_driver_handle_t>& ZeDriverInit::Drivers() { return drivers_; }

bool ZeDriverInit::InitDrivers() {
  std::uint32_t drv_cnt = 0;
  ze_init_driver_type_desc_t desc{};
  desc.stype = ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC;
  desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
  desc.pNext = nullptr;
  overhead::Init();
  auto status = ZeInitDrivers(&drv_cnt, nullptr, &desc);
  overhead_fini("zeInitDrivers");

  bool result = (status == ZE_RESULT_SUCCESS);

  if (!result) {
    SPDLOG_INFO("ZeInitDrivers returned: {}.", static_cast<uint32_t>(status));
    return result;
  }

  std::vector<ze_driver_handle_t> driver_list(static_cast<size_t>(drv_cnt));

  overhead::Init();
  status = ZeInitDrivers(&drv_cnt, driver_list.data(), &desc);
  overhead_fini("zeInitDrivers");
  result = (status == ZE_RESULT_SUCCESS);

  if (!result) {
    SPDLOG_INFO("ZeInitDrivers returned: {}.", static_cast<uint32_t>(status));
    return result;
  }

  for (auto* driver : driver_list) {
    drivers_.push_back(driver);
  }

  return result;
}

void ZeDriverInit::CollectLegacyDrivers() { drivers_ = utils::ze::GetDriverList(); }

void ZeDriverInit::InitSysmanDrivers() {
  auto call_zesinit_env = GetZesInitEnv();

  // As of oneAPI 2025.1, zesInit will be called for BMG/LNL and later platforms. Until now,
  // pti_view has not required the Sysman API. However, without this, subsequent calls to zesInit
  // will disable tracing for users.
  // The SYCL runtime will call zesInit on BMG and later platforms if zesDriverGetDeviceByUuid
  // symbol is available. We will mirror this behavior due to potential compatibility issues with
  // other oneAPI components.
  //
  // zesInit is only supported on platforms newer than PVC.
  //
  // See specifics regarding Sysman / zes* compatiblity below:
  // https://github.com/intel/compute-runtime/blob/master/programmers-guide/SYSMAN.md
  bool call_zesinit = false;
  if (!call_zesinit_env) {
    if (pti::PtiLzTracerLoader::Instance().zesDriverGetDeviceByUuidExp_) {
      if (utils::ze::ContainsDeviceWithAtLeastIpVersion(drivers_, kBmgIpVersion)) {
        call_zesinit = true;
      }
    }
  } else {
    call_zesinit = *call_zesinit_env;
  }
  if (call_zesinit) {
    constexpr zes_init_flags_t kZesInitFlags = 0;
    if (zesInit(kZesInitFlags) != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN(
          "zesInit failed, tracing state might be disabled after another call to a oneAPI "
          "component or driver");
    }
  }
}
