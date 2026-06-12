//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_RAW_DATA_FILE_H_
#define PTI_PC_SAMPLING_RAW_DATA_FILE_H_

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "utils/pti_filesystem.h"
#include "utils/utils.h"

namespace pti::pc_sampling {

class TempRawDataFile {
 public:
  TempRawDataFile() = default;
  ~TempRawDataFile() { Reset(); }

  TempRawDataFile(const TempRawDataFile&) = delete;
  TempRawDataFile& operator=(const TempRawDataFile&) = delete;

  TempRawDataFile(TempRawDataFile&& other) noexcept
      : stream_(std::move(other.stream_)), path_(std::move(other.path_)), size_(other.size_) {
    other.path_.clear();
    other.size_ = 0;
  }

  TempRawDataFile& operator=(TempRawDataFile&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    Reset();
    stream_ = std::move(other.stream_);
    path_ = std::move(other.path_);
    size_ = other.size_;
    other.path_.clear();
    other.size_ = 0;
    return *this;
  }

  bool OpenTemp() {
    Reset();
    path_ = CreateRawDataPath();
    if (path_.empty()) {
      SPDLOG_ERROR("{}: failed to create a unique raw data file path", __FUNCTION__);
      return false;
    }

    stream_.open(path_, std::ios::binary | std::ios::trunc);
    if (!stream_.is_open()) {
      SPDLOG_ERROR("{}: failed to open raw data file {}", __FUNCTION__, path_);
      Reset();
      return false;
    }

    SPDLOG_INFO("{}: created raw data file {}", __FUNCTION__, path_);

    return true;
  }

  bool Append(const uint8_t* data, size_t data_size) {
    if (!stream_.is_open() || (data == nullptr && data_size != 0)) {
      return false;
    }

    if (data_size == 0) {
      return true;
    }

    stream_.write(reinterpret_cast<const char*>(data), data_size);
    if (!stream_) {
      stream_.clear();
      return false;
    }

    size_ += data_size;
    return true;
  }

  bool Finalize() {
    if (!stream_.is_open()) {
      return path_.empty() || pti::utils::filesystem::exists(path_);
    }

    stream_.flush();
    const bool flushed = static_cast<bool>(stream_);
    if (!flushed) {
      stream_.clear();
    }
    stream_.close();
    return flushed;
  }

  bool ReadAll(std::vector<uint8_t>* raw_data) const {
    if (raw_data == nullptr) {
      return false;
    }

    raw_data->clear();
    if (path_.empty() || size_ == 0) {
      return true;
    }

    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
      SPDLOG_ERROR("{}: failed to open raw data file {}", __FUNCTION__, path_);
      return false;
    }

    raw_data->resize(size_);
    input.read(reinterpret_cast<char*>(raw_data->data()), size_);
    if (!input || static_cast<size_t>(input.gcount()) != size_) {
      SPDLOG_ERROR("{}: failed to read {} bytes from {}", __FUNCTION__, size_, path_);
      raw_data->clear();
      return false;
    }

    return true;
  }

  void Reset() {
    if (stream_.is_open()) {
      stream_.close();
    }

    if (!path_.empty()) {
      std::error_code error;
      const std::string path = path_;
      const bool removed = pti::utils::filesystem::remove(path_, error);
      if (error) {
        SPDLOG_WARN("{}: failed to delete raw data file {}: {}", __FUNCTION__, path,
                    error.message());
      } else if (removed) {
        SPDLOG_INFO("{}: deleted raw data file {}", __FUNCTION__, path);
      } else {
        SPDLOG_DEBUG("{}: raw data file {} was already absent during reset", __FUNCTION__, path);
      }
      path_.clear();
    }

    size_ = 0;
  }

  bool IsOpen() const { return stream_.is_open(); }
  bool HasPath() const { return !path_.empty(); }
  const std::string& path() const { return path_; }
  size_t size() const { return size_; }

 private:
  static std::string CreateRawDataPath() {
    std::random_device rand_dev;
    std::mt19937_64 prng(rand_dev());
    std::uniform_int_distribution<uint64_t> rand_num;
    const auto temp_dir = pti::utils::filesystem::temp_directory_path();
    const uint32_t pid = ::utils::GetPid();
    const uint32_t tid = ::utils::GetTid();

    for (size_t attempt = 0; attempt < 16; ++attempt) {
      const auto candidate =
          temp_dir / ("pti_pc_sampling_" + std::to_string(pid) + "_" + std::to_string(tid) + "_" +
                      std::to_string(rand_num(prng)) + ".bin");
      if (!pti::utils::filesystem::exists(candidate)) {
        return candidate.string();
      }

      SPDLOG_WARN("{}: duplicate raw data filename candidate {}, retrying", __FUNCTION__,
                  candidate.string());
    }

    SPDLOG_ERROR("{}: exhausted attempts while creating a raw data filename in {}", __FUNCTION__,
                 temp_dir.string());
    return {};
  }

  std::ofstream stream_;
  std::string path_;
  size_t size_ = 0;
};

}  // namespace pti::pc_sampling

#endif  // PTI_PC_SAMPLING_RAW_DATA_FILE_H_
