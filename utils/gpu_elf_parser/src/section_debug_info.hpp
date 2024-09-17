//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file section_debug_info.hpp
 * @brief Contains the definition of the DebugInfoParser class, which is responsible for
 * parsing the .debug_info section in an ELF file. It also provides the compilation directory.
 */

#ifndef PTI_SECTION_DEBUG_INFO_H_
#define PTI_SECTION_DEBUG_INFO_H_

#include "elf_parser_def.hpp"

namespace elf_parser {

class DebugInfoParser {  // parses one unit of debug_info section
 public:
  DebugInfoParser(const uint8_t* data, uint32_t size);

  inline bool IsValid() const { return is_valid_; }

  inline uint32_t GetBitness() const { return is_valid_ ? bitness_ : -1; }

  inline uint32_t GetUnitLength() const { return is_valid_ ? unit_length_from_beginning_ : -1; }

  inline uint64_t GetDebugAbbrevOffset() const { return is_valid_ ? debug_abbrev_offset_ : -1; }

  const char* GetCompDir(const DwarfCompUnitMap& comp_unit_map);

 private:
  template <typename DwarfCompUnitHeaderT>
  void ProcessDwarf4Header();
  template <typename DwarfCompUnitHeaderT>
  void ProcessDwarf5Header();

  const uint8_t* data_;
  uint32_t size_;
  uint32_t bitness_ = 32;
  uint32_t version_ = 0;
  uint32_t unit_length_from_beginning_ = 0;
  uint64_t debug_abbrev_offset_ = 0;
  uint8_t address_size_ = 0;
  uint64_t data_offset_ = 0;
  bool is_valid_ = false;
};

}  // namespace elf_parser

#endif  // PTI_SECTION_DEBUG_INFO_H_
