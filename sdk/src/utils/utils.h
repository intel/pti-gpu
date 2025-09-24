//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifdef __clang__
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#ifndef PTI_UTILS_UTILS_H_
#define PTI_UTILS_UTILS_H_

#if defined(_WIN32)
#include <windows.h>

#include <chrono>
#else /* _WIN32 */
#include <dlfcn.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#define HMODULE void*
#endif /* _WIN32 */

#if defined(_WIN32)
#define PTI_LIB_PREFIX "pti"
#else
#define PTI_LIB_PREFIX "libpti"
#endif

#include <spdlog/cfg/env.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "pti_assert.h"
#include "pti_filesystem.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_STR_SIZE 1024

#define BYTES_IN_MBYTES (1024 * 1024)

#define NSEC_IN_USEC 1'000
#define MSEC_IN_SEC 1'000
#define NSEC_IN_MSEC 1'000'000
#define NSEC_IN_SEC 1'000'000'000

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

// if enable_logging is false, the logger is set to console and it is off
// if enable_loging is true
//  if logfile is empty, then the logger is set to console and it is on
//  else logfile is not empty, then the logger is set to log file and it is on
inline std::shared_ptr<spdlog::logger> GetLogStream(bool enable_logging = false,
                                                    std::string logfile = std::string()) {
  std::shared_ptr<spdlog::logger> logger;
  std::random_device rand_dev;    // uniformly-distributed integer random number generator
  std::mt19937 prng(rand_dev());  // pseudorandom number generator
  std::uniform_int_distribution<uint64_t> rand_num(0);  // random number

  try {
    // Default user logger is to the console and it is off
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::off);
    logger = std::make_shared<spdlog::logger>("logger_" + fmt::format("{:x}", rand_num(prng)),
                                              console_sink);
    if (enable_logging) {
      if (!logfile.empty()) {
        // if logfile not empty, then the file is created and the data is logged to it
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
        logger = std::make_shared<spdlog::logger>(
            "file_logger_" + fmt::format("{:x}", rand_num(prng)), file_sink);
      } else {
        // the logging is enabled to the console
        console_sink->set_level(spdlog::level::debug);
        logger = std::make_shared<spdlog::logger>("logger_" + fmt::format("{:x}", rand_num(prng)),
                                                  console_sink);
      }
    }
    auto format = std::make_unique<spdlog::pattern_formatter>(
        "%v", spdlog::pattern_time_type::local, std::string(""));  // disable eol
    logger->set_formatter(std::move(format));
    spdlog::register_logger(logger);

  } catch (const spdlog::spdlog_ex& exception) {
    std::cerr << "Failed to initialize log file: " << exception.what() << std::endl;
  }
  return logger;
}

inline pti::utils::filesystem::path CreateTempDirectory() {
  auto tmp_dir = pti::utils::filesystem::temp_directory_path();
  std::random_device rand_dev;    // uniformly-distributed integer random number generator
  std::mt19937 prng(rand_dev());  // pseudorandom number generator
  std::uniform_int_distribution<uint64_t> rand_num(0);  // random number
  pti::utils::filesystem::path path;
  std::string dir_name = "pti_" + std::to_string(rand_num(prng));
  path = tmp_dir / dir_name;
  pti::utils::filesystem::create_directory(path);

  return path;
}

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

inline HMODULE LoadLibrary(const char* lib_name) {
#if defined(_WIN32)
  return LoadLibraryExA(lib_name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else  /* _WIN32 */
  return dlopen(lib_name, RTLD_LAZY | RTLD_LOCAL);
#endif /* _WIN32 */
}

inline void UnloadLibrary(HMODULE lib_handle) {
#if defined(_WIN32)
  FreeLibrary(lib_handle);
#else  /* _WIN32 */
  dlclose(lib_handle);
#endif /* _WIN32 */
}

inline void* GetFunctionPtr(HMODULE lib_handle, const char* func_name) {
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(lib_handle, func_name));
#else  /* _WIN32 */
  return dlsym(lib_handle, func_name);
#endif /* _WIN32 */
}

inline std::pair<bool, bool> IsSubscriberToXPTI() {
  auto current_xpti_subscriber = utils::GetEnv("XPTI_SUBSCRIBERS");
  bool is_unitrace = current_xpti_subscriber.find("unitrace") != std::string::npos;

  if (current_xpti_subscriber.empty()) {
    return {false, is_unitrace};
  }

  // NOTE: This is not a 100% bulletproof solution, but a practical one.
  // In rare cases, a "foreign" subscriber such as libpti_some_another.so
  // might be incorrectly recognized as PTI.
  if (current_xpti_subscriber.find(PTI_LIB_PREFIX) != std::string::npos) {
    return {false, is_unitrace};
  }

  // It's a real foreign subscriber (not PTI)
  return {true, is_unitrace};
}

inline void SetGlobalSpdLogPattern() {
  // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
  spdlog::set_pattern("PTI:[%H:%M][%^-%l-%$]%P:%t %s:%# %v");
}
}  // namespace utils

#endif  // PTI_UTILS_UTILS_H_
