// ==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UTILS_LIBRARY_LOADER_H_
#define UTILS_LIBRARY_LOADER_H_

#include <memory>
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

#if defined(__linux__)
  // The caller owns this handle and releases it when the recovery chain completes.
  static std::unique_ptr<LibraryLoader> Preload(const std::string& lib_name, std::string& error) {
    auto* const handle = dlopen(lib_name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
      const auto* const dl_error = dlerror();
      error = dl_error != nullptr ? dl_error : "unknown dlopen failure";
      return nullptr;
    }
    return std::unique_ptr<LibraryLoader>(new LibraryLoader(handle));
  }
#endif  // __linux__

  LibraryLoader(const LibraryLoader&) = delete;
  LibraryLoader(LibraryLoader&& other) noexcept { std::swap(other.handle_, handle_); }
  LibraryLoader& operator=(const LibraryLoader&) = delete;
  LibraryLoader& operator=(LibraryLoader&& other) noexcept {
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
    return reinterpret_cast<T>(reinterpret_cast<SymHandle>(sym_addr));  // NOLINT
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
#if defined(__linux__)
  explicit LibraryLoader(Handle handle) : handle_(handle) {}
#endif  // __linux__

  Handle handle_ = nullptr;
};

#endif
