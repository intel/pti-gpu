//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "itt_lib_env.h"

#include <dlfcn.h>

#include <memory>
#include <string>
#include <system_error>

#include "utils/pti_filesystem.h"
#include "utils/utils.h"

namespace itt_lib_env {

IttCollectorConfig ConfigureIttCollector(const std::string& this_module_path) {
  if (this_module_path.empty()) {
    return IttCollectorConfig::kConfigurationFailed;
  }

  const std::string env_value = ::utils::GetEnv(kIttLibEnvVarName);

  if (env_value.empty()) {
    // Preserve the module actually mapped into this process, but make its path
    // independent of the working directory inherited by child processes.
    std::error_code error;
    const auto absolute_path = pti::utils::AbsolutePath(this_module_path, error);
    if (error || absolute_path.empty()) {
      return IttCollectorConfig::kConfigurationFailed;
    }
    ::utils::SetEnv(kIttLibEnvVarName, absolute_path.c_str());
    return IttCollectorConfig::kSetByPti;
  }

  if (env_value.find('/') == std::string::npos) {
    // ITT resolves bare names through the dynamic loader, not the working
    // directory. Observe loaded modules without loading a foreign collector.
    using ModuleHandle = std::unique_ptr<void, decltype(&::dlclose)>;
    ModuleHandle selected_module(::dlopen(env_value.c_str(), RTLD_LAZY | RTLD_NOLOAD), ::dlclose);
    if (!selected_module) {
      return IttCollectorConfig::kUnresolvedLibraryName;
    }

    ModuleHandle pti_module(::dlopen(this_module_path.c_str(), RTLD_LAZY | RTLD_NOLOAD), ::dlclose);
    if (!pti_module) {
      return IttCollectorConfig::kConfigurationFailed;
    }
    return selected_module.get() == pti_module.get() ? IttCollectorConfig::kAlreadyPti
                                                     : IttCollectorConfig::kExternalLibrary;
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
