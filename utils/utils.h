//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_UTILS_H_
#define PTI_UTILS_UTILS_H_

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include <stdint.h>

#include <fstream>
#include <string>
#include <vector>

#include "pti_assert.h"

#ifdef _WIN32
#define PTI_EXPORT __declspec(dllexport)
#else
#define PTI_EXPORT __attribute__ ((visibility ("default")))
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_STR_SIZE 1024

#define BYTES_IN_MBYTES (1024 * 1024)

#define NSEC_IN_USEC 1000
#define MSEC_IN_SEC  1000
#define NSEC_IN_MSEC 1000000
#define NSEC_IN_SEC  1000000000

namespace utils {

struct DeviceUUID {
  uint16_t vendorID;
  uint16_t deviceID;
  uint16_t revisionID;
  uint16_t pciDomain;
  uint8_t pciBus;
  uint8_t pciDevice;
  uint8_t pciFunction;
  uint8_t reserved[4];
  uint8_t subDeviceId;
};

struct Comparator {
  template<typename T>
  bool operator()(const T& left, const T& right) const {
    if (left.second != right.second) {
      return left.second > right.second;
    }
    return left.first > right.first;
  }
};

template<typename T>
struct ComparatorPciAddress {
  bool operator()(const T& left, const T& right) const {
    if (left.BusNumber != right.BusNumber) {
        return (left.BusNumber < right.BusNumber);
    }
    if (left.DeviceNumber != right.DeviceNumber) {
        return (left.DeviceNumber < right.DeviceNumber);
    }
    return left.FunctionNumber < right.FunctionNumber;
  }
};

#if defined(__gnu_linux__)

inline uint64_t GetTime(clockid_t id) {
  timespec ts{0};
  int status = clock_gettime(id, &ts);
  PTI_ASSERT(status == 0);
  return ts.tv_sec * NSEC_IN_SEC + ts.tv_nsec;
}

inline uint64_t ConvertClockMonotonicToRaw(uint64_t clock_monotonic) {
  uint64_t raw = GetTime(CLOCK_MONOTONIC_RAW);
  uint64_t monotonic = GetTime(CLOCK_MONOTONIC);
  return (raw > monotonic) ?
    clock_monotonic + (raw - monotonic) :
    clock_monotonic - (monotonic - raw);
}

#endif

inline std::string GetFilePath(const std::string& filename) {
  PTI_ASSERT(!filename.empty());

  size_t pos = filename.find_last_of("/\\");
  if (pos == std::string::npos) {
    return "";
  }

  return filename.substr(0, pos + 1);
}

inline std::string GetExecutablePath() {
  char buffer[MAX_STR_SIZE] = { 0 };
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
  char buffer[MAX_STR_SIZE] = { 0 };
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
  stream.read(reinterpret_cast<char *>(binary.data()), size);
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
  return (uint32_t)GetCurrentThreadId();
#else
#ifdef SYS_gettid
  return (uint32_t)syscall(SYS_gettid);
#else
  #error "SYS_gettid is unavailable on this system"
#endif
#endif
}

inline uint64_t GetSystemTime() {
#if defined(_WIN32)
  LARGE_INTEGER ticks{0};
  LARGE_INTEGER frequency{0};
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

} // namespace utils

#endif // PTI_UTILS_UTILS_H_
