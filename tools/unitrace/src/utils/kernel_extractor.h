//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Kernel ISA Extraction
//
// Given a module's native binary (zeModuleGetNativeBinary()), GetKernelBinary() finds a
// named kernel's ISA and the architecture it was compiled for. The ISA is the kernel's
// ".text.<mangled name>" section, without its padding (GenCodeSize()); the architecture
// is in IGC's compatibility notes. Container reading is elf64_reader.h's.
//
// The ISA is a view into the caller's buffer, which must outlive it.
//
// References:
//   Intel Graphics Compiler, the ZEBIN specification (elf.md):
//   https://github.com/intel/intel-graphics-compiler/tree/master/IGC/ZEBinWriter/zebin/spec
//
//==============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_KERNEL_EXTRACTOR_H_
#define PTI_TOOLS_UNITRACE_UTILS_KERNEL_EXTRACTOR_H_

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "elf64_reader.h"

// ---------------------------------------------------------------------------
// Which architecture the code was built for
// ---------------------------------------------------------------------------
//
// Recorded in ".note.intelgt.compat" as standard ELF notes (name size, descriptor size,
// type, name, descriptor) with owner "IntelGT". elf.md does not specify padding; IGC pads
// to 4 bytes, not the gABI's 8 for ELF64.

constexpr const char* kIntelGtNoteSectionName = ".note.intelgt.compat";
constexpr const char* kIntelGtNoteOwner = "IntelGT";
constexpr uint64_t kIntelGtNoteAlignment = 4;

// The NT_INTELGT_* note types from elf.md. Only the three naming an architecture are
// decoded.
constexpr uint32_t kIntelGtNoteProductFamily = 1;   // PRODUCT_FAMILY, u32
constexpr uint32_t kIntelGtNoteGfxCoreFamily = 2;   // GFXCORE_FAMILY, u32
constexpr uint32_t kIntelGtNoteTargetMetadata = 3;  // TargetMetadata bitfield, u32
constexpr uint32_t kIntelGtNoteZebinVersion = 4;    // "<major>.<minor>", NUL-terminated
constexpr uint32_t kIntelGtNoteVisaAbiVersion = 5;  // u32; absent unless stack calls
constexpr uint32_t kIntelGtNoteProductConfig = 6;   // AOT product config, u32
constexpr uint32_t kIntelGtNoteIndirectAccessDetectionVersion = 7;   // u32
constexpr uint32_t kIntelGtNoteIndirectAccessBufferMajorVersion = 8;  // u32

// What a module binary was built for, as IGC recorded it. 0 means not recorded (0 is
// never a valid value).
struct GenArchitecture {
  // GFXCORE_FAMILY, e.g. IGFX_GEN12LP_CORE = 18, IGFX_XE_HPC_CORE = 0x0C08
  // (igfxfmid.h, not included).
  uint32_t gfx_core_family_ = 0;
  // PRODUCT_FAMILY, e.g. IGFX_TIGERLAKE_LP = 29, IGFX_PVC = 1271.
  uint32_t product_family_ = 0;
  // The AOT product config, packed like ze_device_ip_version_ext_t::ipVersion, and
  // superseding the two above. IpVersionToIgaGen() (kernel_disassembler.h) takes it as
  // is, so the binary alone picks the disassembler.
  uint32_t product_config_ = 0;

  bool Valid() const {
    return gfx_core_family_ != 0 || product_family_ != 0 || product_config_ != 0;
  }
};

