//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// ELF64 Reading
//
// Reads the ELF64 container of a ZeBin module binary: identifies it, walks the section
// header table and resolves section names. Finding a kernel's ISA (kernel_extractor.h)
// and its line table (source_resolver.h) both start here. Section contents are returned
// as raw bytes.
//
// The structures are read per the gABI rather than through <elf.h>, which Windows lacks.
// Every offset and length from the file is bounds-checked, since a truncated file can
// claim sections that are not there.
//
// References:
//   System V ABI, Chapter 4: Object Files (ELF)
//   https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html
//   Intel Graphics Compiler, the ZEBIN specification; elf.md covers the container
//   https://github.com/intel/intel-graphics-compiler/tree/master/IGC/ZEBinWriter/zebin/spec
//
//==============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_ELF64_READER_H_
#define PTI_TOOLS_UNITRACE_UTILS_ELF64_READER_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Reading bytes safely
//
// Bounds-checked primitives, also used by callers decoding section contents.
// ---------------------------------------------------------------------------

// Is [offset, offset + length) inside a buffer of total bytes? Written so it cannot
// overflow, since both values come from the file.
inline bool Elf64RangeInBounds(uint64_t offset, uint64_t length, uint64_t total) {
  return length <= total && offset <= total - length;
}

// Little-endian reads, byte by byte so the result does not depend on host byte order.
inline uint16_t Elf64ReadU16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

inline uint32_t Elf64ReadU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline uint64_t Elf64ReadU64(const uint8_t* p) {
  return static_cast<uint64_t>(Elf64ReadU32(p)) |
         (static_cast<uint64_t>(Elf64ReadU32(p + 4)) << 32);
}

// The NUL ending the string at data, or nullptr if there is none within length bytes.
// A zero length does not touch data, which may then be null or one past the end.
inline const uint8_t* FindNul(const uint8_t* data, uint64_t length) {
  if (length == 0) {
    return nullptr;
  }
  return static_cast<const uint8_t*>(std::memchr(data, 0, static_cast<size_t>(length)));
}

// A position in a buffer that cannot move outside it. Errors are sticky: after a
// failure every read returns zero, so a caller decoding byte code (e.g. a DWARF line
// program) checks Ok() once rather than after every operand.
class ByteCursor {
 public:
  ByteCursor(const uint8_t* data, uint64_t size) : data_(data), size_(size) {
    if (data_ == nullptr) {
      size_ = 0;
    }
  }

  bool Ok() const { return !failed_; }
  uint64_t Offset() const { return pos_; }
  uint64_t Size() const { return size_; }
  // Has the cursor reached the end, or failed trying?
  bool Done() const { return failed_ || pos_ >= size_; }

  void Seek(uint64_t pos) {
    if (pos > size_) {
      failed_ = true;
      return;
    }
    pos_ = pos;
  }

  // Returns count bytes and advances past them, or fails. Every fixed-width read goes
  // through it; String() does its own bounds check.
  const uint8_t* Take(uint64_t count) {
    if (failed_ || count > size_ - pos_) {
      failed_ = true;
      return nullptr;
    }
    const uint8_t* p = data_ + pos_;
    pos_ += count;
    return p;
  }

  void Skip(uint64_t count) { (void)Take(count); }

  uint8_t U8() {
    const uint8_t* p = Take(1);
    return (p == nullptr) ? 0 : p[0];
  }

  int8_t I8() { return static_cast<int8_t>(U8()); }

  uint16_t U16() {
    const uint8_t* p = Take(2);
    return (p == nullptr) ? 0 : Elf64ReadU16(p);
  }

  uint32_t U32() {
    const uint8_t* p = Take(4);
    return (p == nullptr) ? 0 : Elf64ReadU32(p);
  }

  uint64_t U64() {
    const uint8_t* p = Take(8);
    return (p == nullptr) ? 0 : Elf64ReadU64(p);
  }

