//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_METRIC_STORAGE_H_
#define PTI_TOOLS_ONEPROF_METRIC_STORAGE_H_

#include <cstring>
#include <fstream>
#include <vector>

#include "utils.h"

#define CACHE_SIZE 134217728

struct CacheBuffer {
  std::vector<uint8_t> buffer = std::vector<uint8_t>(CACHE_SIZE);
  size_t used_size = 0;
};

class MetricStorage {
 public:
  MetricStorage(
      uint32_t count,
      const std::string& ext,
      const std::string& raw_data_path) {
    for (uint32_t i = 0; i < count; ++i) {
      std::string filename =
        std::string("data.") + std::to_string(utils::GetPid()) +
        "." + std::to_string(i) + "." + ext;
      if (!raw_data_path.empty()) {
        filename = raw_data_path + "/" + filename;
      }
      storage_.emplace(
          storage_.end(),
          std::ofstream(filename, std::ios::out | std::ios::binary));
      PTI_ASSERT(storage_.back().is_open());

      cache_.emplace_back(CacheBuffer());
      PTI_ASSERT(!cache_.back().buffer.empty());
    }
  }

  ~MetricStorage() {
    PTI_ASSERT(storage_.size() == cache_.size());

    for (size_t i = 0; i < cache_.size(); ++i) {
      if (cache_[i].used_size > 0) {
        const char* buffer = reinterpret_cast<const char*>(
            cache_[i].buffer.data());
        storage_[i].write(
            buffer, cache_[i].used_size * sizeof(uint8_t));
      }
    }

    for (auto& storage : storage_) {
      storage.close();
    }
  }

  void Dump(const uint8_t* data, uint32_t size, uint32_t storage_id) {
    PTI_ASSERT(data != nullptr);
    PTI_ASSERT(size > 0);
    PTI_ASSERT(storage_id < storage_.size());
    PTI_ASSERT(storage_id < cache_.size());

    if (cache_[storage_id].used_size + size >
        cache_[storage_id].buffer.size()) {
      const char* buffer = reinterpret_cast<const char*>(
          cache_[storage_id].buffer.data());
      storage_[storage_id].write(
          buffer, cache_[storage_id].used_size * sizeof(uint8_t));
      cache_[storage_id].used_size = 0;
    }

    PTI_ASSERT(cache_[storage_id].used_size + size <=
               cache_[storage_id].buffer.size());
    memcpy(
        cache_[storage_id].buffer.data() + cache_[storage_id].used_size,
        data, size * sizeof(uint8_t));
    cache_[storage_id].used_size += size;
  }

 private:
  std::vector<std::ofstream> storage_;
  std::vector<CacheBuffer> cache_;
};

class MetricReader {
 public:
  MetricReader(
      uint32_t count,
      const std::string& ext,
      const std::string& raw_data_path) {
    for (uint32_t i = 0; i < count; ++i) {
      std::string filename =
        std::string("data.") + std::to_string(utils::GetPid()) +
        "." + std::to_string(i) + "." + ext;
      if (!raw_data_path.empty()) {
        filename = raw_data_path + "/" + filename;
      }
      storage_.emplace(
          storage_.end(),
          std::ifstream(filename, std::ios::in | std::ios::binary));
      PTI_ASSERT(storage_.back().is_open());
    }
  }

  void Reset() {
    for (auto& storage : storage_) {
      storage.clear();
      storage.seekg(0, storage.beg);
    }
  }

  uint8_t* ReadChunk(uint32_t& size, uint32_t storage_id) {
    PTI_ASSERT(storage_id < storage_.size());
    if (storage_[storage_id].peek() == std::ifstream::traits_type::eof()) {
      size = 0;
      return nullptr;
    }

    uint8_t* data = new uint8_t[size];
    PTI_ASSERT(data != nullptr);

    storage_[storage_id].read(reinterpret_cast<char*>(data), size);
    if (storage_[storage_id].gcount() < size) {
      size = storage_[storage_id].gcount();
    }

    return data;
  }

  ~MetricReader() {
    for (auto& storage : storage_) {
      storage.close();
    }
  }

 private:
  std::vector<std::ifstream> storage_;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_STORAGE_H_