//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Kernel Instruction Offset -> Source Location
//
// Maps an instruction offset in a kernel to its source file, line and column, using
// the DWARF line number program in the module binary's ".debug_line" (versions 2-5,
// implemented from the spec; nothing else in the container is decoded).
//
// IGC emits one unit per kernel, and a unit does not name its kernel, so it is found
// through ".rela.debug_line": the relocations against the kernel's ".text.<mangled
// name>" section fall in its unit. In an unlinked ZeBin the row addresses are
// placeholders, so they are normalized against their lowest, the kernel's entry.
//
// Building a table is the costly part and a lookup is a binary search, so build once
// per kernel: ResolveAllKernelInstructions() returns the whole mapping, and
// FindSourceLocation() looks offsets up in it.
//
// References:
//   DWARF Debugging Information Format, Version 5, Section 6.2 (Line Number
//   Information) https://dwarfstd.org/doc/DWARF5.pdf
//   DWARF Debugging Information Format, Version 4, Section 6.2
//   https://dwarfstd.org/doc/DWARF4.pdf
//
//==============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_SOURCE_RESOLVER_H_
#define PTI_TOOLS_UNITRACE_UTILS_SOURCE_RESOLVER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// ELF64 sections and ByteCursor.
#include "elf64_reader.h"

// ---------------------------------------------------------------------------
// What a lookup produces
// ---------------------------------------------------------------------------

// Where one instruction came from, as the line table records it.
struct SourceLocation {
  std::string file_path_;  // As the line table spells it, directory prefix applied
  uint32_t line_ = 0;      // 1-based; 0 means the row deliberately had no line
  uint32_t column_ = 0;    // 1-based; 0 means "anywhere on the line"
  // The kernel-relative range the row covers.
  uint64_t range_offset_ = 0;
  uint64_t range_size_ = 0;
  // The first instruction of a statement (a recommended breakpoint).
  bool is_statement_ = false;
};

// The relocations against ".debug_line", sorted by CollectRelocations() for binary
// search.
struct Relocations {
  // Offsets of relocations naming the kernel's ".text." section, each on the
  // DW_LNE_set_address operand of one of its sequences.
  std::vector<uint64_t> offsets_;
  // Every relocation as (offset patched, value), for DWARF 5 string references whose
  // in-place value is 0.
  std::vector<std::pair<uint64_t, uint64_t>> values_;

  // Whether a relocation names the kernel's section at exactly this offset.
  bool Names(uint64_t offset) const {
    return std::binary_search(offsets_.begin(), offsets_.end(), offset);
  }

  // Whether any lands in [begin, end), e.g. a unit.
  bool NamesRange(uint64_t begin, uint64_t end) const {
    const auto it = std::lower_bound(offsets_.begin(), offsets_.end(), begin);
    return it != offsets_.end() && *it < end;
  }

  bool Lookup(uint64_t offset, uint64_t& out_value) const {
    const auto it = std::lower_bound(
        values_.begin(), values_.end(), offset,
        [](const std::pair<uint64_t, uint64_t>& entry, uint64_t at) { return entry.first < at; });
    if (it == values_.end() || it->first != offset) {
      return false;
    }
    out_value = it->second;
    return true;
  }
};

