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

#define MAX_REPORT_SIZE  512
#define MAX_REPORT_COUNT 32768
#define MAX_BUFFER_SIZE  (MAX_REPORT_COUNT * MAX_REPORT_SIZE * 2)

#define CACHE_SIZE 134217728

struct CacheBuffer {
  std::vector<uint8_t> buffer = std::vector<uint8_t>(CACHE_SIZE);
  size_t used_size = 0;
};

class MetricStorage {
 public:
  static MetricStorage* Create(
      uint32_t count,
      uint32_t pid,
      const std::string& ext,
      const std::string& path) {
    PTI_ASSERT(!ext.empty());

    MetricStorage* storage = new MetricStorage(count, pid, ext, path);
    PTI_ASSERT(storage != nullptr);

    bool succeed = true;
    for (const auto& file : storage->storage_) {
      succeed = succeed && file.is_open();
    }

    if (!succeed) {
      delete storage;
      return nullptr;
    }

    return storage;
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
  MetricStorage(
      uint32_t count,
      uint32_t pid,
      const std::string& ext,
      const std::string& path) {
    std::vector<std::string> filename_list;
    if (count == 0) {
      std::string filename =
        std::string("data.") + std::to_string(pid) + "." + ext;
      if (!path.empty()) {
        filename = path + "/" + filename;
      }
      filename_list.push_back(filename);
    } else {
      for (uint32_t i = 0; i < count; ++i) {
        std::string filename =
          std::string("data.") + std::to_string(pid) + "." +
          std::to_string(i) + "." + ext;
        if (!path.empty()) {
          filename = path + "/" + filename;
        }
        filename_list.push_back(filename);
      }
    }

    for (const auto& filename : filename_list) {
      storage_.emplace(
          storage_.end(),
          std::ofstream(filename, std::ios::out | std::ios::binary));
      cache_.emplace_back(CacheBuffer());
      PTI_ASSERT(!cache_.back().buffer.empty());
    }
  }

 private:
  std::vector<std::ofstream> storage_;
  std::vector<CacheBuffer> cache_;
};

class MetricReader {
 public:
  static MetricReader* Create(
      uint32_t count,
      uint32_t pid,
      const std::string& ext,
      const std::string& path) {
    PTI_ASSERT(!ext.empty());

    MetricReader* reader = new MetricReader(count, pid, ext, path);
    PTI_ASSERT(reader != nullptr);

    bool succeed = true;
    for (const auto& file : reader->storage_) {
      succeed = succeed && file.is_open();
    }

    if (!succeed) {
      delete reader;
      return nullptr;
    }

    return reader;
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

  void Read(uint32_t storage_id, size_t start, size_t size, char* data) {
    PTI_ASSERT(storage_id < storage_.size());
    PTI_ASSERT(data != nullptr);

    storage_[storage_id].seekg(start);
    storage_[storage_id].read(data, size);
    PTI_ASSERT(storage_[storage_id].gcount() == size);
  }

  bool ReadNext(uint32_t storage_id, size_t size, char* data) {
    PTI_ASSERT(storage_id < storage_.size());
    PTI_ASSERT(data != nullptr);

    storage_[storage_id].read(data, size);
    if (storage_[storage_id].gcount() < size) {
      return false;
    }

    return true;
  }

  ~MetricReader() {
    for (auto& storage : storage_) {
      storage.close();
    }
  }

 private:
  MetricReader(
      uint32_t count,
      uint32_t pid,
      const std::string& ext,
      const std::string& path) {
    std::vector<std::string> filename_list;
    if (count == 0) {
      std::string filename =
        std::string("data.") + std::to_string(pid) + "." + ext;
      if (!path.empty()) {
        filename = path + "/" + filename;
      }
      filename_list.push_back(filename);
    } else {
      for (uint32_t i = 0; i < count; ++i) {
        std::string filename =
          std::string("data.") + std::to_string(pid) + "." +
          std::to_string(i) + "." + ext;
        if (!path.empty()) {
          filename = path + "/" + filename;
        }
        filename_list.push_back(filename);
      }
    }

    for (const auto& filename : filename_list) {
      storage_.emplace(
          storage_.end(),
          std::ifstream(filename, std::ios::in | std::ios::binary));
    }
  }

 private:
  std::vector<std::ifstream> storage_;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_STORAGE_H_
