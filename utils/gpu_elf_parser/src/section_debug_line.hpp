//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file section_debug_line.hpp
 * @brief Contains the definition of the DwarfDebugLineParser class, which is responsible for
 * parsing the .debug_line section in an ELF file.
 */

#ifndef PTI_SECTION_DEBUG_LINE_H_
#define PTI_SECTION_DEBUG_LINE_H_

#include <vector>

#include "elf_parser_def.hpp"
#include "elf_parser_mapping.h"

namespace elf_parser {

class DwarfDebugLineParser {
 public:
  DwarfDebugLineParser(const uint8_t* data, uint64_t offset, uint64_t size, uint32_t address_width);

  inline bool IsValid() const { return is_valid_; }

  inline uint32_t GetBitness() { return is_valid_ ? header_.bitness : -1; }

  inline uint64_t GetUnitLength() { return is_valid_ ? header_.unit_length_from_beginning : -1; }

  std::vector<SourceMapping> GetMapping(const char* comp_dir);

 private:
  std::vector<PathFilename> GetSourceFiles(const char* comp_dir);

  template <typename DwarfLineNumberProgramHeaderT>
  void ProcessHeader();

  const uint8_t* data_;
  const uint64_t size_;
  const uint64_t offset_;
  const uint32_t address_width_;
  DwarfLineNumberProgramHeader header_ = {};
  bool is_valid_ = false;
  bool is_header_processed_ = false;
};

}  // namespace elf_parser

#endif  // PTI_SECTION_DEBUG_LINE_H_
