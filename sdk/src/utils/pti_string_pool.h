// ==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_STRING_POOL_H_
#define PTI_STRING_POOL_H_

#include <shared_mutex>
#include <string>
#include <unordered_set>

/**
 * Thread-safe string pool for efficient string storage and deduplication.
 *
 * This class stores unique strings and provides stable pointers to them.
 * Multiple registrations of the same string return the same pointer.
 * All returned pointers remain valid for the lifetime of the StringPool.
 */
class StringPool {
 public:
  StringPool() = default;
  ~StringPool() = default;

  // Non-copyable, non-movable for pointer stability
  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;
  StringPool(StringPool&&) = delete;
  StringPool& operator=(StringPool&&) = delete;

  const char* Get(const std::string& str) {
    // Try with read lock - return immediately if string already exists
    {
      std::shared_lock lock(mutex_);
      auto it = strings_.find(str);
      if (it != strings_.end()) {
        return it->c_str();
      }
    }

    std::unique_lock lock(mutex_);

    auto [it, inserted] = strings_.emplace(str);
    return it->c_str();
  }

  // Get number of unique strings stored.
  size_t Size() const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    return strings_.size();
  }

  // Clear all stored strings.
  void Clear() {
    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    strings_.clear();
  }

  // Check if a string is registered.
  bool Contains(const std::string& str) const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    return strings_.find(str) != strings_.end();
  }

 private:
  // Map from string to pointer to that same string's internal storage
  // The pointer points to the key's c_str(), ensuring stability
  mutable std::shared_mutex mutex_;
  std::unordered_set<std::string> strings_;
};

#endif  // PTI_STRING_POOL_H_
