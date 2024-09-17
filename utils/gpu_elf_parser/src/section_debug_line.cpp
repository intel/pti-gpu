//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "section_debug_line.hpp"

#include <string.h>

#include <cstddef>  // offsetof
#include <cstdint>
#include <limits>
#include <vector>

#include "dwarf_state_machine.hpp"
#include "leb128.h"

using namespace elf_parser;

DwarfDebugLineParser::DwarfDebugLineParser(const uint8_t* data, uint64_t offset, uint64_t size,
                                           uint32_t address_width)
    : data_(data + offset), size_(size), offset_(offset), address_width_(address_width) {
  if (data_ == nullptr || size_ < sizeof(uint32_t)) {
    return;
  }

  header_.unit_length_from_beginning = (reinterpret_cast<const uint32_t*>(data_))[0];
  if (header_.unit_length_from_beginning > 0xfffffff0) {
    if (header_.unit_length_from_beginning != 0xffffffff ||
        size_ < (sizeof(uint32_t) /*0xffffffff*/ + sizeof(uint64_t) /*unit_length*/ +
                 sizeof(uint16_t) /*version*/)) {
      return;
    }
    header_.bitness = 64;
    header_.unit_length_from_beginning =
        *(reinterpret_cast<const uint64_t*>(data + sizeof(uint32_t))) + sizeof(uint64_t);
  } else {
    header_.bitness = 32;
    header_.unit_length_from_beginning +=
        sizeof(uint32_t);  // length field not included sizeof itself
  }

  if (header_.unit_length_from_beginning > size_) {
    return;
  }

  is_valid_ = true;
}

std::vector<SourceMapping> DwarfDebugLineParser::GetMapping(const char* comp_dir) {
  if (!IsValid()) {
    return std::vector<SourceMapping>();
  }

  if (comp_dir == nullptr) {
    return std::vector<SourceMapping>();
  }

  if (!is_header_processed_) {
    switch (header_.bitness) {
      case 32:
        ProcessHeader<DwarfLineNumberProgramHeader32>();
        break;
      case 64:
        ProcessHeader<DwarfLineNumberProgramHeader64>();
        break;
      default:
        PTI_ASSERT(0);
        return std::vector<SourceMapping>();
    }
    if (!is_header_processed_) {
      return std::vector<SourceMapping>();
    }
  }

  // get source dirs & files
  std::vector<PathFilename> source_files = this->GetSourceFiles(comp_dir);

  return DwarfStateMachine(data_, size_, this->address_width_, header_, source_files, this->offset_)
      .Run();
}

std::vector<PathFilename> DwarfDebugLineParser::GetSourceFiles(const char* comp_dir) {
  std::vector<const char*> dir_list = {comp_dir};
  const uint8_t* ptr = header_.include_directories_offset;

  const char* include_directory = reinterpret_cast<const char*>(ptr);
  while (include_directory[0] != 0) {
    dir_list.push_back(include_directory);
    include_directory += strlen(include_directory) + 1 /*NULL*/;
  }
  include_directory++;

  std::vector<PathFilename> source_files = {{nullptr, nullptr}};
  const uint8_t* file_data = reinterpret_cast<const uint8_t*>(include_directory);
  while (file_data[0] != 0) {
    const char* file_name = reinterpret_cast<const char*>(file_data);
    file_data += strlen(file_name) + 1 /*NULL*/;

    bool done = false;
    uint32_t directory_index = 0;
    file_data = utils::leb128::Decode32(file_data, directory_index, done);
    PTI_ASSERT(done);
    PTI_ASSERT(directory_index < dir_list.size() && directory_index >= 0);

    uint32_t time = 0;
    file_data = utils::leb128::Decode32(file_data, time, done);
    PTI_ASSERT(done);

    uint32_t size = 0;
    file_data = utils::leb128::Decode32(file_data, size, done);
    PTI_ASSERT(done);

    source_files.push_back({dir_list[directory_index], file_name});
  }

  return (source_files.size() > 1) ? source_files : std::vector<PathFilename>();
}

template <typename DwarfLineNumberProgramHeaderT>
void DwarfDebugLineParser::ProcessHeader() {
  if (size_ < sizeof(DwarfLineNumberProgramHeaderT)) {
    return;
  }

  const DwarfLineNumberProgramHeaderT* header =
      reinterpret_cast<const DwarfLineNumberProgramHeaderT*>(data_);

  if (header->version != DWARF_VERSION_DEBUG_LINE) {
    return;
  }

  header_.header_length_from_beginning = header->header_length +
                                         offsetof(DwarfLineNumberProgramHeaderT, header_length) +
                                         sizeof(DwarfLineNumberProgramHeaderT::header_length);
  header_.minimum_instruction_length = header->minimum_instruction_length;
  header_.maximum_operations_per_instruction = header->maximum_operations_per_instruction;
  header_.default_is_stmt = header->default_is_stmt;
  header_.line_base = header->line_base;
  header_.line_range = header->line_range;
  header_.opcode_base = header->opcode_base;

  header_.standard_opcode_lengths = data_ + sizeof(DwarfLineNumberProgramHeaderT);
  const uint8_t* ptr = header_.standard_opcode_lengths;

  for (uint32_t i = 1; i < header_.opcode_base; ++i) {  // opcodes from 1 to header_.opcode_base - 1
    bool done = false;
    uint32_t value = 0;
    ptr = utils::leb128::Decode32(ptr, value, done);
    PTI_ASSERT(done);
  }

  header_.include_directories_offset = ptr;

  is_header_processed_ = true;
  return;
}