// Reads the relocations whose sh_info is ".debug_line". One names the kernel if its
// symbol is in the kernel's ".text." section (text_index; an assembler's section
// symbol) or is named after it (text_section_name; IGC's symbols, whose st_shndx is
// the patched section).
inline void CollectRelocations(const std::vector<Elf64Section>& sections, uint32_t debug_line_index,
                               uint32_t text_index, const std::string& text_section_name,
                               const Elf64Section& debug_line, Relocations& out_relocations) {
  std::vector<uint64_t>& out_offsets = out_relocations.offsets_;
  out_offsets.clear();
  out_relocations.values_.clear();
  for (const Elf64Section& relocations : sections) {
    const bool is_rela = relocations.type_ == kElf64ShtRela;
    const bool is_rel = relocations.type_ == kElf64ShtRel;
    if ((!is_rela && !is_rel) || relocations.info_ != debug_line_index) {
      continue;
    }
    if (relocations.link_ >= sections.size()) {
      continue;
    }
    const Elf64Section& symbols = sections[relocations.link_];
    if (symbols.data_ == nullptr) {
      continue;
    }
    // Symbol names, if present; the st_shndx test does not need them.
    StringTableView symbol_names;
    if (symbols.link_ < sections.size()) {
      symbol_names.data_ = sections[symbols.link_].data_;
      symbol_names.size_ = sections[symbols.link_].size_;
    }

    // Honour a larger sh_entsize.
    const uint64_t abi_size = is_rela ? kElf64RelaSize : kElf64RelSize;
    const uint64_t stride = (relocations.entry_size_ >= abi_size) ? relocations.entry_size_ : abi_size;

    ByteCursor entries(relocations.data_, relocations.size_);
    out_relocations.values_.reserve(out_relocations.values_.size() +
                                    static_cast<size_t>(relocations.size_ / stride));
    for (uint64_t at = 0; at + stride <= relocations.size_; at += stride) {
      entries.Seek(at + kElf64RelOffsetOffset);
      const uint64_t r_offset = entries.U64();
      entries.Seek(at + kElf64RelInfoOffset);
      const uint64_t r_info = entries.U64();
      if (!entries.Ok()) {
        break;
      }

      // The high half of r_info is the symbol index; the type is not needed.
      const uint64_t symbol_index = r_info >> 32;
      const uint64_t symbol_stride =
          (symbols.entry_size_ >= kElf64SymSize) ? symbols.entry_size_ : kElf64SymSize;
      const uint64_t symbol_at = symbol_index * symbol_stride;
      if (!Elf64RangeInBounds(symbol_at, symbol_stride, symbols.size_)) {
        continue;
      }
      ByteCursor symbol(symbols.data_, symbols.size_);
      symbol.Seek(symbol_at + kElf64SymNameOffset);
      const uint32_t name_offset = symbol.U32();
      symbol.Seek(symbol_at + kElf64SymShndxOffset);
      const uint16_t section_index = symbol.U16();
      symbol.Seek(symbol_at + kElf64SymValueOffset);
      const uint64_t symbol_value = symbol.U64();
      if (!symbol.Ok()) {
        continue;
      }
      // st_name 0 is "no name". The name is compared only if st_shndx did not match.
      bool names_kernel = section_index == text_index;
      if (!names_kernel && name_offset != 0 && !text_section_name.empty()) {
        names_kernel = symbol_names.Equals(name_offset, text_section_name);
      }
      if (names_kernel) {
        out_offsets.push_back(r_offset);
      }

      // SHT_RELA carries the addend; SHT_REL leaves it in the patched bytes.
      uint64_t addend = 0;
      if (is_rela) {
        entries.Seek(at + kElf64RelaAddendOffset);
        addend = entries.U64();
        if (!entries.Ok()) {
          break;
        }
      } else if (Elf64RangeInBounds(r_offset, 4, debug_line.size_) &&
                 debug_line.data_ != nullptr) {
        ByteCursor in_place(debug_line.data_, debug_line.size_);
        in_place.Seek(r_offset);
        addend = in_place.U32();
      }
      out_relocations.values_.push_back({r_offset, symbol_value + addend});
    }
  }

  // Stable, so Lookup() returns the first of two relocations at one offset.
  std::sort(out_offsets.begin(), out_offsets.end());
  std::stable_sort(out_relocations.values_.begin(), out_relocations.values_.end(),
                   [](const std::pair<uint64_t, uint64_t>& a,
                      const std::pair<uint64_t, uint64_t>& b) { return a.first < b.first; });
}

// ---------------------------------------------------------------------------
// DWARF line number program
// ---------------------------------------------------------------------------

// Standard opcodes, DWARF 5 section 6.2.5.2.
constexpr uint8_t kDwLnsCopy = 1;
constexpr uint8_t kDwLnsAdvancePc = 2;
constexpr uint8_t kDwLnsAdvanceLine = 3;
constexpr uint8_t kDwLnsSetFile = 4;
constexpr uint8_t kDwLnsSetColumn = 5;
constexpr uint8_t kDwLnsNegateStmt = 6;
constexpr uint8_t kDwLnsSetBasicBlock = 7;
constexpr uint8_t kDwLnsConstAddPc = 8;
constexpr uint8_t kDwLnsFixedAdvancePc = 9;
constexpr uint8_t kDwLnsSetPrologueEnd = 10;
constexpr uint8_t kDwLnsSetEpilogueBegin = 11;
constexpr uint8_t kDwLnsSetIsa = 12;

// Extended opcodes, DWARF 5 section 6.2.5.3.
constexpr uint8_t kDwLneEndSequence = 1;
constexpr uint8_t kDwLneSetAddress = 2;
constexpr uint8_t kDwLneDefineFile = 3;  // DWARF 4 and earlier only
constexpr uint8_t kDwLneSetDiscriminator = 4;

// Content type codes for the DWARF 5 directory and file tables, table 7.27.
constexpr uint64_t kDwLnctPath = 1;
constexpr uint64_t kDwLnctDirectoryIndex = 2;

// Attribute forms, table 7.6, that a file or directory entry (including a vendor
// content type) may use.
constexpr uint64_t kDwFormAddr = 0x01;
constexpr uint64_t kDwFormBlock2 = 0x03;
constexpr uint64_t kDwFormBlock4 = 0x04;
constexpr uint64_t kDwFormData2 = 0x05;
constexpr uint64_t kDwFormData4 = 0x06;
constexpr uint64_t kDwFormData8 = 0x07;
constexpr uint64_t kDwFormString = 0x08;
constexpr uint64_t kDwFormBlock = 0x09;
constexpr uint64_t kDwFormBlock1 = 0x0A;
constexpr uint64_t kDwFormData1 = 0x0B;
constexpr uint64_t kDwFormFlag = 0x0C;
constexpr uint64_t kDwFormSdata = 0x0D;
constexpr uint64_t kDwFormStrp = 0x0E;
constexpr uint64_t kDwFormUdata = 0x0F;
constexpr uint64_t kDwFormFlagPresent = 0x19;
constexpr uint64_t kDwFormStrx = 0x1A;
constexpr uint64_t kDwFormData16 = 0x1E;
constexpr uint64_t kDwFormLineStrp = 0x1F;
constexpr uint64_t kDwFormImplicitConst = 0x21;
constexpr uint64_t kDwFormStrx1 = 0x25;
constexpr uint64_t kDwFormStrx2 = 0x26;
constexpr uint64_t kDwFormStrx3 = 0x27;
constexpr uint64_t kDwFormStrx4 = 0x28;

