//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_UTILS_H_
#define PTI_UTILS_UTILS_H_

#if defined(_WIN32)
#include <windows.h>

#include <chrono>
#else
#include <dlfcn.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#endif

#include <stdint.h>

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "pti_assert.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_STR_SIZE 1024

#define BYTES_IN_MBYTES (1024 * 1024)

#define NSEC_IN_USEC 1000
#define MSEC_IN_SEC 1000
#define NSEC_IN_MSEC 1000000
#define NSEC_IN_SEC 1000000000

namespace utils {

typedef uint64_t (*fptr_get_timestamp_unknown_clock)(void);

// Duplicated from test/utils --- there are some useful methods there that can be pulled here as
// needed.
//-----
template <typename T>
inline void Zeroize(T& item) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Can't zeroize an object that's not trivially copyable");
  std::memset(&item, 0, sizeof(T));
}
//-----

struct Comparator {
  template <typename T>
  bool operator()(const T& left, const T& right) const {
    if (left.second != right.second) {
      return left.second > right.second;
    }
    return left.first > right.first;
  }
};

#if defined(__gnu_linux__)

inline uint64_t GetTime(clockid_t id) {
  timespec ts{0, 0};
  int status = clock_gettime(id, &ts);
  PTI_ASSERT(status == 0);
  return ts.tv_sec * NSEC_IN_SEC + ts.tv_nsec;
}

inline uint64_t ConvertClockMonotonicToRaw(uint64_t clock_monotonic) {
  uint64_t raw = GetTime(CLOCK_MONOTONIC_RAW);
  uint64_t monotonic = GetTime(CLOCK_MONOTONIC);
  return (raw > monotonic) ? clock_monotonic + (raw - monotonic)
                           : clock_monotonic - (monotonic - raw);
}

#endif

inline uint64_t GetMonotonicRawTime() {
#if defined(_WIN32)
  LARGE_INTEGER ticks{};
  LARGE_INTEGER frequency{};
  BOOL status = QueryPerformanceFrequency(&frequency);
  PTI_ASSERT(status != 0);
  status = QueryPerformanceCounter(&ticks);
  PTI_ASSERT(status != 0);
  return ticks.QuadPart * (NSEC_IN_SEC / frequency.QuadPart);
#else
  return GetTime(CLOCK_MONOTONIC_RAW);
#endif
}

inline uint64_t GetRealTime() {
#if defined(_WIN32)
  // convert to ns ticks the proper way
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
#else
  return GetTime(CLOCK_REALTIME);
#endif
}

inline int64_t ConversionFactorMonotonicRawToUnknownClock(
    fptr_get_timestamp_unknown_clock user_provided_get_timestamp) {
  uint64_t user_final = user_provided_get_timestamp();
  uint64_t raw_final = GetMonotonicRawTime();
  constexpr auto kNumberOfIterations = 50;
  std::array<uint64_t, kNumberOfIterations> raw_start = {};
  std::array<uint64_t, kNumberOfIterations> raw_end = {};
  std::array<uint64_t, kNumberOfIterations> user = {};

  int i_at_min = -1;
  int64_t diff_min = (std::numeric_limits<int64_t>::max)();  // some big number
  int64_t diff;

  for (int i = 0; i < kNumberOfIterations; i++) {
    raw_start[i] = GetMonotonicRawTime();
    user[i] = user_provided_get_timestamp();
    raw_end[i] = GetMonotonicRawTime();
    diff = raw_end[i] - raw_start[i];
    if (diff < diff_min) {
      diff_min = diff;
      i_at_min = i;
    }
  }

  raw_final = (raw_start[i_at_min] + raw_end[i_at_min]) / 2;
  user_final = user[i_at_min];

  return (user_final > raw_final) ? static_cast<int64_t>(user_final - raw_final)
                                  : -static_cast<int64_t>(raw_final - user_final);
}

inline uint64_t GetTime() { return GetMonotonicRawTime(); }

inline std::string GetFilePath(const std::string& filename) {
  PTI_ASSERT(!filename.empty());

  size_t pos = filename.find_last_of("/\\");
  if (pos == std::string::npos) {
    return "";
  }

  return filename.substr(0, pos + 1);
}

inline std::string GetExecutablePath() {
  char buffer[MAX_STR_SIZE] = {0};
#if defined(_WIN32)
  DWORD status = GetModuleFileNameA(nullptr, buffer, MAX_STR_SIZE);
  PTI_ASSERT(status > 0);
#else
  ssize_t status = readlink("/proc/self/exe", buffer, MAX_STR_SIZE);
  PTI_ASSERT(status > 0);
#endif
  return GetFilePath(buffer);
}

inline std::string GetExecutableName() {
  char buffer[MAX_STR_SIZE] = {0};
#if defined(_WIN32)
  DWORD status = GetModuleFileNameA(nullptr, buffer, MAX_STR_SIZE);
  PTI_ASSERT(status > 0);
#else
  ssize_t status = readlink("/proc/self/exe", buffer, MAX_STR_SIZE);
  PTI_ASSERT(status > 0);
#endif
  std::string path(buffer);
  return path.substr(path.find_last_of("/\\") + 1);
}

inline std::vector<uint8_t> LoadBinaryFile(const std::string& path) {
  std::vector<uint8_t> binary;
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream.good()) {
    return binary;
  }

  stream.seekg(0, std::ifstream::end);
  size_t size = stream.tellg();
  stream.seekg(0, std::ifstream::beg);
  if (size == 0) {
    return binary;
  }

  binary.resize(size);
  stream.read(reinterpret_cast<char*>(binary.data()), size);
  return binary;
}

