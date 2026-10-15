//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UNITRACE_TEST_UTILS_SHARED_LIBRARY_H_
#define UNITRACE_TEST_UTILS_SHARED_LIBRARY_H_

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <dlfcn.h>
#endif

#include <string>
#include <vector>

#include "utils/unitrace_assert.h"

class SharedLibrary {
 public:
  static SharedLibrary* Create(const std::string& name) {
#if defined(_WIN32)
    HMODULE handle = nullptr;
    handle = LoadLibraryA(name.c_str());
#else
    void* handle = nullptr;
    handle = dlopen(name.c_str(), RTLD_NOW);
#endif
    if (handle != nullptr) {
      return new SharedLibrary(handle);
    }
    return nullptr;
  }

  ~SharedLibrary() {
#if defined(_WIN32)
    BOOL completed = FreeLibrary(handle_);
    UNITRACE_ASSERT(completed == TRUE);
#else
    int completed = dlclose(handle_);
    UNITRACE_ASSERT(completed == 0);
#endif
  }

  template<typename T> T GetSym(const char* name) {
    void* sym = nullptr;
#if defined(_WIN32)
    sym = GetProcAddress(handle_, name);
#else
    sym = dlsym(handle_, name);
#endif
    return reinterpret_cast<T>(sym);
  }

#if defined(_WIN32)
  HMODULE GetHandle() {
#else
  void* GetHandle() {
#endif
    return handle_;
  }

 private:
#if defined(_WIN32)
  SharedLibrary(HMODULE handle) : handle_(handle) {}
#else
  SharedLibrary(void* handle) : handle_(handle) {}
#endif

#if defined(_WIN32)
  HMODULE handle_ = nullptr;
#else
  void* handle_ = nullptr;
#endif
};

#endif // UNITRACE_TEST_UTILS_SHARED_LIBRARY_H_