// One row of the line table: the registers a source lookup needs.
struct LineRow {
  uint64_t address_ = 0;
  uint64_t file_ = 0;
  uint32_t line_ = 0;
  uint32_t column_ = 0;
  bool is_stmt_ = false;
  // Closes a sequence: its address ends the last range but it is no range itself.
  bool end_sequence_ = false;
  // Where the sequence's DW_LNE_set_address operand sits, which a relocation ties to
  // a kernel; kNoOffset if none.
  uint64_t address_source_ = ~static_cast<uint64_t>(0);
};

constexpr uint64_t kNoOffset = ~static_cast<uint64_t>(0);

// One unit of ".debug_line": the file table its rows name, and the rows themselves.
struct LineUnit {
  uint64_t offset_ = 0;  // Where the unit starts, which relocations point into
  uint64_t end_ = 0;
  uint16_t version_ = 0;
  // Indexed as the rows index it: from 0 in DWARF 5, from 1 before (entry 0 empty).
  std::vector<std::string> file_paths_;
  std::vector<LineRow> rows_;
};

// Reads one value of a form, keeping a string or number and skipping anything else.
// Returns false for a form it cannot read.
inline bool ReadFormValue(ByteCursor& cursor, uint64_t form, uint8_t offset_size,
                          uint8_t address_size, const StringTableView& debug_str,
                          const StringTableView& debug_line_str, const Relocations* relocations,
                          std::string& out_string, uint64_t& out_number, bool& out_has_string,
                          bool& out_has_number) {
  out_has_string = false;
  out_has_number = false;
  switch (form) {
    case kDwFormString:
      out_has_string = cursor.String(out_string);
      return true;
    case kDwFormLineStrp:
    case kDwFormStrp: {
      // In an unlinked object the offset is the relocation's, not the bytes'.
      const uint64_t patched_at = cursor.Offset();
      uint64_t at = (offset_size == 8) ? cursor.U64() : cursor.U32();
      uint64_t relocated_at = 0;
      if (relocations != nullptr && relocations->Lookup(patched_at, relocated_at)) {
        at = relocated_at;
      }
      const StringTableView& table = (form == kDwFormLineStrp) ? debug_line_str : debug_str;
      // A missing string is not fatal; only this name is lost.
      out_has_string = table.Get(at, out_string);
      return true;
    }
    case kDwFormUdata:
      out_number = cursor.Uleb();
      out_has_number = true;
      return true;
    case kDwFormSdata:
      out_number = static_cast<uint64_t>(cursor.Sleb());
      out_has_number = true;
      return true;
    case kDwFormData1:
    case kDwFormFlag:
      out_number = cursor.U8();
      out_has_number = true;
      return true;
    case kDwFormData2:
      out_number = cursor.U16();
      out_has_number = true;
      return true;
    case kDwFormData4:
      out_number = cursor.U32();
      out_has_number = true;
      return true;
    case kDwFormData8:
      out_number = cursor.U64();
      out_has_number = true;
      return true;
    case kDwFormData16:  // An MD5 checksum, in practice
      cursor.Skip(16);
      return true;
    case kDwFormAddr:
      out_number = cursor.Address(address_size);
      out_has_number = true;
      return true;
    case kDwFormFlagPresent:
    case kDwFormImplicitConst:
      return true;  // No bytes in the entry at all
    case kDwFormBlock1:
      cursor.Skip(cursor.U8());
      return true;
    case kDwFormBlock2:
      cursor.Skip(cursor.U16());
      return true;
    case kDwFormBlock4:
      cursor.Skip(cursor.U32());
      return true;
    case kDwFormBlock:
      cursor.Skip(cursor.Uleb());
      return true;
    // Indexed strings need ".debug_info" for their base, so are not supported.
    case kDwFormStrx:
      (void)cursor.Uleb();
      return false;
    case kDwFormStrx1:
      cursor.Skip(1);
      return false;
    case kDwFormStrx2:
      cursor.Skip(2);
      return false;
    case kDwFormStrx3:
      cursor.Skip(3);
      return false;
    case kDwFormStrx4:
      cursor.Skip(4);
      return false;
    default:
      return false;
  }
}

// Joins a directory and a file name.
inline std::string JoinPath(const std::string& directory, const std::string& name) {
  if (name.empty()) {
    return directory;
  }
  // An absolute name, POSIX or Windows, ignores the directory.
  const bool absolute = name[0] == '/' || name[0] == '\\' ||
                        (name.size() > 2 && name[1] == ':' && (name[2] == '/' || name[2] == '\\'));
  if (absolute || directory.empty()) {
    return name;
  }
  const char last = directory[directory.size() - 1];
  if (last == '/' || last == '\\') {
    return directory + name;
  }
  return directory + "/" + name;
}

// Parses the directory and file tables of a DWARF 2-4 header, each ended by an empty
// entry.
inline bool ReadLegacyFileTable(ByteCursor& cursor, LineUnit& unit) {
  std::vector<std::string> directories;
  // Index 0, the compilation directory, is not here: left empty.
  directories.push_back(std::string());
  for (;;) {
    std::string entry;
    if (!cursor.String(entry)) {
      return false;
    }
    if (entry.empty()) {
      break;
    }
    directories.push_back(std::move(entry));
  }

  // Files are numbered from 1; index 0 is a placeholder.
  unit.file_paths_.push_back(std::string());
  for (;;) {
    std::string name;
    if (!cursor.String(name)) {
      return false;
    }
    if (name.empty()) {
      break;
    }
    const uint64_t directory_index = cursor.Uleb();
    (void)cursor.Uleb();  // Modification time
    (void)cursor.Uleb();  // File length
    if (!cursor.Ok()) {
      return false;
    }
    const std::string& directory =
        (directory_index < directories.size()) ? directories[directory_index] : std::string();
    unit.file_paths_.push_back(JoinPath(directory, name));
  }
  return true;
}

