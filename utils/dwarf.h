//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_DWARF_H_
#define PTI_UTILS_DWARF_H_

#include <map>
#include <vector>

#include <stdint.h>

#define DWARF_VERSION 4

#define DW_LNS_COPY             0x01
#define DW_LNS_ADVANCE_PC       0x02
#define DW_LNS_ADVANCE_LINE     0x03
#define DW_LNS_SET_FILE         0x04
#define DW_LNS_SET_COLUMN       0x05
#define DW_LNS_NEGATE_STMT      0x06
#define DW_LNS_SET_BASIC_BLOCK  0x07
#define DW_LNS_CONST_ADD_PC     0x08
#define DW_LNS_FIXED_ADVANCE_PC 0x09
#define DW_LNS_SET_PROLOGUE_END 0x0A

#define DW_LNS_END_SEQUENCE     0x01
#define DW_LNE_SET_ADDRESS      0x02

#define DW_TAG_compile_unit     0x11

#define DW_AT_name              0x03
#define DW_AT_stmt_list         0x10
#define DW_AT_comp_dir          0x1b

#define DW_FORM_addr            0x01
#define DW_FORM_data2           0x05
#define DW_FORM_data4           0x06
#define DW_FORM_data8           0x07
#define DW_FORM_string          0x08
#define DW_FORM_data1           0x0b
#define DW_FORM_sec_offset      0x17

#pragma pack(push, 1)
struct Dwarf32LineNumberProgramHeader {
  uint32_t unit_length;
  uint16_t version;
  uint32_t header_length;
  uint8_t  minimum_instruction_length;
  uint8_t  maximum_operations_per_instruction;
  uint8_t  default_is_stmt;
  int8_t   line_base;
  uint8_t  line_range;
  uint8_t  opcode_base;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Dwarf32CompUnitHeader {
  uint32_t unit_length;
  uint16_t version;
  uint32_t debug_abbrev_offset;
  uint8_t  address_size;
};
#pragma pack(pop)

struct DwarfAttribute {
  uint32_t attribute;
  uint32_t form;
};

using DwarfCompUnitMap = std::map<uint32_t, std::vector<DwarfAttribute> >;

#endif // PTI_UTILS_DWARF_H_