// Decodes the compatibility notes from already-read sections. Returns true and fills
// out_architecture if any value was recorded; otherwise returns false and leaves it
// untouched. binary/binary_size is the buffer the sections came from, used only for
// e_machine. warn_when_absent reports a failure on std::cerr.
inline bool ReadGenArchitectureNotes(const uint8_t* binary, size_t binary_size,
                                     const std::vector<Elf64Section>& sections,
                                     GenArchitecture& out_architecture, bool warn_when_absent) {
  auto fail = [warn_when_absent](const std::string& message) {
    if (warn_when_absent) {
      std::cerr << "[WARNING] Cannot read the architecture of the module binary: " << message
                << std::endl;
    }
    return false;
  };

  const int64_t index = FindElf64Section(sections, kIntelGtNoteSectionName);
  if (index < 0) {
    // Safe: ReadElf64Sections() has checked the header is complete.
    ByteCursor header(binary, static_cast<uint64_t>(binary_size));
    header.Seek(kElf64MachineOffset);
    const uint16_t machine = header.U16();
    if (!header.Ok() || machine != kElf64MachineIntelGt) {
      return fail("ELF container is not a ZeBin (e_machine is not EM_INTELGT)");
    }
    return fail(std::string("ZeBin holds no \"") + kIntelGtNoteSectionName + "\" section");
  }
  const Elf64Section& section = sections[index];
  if (!section.HasContents()) {
    return fail(section.size_ == 0
                    ? "the compatibility note section is empty"
                    : "the compatibility note section lies outside the module binary");
  }

  // Accumulated locally so a failure leaves the caller's value alone. The first of a
  // repeated note wins.
  GenArchitecture found;
  auto keep = [](uint32_t& slot, uint32_t value) {
    if (slot == 0) {
      slot = value;
    }
  };

  ByteCursor notes(section.data_, section.size_);
  bool truncated = false;
  while (!notes.Done()) {
    const uint32_t name_size = notes.U32();
    const uint32_t descriptor_size = notes.U32();
    const uint32_t type = notes.U32();
    // Padded in 64 bits, since rounding up a size near 2^32 overflows 32 bits.
    const uint64_t padded_name = (static_cast<uint64_t>(name_size) + kIntelGtNoteAlignment - 1) &
                                 ~(kIntelGtNoteAlignment - 1);
    const uint64_t padded_descriptor =
        (static_cast<uint64_t>(descriptor_size) + kIntelGtNoteAlignment - 1) &
        ~(kIntelGtNoteAlignment - 1);
    const uint8_t* name = notes.Take(padded_name);
    const uint8_t* descriptor = notes.Take(padded_descriptor);
    if (!notes.Ok()) {
      // Keep the notes already read.
      truncated = true;
      break;
    }
    // The owner name is NUL-terminated and its size includes the NUL.
    if (name_size == 0 || name[name_size - 1] != 0 ||
        std::string(reinterpret_cast<const char*>(name), name_size - 1) != kIntelGtNoteOwner) {
      continue;  // another owner's note
    }
    // The architecture notes are all u32.
    if (descriptor_size != sizeof(uint32_t)) {
      continue;
    }
    const uint32_t value = Elf64ReadU32(descriptor);
    switch (type) {
      case kIntelGtNoteGfxCoreFamily:
        keep(found.gfx_core_family_, value);
        break;
      case kIntelGtNoteProductFamily:
        keep(found.product_family_, value);
        break;
      case kIntelGtNoteProductConfig:
        keep(found.product_config_, value);
        break;
      default:
        // the other types name no architecture
        break;
    }
  }

  if (!found.Valid()) {
    return fail(truncated ? "the compatibility notes are truncated"
                          : "the compatibility notes record no architecture");
  }
  out_architecture = found;
  return true;
}

// ---------------------------------------------------------------------------
// The encoding, only as far as an instruction's length
// ---------------------------------------------------------------------------

constexpr uint32_t kGenIsaCompactedSize = 8;
constexpr uint32_t kGenIsaUncompactedSize = 16;
// Compaction control: bit 29 of the first dword, on every platform IGA supports.
constexpr uint32_t kGenIsaCompactionBit = 29;

// The encoded length of the instruction at 'instruction', whose first four bytes must
// be readable. Read byte by byte, since the buffer may be unaligned.
inline uint32_t GenIsaInstructionLength(const uint8_t* instruction) {
  const uint32_t word = static_cast<uint32_t>(instruction[0]) |
                        (static_cast<uint32_t>(instruction[1]) << 8) |
                        (static_cast<uint32_t>(instruction[2]) << 16) |
                        (static_cast<uint32_t>(instruction[3]) << 24);
  return ((word >> kGenIsaCompactionBit) & 1u) != 0 ? kGenIsaCompactedSize
                                                    : kGenIsaUncompactedSize;
}

// Where the trailing run of zero bytes (IGC's alignment padding) begins, or isa_size
// if the last byte is not zero.
inline size_t GenIsaPaddingStart(const uint8_t* isa, size_t isa_size) {
  if (isa == nullptr) {
    return 0;
  }
  size_t start = isa_size;
  while (start > 0 && isa[start - 1] == 0) {
    --start;
  }
  return start;
}