// Parses the directory and file tables of a DWARF 5 header, each described by a list
// of (content type, form) pairs.
inline bool ReadDwarf5FileTable(ByteCursor& cursor, uint8_t offset_size, uint8_t address_size,
                                const StringTableView& debug_str,
                                const StringTableView& debug_line_str,
                                const Relocations* relocations, LineUnit& unit) {
  // Reads either table; only the file table uses a directory index.
  auto read_table = [&](bool is_file_table, std::vector<std::string>& out_paths,
                        const std::vector<std::string>* directories) -> bool {
    const uint8_t format_count = cursor.U8();
    std::vector<std::pair<uint64_t, uint64_t>> format;  // (content type, form)
    format.reserve(format_count);
    for (uint8_t i = 0; i < format_count; ++i) {
      const uint64_t content_type = cursor.Uleb();
      const uint64_t form = cursor.Uleb();
      if (!cursor.Ok()) {
        return false;
      }
      format.push_back({content_type, form});
    }

    const uint64_t entry_count = cursor.Uleb();
    if (!cursor.Ok()) {
      return false;
    }
    // Each entry is at least one byte; a larger count is malformed.
    if (entry_count > cursor.Size() - cursor.Offset()) {
      return false;
    }
    out_paths.reserve(static_cast<size_t>(entry_count));

    for (uint64_t i = 0; i < entry_count; ++i) {
      std::string path;
      uint64_t directory_index = 0;
      for (const std::pair<uint64_t, uint64_t>& field : format) {
        std::string value_string;
        uint64_t value_number = 0;
        bool has_string = false;
        bool has_number = false;
        if (!ReadFormValue(cursor, field.second, offset_size, address_size, debug_str,
                           debug_line_str, relocations, value_string, value_number, has_string,
                           has_number)) {
          return false;
        }
        if (!cursor.Ok()) {
          return false;
        }
        if (field.first == kDwLnctPath && has_string) {
          path = std::move(value_string);
        } else if (field.first == kDwLnctDirectoryIndex && has_number) {
          directory_index = value_number;
        }
      }
      if (is_file_table && directories != nullptr) {
        const std::string& directory = (directory_index < directories->size())
                                           ? (*directories)[directory_index]
                                           : std::string();
        out_paths.push_back(JoinPath(directory, path));
      } else {
        out_paths.push_back(std::move(path));
      }
    }
    return true;
  };

  std::vector<std::string> directories;
  if (!read_table(false, directories, nullptr)) {
    return false;
  }
  // Files are numbered from 0 in DWARF 5, so no placeholder.
  return read_table(true, unit.file_paths_, &directories);
}