  // An address of address_size (1 to 8) bytes; any other size fails.
  uint64_t Address(uint8_t address_size) {
    if (address_size == 0 || address_size > 8) {
      failed_ = true;
      return 0;
    }
    const uint8_t* p = Take(address_size);
    if (p == nullptr) {
      return 0;
    }
    uint64_t value = 0;
    for (unsigned i = 0; i < address_size; ++i) {
      value |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return value;
  }

  // Unsigned LEB128. Padding (extra 0x80 bytes) can take shift past 63, and shifting
  // by 64 or more is undefined, so bits past 63 are dropped and over ten bytes fail.
  uint64_t Uleb() {
    // Fast path: most ULEBs in IGC's .debug_line are one byte.
    if (!failed_ && pos_ < size_ && (data_[pos_] & 0x80) == 0) {
      return data_[pos_++];
    }
    uint64_t result = 0;
    unsigned shift = 0;
    for (;;) {
      const uint8_t byte = U8();
      if (failed_) {
        return 0;
      }
      if (shift < 64) {
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
      }
      if ((byte & 0x80) == 0) {
        break;
      }
      shift += 7;
      if (shift > 64) {
        failed_ = true;
        return 0;
      }
    }
    return result;
  }

  // Signed LEB128, sign-extended from wherever the encoding stopped.
  int64_t Sleb() {
    // Fast path, as in Uleb(): one byte, with bit 6 as the sign.
    if (!failed_ && pos_ < size_ && (data_[pos_] & 0x80) == 0) {
      const uint8_t byte = data_[pos_++];
      return static_cast<int64_t>(((byte & 0x40) != 0) ? (byte | ~static_cast<uint64_t>(0x7F))
                                                       : byte);
    }
    uint64_t result = 0;
    unsigned shift = 0;
    uint8_t byte = 0;
    do {
      byte = U8();
      if (failed_) {
        return 0;
      }
      if (shift < 64) {
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
      }
      shift += 7;
      if (shift > 71) {
        failed_ = true;
        return 0;
      }
    } while ((byte & 0x80) != 0);
    if (shift < 64 && (byte & 0x40) != 0) {
      result |= ~static_cast<uint64_t>(0) << shift;
    }
    return static_cast<int64_t>(result);
  }

  // A NUL-terminated string; fails if the terminator is not inside the buffer.
  bool String(std::string& out) {
    if (failed_) {
      return false;
    }
    const uint8_t* terminator = FindNul(data_ + pos_, size_ - pos_);
    if (terminator == nullptr) {
      failed_ = true;
      return false;
    }
    const uint64_t length = static_cast<uint64_t>(terminator - (data_ + pos_));
    out.assign(reinterpret_cast<const char*>(data_ + pos_), static_cast<size_t>(length));
    pos_ += length + 1;
    return true;
  }

 private:
  const uint8_t* data_ = nullptr;
  uint64_t size_ = 0;
  uint64_t pos_ = 0;
  bool failed_ = false;
};

// A table of NUL-terminated strings addressed by byte offset, such as the section-name
// table, .debug_str and .debug_line_str.
struct StringTableView {
  const uint8_t* data_ = nullptr;
  uint64_t size_ = 0;

  // Copies out the string at offset. False, with out untouched, if there is none.
  bool Get(uint64_t offset, std::string& out) const {
    const uint8_t* text = nullptr;
    uint64_t length = 0;
    if (!Find(offset, text, length)) {
      return false;
    }
    out.assign(reinterpret_cast<const char*>(text), static_cast<size_t>(length));
    return true;
  }

  // Is the string at offset equal to text? Like Get(), without the copy.
  bool Equals(uint64_t offset, const std::string& text) const {
    const uint8_t* found = nullptr;
    uint64_t length = 0;
    if (!Find(offset, found, length)) {
      return false;  // unterminated or out of bounds
    }
    return length == text.size() && std::memcmp(found, text.data(), text.size()) == 0;
  }

  // The string at offset, as a view into the table. Empty for both an empty string and
  // none; Get() tells them apart.
  std::string_view View(uint64_t offset) const {
    const uint8_t* text = nullptr;
    uint64_t length = 0;
    if (!Find(offset, text, length)) {
      return std::string_view();
    }
    return std::string_view(reinterpret_cast<const char*>(text), static_cast<size_t>(length));
  }

