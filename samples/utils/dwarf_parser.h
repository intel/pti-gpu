//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_DWARF_PARSER_H_
#define PTI_SAMPLES_UTILS_DWARF_PARSER_H_

#include <string.h>

#include <string>
#include <vector>

#include "dwarf_state_machine.h"

struct LineInfo {
  int32_t address;
  uint32_t line;
};

class DwarfParser {
 public:
  DwarfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Dwarf32Header)) {
      return false;
    }

    const Dwarf32Header* header =
      reinterpret_cast<const Dwarf32Header*>(data_);
    if (header->version != DWARF_VERSION) {
      return false;
    }

    return true;
  }

  std::vector<std::string> GetFileNames() const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }
    std::vector<std::string> file_names;
    ProcessHeader(&file_names);
    return file_names;
  }

  std::vector<LineInfo> GetLineInfo(uint32_t file_id) const {
    PTI_ASSERT(file_id > 0);
    
    if (!IsValid()) {
      return std::vector<LineInfo>();
    }

    const uint8_t* ptr = ProcessHeader(nullptr);
    PTI_ASSERT(ptr < data_ + size_);
    const uint8_t* line_number_program = ptr;
    PTI_ASSERT(data_ + size_ - line_number_program <
        (std::numeric_limits<uint32_t>::max)());
    uint32_t line_number_program_size =
      static_cast<uint32_t>(data_ + size_ - line_number_program);
    std::vector<DwarfLineInfo> line_info =
      DwarfStateMachine(line_number_program, line_number_program_size,
                        reinterpret_cast<const Dwarf32Header*>(data_)).Run();
    if (line_info.size() == 0) {
      return std::vector<LineInfo>();    
    }

    return ProcessLineInfo(line_info, file_id);
  }

 private:
  const uint8_t* ProcessHeader(std::vector<std::string>* file_names) const {
    const uint8_t* ptr = data_;
    const Dwarf32Header* header = reinterpret_cast<const Dwarf32Header*>(ptr);

    ptr += sizeof(Dwarf32Header);

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

      if (file_names != nullptr) {
        file_names->push_back(file_name);
      }
    }

    ++ptr;
    return ptr;
  }

  std::vector<LineInfo> ProcessLineInfo(
      const std::vector<DwarfLineInfo>& line_info, uint32_t file) const {
    std::vector<LineInfo> result;

    int32_t address = 0;
    uint32_t line = 0;
    for (auto item : line_info) {
      PTI_ASSERT(address <= item.address);
      if (item.file != file) {
        continue;
      }
      if (item.line == line) {
        continue;
      }
      PTI_ASSERT(item.address < (std::numeric_limits<int>::max)());
      address = static_cast<int>(item.address);
      line = item.line;
      result.push_back({address, line});
    }
    
    return result;
}

  const uint8_t* data_;
  uint32_t size_;
};

#endif // PTI_SAMPLES_UTILS_DWARF_PARSER_H_