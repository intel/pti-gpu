//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_ITT_ITT_LIB_ENV_H_
#define PTI_ITT_ITT_LIB_ENV_H_

#include <cstdlib>
#include <string>
#include <system_error>

#include "utils/pti_filesystem.h"
#include "utils/utils.h"

//
// Helpers around INTEL_LIBITTNOTIFY64, the variable ittnotify uses to pick its
// collector.
//
// ittnotify reads it once per provider library, on that library's first ITT
// call, and latches collector-present-or-absent for the rest of the process.
// libpti_view.so therefore registers itself at load time, from
// GlobalIttInitializer in itt_adapter.cc, which runs before main() and so
// before oneCCL's first ITT call. A late dlopen of libpti_view.so is not
// covered: once a provider has latched, nothing here can undo it.
//
namespace itt_lib_env {

// PTI is 64-bit only, so the 32-bit spelling of this variable is not handled.
inline constexpr const char* kIttLibEnvVarName = "INTEL_LIBITTNOTIFY64";

enum class IttCollectorConfig {
  kSetByPti,
  kAlreadyPti,
  kExternalLibrary,
  kExternalNotFound,
  kConfigurationFailed,
};

// Point ittnotify at this module unless a collector was already chosen.
//
// This may call setenv; callers loading PTI after startup must prevent
// concurrent environment access. The XPTI initializer has the same constraint.
//
// Logs nothing itself; the caller reports the returned value.
inline IttCollectorConfig ConfigureIttCollector(const std::string& this_module_path) {
  if (this_module_path.empty()) {
    return IttCollectorConfig::kConfigurationFailed;
  }

  const std::string env_value = ::utils::GetEnv(kIttLibEnvVarName);

  if (env_value.empty()) {
    // Preserve the module actually mapped into this process, but make its path
    // independent of the working directory inherited by child processes.
    std::error_code error;
    const auto absolute_path = pti::utils::filesystem::absolute(this_module_path, error);
    if (error || absolute_path.empty() ||
        ::setenv(kIttLibEnvVarName, absolute_path.c_str(), 1) != 0) {
      return IttCollectorConfig::kConfigurationFailed;
    }
    return IttCollectorConfig::kSetByPti;
  }

  std::error_code error;
  const bool exists = pti::utils::filesystem::exists(env_value, error);
  if (error) {
    return IttCollectorConfig::kConfigurationFailed;
  }
  if (!exists) {
    return IttCollectorConfig::kExternalNotFound;
  }

  // Compare file identity so symlinks and alternate paths to this module match.
  const bool is_this_module =
      pti::utils::filesystem::equivalent(env_value, this_module_path, error);
  if (error) {
    return IttCollectorConfig::kConfigurationFailed;
  }
  if (is_this_module) {
    return IttCollectorConfig::kAlreadyPti;
  }

  return IttCollectorConfig::kExternalLibrary;
}

}  // namespace itt_lib_env

#endif  // PTI_ITT_ITT_LIB_ENV_H_