// Whether the instruction has nothing but an opcode: a bare "nop" or all zeros, which
// after the kernel's last instruction is padding. Checked by encoding, not opcode
// number, since opcodes differ across platforms. 'length' must be the instruction's
// own length, with all its bytes readable.
inline bool GenIsaIsPaddingInstruction(const uint8_t* instruction, uint32_t length) {
  // Every bit after the opcode byte must be clear, except the compaction bit.
  constexpr uint8_t kCompactionByteBit = 1u << (kGenIsaCompactionBit - 24);
  if (instruction[1] != 0 || instruction[2] != 0 ||
      (instruction[3] & static_cast<uint8_t>(~kCompactionByteBit)) != 0) {
    return false;
  }
  for (uint32_t i = 4; i < length; ++i) {
    if (instruction[i] != 0) {
      return false;
    }
  }
  return true;
}

// The size of the code in an ISA buffer: the end of the last whole instruction that
// is not padding. Walks instruction lengths from the start, since the zero run can
// begin inside an instruction and a trailing nop is padding too. 0 if there is no
// code.
inline uint64_t GenCodeSize(const uint8_t* isa, size_t isa_size) {
  const size_t padding_start = GenIsaPaddingStart(isa, isa_size);
  uint64_t at = 0;
  uint64_t code_end = 0;
  while (at < padding_start && at + kGenIsaCompactedSize <= isa_size) {
    const uint32_t length = GenIsaInstructionLength(isa + at);
    if (length > isa_size - at) {
      break;
    }
    if (!GenIsaIsPaddingInstruction(isa + at, length)) {
      code_end = at + length;
    }
    at += length;
  }
  return code_end;
}

// ---------------------------------------------------------------------------
// One kernel's code, and what decodes it
// ---------------------------------------------------------------------------

// One kernel's ISA, as a view into its module binary, with the binary's architecture.
// Valid only while that buffer is alive and unmodified.
struct KernelBinaryView {
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
  // The section the ISA came from (".text.<mangled name>").
  std::string section_name_;
  // What the module was compiled for; not Valid() if the binary does not record it.
  GenArchitecture architecture_;

  // The code size in bytes (GenCodeSize()), the extent IPs are matched against. 0 if
  // never filled, or if the section holds no code.
  size_t Size() const { return size_; }
};

// Finds the ISA of the kernel with mangled name kernel_name in a module binary. Returns
// true and fills out_kernel_binary on success; a missing architecture is not a failure.
// Returns false, leaving out_kernel_binary untouched, if:
//   - the buffer is not an ELF64 little-endian container (a pre-ZeBin binary is raw ISA)
//   - the container is malformed or truncated
//   - no section holds the kernel's code
//   - the section has no bytes: SHT_NOBITS, empty, or outside the buffer
// Failures are reported on std::cerr as "[WARNING] ...", the first two by
// ReadElf64Sections().
inline bool GetKernelBinary(const uint8_t* binary, size_t binary_size,
                            const std::string& kernel_name, KernelBinaryView& out_kernel_binary) {
  auto fail = [&kernel_name](const std::string& message) {
    std::cerr << "[WARNING] Cannot get the binary of kernel " << kernel_name << ": " << message
              << std::endl;
    return false;
  };

  if (kernel_name.empty()) {
    // not through fail(), which prints the name
    std::cerr << "[WARNING] Cannot get a kernel's binary: no kernel name to look up" << std::endl;
    return false;
  }

  std::vector<Elf64Section> sections;
  const Elf64Status status = ReadElf64Sections(binary, binary_size, sections);
  if (status != Elf64Status::kOk) {
    // already reported by ReadElf64Sections()
    return false;
  }

  const std::string want = Elf64KernelTextSectionName(kernel_name);
  const int64_t index = FindElf64Section(sections, want);
  if (index < 0) {
    return fail("no section holds this kernel's code");
  }

  const Elf64Section& section = sections[index];
  if (section.type_ == kElf64ShtNoBits) {
    return fail("the kernel's section occupies no space in the module binary");
  }
  if (section.size_ == 0) {
    return fail("the kernel's section is empty");
  }
  if (section.data_ == nullptr) {
    return fail("the kernel's section lies outside the module binary");
  }

  // Into a local, so a binary with no architecture leaves the default rather than a
  // reused view's old value. Silent: the architecture is optional.
  GenArchitecture architecture;
  (void)ReadGenArchitectureNotes(binary, binary_size, sections, architecture, false);

  // The narrowing is safe: the section is inside a buffer of size_t bytes.
  out_kernel_binary.data_ = section.data_;
  out_kernel_binary.size_ = static_cast<size_t>(
      GenCodeSize(section.data_, static_cast<size_t>(section.size_)));
  out_kernel_binary.section_name_ = want;
  out_kernel_binary.architecture_ = architecture;
  return true;
}

#endif  // PTI_TOOLS_UNITRACE_UTILS_KERNEL_EXTRACTOR_H_