 private:
  // Locates the string at offset, bounded by the table: a truncated table need not
  // contain a NUL.
  bool Find(uint64_t offset, const uint8_t*& out_text, uint64_t& out_length) const {
    if (data_ == nullptr || offset >= size_) {
      return false;
    }
    const uint8_t* terminator = FindNul(data_ + offset, size_ - offset);
    if (terminator == nullptr) {
      return false;
    }
    out_text = data_ + offset;
    out_length = static_cast<uint64_t>(terminator - out_text);
    return true;
  }
};

// ---------------------------------------------------------------------------
// ELF64, only as far as the sections
// ---------------------------------------------------------------------------

// Field offsets from the gABI, used instead of structs so host alignment and padding
// do not matter.
constexpr size_t kElf64MagicSize = 4;         // \x7f E L F
constexpr size_t kElf64IdentSize = 16;        // e_ident, the whole prefix
constexpr size_t kElf64ClassOffset = 4;       // e_ident[EI_CLASS]
constexpr size_t kElf64DataOffset = 5;        // e_ident[EI_DATA]
constexpr uint8_t kElf64Class64 = 2;          // ELFCLASS64
constexpr uint8_t kElf64Data2Lsb = 1;         // ELFDATA2LSB

constexpr size_t kElf64HeaderSize = 64;
constexpr size_t kElf64MachineOffset = 0x12;    // e_machine,   u16
constexpr size_t kElf64ShoffOffset = 0x28;      // e_shoff,     u64
constexpr size_t kElf64ShentsizeOffset = 0x3A;  // e_shentsize, u16
constexpr size_t kElf64ShnumOffset = 0x3C;      // e_shnum,     u16
constexpr size_t kElf64ShstrndxOffset = 0x3E;   // e_shstrndx,  u16

constexpr size_t kElf64SectionHeaderSize = 64;
constexpr size_t kElf64ShNameOffset = 0x00;     // sh_name,    u32
constexpr size_t kElf64ShTypeOffset = 0x04;     // sh_type,    u32
constexpr size_t kElf64ShOffsetOffset = 0x18;   // sh_offset,  u64
constexpr size_t kElf64ShSizeOffset = 0x20;     // sh_size,    u64
constexpr size_t kElf64ShLinkOffset = 0x28;     // sh_link,    u32
constexpr size_t kElf64ShInfoOffset = 0x2C;     // sh_info,    u32
constexpr size_t kElf64ShEntSizeOffset = 0x38;  // sh_entsize, u64

// Elf64_Sym and Elf64_Rel/Elf64_Rela, for callers reading symbol or relocation tables.
constexpr uint64_t kElf64SymSize = 24;
constexpr size_t kElf64SymNameOffset = 0x00;   // st_name,  u32 (into sh_link's table)
constexpr size_t kElf64SymShndxOffset = 0x06;  // st_shndx, u16
constexpr size_t kElf64SymValueOffset = 0x08;  // st_value, u64
constexpr uint64_t kElf64RelaSize = 24;
constexpr uint64_t kElf64RelSize = 16;
constexpr size_t kElf64RelOffsetOffset = 0x00;   // r_offset, u64
constexpr size_t kElf64RelInfoOffset = 0x08;     // r_info,   u64
constexpr size_t kElf64RelaAddendOffset = 0x10;  // r_addend, i64 (SHT_RELA only)

constexpr uint32_t kElf64ShtNoBits = 8;    // SHT_NOBITS: occupies no file space
constexpr uint32_t kElf64ShtRela = 4;      // SHT_RELA
constexpr uint32_t kElf64ShtRel = 9;       // SHT_REL
constexpr uint16_t kElf64ShnUndef = 0;     // SHN_UNDEF
constexpr uint16_t kElf64ShnXindex = 0xFFFF;  // SHN_XINDEX: the real index is elsewhere

// One section, as a view into the buffer it was read from. Non-owning: valid only
// while that buffer is alive and unmodified.
struct Elf64Section {
  // A view into the section-name table, not NUL-terminated.
  std::string_view name_;
  uint32_t type_ = 0;
  uint32_t link_ = 0;
  uint32_t info_ = 0;
  uint64_t entry_size_ = 0;
  // As recorded in the header, even if outside the buffer, so a caller can report it.
  uint64_t file_offset_ = 0;
  uint64_t size_ = 0;
  // The contents, or nullptr for SHT_NOBITS or a range outside the buffer.
  const uint8_t* data_ = nullptr;

