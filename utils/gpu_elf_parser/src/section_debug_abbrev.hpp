//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SECTION_DEBUG_ABBREV_H_
#define PTI_SECTION_DEBUG_ABBREV_H_

#include "elf_parser_def.hpp"

namespace elf_parser {

class DebugAbbrevParser {
 public:
  DebugAbbrevParser(const uint8_t* data, uint32_t size);

  inline bool IsValid() const { return (data_ != nullptr && size_ != 0) ? true : false; }

  DwarfCompUnitMap GetCompUnitMap() const;

 private:
  const uint8_t* data_;
  uint32_t size_;
};

}  // namespace elf_parser

#endif  // PTI_SECTION_DEBUG_ABBREV_H_
