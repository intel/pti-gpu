//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "dwarf_state_machine.hpp"

#include "elf_parser_def.hpp"

using namespace elf_parser;

#ifdef DEBUG_PRINT
#include <iomanip>
#include <iostream>
void printBin(const uint8_t* data, uint32_t size) {
  int inRow = 16;
  int rows = 32;
  for (uint32_t i = 0; i < size; i++) {
    if (i % inRow == 0) {
      if (i) {
        std::cout << " | ";
        for (unsigned int j = 0; j < inRow; j++)
          std::cout << ((data[i - inRow + j] > 32 && data[i - inRow + j] < 127)
                            ? data[i - inRow + j]
                            : uint8_t('.'))
                    << " ";
      }
      std::cout << "\n";
      if (i % (inRow * rows) == 0) {
        std::cout << "     ";
        for (unsigned int j = 0; j < inRow; j++)
          std::cout << std::hex << std::setfill(' ') << std::setw(3) << j << std::dec;
        std::cout << "\n";
      }
      std::cout << std::hex << std::setfill('0') << std::setw(4) << i << ": " << std::dec;
    }
    std::cout << std::hex << std::setfill('0') << std::setw(2) << uint32_t(data[i]) << " "
              << std::dec;
  }
  std::cout << std::endl;
};
#endif  // DEBUG_PRINT

DwarfStateMachine::DwarfStateMachine(const uint8_t* data, uint64_t size, uint32_t address_width,
                                     const DwarfLineNumberProgramHeader& header,
                                     std::vector<PathFilename>& source_files, uint64_t offset)
    : data_(data),
      size_(size),
      address_width_(address_width),
      header_(header),
      source_files_(source_files),
      offset_(offset) {
  PTI_ASSERT(data_ != nullptr);
  PTI_ASSERT(size_ > 0);
  PTI_ASSERT(address_width_ == 32 || address_width_ == 64);

  state_.is_stmt = header_.default_is_stmt;
}

std::vector<SourceMapping> DwarfStateMachine::Run() {
#ifdef DEBUG_PRINT
  std::cout << "header = " << std::hex << header_.header_length_from_beginning << std::endl;
  printBin(data_ + header_.header_length_from_beginning - 16, 1024);
#endif  // DEBUG_PRINT

  const uint8_t* ptr = data_ + header_.header_length_from_beginning;
  const uint8_t* end = data_ + size_;

  while (ptr < end) {
#ifdef DEBUG_PRINT
    std::cout << "  [0x" << std::setfill('0') << std::setw(8) << std::hex << (ptr - data_ + offset_)
              << "]  " << std::dec;
#endif  // DEBUG_PRINT
    if (*ptr == 0) {
      ptr = RunExtended(ptr);
    } else if (*ptr < header_.opcode_base) {
      ptr = RunStandard(ptr);
    } else {
      ptr = RunSpecial(ptr);
    }
#ifdef DEBUG_PRINT
    std::cout << std::endl;
#endif  // DEBUG_PRINT
  }

  return line_info_;
}

const uint8_t* DwarfStateMachine::RunSpecial(const uint8_t* ptr) {
  PTI_ASSERT(*ptr >= header_.opcode_base);

  uint8_t adjusted_opcode = (*ptr) - header_.opcode_base;
  uint8_t operation_advance = adjusted_opcode / header_.line_range;
  auto line_increment = /*1*/ UpdateLine(adjusted_opcode);
  /*2.1*/ UpdateOperation(operation_advance);
  auto address_increment = /*2.2*/ UpdateAddress(operation_advance);
  /*3*/ UpdateLineInfo();
  /*4*/  // set basic_block to false
  /*5*/  // set prologue_end to false
  /*6*/  // set epilogue_begin to false
  /*7*/ state_.discriminator = 0;

#ifdef DEBUG_PRINT
  std::cout << "Special opcode " << std::dec << (uint32_t)(adjusted_opcode)
            << ": advance Address by " << address_increment << " to 0x" << std::hex
            << state_.address << " and Line by " << std::dec << line_increment << " to "
            << state_.line;
#endif  // DEBUG_PRINT

  ++ptr;
  PTI_ASSERT(ptr < data_ + size_);
  return ptr;
}

