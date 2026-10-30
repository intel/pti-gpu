//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_ITT_ITT_LIB_ENV_H_
#define PTI_ITT_ITT_LIB_ENV_H_

#include <string>

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
  kUnresolvedLibraryName,
};

// Return the decision and diagnostic paths saved when the PTI interface library
// loaded. These getters do not load the core or inspect the environment again.
IttCollectorConfig GetIttCollectorConfig();
const std::string& GetSelectedIttCollectorPath();
const std::string& GetPtiIttCollectorPath();

// Point ittnotify at this module unless a collector was already chosen.
//
// This may call setenv; callers loading PTI after startup must prevent
// concurrent environment access. The XPTI initializer has the same constraint.
//
// Logs nothing itself; the caller reports the returned value.
// Bare library names are identified only among modules already loaded into the
// process. A name that cannot be identified is preserved and reported unresolved.
IttCollectorConfig ConfigureIttCollector(const std::string& this_module_path);

}  // namespace itt_lib_env

#endif  // PTI_ITT_ITT_LIB_ENV_H_