// Runs one unit's line number program from cursor to program_end, the state machine
// of DWARF 5 section 6.2.5, op_index included (always 0 on Intel GPUs).
inline bool RunLineProgram(ByteCursor& cursor, uint64_t program_end, uint8_t address_size,
                           uint8_t minimum_instruction_length,
                           uint8_t maximum_operations_per_instruction, bool default_is_stmt,
                           int8_t line_base, uint8_t line_range, uint8_t opcode_base,
                           const std::vector<uint8_t>& standard_opcode_lengths, LineUnit& unit) {
  struct State {
    uint64_t address_ = 0;
    uint32_t op_index_ = 0;
    uint64_t file_ = 1;
    uint32_t line_ = 1;
    uint32_t column_ = 0;
    bool is_stmt_ = false;
    bool end_sequence_ = false;
    // Not a DWARF register: see LineRow::address_source_.
    uint64_t address_source_ = kNoOffset;
  };

  State state;
  state.is_stmt_ = default_is_stmt;
  // The file register starts at 1 in every version, even though DWARF 5 numbers the
  // table from 0 (section 6.2.2).

  auto reset = [&]() {
    state = State();
    state.is_stmt_ = default_is_stmt;
  };

  auto emit = [&]() {
    LineRow row;
    row.address_ = state.address_;
    row.file_ = state.file_;
    row.line_ = state.line_;
    row.column_ = state.column_;
    row.is_stmt_ = state.is_stmt_;
    row.end_sequence_ = state.end_sequence_;
    row.address_source_ = state.address_source_;
    unit.rows_.push_back(row);
  };

  const uint32_t max_ops =
      (maximum_operations_per_instruction == 0) ? 1 : maximum_operations_per_instruction;

  auto advance = [&](uint64_t operation_advance) {
    const uint64_t total = state.op_index_ + operation_advance;
    state.address_ += static_cast<uint64_t>(minimum_instruction_length) * (total / max_ops);
    state.op_index_ = static_cast<uint32_t>(total % max_ops);
  };

  while (cursor.Ok() && cursor.Offset() < program_end) {
    const uint8_t opcode = cursor.U8();
    if (!cursor.Ok()) {
      return false;
    }

    // A special opcode: advances the address and line, and emits a row.
    if (opcode_base != 0 && opcode >= opcode_base) {
      const uint8_t adjusted = static_cast<uint8_t>(opcode - opcode_base);
      if (line_range == 0) {
        return false;  // Would divide by zero; the header is malformed
      }
      advance(adjusted / line_range);
      const int64_t line_advance = line_base + static_cast<int64_t>(adjusted % line_range);
      const int64_t new_line = static_cast<int64_t>(state.line_) + line_advance;
      // Clamp a negative line rather than reject the table.
      state.line_ = (new_line < 0) ? 0 : static_cast<uint32_t>(new_line);
      emit();
      continue;
    }

    if (opcode == 0) {
      // An extended opcode, length-prefixed so an unknown one can be stepped over.
      const uint64_t length = cursor.Uleb();
      if (!cursor.Ok()) {
        return false;
      }
      const uint64_t after = cursor.Offset() + length;
      if (length == 0 || after > program_end) {
        return false;
      }
      const uint8_t sub_opcode = cursor.U8();
      switch (sub_opcode) {
        case kDwLneEndSequence:
          state.end_sequence_ = true;
          emit();
          reset();
          break;
        case kDwLneSetAddress: {
          // The operand width is the length's, which may be narrower than
          // address_size.
          const uint64_t operand_size = length - 1;
          state.address_source_ = cursor.Offset();
          state.address_ =
              cursor.Address(static_cast<uint8_t>((operand_size > 8) ? address_size : operand_size));
          state.op_index_ = 0;
          break;
        }
        case kDwLneDefineFile: {
          // DWARF 4 and earlier could append to the file table mid-program.
          std::string name;
          if (!cursor.String(name)) {
            return false;
          }
          const uint64_t directory_index = cursor.Uleb();
          (void)cursor.Uleb();  // Modification time
          (void)cursor.Uleb();  // Length
          // The directory table is gone by now, so only the name is recorded.
          (void)directory_index;
          unit.file_paths_.push_back(std::move(name));
          break;
        }
        case kDwLneSetDiscriminator:
          (void)cursor.Uleb();
          break;
        default:
          break;  // Vendor extension: stepped over by its length
      }
      // The length decides where the next opcode is, not what was read.
      cursor.Seek(after);
      continue;
    }

    switch (opcode) {
      case kDwLnsCopy:
        emit();
        break;
      case kDwLnsAdvancePc:
        advance(cursor.Uleb());
        break;
      case kDwLnsAdvanceLine: {
        const int64_t new_line = static_cast<int64_t>(state.line_) + cursor.Sleb();
        state.line_ = (new_line < 0) ? 0 : static_cast<uint32_t>(new_line);
        break;
      }
      case kDwLnsSetFile:
        state.file_ = cursor.Uleb();
        break;
      case kDwLnsSetColumn:
        state.column_ = static_cast<uint32_t>(cursor.Uleb());
        break;
      case kDwLnsNegateStmt:
        state.is_stmt_ = !state.is_stmt_;
        break;
      case kDwLnsSetBasicBlock:
        break;  // Recorded by DWARF, not needed here
      case kDwLnsConstAddPc:
        // The address advance of special opcode 255, without a row.
        if (line_range == 0) {
          return false;
        }
        advance(static_cast<uint8_t>(255 - opcode_base) / line_range);
        break;
      case kDwLnsFixedAdvancePc:
        // A plain byte count; resets op_index.
        state.address_ += cursor.U16();
        state.op_index_ = 0;
        break;
      case kDwLnsSetPrologueEnd:
      case kDwLnsSetEpilogueBegin:
        break;  // Recorded by DWARF, not needed here
      case kDwLnsSetIsa:
        (void)cursor.Uleb();
        break;
      default: {
        // An unknown standard opcode: skip the operand count the header gives.
        const size_t index = opcode - 1u;
        if (index >= standard_opcode_lengths.size()) {
          return false;
        }
        for (uint8_t i = 0; i < standard_opcode_lengths[index]; ++i) {
          (void)cursor.Uleb();
        }
        break;
      }
    }
  }
  return cursor.Ok();
}

// Where one unit of ".debug_line" is, from its length field alone.
struct LineUnitExtent {
  uint64_t offset_ = 0;        // Where the unit starts, which relocations point into
  uint64_t end_ = 0;           // Where the next unit starts
  uint64_t header_start_ = 0;  // Just past the length field, where the version sits
  uint8_t offset_size_ = 4;    // The width of a DWARF offset in this unit, 4 or 8
};
// Reads the extent of the unit at cursor and leaves cursor at the next unit, so other
// kernels' units can be skipped cheaply. Returns false for a reserved length or one
// past the section, when there is no next unit.
inline bool ReadLineUnitExtent(ByteCursor& cursor, LineUnitExtent& out_extent) {
  out_extent = LineUnitExtent();
  out_extent.offset_ = cursor.Offset();

  // The initial length; 0xFFFFFFFF means 64-bit DWARF, other values from 0xFFFFFFF0
  // are reserved.
  uint64_t unit_length = cursor.U32();
  if (unit_length == 0xFFFFFFFFu) {
    unit_length = cursor.U64();
    out_extent.offset_size_ = 8;
  } else if (unit_length >= 0xFFFFFFF0u) {
    return false;
  }
  if (!cursor.Ok()) {
    return false;
  }
  out_extent.header_start_ = cursor.Offset();
  if (!Elf64RangeInBounds(out_extent.header_start_, unit_length, cursor.Size())) {
    return false;  // The unit claims to run past the section
  }
  out_extent.end_ = out_extent.header_start_ + unit_length;
  cursor.Seek(out_extent.end_);
  return true;
}