  bool HasContents() const { return data_ != nullptr && size_ != 0; }
};

// Why reading the sections failed. kNotElf is not always an error: a pre-ZeBin module
// binary is raw ISA with no container.
enum class Elf64Status {
  kOk,
  kNotElf,            // No ELF magic
  kNotElf64,          // Not ELFCLASS64
  kNotLittleEndian,   // Not ELFDATA2LSB
  kMalformed,         // Truncated, or a header that does not describe this file
};

// Reads the section header table into out_sections, indexed as in the file (entry 0 is
// the null section), since relocations and symbols refer to sections by index.
//
// A section whose range is outside the buffer is kept with no contents; the read fails
// only if the header or the section table is bad. A failure is reported on std::cerr
// as "[WARNING] ..."; the status lets the caller handle kNotElf.
inline Elf64Status ReadElf64Sections(const uint8_t* binary, size_t binary_size,
                                     std::vector<Elf64Section>& out_sections) {
  auto fail = [](Elf64Status status, const char* message) {
    std::cerr << "[WARNING] Cannot read ELF sections: " << message << std::endl;
    return status;
  };

  out_sections.clear();
  const uint64_t total = static_cast<uint64_t>(binary_size);
  if (binary == nullptr || total < kElf64HeaderSize) {
    return fail(Elf64Status::kNotElf, "binary is empty or too small to be an ELF container");
  }
  static const uint8_t kMagic[kElf64MagicSize] = {0x7F, 'E', 'L', 'F'};
  for (size_t i = 0; i < kElf64MagicSize; ++i) {
    if (binary[i] != kMagic[i]) {
      // a pre-ZeBin module binary is raw ISA
      return fail(Elf64Status::kNotElf, "binary is not an ELF container (raw ISA?)");
    }
  }
  // Only ELF64 little-endian is read. elf.md requires ELFDATA2LSB but allows ELFCLASS32,
  // which is refused: the ELF64 offsets above would silently misread it.
  if (binary[kElf64ClassOffset] != kElf64Class64) {
    return fail(Elf64Status::kNotElf64, "ELF container is not 64-bit");
  }
  if (binary[kElf64DataOffset] != kElf64Data2Lsb) {
    return fail(Elf64Status::kNotLittleEndian, "ELF container is not little-endian");
  }

  ByteCursor header(binary, total);
  header.Seek(kElf64ShoffOffset);
  const uint64_t table_offset = header.U64();
  header.Seek(kElf64ShentsizeOffset);
  const uint16_t entry_size = header.U16();
  uint64_t section_count = header.U16();
  uint32_t name_table_index = header.U16();
  if (!header.Ok()) {
    return fail(Elf64Status::kMalformed, "ELF header is truncated");
  }

  if (table_offset == 0) {
    return fail(Elf64Status::kMalformed, "ELF container has no section header table");
  }
  // Larger entries are allowed (the stride below honours them); smaller ones overlap.
  if (entry_size < kElf64SectionHeaderSize) {
    return fail(Elf64Status::kMalformed, "ELF section header entry is too small");
  }
  if (!Elf64RangeInBounds(table_offset, kElf64SectionHeaderSize, total)) {
    return fail(Elf64Status::kMalformed,
                "ELF section header table starts past the end of the binary");
  }

  // With 0xFF00 or more sections, or a name table index that does not fit in 16 bits,
  // the real values are in the null section's sh_size and sh_link.
  ByteCursor null_entry(binary, total);
  null_entry.Seek(table_offset + kElf64ShSizeOffset);
  const uint64_t null_size = null_entry.U64();
  null_entry.Seek(table_offset + kElf64ShLinkOffset);
  const uint32_t null_link = null_entry.U32();
  if (!null_entry.Ok()) {
    return fail(Elf64Status::kMalformed, "ELF null section header is truncated");
  }
  if (section_count == 0) {
    section_count = null_size;
  }
  if (name_table_index == kElf64ShnXindex) {
    name_table_index = null_link;
  }

  if (section_count == 0) {
    return fail(Elf64Status::kMalformed, "ELF container has no sections");
  }
  // Divide rather than multiply: a count from the null section's sh_size is 64 bits,
  // so count * entry_size can wrap. table_offset <= total was checked above.
  if (section_count > (total - table_offset) / entry_size) {
    return fail(Elf64Status::kMalformed,
                "ELF section header table runs past the end of the binary");
  }
  // Sections are found by name, so the name table is required.
  if (name_table_index == kElf64ShnUndef || name_table_index >= section_count) {
    return fail(Elf64Status::kMalformed, "ELF container has no valid section name table");
  }

  // Read the name table first, since it names the sections.
  const uint64_t name_table_header =
      table_offset + static_cast<uint64_t>(name_table_index) * entry_size;
  ByteCursor name_header(binary, total);
  name_header.Seek(name_table_header + kElf64ShOffsetOffset);
  const uint64_t name_table_offset = name_header.U64();
  name_header.Seek(name_table_header + kElf64ShSizeOffset);
  const uint64_t name_table_size = name_header.U64();
  if (!name_header.Ok() || name_table_size == 0 ||
      !Elf64RangeInBounds(name_table_offset, name_table_size, total)) {
    return fail(Elf64Status::kMalformed, "ELF section name table lies outside the binary");
  }
  const StringTableView name_table{binary + name_table_offset, name_table_size};

  out_sections.reserve(static_cast<size_t>(section_count));
  for (uint64_t i = 0; i < section_count; ++i) {
    const uint64_t base = table_offset + i * static_cast<uint64_t>(entry_size);
    // One bounds check for the whole entry covers every field read from it.
    if (!Elf64RangeInBounds(base, kElf64SectionHeaderSize, total)) {
      return fail(Elf64Status::kMalformed, "ELF section header is truncated");
    }
    const uint8_t* entry = binary + base;

    Elf64Section section;
    const uint32_t name_offset = Elf64ReadU32(entry + kElf64ShNameOffset);
    section.type_ = Elf64ReadU32(entry + kElf64ShTypeOffset);
    section.file_offset_ = Elf64ReadU64(entry + kElf64ShOffsetOffset);
    section.size_ = Elf64ReadU64(entry + kElf64ShSizeOffset);
    section.link_ = Elf64ReadU32(entry + kElf64ShLinkOffset);
    section.info_ = Elf64ReadU32(entry + kElf64ShInfoOffset);
    section.entry_size_ = Elf64ReadU64(entry + kElf64ShEntSizeOffset);

    // A nameless section is kept, so later indices do not shift.
    section.name_ = name_table.View(name_offset);

    // SHT_NOBITS occupies no file space, so it has no contents.
    if (section.type_ != kElf64ShtNoBits && section.size_ != 0 &&
        Elf64RangeInBounds(section.file_offset_, section.size_, total)) {
      section.data_ = binary + section.file_offset_;
    }
    out_sections.push_back(std::move(section));
  }
  return Elf64Status::kOk;
}

// Returns the table index of the first section named name, or -1.
inline int64_t FindElf64Section(const std::vector<Elf64Section>& sections,
                                std::string_view name) {
  for (size_t i = 0; i < sections.size(); ++i) {
    if (sections[i].name_ == name) {
      return static_cast<int64_t>(i);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
// ZeBin conventions
// ---------------------------------------------------------------------------

// The e_machine of a ZeBin. Not needed to find sections, but it tells a ZeBin that
// lacks a section apart from an ELF that is not a ZeBin.
constexpr uint16_t kElf64MachineIntelGt = 205;  // EM_INTELGT

// A ZeBin has one ".text.<mangled kernel name>" section per kernel, holding its code;
// the kernel's debug information refers to it by relocation. Code outside the kernel's
// section (e.g. external functions) is in the module's plain ".text", not read here.
constexpr const char* kElf64KernelTextSectionPrefix = ".text.";

// The name of a kernel's code section. Takes the mangled name.
inline std::string Elf64KernelTextSectionName(const std::string& mangled_kernel_name) {
  return std::string(kElf64KernelTextSectionPrefix) + mangled_kernel_name;
}

#endif  // PTI_TOOLS_UNITRACE_UTILS_ELF64_READER_H_
