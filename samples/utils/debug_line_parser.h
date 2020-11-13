//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_DEBUG_LINE_PARSER_H_
#define PTI_SAMPLES_UTILS_DEBUG_LINE_PARSER_H_

#include <string.h>

#include <string>
#include <vector>

#include "dwarf_state_machine.h"

struct FileInfo {
  std::string name;
  uint32_t path_index;
};

class DebugLineParser {
 public:
  DebugLineParser(const uint8_t* data, uint32_t size) :
      data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Dwarf32LineNumberProgramHeader)) {
      return false;
    }

    const Dwarf32LineNumberProgramHeader* header =
      reinterpret_cast<const Dwarf32LineNumberProgramHeader*>(data_);
    if (header->version != DWARF_VERSION) {
      return false;
    }

    return true;
  }

  std::vector<FileInfo> GetFileList() const {
    if (!IsValid()) {
      return std::vector<FileInfo>();
    }
    std::vector<FileInfo> file_list;
    ProcessHeader(&file_list, nullptr);
    return file_list;
  }

  std::vector<std::string> GetDirList() const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }
    std::vector<std::string> dir_list;
    ProcessHeader(nullptr, &dir_list);
    return dir_list;
  }

  std::vector<LineInfo> GetLineInfo() const {
    if (!IsValid()) {
      return std::vector<LineInfo>();
    }

    const uint8_t* ptr = ProcessHeader(nullptr, nullptr);
    PTI_ASSERT(ptr < data_ + size_);
    const uint8_t* line_number_program = ptr;
    PTI_ASSERT(data_ + size_ - line_number_program <
        (std::numeric_limits<uint32_t>::max)());
    uint32_t line_number_program_size =
      static_cast<uint32_t>(data_ + size_ - line_number_program);
    std::vector<LineInfo> line_info = DwarfStateMachine(
        line_number_program, line_number_program_size,
        reinterpret_cast<const Dwarf32LineNumberProgramHeader*>(data_)).Run();
    if (line_info.size() == 0) {
      return std::vector<LineInfo>();
    }

    return line_info;
  }

 private:
  const uint8_t* ProcessHeader(std::vector<FileInfo>* file_list,
                               std::vector<std::string>* dir_list) const {
    const uint8_t* ptr = data_;
    const Dwarf32LineNumberProgramHeader* header =
      reinterpret_cast<const Dwarf32LineNumberProgramHeader*>(ptr);

    ptr += sizeof(Dwarf32LineNumberProgramHeader);

    // standard_opcode_lengths
    for (uint8_t i = 1; i < header->opcode_base; ++i) {
      uint32_t value = 0;
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, value, done);
      PTI_ASSERT(done);
    }

    // include_directories
    while (*ptr != 0) {
      const char* include_directory = reinterpret_cast<const char*>(ptr);
      if (dir_list != nullptr) {
        dir_list->push_back(include_directory);
      }
      ptr += strlen(include_directory) + 1;
    }
    ++ptr;

    // file_names
    PTI_ASSERT(*ptr != 0);
    while (*ptr != 0) {
      std::string file_name(reinterpret_cast<const char*>(ptr));
      ptr += file_name.size() + 1;

      bool done = false;
      uint32_t directory_index = 0;
      ptr = utils::leb128::Decode32(ptr, directory_index, done);
      PTI_ASSERT(done);

      uint32_t time = 0;
      ptr = utils::leb128::Decode32(ptr, time, done);
      PTI_ASSERT(done);

      uint32_t size = 0;
      ptr = utils::leb128::Decode32(ptr, size, done);
      PTI_ASSERT(done);

      if (file_list != nullptr) {
        file_list->push_back({file_name, directory_index});
      }
    }

    ++ptr;
    return ptr;
  }

  const uint8_t* data_;
  uint32_t size_;
};

#endif // PTI_SAMPLES_UTILS_DEBUG_LINE_PARSER_H_