// Parses the ".debug_line" unit at cursor. Unless out_fatal is set, cursor is left at
// the next unit even if this one fails to parse.
inline bool ParseLineUnit(ByteCursor& cursor, const StringTableView& debug_str,
                          const StringTableView& debug_line_str, const Relocations* relocations,
                          LineUnit& out_unit, bool& out_fatal) {
  out_unit = LineUnit();
  LineUnitExtent extent;
  out_fatal = !ReadLineUnitExtent(cursor, extent);
  if (out_fatal) {
    return false;
  }
  out_unit.offset_ = extent.offset_;
  out_unit.end_ = extent.end_;
  const uint8_t offset_size = extent.offset_size_;
  // From here a failure is not fatal: cursor is already at the next unit.
  ByteCursor unit_cursor = cursor;
  unit_cursor.Seek(extent.header_start_);

  out_unit.version_ = unit_cursor.U16();
  if (!unit_cursor.Ok()) {
    return false;
  }
  // Only the known header layouts.
  if (out_unit.version_ < 2 || out_unit.version_ > 5) {
    return false;
  }

  uint8_t address_size = 8;
  if (out_unit.version_ >= 5) {
    address_size = unit_cursor.U8();
    const uint8_t segment_selector_size = unit_cursor.U8();
    // Segmented addresses are not supported.
    if (segment_selector_size != 0) {
      return false;
    }
  }

  const uint64_t header_length = (offset_size == 8) ? unit_cursor.U64() : unit_cursor.U32();
  if (!unit_cursor.Ok()) {
    return false;
  }
  // Where the program starts, which allows header fields this does not read.
  const uint64_t program_start = unit_cursor.Offset() + header_length;
  if (program_start > out_unit.end_) {
    return false;
  }

  const uint8_t minimum_instruction_length = unit_cursor.U8();
  const uint8_t maximum_operations_per_instruction =
      (out_unit.version_ >= 4) ? unit_cursor.U8() : 1;
  const bool default_is_stmt = unit_cursor.U8() != 0;
  const int8_t line_base = unit_cursor.I8();
  const uint8_t line_range = unit_cursor.U8();
  const uint8_t opcode_base = unit_cursor.U8();
  if (!unit_cursor.Ok() || line_range == 0 || opcode_base == 0) {
    return false;
  }

  std::vector<uint8_t> standard_opcode_lengths;
  standard_opcode_lengths.reserve(opcode_base - 1u);
  for (uint8_t i = 1; i < opcode_base; ++i) {
    standard_opcode_lengths.push_back(unit_cursor.U8());
  }
  if (!unit_cursor.Ok()) {
    return false;
  }

  const bool tables_read =
      (out_unit.version_ >= 5)
          ? ReadDwarf5FileTable(unit_cursor, offset_size, address_size, debug_str, debug_line_str,
                                relocations, out_unit)
          : ReadLegacyFileTable(unit_cursor, out_unit);
  // A bad file table still leaves the rows useful, with lines but no names.
  (void)tables_read;

  unit_cursor.Seek(program_start);
  if (!unit_cursor.Ok()) {
    return false;
  }
  return RunLineProgram(unit_cursor, out_unit.end_, address_size, minimum_instruction_length,
                        maximum_operations_per_instruction, default_is_stmt, line_base, line_range,
                        opcode_base, standard_opcode_lengths, out_unit);
}

// ---------------------------------------------------------------------------
// The public interface
// ---------------------------------------------------------------------------

