//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file elf_parser_def.hpp
 * @brief Definition file for ELF parser utility.
 */

#ifndef PTI_ELF_PARSER_DEF_H_
#define PTI_ELF_PARSER_DEF_H_

#include <stdint.h>

#include <map>
#include <vector>

namespace elf_parser {

#define ELF_MAGIC_NUMBER 0x7F

#define ELF_NIDENT 16

#define ELFDATA2LSB 1
#define ELFDATA2SMB 2

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ET_EXEC 2

#define EM_INTELGT 205

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4

#define MIN_INSTRUCTION_SIZE 8

#define DWARF_VERSION4 4
#define DWARF_VERSION5 5

#define DWARF_VERSION_DEBUG_LINE 4

#define DW_LNS_COPY 0x01
#define DW_LNS_ADVANCE_PC 0x02
#define DW_LNS_ADVANCE_LINE 0x03
#define DW_LNS_SET_FILE 0x04
#define DW_LNS_SET_COLUMN 0x05
#define DW_LNS_NEGATE_STMT 0x06
#define DW_LNS_SET_BASIC_BLOCK 0x07
#define DW_LNS_CONST_ADD_PC 0x08
#define DW_LNS_FIXED_ADVANCE_PC 0x09
#define DW_LNS_SET_PROLOGUE_END 0x0A
#define DW_LNS_SET_PROLOGUE_BEGIN 0x0B
#define DW_LNS_SET_ISA 0x0C

#define DW_LNS_END_SEQUENCE 0x01
#define DW_LNE_SET_ADDRESS 0x02
#define DW_LNE_DEFINE_FILE 0x03
#define DW_LNE_SET_DISCRIMINATOR 0x04
#define DW_LNE_LO_USER 0x80
#define DW_LNE_HI_USER 0xFF

#define DW_TAG_compile_unit 0x11

#define DW_AT_name 0x03
#define DW_AT_stmt_list 0x10
#define DW_AT_comp_dir 0x1b

#define DW_FORM_addr 0x01
#define DW_FORM_data2 0x05
#define DW_FORM_data4 0x06
#define DW_FORM_data8 0x07
#define DW_FORM_string 0x08
#define DW_FORM_data1 0x0b
#define DW_FORM_sec_offset 0x17

#define NT_INTELGT_GFXCORE_FAMILY \
  2  // https://github.com/intel/intel-graphics-compiler/blob/master/IGC/ZEBinWriter/zebin/spec/elf.md

#pragma pack(push, 1)
struct ElfHeaderIndent {
  char mag0;
  char mag1;
  char mag2;
  char mag3;
  char class_;
  char data;
  char version;
  char osabi;
  char abiversion;
  char pad[7];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ElfHeader64 {
  unsigned char ident[ELF_NIDENT];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ElfSectionHeader64 {
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  uint64_t addr;
  uint64_t offset;
  uint64_t size;
  uint32_t link;
  uint32_t info;
  uint64_t addralign;
  uint64_t entsize;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ElfHeader32 {
  unsigned char ident[ELF_NIDENT];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ElfSectionHeader32 {
  uint32_t name;
  uint32_t type;
  uint32_t flags;
  uint32_t addr;
  uint32_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t addralign;
  uint32_t entsize;
};
#pragma pack(pop)

using PathFilename = std::pair<const char*, const char*>;

/// @brief Dwarf debug line bitness-independent structure
struct DwarfLineNumberProgramHeader {
  uint32_t bitness;
  uint64_t unit_length_from_beginning;
  uint16_t version;
  uint64_t header_length_from_beginning;
  uint8_t minimum_instruction_length;
  uint8_t maximum_operations_per_instruction;
  uint8_t default_is_stmt;
  int8_t line_base;
  uint8_t line_range;
  uint8_t opcode_base;
  const uint8_t* standard_opcode_lengths;
  const uint8_t* include_directories_offset;
};

#pragma pack(push, 1)
struct DwarfLineNumberProgramHeader32 {
  uint32_t unit_length;
  uint16_t version;
  uint32_t header_length;
  uint8_t minimum_instruction_length;
  uint8_t maximum_operations_per_instruction;
  uint8_t default_is_stmt;
  int8_t line_base;
  uint8_t line_range;
  uint8_t opcode_base;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DwarfLineNumberProgramHeader64 {
  uint32_t _offset;
  uint64_t unit_length;
  uint16_t version;
  uint64_t header_length;
  uint8_t minimum_instruction_length;
  uint8_t maximum_operations_per_instruction;
  uint8_t default_is_stmt;
  int8_t line_base;
  uint8_t line_range;
  uint8_t opcode_base;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Dwarf4CompUnitHeader32 {
  uint32_t unit_length;
  uint16_t version;
  uint32_t debug_abbrev_offset;
  uint8_t address_size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Dwarf4CompUnitHeader64 {
  uint32_t _offset;
  uint64_t unit_length;
  uint16_t version;
  uint64_t debug_abbrev_offset;
  uint8_t address_size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Dwarf5CompUnitHeader32 {
  uint32_t unit_length;
  uint16_t version;
  uint8_t type;
  uint8_t address_size;
  uint32_t debug_abbrev_offset;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Dwarf5CompUnitHeader64 {
  uint32_t _offset;
  uint64_t unit_length;
  uint16_t version;
  uint8_t type;
  uint8_t address_size;
  uint64_t debug_abbrev_offset;
};
#pragma pack(pop)

struct DwarfAttribute {
  uint32_t attribute;
  uint32_t form;
};

using DwarfCompUnitMap = std::map<uint32_t, std::vector<DwarfAttribute> >;

/// @brief Relocation table entry bitness-independent structure
struct RelaEntry {
  uint64_t offset;
  uint64_t info;
  uint32_t sym;
  uint32_t type;
  uint64_t addend;
};

#pragma pack(push, 1)
struct RelaEntry32 {
  uint32_t offset;
  uint32_t info;
  int32_t addend;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RelaEntry64 {
  uint64_t offset;
  uint64_t info;
  int64_t addend;
};
#pragma pack(pop)

/// @brief Symbol table entry bitness-independent structure
struct SymtabEntry {
  uint32_t name;
  uint8_t info;
  uint8_t bind;
  uint8_t type;
  uint8_t other;
  uint16_t shndx;
  uint64_t value;
  uint64_t size;
};

#pragma pack(push, 1)
struct SymtabEntry32 {
  uint32_t name;
  uint32_t value;
  uint32_t size;
  uint8_t info;
  uint8_t other;
  uint16_t shndx;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SymtabEntry64 {
  uint32_t name;
  uint8_t info;
  uint8_t other;
  uint16_t shndx;
  uint64_t value;
  uint64_t size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ElfNote {
  uint32_t name_size;
  uint32_t desc_size;
  uint32_t type;
};
#pragma pack(pop)

}  // namespace elf_parser

#endif  // PTI_ELF_PARSER_DEF_H_