inline void SetEnv(const char* name, const char* value) {
  PTI_ASSERT(name != nullptr);
  PTI_ASSERT(value != nullptr);

  int status = 0;
#if defined(_WIN32)
  std::string str = std::string(name) + "=" + value;
  status = _putenv(str.c_str());
#else
  status = setenv(name, value, 1);
#endif
  PTI_ASSERT(status == 0);
}

// Returns:  -1 -- is env not set; 1 if set to 1; 0 otherwise.
inline int32_t IsSetEnv(const char* name) {
  PTI_ASSERT(name != nullptr);
  int32_t env_value = -1;
#if defined(_WIN32)
  char* value = nullptr;
  errno_t status = _dupenv_s(&value, nullptr, name);
  PTI_ASSERT(status == 0);
  if (value == nullptr) {
    return env_value;
  } else if (std::strcmp(value, "ON") == 0) {
    return 1;
  } else if (std::strcmp(value, "OFF") == 0) {
    return 0;
  }
  try {
    env_value = std::stoi(value);
  } catch (std::invalid_argument const& /*ex*/) {
    env_value = 1;  // fallback to default
  } catch (std::out_of_range const& /*ex*/) {
    env_value = 1;  // fallback to default
  }
  free(value);
  if (env_value == 1) return 1;
  return 0;
#else
  const char* value = getenv(name);
  if (value == nullptr) {
    return env_value;
  } else if (std::strcmp(value, "ON") == 0) {
    return 1;
  } else if (std::strcmp(value, "OFF") == 0) {
    return 0;
  }
  try {
    env_value = std::stoi(value);
  } catch (std::invalid_argument const& /*ex*/) {
    env_value = 1;  // fallback to default
  } catch (std::out_of_range const& /*ex*/) {
    env_value = 1;  // fallback to default
  }
  if (env_value == 1) return 1;
  return 0;
#endif
}

inline std::string GetEnv(const char* name) {
  PTI_ASSERT(name != nullptr);
#if defined(_WIN32)
  char* value = nullptr;
  errno_t status = _dupenv_s(&value, nullptr, name);
  PTI_ASSERT(status == 0);
  if (value == nullptr) {
    return std::string();
  }
  std::string result(value);
  free(value);
  return result;
#else
  const char* value = getenv(name);
  if (value == nullptr) {
    return std::string();
  }
  return std::string(value);
#endif
}

inline uint32_t GetPid() {
#if defined(_WIN32)
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

inline uint32_t GetTid() {
#if defined(_WIN32)
  return GetCurrentThreadId();
#else
#ifdef SYS_gettid
  return syscall(SYS_gettid);
#else
#error "SYS_gettid is unavailable on this system"
#endif
#endif
}

inline uint64_t GetSystemTime() {
#if defined(_WIN32)
  LARGE_INTEGER ticks{};
  LARGE_INTEGER frequency{};
  BOOL status = QueryPerformanceFrequency(&frequency);
  PTI_ASSERT(status != 0);
  status = QueryPerformanceCounter(&ticks);
  PTI_ASSERT(status != 0);
  return ticks.QuadPart * (NSEC_IN_SEC / frequency.QuadPart);
#else
  return GetTime(CLOCK_MONOTONIC_RAW);
#endif
}

inline size_t LowerBound(const std::vector<uint64_t>& data, uint64_t value) {
  size_t start = 0;
  size_t end = data.size();
  while (start < end) {
    size_t middle = (start + end) / 2;
    if (value <= data[middle]) {
      end = middle;
    } else {
      start = middle + 1;
    }
  }
  return start;
}

inline size_t UpperBound(const std::vector<uint64_t>& data, uint64_t value) {
  size_t start = 0;
  size_t end = data.size();
  while (start < end) {
    size_t middle = (start + end) / 2;
    if (value >= data[middle]) {
      start = middle + 1;
    } else {
      end = middle;
    }
  }
  return start;
}

#if defined(_WIN32)
inline std::string GetDllPath(HMODULE dll_addr) {
  std::array<CHAR, MAX_STR_SIZE> buffer{};
  auto status =
      GetModuleFileNameA(dll_addr, static_cast<LPSTR>(std::data(buffer)), std::size(buffer));
  return status ? std::string{std::data(buffer), status} : std::string{};
}
#endif

template <typename T>
inline std::string GetPathToSharedObject(T address) {
  static_assert(std::is_pointer<T>::value && !std::is_same<T, const char*>::value,
                "Expecting a function pointer");
#if defined(_WIN32)
  HMODULE dll_addr = nullptr;

  auto got_module = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                       reinterpret_cast<LPCSTR>(address), &dll_addr);
  if (!got_module) {
    return std::string{};
  }

  return GetDllPath(dll_addr);

#else
  Dl_info info{nullptr, nullptr, nullptr, nullptr};
  auto status = dladdr(reinterpret_cast<const void*>(address), &info);
  PTI_ASSERT(status != 0);
  return std::string{info.dli_fname};
#endif
}

#if defined(_WIN32)
template <>
inline std::string GetPathToSharedObject(HMODULE address) {
  return GetDllPath(address);
}

template <>
inline std::string GetPathToSharedObject(const char* name) {
  HMODULE dll_addr = GetModuleHandleA(name);
  if (!dll_addr) {
    return std::string{};
  }
  return GetPathToSharedObject(dll_addr);
}

#endif

}  // namespace utils

#endif  // PTI_UTILS_UTILS_H_