// One kernel's line table. Build() does the parsing, once; Resolve() is a binary
// search.
class KernelLineTable {
 public:
  // binary/binary_size is the module binary; kernel_name is the mangled name.
  // Returns false, with a "[WARNING] ..." on std::cerr naming the kernel, if there is
  // no line information for it (e.g. built without -g).
  bool Build(const uint8_t* binary, size_t binary_size, const std::string& kernel_name) {
    rows_.clear();
    file_paths_.clear();
    spans_.clear();
    kernel_base_ = 0;

    auto fail = [&kernel_name](const std::string& message) {
      std::cerr << "[WARNING] No source lines for kernel " << kernel_name << ": " << message
                << std::endl;
      return false;
    };

    if (kernel_name.empty()) {
      // Not through fail(): there is no name to print.
      std::cerr << "[WARNING] No source lines for a kernel: no kernel name to look up" << std::endl;
      return false;
    }
    std::vector<Elf64Section> sections;
    if (ReadElf64Sections(binary, binary_size, sections) != Elf64Status::kOk) {
      // ReadElf64Sections() has already reported why.
      return false;
    }

    const int64_t debug_line_index = FindElf64Section(sections, ".debug_line");
    if (debug_line_index < 0 || sections[debug_line_index].size_ == 0) {
      return fail("module binary has no line number information (built without -g?)");
    }
    const Elf64Section& debug_line = sections[debug_line_index];
    // Present but outside the binary, e.g. truncated.
    if (debug_line.data_ == nullptr) {
      return fail("line number information lies outside the module binary");
    }

    StringTableView debug_str;
    const int64_t debug_str_index = FindElf64Section(sections, ".debug_str");
    if (debug_str_index >= 0) {
      debug_str.data_ = sections[debug_str_index].data_;
      debug_str.size_ = sections[debug_str_index].size_;
    }
    StringTableView debug_line_str;
    const int64_t debug_line_str_index = FindElf64Section(sections, ".debug_line_str");
    if (debug_line_str_index >= 0) {
      debug_line_str.data_ = sections[debug_line_str_index].data_;
      debug_line_str.size_ = sections[debug_line_str_index].size_;
    }

    // The kernel's unit is the one its ".text." relocations point into; without them,
    // only a sole usable unit. The section must exist, or the sole-unit case would
    // answer from another kernel's table.
    const std::string text_section_name = Elf64KernelTextSectionName(kernel_name);
    const int64_t text_index = FindElf64Section(sections, text_section_name);
    if (text_index < 0) {
      return fail("module binary holds no section for this kernel");
    }
    Relocations relocations;
    CollectRelocations(sections, static_cast<uint32_t>(debug_line_index),
                       static_cast<uint32_t>(text_index), text_section_name, debug_line,
                       relocations);
    const bool relocated = !relocations.offsets_.empty();

    // With relocations, units they do not point into are skipped unparsed. Without,
    // every unit is parsed to find whether exactly one is usable.
    LineUnit chosen;
    size_t usable = 0;  // Units that could be this kernel's and produced rows
    ByteCursor cursor(debug_line.data_, debug_line.size_);
    while (!cursor.Done()) {
      LineUnitExtent extent;
      if (!ReadLineUnitExtent(cursor, extent)) {
        break;  // The section's own structure is broken; there is no next unit
      }
      if (relocated && !relocations.NamesRange(extent.offset_, extent.end_)) {
        continue;  // Nothing in this unit refers to the kernel, so it is another's
      }

      // From the unit's start; the outer cursor stays at the next unit.
      ByteCursor unit_cursor(debug_line.data_, debug_line.size_);
      unit_cursor.Seek(extent.offset_);
      LineUnit unit;
      bool fatal = false;  // Cannot fire: the extent above read the same length field
      if (!ParseLineUnit(unit_cursor, debug_str, debug_line_str, &relocations, unit, fatal) ||
          unit.rows_.empty()) {
        continue;
      }

      ++usable;
      if (usable > 1) {
        break;  // Nothing named a unit and more than one could be it: ambiguous
      }
      chosen = std::move(unit);
      if (relocated) {
        break;  // A relocation names this unit; no later one can displace it
      }
    }
    if (usable != 1) {
      // To say why, check whether any unit at all is usable, skipped ones included.
      bool any_usable = usable > 0;
      ByteCursor rescan(debug_line.data_, debug_line.size_);
      while (!any_usable && !rescan.Done()) {
        LineUnit unit;
        bool fatal = false;
        if (ParseLineUnit(rescan, debug_str, debug_line_str, &relocations, unit, fatal)) {
          any_usable = !unit.rows_.empty();
        }
        if (fatal) {
          break;
        }
      }
      return fail(any_usable ? "cannot tell which line table belongs to this kernel"
                             : "line number information holds no usable line table");
    }

    file_paths_ = std::move(chosen.file_paths_);

    // Keep only the kernel's sequences, for a producer that puts several functions'
    // sequences, overlapping at address 0, in one unit.
    for (const LineRow& row : chosen.rows_) {
      if (row.address_source_ != kNoOffset && relocations.Names(row.address_source_)) {
        rows_.push_back(row);
      }
    }
    // No sequence matched, but the unit is still this kernel's: keep all its rows.
    if (rows_.empty()) {
      rows_ = std::move(chosen.rows_);
    }

    // Offsets are relative to the lowest address, the kernel's entry, whether the
    // binary is linked or not.
    bool have_base = false;
    for (const LineRow& row : rows_) {
      if (!have_base || row.address_ < kernel_base_) {
        kernel_base_ = row.address_;
        have_base = true;
      }
    }

    BuildIndex();
    return true;
  }

  // The source of the instruction at instruction_offset. On success sets every field
  // of out_location; returns false, leaving it untouched, if no row covers the offset.
  bool Resolve(uint64_t instruction_offset, SourceLocation& out_location) const {
    if (spans_.empty()) {
      return false;
    }
    const uint64_t target = kernel_base_ + instruction_offset;

    // The last span starting at or before the target.
    const auto it = std::upper_bound(spans_.begin(), spans_.end(), target,
                                     [](uint64_t at, const Span& span) {
                                       return at < span.address_;
                                     });
    if (it == spans_.begin()) {
      return false;
    }
    const size_t winner = (it - 1)->row_;
    if (winner == kNoRow) {
      return false;  // A gap between sequences, or past the last of them
    }
    const LineRow* best = &rows_[winner];
    const uint64_t best_end = rows_[winner + 1].address_;

    // Cleared, since it is the one field not always written; keeps the buffer.
    out_location.file_path_.clear();
    out_location.line_ = best->line_;
    out_location.column_ = best->column_;
    out_location.is_statement_ = best->is_stmt_;
    out_location.range_offset_ = best->address_ - kernel_base_;
    out_location.range_size_ = best_end - best->address_;
    if (best->file_ < file_paths_.size()) {
      out_location.file_path_ = file_paths_[static_cast<size_t>(best->file_)];
    }
    return true;
  }

