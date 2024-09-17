//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file dwarf_state_machine.hpp
 * @brief Interface for Dwarf line program state machine.
 *
 * The Dwarf line program state machine is used to extract information such as addresses, file
 * names, line numbers, and column numbers from the line number program section of an ELF file. It
 * provides methods to run and retrieve the parsed line number information.
 */

#ifndef PTI_LINE_NUMBER_PARSER_H_
#define PTI_LINE_NUMBER_PARSER_H_

#include "elf_parser_def.hpp"
#include "elf_parser_mapping.h"
#include "leb128.h"
#include "pti_assert.h"

/// Define print should be used to compare with reference output
// #define DEBUG_PRINT

namespace elf_parser {

struct DwarfState {
  uint64_t address = 0;
  uint32_t operation = 0;
  uint32_t file = 1;
  uint32_t line = 1;
  uint32_t column = 0;
  uint32_t discriminator = 0;
  uint32_t isa = 0;
  bool is_stmt = false;
};

class DwarfStateMachine {
 public:
  DwarfStateMachine(const uint8_t* data, uint64_t size, uint32_t address_width,
                    const DwarfLineNumberProgramHeader& header,
                    std::vector<PathFilename>& source_files, uint64_t offset = 0);

  std::vector<SourceMapping> Run();

 private:
  const uint8_t* RunSpecial(const uint8_t* ptr);
  const uint8_t* RunStandard(const uint8_t* ptr);
  const uint8_t* RunExtended(const uint8_t* ptr);

  uint32_t UpdateAddress(uint32_t operation_advance);
  int32_t UpdateOperation(uint32_t operation_advance);
  int32_t UpdateLine(uint32_t adjusted_opcode);
  void UpdateLineInfo();

 private:
  const uint8_t* data_;
  const uint32_t size_;
  const uint32_t address_width_;
  const DwarfLineNumberProgramHeader& header_;
  std::vector<PathFilename>& source_files_;
  std::vector<SourceMapping> line_info_;
  uint32_t offset_ = 0;

  DwarfState state_;
};

}  // namespace elf_parser

#endif  // PTI_LINE_NUMBER_PARSER_H_