const uint8_t* DwarfStateMachine::RunStandard(const uint8_t* ptr) {
  uint16_t opcode = *ptr;
  ++ptr;

  PTI_ASSERT(opcode < header_.opcode_base);
  PTI_ASSERT(ptr < data_ + size_);

  switch (opcode) {
    case DW_LNS_COPY: {
      UpdateLineInfo();
      state_.discriminator = 0;
      // set basic_block to false
      // set prologue_end to false
      // set epilogue_begin to false
#ifdef DEBUG_PRINT
      std::cout << "Copy";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_ADVANCE_PC: {
      uint32_t operation_advance = 0;
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, operation_advance, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);
      UpdateAddress(operation_advance);
      UpdateOperation(operation_advance);
#ifdef DEBUG_PRINT
      std::cout << "Advance PC by " << operation_advance << " to 0x" << std::hex << (state_.address)
                << std::dec;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_ADVANCE_LINE: {
      int32_t advance_line = 0;
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, advance_line, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);
      state_.line += advance_line;
#ifdef DEBUG_PRINT
      std::cout << "Advance Line by " << advance_line << " to " << std::dec << state_.line;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_FILE: {
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, state_.file, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);
#ifdef DEBUG_PRINT
      std::cout << "Set File Name to entry " << state_.file << " in the File Name Table";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_COLUMN: {
      uint32_t column = 0;
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, column, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);
      state_.column = column;
#ifdef DEBUG_PRINT
      std::cout << "Set column to " << column;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_NEGATE_STMT: {
      state_.is_stmt = !state_.is_stmt;
#ifdef DEBUG_PRINT
      std::cout << "Set is_stmt to " << state_.is_stmt;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_BASIC_BLOCK: {
      // Set basic_block to true
#ifdef DEBUG_PRINT
      std::cout << "DW_LNS_SET_BASIC_BLOCK";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_CONST_ADD_PC: {
      uint8_t adjusted_opcode = 255 - header_.opcode_base;
      uint8_t operation_advance = adjusted_opcode / header_.line_range;
      auto advance = UpdateAddress(operation_advance);
      UpdateOperation(operation_advance);
#ifdef DEBUG_PRINT
      std::cout << "Advance PC by constant " << advance << " to 0x" << std::hex << (state_.address)
                << std::dec;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_FIXED_ADVANCE_PC: {
      uint16_t advance = *((uint16_t*)ptr);
      ptr += sizeof(uint16_t);
      PTI_ASSERT(ptr < data_ + size_);
      state_.address += advance;
      state_.operation = 0;
#ifdef DEBUG_PRINT
      std::cout << "DW_LNS_FIXED_ADVANCE_PC";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_ISA: {
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, state_.isa, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);
#ifdef DEBUG_PRINT
      std::cout << "DW_LNS_SET_ISA";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_PROLOGUE_END: {
#ifdef DEBUG_PRINT
      std::cout << "Set prologue_end to true";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNS_SET_PROLOGUE_BEGIN: {
#ifdef DEBUG_PRINT
      std::cout << "DW_LNS_SET_PROLOGUE_BEGIN";
#endif  // DEBUG_PRINT
      break;
    }
    default: {
#ifdef DEBUG_PRINT
      std::cout << "Unknown opcode = " << (uint32_t)(opcode) << std::endl;
#endif  // DEBUG_PRINT
      PTI_ASSERT(0);
      break;
    }
  }

  return ptr;
}

const uint8_t* DwarfStateMachine::RunExtended(const uint8_t* ptr) {
  PTI_ASSERT(*ptr == 0);
  ++ptr;
  PTI_ASSERT(ptr < data_ + size_);

  uint8_t size = *ptr;
  PTI_ASSERT(size > 0);
  ++ptr;
  PTI_ASSERT(ptr < data_ + size_);

  uint8_t opcode = *ptr;
  ++ptr;
  PTI_ASSERT(ptr <= data_ + size_);

  switch (opcode) {
    case DW_LNS_END_SEQUENCE: {
      UpdateLineInfo();
      ptr = data_ + size_;
#ifdef DEBUG_PRINT
      std::cout << "Extended opcode 1: End of Sequence";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNE_SET_ADDRESS: {
      if (this->address_width_ == 64) {
        uint64_t address = *((const uint64_t*)ptr);
        PTI_ASSERT(size - 1 == sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        PTI_ASSERT(ptr < data_ + size_);
        state_.address = address;
      } else if (this->address_width_ == 32) {
        uint32_t address = *((const uint32_t*)ptr);
        PTI_ASSERT(size - 1 == sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        PTI_ASSERT(ptr < data_ + size_);
        state_.address = address;
      }
#ifdef DEBUG_PRINT
      std::cout << "Extended opcode 2: set Address to 0x" << std::hex << state_.address;
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNE_DEFINE_FILE: {
      PTI_ASSERT(0);
#ifdef DEBUG_PRINT
      std::cout << "DW_LNE_DEFINE_FILE";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNE_SET_DISCRIMINATOR: {
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, state_.discriminator, done);
      PTI_ASSERT(done);
#ifdef DEBUG_PRINT
      std::cout << "Extended opcode DW_LNE_SET_DISCRIMINATOR";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNE_LO_USER: {
#ifdef DEBUG_PRINT
      std::cout << "Extended opcode DW_LNE_LO_USER";
#endif  // DEBUG_PRINT
      break;
    }
    case DW_LNE_HI_USER: {
#ifdef DEBUG_PRINT
      std::cout << "Extended opcode DW_LNE_HI_USER";
#endif  // DEBUG_PRINT
      break;
    } break;
    default: {
#ifdef DEBUG_PRINT
      std::cout << "Unknown opcode = " << (uint32_t)opcode << std::endl;
#endif  // DEBUG_PRINT
      PTI_ASSERT(0);
      break;
    }
  }

  return ptr;
}

uint32_t DwarfStateMachine::UpdateAddress(uint32_t operation_advance) {
  uint32_t advance =
      header_.minimum_instruction_length *
      ((state_.operation + operation_advance) / header_.maximum_operations_per_instruction);
  state_.address += advance;
  return advance;
}

int32_t DwarfStateMachine::UpdateOperation(uint32_t operation_advance) {
  uint32_t advance =
      (state_.operation + operation_advance) % header_.maximum_operations_per_instruction;
  state_.operation = advance;
  return advance;
}

int32_t DwarfStateMachine::UpdateLine(uint32_t adjusted_opcode) {
  uint32_t advance = header_.line_base + (adjusted_opcode % header_.line_range);
  state_.line += advance;
  return advance;
}

void DwarfStateMachine::UpdateLineInfo() {
  line_info_.push_back({/*file_id*/ state_.file,
                        /*file_path*/ source_files_[state_.file].first,
                        /*file_name*/ source_files_[state_.file].second,
                        /*address*/ state_.address,
                        /*line*/ state_.line,
                        /*column*/ state_.column});
}
