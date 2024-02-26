//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_METRIC_UTILS_H_
#define PTI_UTILS_METRIC_UTILS_H_

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <algorithm>
#endif

#include <memory>
#include <string>
#include <vector>

#include <metrics_discovery_api.h>

namespace utils {
namespace metrics {

inline constexpr const char* const kProcFilePerfLocation =
  "/proc/sys/dev/i915/perf_stream_paranoid";

using ProcFileResultType = char;
inline constexpr ProcFileResultType kProcFileExpectedValue = '0';
inline constexpr ProcFileResultType kProcFileBadValue = '1';

inline bool SufficientPrivilegesForMetrics() {
#if defined(__gnu_linux__)
  std::ifstream setting {kProcFilePerfLocation};
  if (setting.fail()) {
    return false;
  }
  auto value = kProcFileBadValue;
  std::copy_n(std::istream_iterator<char>(setting), sizeof value, &value);

  return value == kProcFileExpectedValue;
#endif
  return true;
}

inline std::vector<std::string> GetMDLibraryName() {
#if defined(_WIN32)
#  if defined(_WIN64)
  return{ "igdmd64.dll" };
#  else
  return{ "igdmd32.dll" };
#  endif
#elif defined(__gnu_linux__)
  return{ "libigdmd.so", "libmd.so" };
#elif defined(__APPLE__)
  return{ "libmd.dylib", "libigdmd.dylib" };
#else
#error "Unsupported platform!"
#endif
}

inline std::string GetPreferedLibraryPath() {
#ifdef _WIN32
  const std::string kKeyName = "SOFTWARE\\Intel\\MDF";
  const std::string kValueName = "DriverStorePath";

  HKEY key_handle = nullptr;
  LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, kKeyName.c_str(),
                                 0, KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
      return "";
  }

  std::unique_ptr<std::remove_pointer<HKEY>::type,
                  decltype(&RegCloseKey)> key(key_handle, RegCloseKey);

  DWORD buffer_size = MAX_PATH * sizeof(char);
  std::unique_ptr<BYTE[]> buffer(new BYTE[buffer_size]);

  status = RegQueryValueExA(key.get(), kValueName.c_str(), nullptr, nullptr,
                            buffer.get(), &buffer_size);
  if (status != ERROR_SUCCESS) {
    return "";
  }

  return reinterpret_cast<char*>(buffer.get());
#else
  return "";
#endif
}

inline std::vector<std::string> GetMDLibraryPossiblePaths() {
  std::vector<std::string> paths;

  std::string prefered_path = GetPreferedLibraryPath();
  std::vector<std::string> library_names = GetMDLibraryName();

  for (auto& library_name : library_names) {
    if (!prefered_path.empty()) {
      paths.push_back(prefered_path + "/" + library_name);
    }
    paths.push_back(library_name);
  }

  return paths;
}

} // namespace metrics
} // namespace utils

#endif // PTI_UTILS_METRIC_UTILS_H_
