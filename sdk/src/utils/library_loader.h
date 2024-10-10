// ==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UTILS_LIBRARY_LOADER_H_
#define UTILS_LIBRARY_LOADER_H_

#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class LibraryLoader {
 public:
  using SymHandle = void*;
#if defined(_WIN32)
  using Handle = HMODULE;
#else
  using Handle = void*;
#endif
  LibraryLoader() {}

  explicit LibraryLoader(const std::string& lib_name) {
#if defined(_WIN32)
    if (!SetDllDirectoryA("")) {
      throw std::system_error(GetLastError(), std::system_category());
    }
    handle_ = LoadLibraryExA(static_cast<LPCSTR>(lib_name.c_str()), nullptr,
                             static_cast<DWORD>(0x000000000));
#else
    handle_ = dlopen(lib_name.c_str(), RTLD_NOW);
#endif
    if (!handle_) {
#if defined(_WIN32)
      throw std::system_error(GetLastError(), std::system_category());
#else
      throw std::runtime_error(dlerror());
#endif
    }
  }

  LibraryLoader(const LibraryLoader&) = delete;
  LibraryLoader(LibraryLoader&& other) { std::swap(other.handle_, handle_); }
  LibraryLoader& operator=(const LibraryLoader&) = delete;
  LibraryLoader& operator=(LibraryLoader&& other) {
    if (this != &other) {
      std::swap(other.handle_, handle_);
    }
    return *this;
  }

  template <typename T>
  [[nodiscard]] T GetSymbol(const char* sym_name) {
    static_assert(std::is_pointer<T>::value);
#if defined(_WIN32)
    auto sym_addr = GetProcAddress(handle_, sym_name);
#else
    auto sym_addr = dlsym(handle_, sym_name);
#endif
    return reinterpret_cast<T>(reinterpret_cast<SymHandle>(sym_addr));
  }

  virtual ~LibraryLoader() {
    if (handle_) {
#if defined(_WIN32)
      [[maybe_unused]] auto result = FreeLibrary(handle_);
#else
      [[maybe_unused]] auto result = dlclose(handle_);
#endif
    }
  }

 private:
  Handle handle_ = nullptr;
};

#endif
