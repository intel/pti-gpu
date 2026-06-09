//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_LEVELZERO_ZE_KERNEL_NAME_CACHE_H_
#define SRC_LEVELZERO_ZE_KERNEL_NAME_CACHE_H_

#include <level_zero/ze_api.h>

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "utils/ze_utils.h"

/**
 * @brief Thread-safe cache for kernel names (mangled and demangled)
 *
 * Avoids repeated zeKernelGetName calls and demangling by caching both versions
 * per kernel. Uses shared_mutex for concurrent reads, exclusive writes.
 *
 * @tparam FetchFn Kernel name fetch function (default: utils::ze::GetKernelName)
 */
template <typename FetchFn = std::string (*)(ze_kernel_handle_t, bool)>
class ZeKernelNameCache {
 public:
  ZeKernelNameCache() : fetch_fn_(utils::ze::GetKernelName) {}
  explicit ZeKernelNameCache(FetchFn fetch) : fetch_fn_(fetch) {}

  ~ZeKernelNameCache() = default;

  ZeKernelNameCache(const ZeKernelNameCache&) = delete;
  ZeKernelNameCache& operator=(const ZeKernelNameCache&) = delete;
  ZeKernelNameCache(ZeKernelNameCache&&) = delete;
  ZeKernelNameCache& operator=(ZeKernelNameCache&&) = delete;

  /**
   * @brief Get kernel name from cache or fetch if not cached
   * @param kernel Kernel handle
   * @param demangle Return demangled name if true, mangled if false
   * @return Kernel name
   */
  std::string GetKernelName(ze_kernel_handle_t kernel, bool demangle) {
    // Fast path: shared lock for concurrent reads
    {
      std::shared_lock<std::shared_mutex> read_lock(mutex_);
      auto it = cache_.find(kernel);
      if (it != cache_.end()) {
        const KernelNames& names = it->second;
        if (demangle && names.demangled_name.has_value()) {
          return names.demangled_name.value();
        }
        if (!demangle && names.mangled_name.has_value()) {
          return names.mangled_name.value();
        }
      }
    }

    // Slow path: unique lock to fetch and cache
    std::unique_lock<std::shared_mutex> write_lock(mutex_);

    // Double-check pattern
    auto it = cache_.find(kernel);
    if (it != cache_.end()) {
      KernelNames& names = it->second;
      if (demangle && names.demangled_name.has_value()) {
        return names.demangled_name.value();
      }
      if (!demangle && names.mangled_name.has_value()) {
        return names.mangled_name.value();
      }

      std::string name = fetch_fn_(kernel, demangle);
      if (demangle) {
        names.demangled_name = name;
      } else {
        names.mangled_name = name;
      }
      return name;
    }

    // New kernel - fetch and create entry
    std::string name = fetch_fn_(kernel, demangle);
    KernelNames names;
    if (demangle) {
      names.demangled_name = name;
    } else {
      names.mangled_name = name;
    }
    cache_[kernel] = names;

    return name;
  }

  void RemoveKernel(ze_kernel_handle_t kernel) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_.erase(kernel);
  }

  void Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_.clear();
  }

 private:
  struct KernelNames {
    std::optional<std::string> mangled_name;
    std::optional<std::string> demangled_name;
  };

  std::unordered_map<ze_kernel_handle_t, KernelNames> cache_;
  mutable std::shared_mutex mutex_;
  FetchFn fetch_fn_;
};

#endif  // SRC_LEVELZERO_ZE_KERNEL_NAME_CACHE_H_
