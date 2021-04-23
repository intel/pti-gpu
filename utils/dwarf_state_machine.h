//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_DWARF_STATE_MACHINE_H_
#define PTI_UTILS_DWARF_STATE_MACHINE_H_

#include "dwarf.h"
#include "leb128.h"
#include "pti_assert.h"

struct LineInfo {
  uint64_t address;
  uint32_t file;
  uint32_t line;
};

struct DwarfState {
  uint64_t address;
  uint32_t operation;
  uint32_t line;
  uint32_t file;
};

class DwarfStateMachine {
 public:
  DwarfStateMachine(const uint8_t* data, uint32_t size,
                    const Dwarf32LineNumberProgramHeader* header)
      : data_(data), size_(size), header_(header) {
    PTI_ASSERT(data_ != nullptr);
    PTI_ASSERT(size_ > 0);
    PTI_ASSERT(header_ != nullptr);
  }

  std::vector<LineInfo> Run() {
    const uint8_t* ptr = data_;
    const uint8_t* end = ptr + size_;

    while (ptr < end) {
      if (*ptr == 0) {
        ptr = RunExtended(ptr);
      } else if (*ptr < header_->opcode_base) {
        ptr = RunStandard(ptr);
      } else {
        ptr = RunSpecial(ptr);
      }
    }

    return line_info_;
  }

 private:
  const uint8_t* RunSpecial(const uint8_t* ptr) {
    PTI_ASSERT(*ptr >= header_->opcode_base);

    uint8_t adjusted_opcode = (*ptr) - header_->opcode_base;
    uint8_t operation_advance = adjusted_opcode / header_->line_range;
    UpdateAddress(operation_advance);
    UpdateOperation(operation_advance);
    UpdateLine(adjusted_opcode);

    UpdateLineInfo();

    ++ptr;
    PTI_ASSERT(ptr < data_ + size_);
    return ptr;
  }

  const uint8_t* RunStandard(const uint8_t* ptr) {
    uint8_t opcode = *ptr;
    ++ptr;

    PTI_ASSERT(opcode < header_->opcode_base);
    PTI_ASSERT(ptr < data_ + size_);

    switch (opcode) {
      case DW_LNS_COPY: {
        UpdateLineInfo();
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
        break;
      }
      case DW_LNS_ADVANCE_LINE: {
        int32_t line = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, line, done);
        PTI_ASSERT(done);
        PTI_ASSERT(ptr < data_ + size_);
        state_.line += line;
        break;
      }
      case DW_LNS_SET_FILE: {
        uint32_t file = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, file, done);
        PTI_ASSERT(done);
        PTI_ASSERT(ptr < data_ + size_);
        state_.file = file;
        break;
      }
      case DW_LNS_SET_COLUMN: {
        uint32_t column = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, column, done);
        PTI_ASSERT(done);
        PTI_ASSERT(ptr < data_ + size_);
        break;
      }
      case DW_LNS_CONST_ADD_PC: {
        uint8_t adjusted_opcode = 255 - header_->opcode_base;
        uint8_t operation_advance = adjusted_opcode / header_->line_range;
        UpdateAddress(operation_advance);
        UpdateOperation(operation_advance);
        break;
      }
      case DW_LNS_FIXED_ADVANCE_PC: {
        uint16_t advance = *((uint16_t*)ptr);
        ptr += sizeof(uint16_t);
        PTI_ASSERT(ptr < data_ + size_);
        state_.address += advance;
        state_.operation = 0;
        break;
      }
      case DW_LNS_NEGATE_STMT:
      case DW_LNS_SET_BASIC_BLOCK:
      case DW_LNS_SET_PROLOGUE_END:
        break;
      default: {
        PTI_ASSERT(0); // Not supported
        break;
      }
    }

    return ptr;
  }

  const uint8_t* RunExtended(const uint8_t* ptr) {
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
        PTI_ASSERT(ptr == data_ + size_);
        UpdateLineInfo();
        break;
      }
      case DW_LNE_SET_ADDRESS: {
        uint64_t address = *((const uint64_t*)ptr);
        PTI_ASSERT(size - 1 == sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        PTI_ASSERT(ptr < data_ + size_);
        state_.address = address;
        break;
      }
      default: {
        PTI_ASSERT(0); // Not supported
        break;
      }
    }

    return ptr;
  }

  void UpdateAddress(uint32_t operation_advance) {
    state_.address += header_->minimum_instruction_length *
                      ((state_.operation + operation_advance) /
                      header_->maximum_operations_per_instruction);
  }

  void UpdateOperation(uint32_t operation_advance) {
    state_.operation = (state_.operation + operation_advance) %
                       header_->maximum_operations_per_instruction;
  }

  void UpdateLine(uint32_t adjusted_opcode) {
    state_.line += header_->line_base +
                   (adjusted_opcode % header_->line_range);
  }

  void UpdateLineInfo() {
    line_info_.push_back({state_.address, state_.file, state_.line});
  }

private:
  const uint8_t* data_ = nullptr;
  uint32_t size_ = 0;    
  const Dwarf32LineNumberProgramHeader* header_ = nullptr;

  DwarfState state_ = { 0, 0, 1, 1 };
  std::vector<LineInfo> line_info_;
};

#endif // PTI_UTILS_DWARF_STATE_MACHINE_H_