  // Every range the table maps, ascending and unique by range_offset_: what Resolve()
  // returns for each row start, so shadowed rows are left out. Returns false, with
  // out_locations empty, if there are none.
  bool ResolveAll(std::vector<SourceLocation>& out_locations) const {
    out_locations.clear();
    if (rows_.empty()) {
      return false;
    }

    // The unique row starts.
    std::vector<uint64_t> starts;
    starts.reserve(rows_.size());
    for (const LineRow& row : rows_) {
      if (row.end_sequence_) {
        continue;  // Closes a sequence; describes no instruction
      }
      if (row.address_ < kernel_base_) {
        continue;  // Cannot happen: the base is the lowest address
      }
      starts.push_back(row.address_ - kernel_base_);
    }
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

    for (uint64_t start : starts) {
      SourceLocation location;
      if (!Resolve(start, location)) {
        continue;  // A row whose range is empty, so it covers no instruction
      }
      out_locations.push_back(location);
    }

    // Two starts can resolve to one range, and not always in order, so sort and
    // deduplicate the answers.
    std::sort(out_locations.begin(), out_locations.end(),
              [](const SourceLocation& a, const SourceLocation& b) {
                return a.range_offset_ < b.range_offset_;
              });
    out_locations.erase(std::unique(out_locations.begin(), out_locations.end(),
                                    [](const SourceLocation& a, const SourceLocation& b) {
                                      return a.range_offset_ == b.range_offset_;
                                    }),
                        out_locations.end());
    return !out_locations.empty();
  }

 private:
  // A span's row_ when no row covers it.
  static constexpr size_t kNoRow = static_cast<size_t>(-1);

  // Addresses from address_ to the next span's, all answered by one row.
  struct Span {
    uint64_t address_ = 0;
    // Never the last row, so rows_[row_ + 1] is where the row's own range ends.
    size_t row_ = kNoRow;
  };

  // Builds spans_ by flattening overlapping row ranges, so Resolve() can binary
  // search. A span reports its row's whole range, not just the span.
  void BuildIndex() {
    spans_.clear();

    // Where a row's range opens and closes.
    struct Event {
      uint64_t at_ = 0;
      bool open_ = false;
      size_t row_ = 0;
    };
    std::vector<Event> events;
    events.reserve(rows_.size() * 2);
    for (size_t i = 0; i + 1 < rows_.size(); ++i) {
      const LineRow& row = rows_[i];
      if (row.end_sequence_) {
        continue;  // Closes a sequence; describes no instruction
      }
      const uint64_t end = rows_[i + 1].address_;
      if (end <= row.address_) {
        continue;  // An empty range, or the start of the next sequence
      }
      events.push_back(Event{row.address_, true, i});
      events.push_back(Event{end, false, i});
    }
    if (events.empty()) {
      return;
    }

    // By address, closes before opens: a range does not cover its end.
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
      if (a.at_ != b.at_) {
        return a.at_ < b.at_;
      }
      return static_cast<int>(a.open_) < static_cast<int>(b.open_);
    });

    // Sweep, keeping the rows covering the point; the last row wins, as in a debugger.
    std::set<size_t> active;
    for (size_t i = 0; i < events.size();) {
      const uint64_t at = events[i].at_;
      for (; i < events.size() && events[i].at_ == at; ++i) {
        if (events[i].open_) {
          active.insert(events[i].row_);
        } else {
          active.erase(events[i].row_);
        }
      }
      const size_t winner = active.empty() ? kNoRow : *active.rbegin();
      // A span per change of answer; a gap is a kNoRow span.
      if (spans_.empty() || spans_.back().row_ != winner) {
        spans_.push_back(Span{at, winner});
      }
    }
  }

  std::vector<LineRow> rows_;
  std::vector<std::string> file_paths_;
  std::vector<Span> spans_;
  uint64_t kernel_base_ = 0;
};

// One kernel's whole mapping, as KernelLineTable::ResolveAll() returns it. Returns
// false, with out_locations empty and a "[WARNING] ..." on std::cerr, if the table
// cannot be built or maps nothing.
inline bool ResolveAllKernelInstructions(const uint8_t* binary, size_t binary_size,
                                         const std::string& kernel_name,
                                         std::vector<SourceLocation>& out_locations) {
  out_locations.clear();
  KernelLineTable table;
  if (!table.Build(binary, binary_size, kernel_name)) {
    return false;
  }
  if (!table.ResolveAll(out_locations)) {
    std::cerr << "[WARNING] No source lines for kernel " << kernel_name
              << ": the kernel's line table covers no instruction offset" << std::endl;
    return false;
  }
  return true;
}

// The location in ResolveAllKernelInstructions()'s output covering offset, or nullptr:
// the last entry starting at or below it, if its range covers it. Where entries
// overlap, the later one wins.
inline const SourceLocation* FindSourceLocation(
    const std::vector<SourceLocation>& locations, uint64_t offset) {
  const auto above = std::upper_bound(locations.begin(), locations.end(), offset,
                                      [](uint64_t value, const SourceLocation& location) {
                                        return value < location.range_offset_;
                                      });
  if (above == locations.begin()) {
    return nullptr;  // Below the first range
  }
  const SourceLocation& candidate = *(above - 1);
  if (offset - candidate.range_offset_ >= candidate.range_size_) {
    return nullptr;  // In a gap, or past the end of the last range
  }
  return &candidate;
}

#endif  // PTI_TOOLS_UNITRACE_UTILS_SOURCE_RESOLVER_H_
