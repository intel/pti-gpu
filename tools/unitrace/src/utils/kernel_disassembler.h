//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Kernel ISA Disassembly
//
// DisassembleKernelInstructions() turns a kernel's Gen ISA into IGA's whole-kernel
// listing, the same as "iga64 -d" prints. ConstructDefsAndUses() reads the listing's
// defs back as records. HexOffset() is the one way offsets are printed.
//
// A ".text.<kernel>" section is padded with zeros, which decode as "illegal", so the
// padding is cut off first (GenCodeSize()). That needs only an instruction's length:
// bit 29 of the first dword is the compaction bit, 8 bytes if set and 16 if not, on
// every platform IGA supports.
//
// Failures are reported on std::cerr as "[WARNING] ..." and return false: a kernel
// that will not disassemble costs its listing, not the trace.
//
// References:
//   iga.h, the IGA C API. The text IGA returns is owned by the context and invalidated
//   by the next call on it, so it is copied out immediately.
//
//==============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_KERNEL_DISASSEMBLER_H_
#define PTI_TOOLS_UNITRACE_UTILS_KERNEL_DISASSEMBLER_H_

#include <iga/iga.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "kernel_extractor.h"
#include "library_loader.h"

// A kernel-relative offset as "0x" and at least 8 hex digits, as the stall report's
// Offset and Dependency columns print it.
inline std::string HexOffset(uint64_t offset) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%08" PRIx64, offset);
  return buf;
}

// ---------------------------------------------------------------------------
// The encoding, only as far as an instruction's length
// ---------------------------------------------------------------------------

constexpr uint32_t kGenIsaCompactedSize = 8;
constexpr uint32_t kGenIsaUncompactedSize = 16;
// Compaction control: bit 29 of the first dword, on every platform.
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
// Device IP version -> IGA architecture
// ---------------------------------------------------------------------------

// A device IP version packs bitfields (compute-runtime NEO::HardwareIpVersion):
//
//   bits 31:22  architecture (10 bits)
//   bits 21:14  release       (8 bits)
//   bits 13:6   reserved
//   bits  5:0   revision      (6 bits)
//
// For example (compute-runtime's AOT product-config table):
//
//   0x03000000 = 12.0.0   TGL      Xe-LP
//   0x030c8004 = 12.50.4  XEHP_SDV Xe-HP
//   0x030dc008 = 12.55.8  DG2 G10  Xe-HPG
//   0x030f0007 = 12.60.7  PVC XT   Xe-HPC
//   0x0311c004 = 12.71.4  MTL H    Xe-LPG
//   0x05004000 = 20.1.0   BMG G21  Xe2-HPG
//   0x07800000 = 30.0.0   PTL H    Xe3-LPG

// Maps a device IP version (ze_device_ip_version_ext_t::ipVersion) to an iga_gen_t,
// as IGC's getIGAPlatform() does. Release ranges keep new steppings on the right
// decoder. IGA_GEN_INVALID for versions not covered.
constexpr iga_gen_t IpVersionToIgaGen(uint32_t ip_version) {
  const uint32_t architecture = (ip_version >> 22) & 0x3FF;
  const uint32_t release = (ip_version >> 14) & 0xFF;

  switch (architecture) {
    // Pre-Xe, best effort: only the base gen, not IGA's sub-platforms.
    case 8:  return IGA_GEN8;
    case 9:  return IGA_GEN9;
    case 10: return IGA_GEN10;
    case 11: return IGA_GEN11;
    case 12:
      // Xe; the release picks the sub-platform. IGA_GEN12p1 is the old name of IGA_XE,
      // kept for older libiga64.
      if (release < 50) return IGA_GEN12p1;  // 12.0-12.10 Xe-LP: TGL, RKL, ADL, DG1
      if (release < 55) return IGA_XE_HP;    // 12.50      Xe-HP: XEHP_SDV
      if (release < 60) return IGA_XE_HPG;   // 12.55-57   Xe-HPG: DG2 G10/G11/G12
      if (release < 62) return IGA_XE_HPC;   // 12.60-61   Xe-HPC: PVC, PVC VG
      // 12.70-74 Xe-LPG / Xe-LPG+: MTL, ARL. IGC decodes both as Xe-HPG.
      if (release >= 70 && release < 80) return IGA_XE_HPG;
      return IGA_GEN_INVALID;
    case 20: return IGA_XE2;       // 20.1 BMG G21, 20.2 BMG G31, 20.4 LNL
    case 30: return IGA_XE3;       // 30.0-1 PTL, 30.3 WCL, 30.4-5 NVL
    case 35: return IGA_XE3P_XPC;  // 35.11 CRI
    default: return IGA_GEN_INVALID;
  }
}

// Pins the mapping, using product configs from compute-runtime's AOT table.
static_assert(IpVersionToIgaGen(0x03000000) == IGA_GEN12p1, "12.0.0 TGL -> Xe-LP");
static_assert(IpVersionToIgaGen(0x0300c000) == IGA_GEN12p1, "12.3.0 ADL-P -> Xe-LP");
static_assert(IpVersionToIgaGen(0x03028000) == IGA_GEN12p1, "12.10.0 DG1 -> Xe-LP");
static_assert(IpVersionToIgaGen(0x030c8004) == IGA_XE_HP, "12.50.4 XEHP_SDV -> Xe-HP");
static_assert(IpVersionToIgaGen(0x030dc000) == IGA_XE_HPG, "12.55.0 DG2 G10 A0 -> Xe-HPG");
static_assert(IpVersionToIgaGen(0x030dc008) == IGA_XE_HPG, "12.55.8 DG2 G10 C0 -> Xe-HPG");
static_assert(IpVersionToIgaGen(0x030e4000) == IGA_XE_HPG, "12.57.0 DG2 G12 A0 -> Xe-HPG");
static_assert(IpVersionToIgaGen(0x030f0007) == IGA_XE_HPC, "12.60.7 PVC XT C0 -> Xe-HPC");
static_assert(IpVersionToIgaGen(0x030f4007) == IGA_XE_HPC, "12.61.7 PVC XT C0 VG -> Xe-HPC");
static_assert(IpVersionToIgaGen(0x0311c004) == IGA_XE_HPG, "12.71.4 MTL H B0 -> Xe-HPG");
static_assert(IpVersionToIgaGen(0x03128004) == IGA_XE_HPG, "12.74.4 ARL H B0 -> Xe-HPG");
static_assert(IpVersionToIgaGen(0x05004000) == IGA_XE2, "20.1.0 BMG G21 A0 -> Xe2");
static_assert(IpVersionToIgaGen(0x05010004) == IGA_XE2, "20.4.4 LNL B0 -> Xe2");
static_assert(IpVersionToIgaGen(0x07800000) == IGA_XE3, "30.0.0 PTL H A0 -> Xe3");
static_assert(IpVersionToIgaGen(0x07814004) == IGA_XE3, "30.5.4 NVL U B0 -> Xe3");
static_assert(IpVersionToIgaGen(0x08c2c000) == IGA_XE3P_XPC, "35.11.0 CRI A0 -> Xe3P");
static_assert(IpVersionToIgaGen(0) == IGA_GEN_INVALID, "unset ipVersion -> invalid");

// ---------------------------------------------------------------------------
// The iga64 library, loaded at runtime
// ---------------------------------------------------------------------------
//
// Loaded rather than linked, so unitrace is not tied to one iga64 version.

// The iga64 library and its entry points. One per process, reached through
// KernelDisassembler(); not constructible, copyable or movable otherwise.
class IgaDisassembler {
 public:
  // Signatures from iga.h. status_to_string_ is optional.
  iga_status_t (*context_create_)(const iga_context_options_t*, iga_context_t*) = nullptr;
  iga_status_t (*context_release_)(iga_context_t) = nullptr;
  iga_status_t (*disassemble_)(iga_context_t, const iga_disassemble_options_t*, const void*,
                               uint32_t, const char* (*)(int32_t, void*), void*, char**) = nullptr;
  const char* (*status_to_string_)(iga_status_t) = nullptr;
  bool available_ = false;
  // Why not available: no library, or missing entry points.
  std::string load_error_;

  IgaDisassembler(const IgaDisassembler&) = delete;
  IgaDisassembler& operator=(const IgaDisassembler&) = delete;
  IgaDisassembler(IgaDisassembler&&) = delete;
  IgaDisassembler& operator=(IgaDisassembler&&) = delete;

  // IGA's name for a status, or its number if iga64 cannot name it.
  std::string Describe(iga_status_t status) const {
    if (status_to_string_ != nullptr) {
      const char* text = status_to_string_(status);
      if (text != nullptr) {
        return text;
      }
    }
    return "IGA status " + std::to_string(static_cast<int>(status));
  }

 private:
  friend const IgaDisassembler& KernelDisassembler();

  // Loads the unversioned name only, which comes with the iga64 headers this is built
  // with; a versioned SONAME alone may be an unrelated IGA.
  IgaDisassembler() {
    try {
      loader_ = LibraryLoader(kLibraryName_);
    } catch (const std::exception& e) {
      // iga64 not being installed is not exceptional
      load_error_ = std::string("cannot load iga64 (") + e.what() + ")";
      return;
    }

    Resolve(context_create_, "iga_context_create");
    Resolve(context_release_, "iga_context_release");
    Resolve(disassemble_, "iga_context_disassemble");
    // optional, only for messages
    Resolve(status_to_string_, "iga_status_to_string");

    available_ = context_create_ != nullptr && context_release_ != nullptr &&
                 disassemble_ != nullptr;
    if (!available_) {
      load_error_ = std::string(kLibraryName_) +
                    " is missing the disassembly entry points this build needs";
    }
  }

  // Resolves symbol into entry_point, typed by the member.
  template <typename T>
  void Resolve(T& entry_point, const char* symbol) {
    entry_point = loader_.GetSymbol<T>(symbol);
  }

#if defined(_WIN32)
  static constexpr const char* kLibraryName_ = "iga64.dll";
#else
  static constexpr const char* kLibraryName_ = "libiga64.so";
#endif

  // Kept open, since the entry points are only valid while it is.
  LibraryLoader loader_;
};

// The process-wide handle, loaded once, thread-safely, on first use.
inline const IgaDisassembler& KernelDisassembler() {
  static const IgaDisassembler instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Disassembly
// ---------------------------------------------------------------------------

// Disassembles a kernel's Gen ISA, without its padding, into IGA's listing, each
// instruction prefixed with its kernel-relative PC ("/* [00A0] */"). gen is the
// architecture to decode as (see IpVersionToIgaGen()).
//
// with_deps (PRINT_DEPS) adds comment lines naming the registers each instruction
// writes and reads; it roughly doubles the listing and triples the time.
// with_defs (PRINT_DEFS) names, by "#<id>", the instructions producing each value an
// instruction reads ("// #12{r10}, IN{r2}", IN for a live-in), and adds the id to the
// prefix. It is a whole-kernel analysis and much slower than the plain listing.
//
// Returns true and fills out_text on success, including a partial listing after a
// decode error (with a warning). On failure warns and leaves out_text alone.
inline bool DisassembleKernelInstructions(const uint8_t* isa, size_t isa_size, iga_gen_t gen,
                                          std::string& out_text, bool with_deps = false,
                                          bool with_defs = false) {
  auto fail = [](const std::string& message) {
    std::cerr << "[WARNING] Unable to disassemble kernel: " << message << std::endl;
    return false;
  };

  if (isa == nullptr || isa_size == 0) {
    return fail("no ISA to disassemble");
  }
  if (gen == IGA_GEN_INVALID) {
    return fail("no architecture to disassemble for");
  }
  // IGA takes the size as a uint32_t.
  if (isa_size > UINT32_MAX) {
    return fail("the ISA is too large for IGA's disassembly API");
  }
  const uint64_t code_size = GenCodeSize(isa, isa_size);
  if (code_size == 0) {
    // all padding, or not even one whole instruction
    if (GenIsaPaddingStart(isa, isa_size) == 0) {
      return fail("the ISA is all zeros: padding, with no code in it");
    }
    return fail("the ISA holds no complete instruction (is it Gen ISA?)");
  }

  const IgaDisassembler& iga = KernelDisassembler();
  if (!iga.available_) {
    return fail(iga.load_error_);
  }

  // One context per call: contexts are not thread-safe and own the returned text.
  iga_context_options_t context_options = IGA_CONTEXT_OPTIONS_INIT(gen);
  iga_context_t context = nullptr;
  const iga_status_t created = iga.context_create_(&context_options, &context);
  if (created != IGA_SUCCESS || context == nullptr) {
    return fail("iga_context_create failed for this architecture: " + iga.Describe(created));
  }

  // Not IGA_DISASSEMBLE_OPTIONS_INIT(), which leaves base_pc_offset out and trips
  // -Wextra. PRINT_BFNEXPRS matches "iga64 -d".
  iga_disassemble_options_t options = {};
  options.cb = sizeof(options);
  options.formatting_opts = IGA_FORMATTING_OPTS_DEFAULT | IGA_FORMATTING_OPT_PRINT_PC |
                            IGA_FORMATTING_OPT_PRINT_BFNEXPRS;
  if (with_deps) {
    options.formatting_opts |= IGA_FORMATTING_OPT_PRINT_DEPS;
  }
  if (with_defs) {
    options.formatting_opts |= IGA_FORMATTING_OPT_PRINT_DEFS;
  }
  options.decoder_opts = IGA_DECODING_OPTS_DEFAULT;

  char* text = nullptr;
  // No label callback: IGA's labels are already kernel-relative.
  const iga_status_t status = iga.disassemble_(
      context, &options, isa, static_cast<uint32_t>(code_size), nullptr, nullptr, &text);
  std::string listing = (text != nullptr) ? text : "";
  iga.context_release_(context);

  if (listing.empty()) {
    return fail("IGA produced no disassembly: " + iga.Describe(status));
  }
  out_text = std::move(listing);
  if (status != IGA_SUCCESS) {
    std::cerr << "[WARNING] IGA did not decode the whole kernel, the listing stops early: "
              << iga.Describe(status) << std::endl;
  }
  return true;
}

// As above, with the architecture from the view's product config. Without one it
// fails; a caller can put the device's ipVersion, packed the same way, in
// architecture_.product_config_.
inline bool DisassembleKernelInstructions(const KernelBinaryView& isa, std::string& out_text,
                                          bool with_deps = false, bool with_defs = false) {
  return DisassembleKernelInstructions(isa.data_, isa.size_,
                                       IpVersionToIgaGen(isa.architecture_.product_config_),
                                       out_text, with_deps, with_defs);
}

// ---------------------------------------------------------------------------
// Reading the defs back out of a listing
// ---------------------------------------------------------------------------

// The offset of a def that names no instruction (0 is the first instruction).
constexpr uint64_t kNoInstructionOffset = UINT64_MAX;

// One producer of a value an instruction reads.
struct DefinedBy {
  // The producer's kernel-relative PC; kNoInstructionOffset for a live-in or an id
  // with no line in the listing.
  uint64_t offset_ = kNoInstructionOffset;
  // The producer's "#<id>"; 0 for a live-in.
  uint32_t id_ = 0;
  // The value is live into the kernel ("IN"), e.g. the thread payload in r0.
  bool live_in_ = false;
  // The registers as the listing prints them, e.g. "r127[8-11]", "r9:2", "r32".
  std::string registers_;
};

// One instruction and its defs.
struct UsedBy {
  // Its kernel-relative PC, the listing's "[<hex>]".
  uint64_t offset_ = 0;
  // Its "#<id>", which other instructions' defs refer to.
  uint32_t id_ = 0;
  // The instruction text, without the prefix or the defs comment, with whitespace runs
  // collapsed to one space.
  std::string text_;
  // The producers of the values it reads, in listing order. Empty if there are none
  // or the listing was made without with_defs.
  std::vector<DefinedBy> defs_;
};

// One kernel's instructions in listing (offset) order, indexed by id and by offset.
struct DefsAndUses {
  std::vector<UsedBy> instructions_;
  std::map<uint32_t, size_t> index_by_id_;
  std::map<uint64_t, size_t> index_by_offset_;

  // The instruction with this id, or nullptr.
  const UsedBy* FindById(uint32_t id) const {
    const auto it = index_by_id_.find(id);
    return (it == index_by_id_.end()) ? nullptr : &instructions_[it->second];
  }

  // The instruction starting at this offset, or nullptr.
  const UsedBy* FindByOffset(uint64_t offset) const {
    const auto it = index_by_offset_.find(offset);
    return (it == index_by_offset_.end()) ? nullptr : &instructions_[it->second];
  }

  bool Empty() const { return instructions_.empty(); }
};

// The value of a hex digit, or -1.
inline int HexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

// Reads the instructions and defs out of a DisassembleKernelInstructions() listing:
//
//   /*  #78 [0390]  */ (W)  send.gtwy (1|M0) ... {EOT,I@1,$7} //  #77{r127}; wr:1+0
//   \______ PRINT_PC ______/                                  \_ PRINT_DEFS _/
//
// Every line with the prefix becomes a record, defs or not; other lines are skipped.
// The defs are "#<id>{<registers>}" or "IN{<registers>}", separated by ", "; a send's
// commentary after ";" is dropped, as is anything unrecognised.
//
// Returns true and fills out_defs if the listing has at least one instruction;
// otherwise warns and returns false.
inline bool ConstructDefsAndUses(const std::string& listing, DefsAndUses& out_defs) {
  out_defs = DefsAndUses();

  size_t line_begin = 0;
  while (line_begin < listing.size()) {
    size_t line_end = listing.find('\n', line_begin);
    if (line_end == std::string::npos) {
      line_end = listing.size();
    }
    const std::string line = listing.substr(line_begin, line_end - line_begin);
    line_begin = line_end + 1;

    // The PRINT_PC prefix "/*  #12 [00A0]  */", first on an instruction's line.
    const size_t prefix_begin = line.find_first_not_of(" \t");
    if (prefix_begin == std::string::npos || line.compare(prefix_begin, 2, "/*") != 0) {
      continue;
    }
    const size_t prefix_end = line.find("*/", prefix_begin);
    if (prefix_end == std::string::npos) {
      continue;
    }

    // The id, in decimal after "#".
    const size_t id_begin = line.find('#', prefix_begin);
    if (id_begin == std::string::npos || id_begin > prefix_end) {
      continue;
    }
    size_t at = id_begin + 1;
    uint32_t id = 0;
    while (at < line.size() && line[at] >= '0' && line[at] <= '9') {
      id = id * 10 + static_cast<uint32_t>(line[at] - '0');
      ++at;
    }
    if (at == id_begin + 1) {
      continue;
    }

    // The offset, in hex inside "[...]", with no "0x".
    const size_t offset_begin = line.find('[', at);
    if (offset_begin == std::string::npos || offset_begin > prefix_end) {
      continue;
    }
    const size_t offset_end = line.find(']', offset_begin);
    if (offset_end == std::string::npos || offset_end > prefix_end) {
      continue;
    }
    uint64_t offset = 0;
    bool offset_read = offset_end > offset_begin + 1;
    for (size_t i = offset_begin + 1; i < offset_end; ++i) {
      const int digit = HexDigit(line[i]);
      if (digit < 0) {
        offset_read = false;
        break;
      }
      offset = (offset << 4) | static_cast<uint64_t>(digit);
    }
    if (!offset_read) {
      continue;
    }

    UsedBy instruction;
    instruction.offset_ = offset;
    instruction.id_ = id;

    // The defs comment, which also ends the instruction text.
    const size_t comment = line.find("//", prefix_end);

    // The instruction text, between the prefix and the comment.
    const size_t text_end = (comment == std::string::npos) ? line.size() : comment;
    for (size_t i = prefix_end + 2; i < text_end; ++i) {
      const char c = line[i];
      if (c == ' ' || c == '\t' || c == '\r') {
        if (!instruction.text_.empty() && instruction.text_.back() != ' ') {
          instruction.text_ += ' ';
        }
      } else {
        instruction.text_ += c;
      }
    }
    const size_t text_last = instruction.text_.find_last_not_of(' ');
    instruction.text_.resize((text_last == std::string::npos) ? 0 : text_last + 1);

    if (comment != std::string::npos) {
      std::string defs = line.substr(comment + 2);
      // Drop a send's commentary, after the first ";".
      const size_t descriptor = defs.find(';');
      if (descriptor != std::string::npos) {
        defs.resize(descriptor);
      }

      // Each entry is a name followed by "{...}".
      size_t brace = 0;
      while ((brace = defs.find('{', brace)) != std::string::npos) {
        const size_t brace_end = defs.find('}', brace);
        if (brace_end == std::string::npos) {
          break;
        }

        // The name: back to the previous ",", trimmed.
        size_t name_begin = defs.rfind(',', brace);
        name_begin = (name_begin == std::string::npos) ? 0 : name_begin + 1;
        const size_t name_first = defs.find_first_not_of(" \t", name_begin);
        std::string name;
        if (name_first != std::string::npos && name_first < brace) {
          name = defs.substr(name_first, brace - name_first);
          const size_t name_last = name.find_last_not_of(" \t");
          name.resize((name_last == std::string::npos) ? 0 : name_last + 1);
        }

        DefinedBy def;
        def.registers_ = defs.substr(brace + 1, brace_end - brace - 1);
        brace = brace_end + 1;

        if (name == "IN") {
          def.live_in_ = true;
        } else if (name.size() > 1 && name[0] == '#') {
          uint32_t producer_id = 0;
          bool all_digits = true;
          for (size_t i = 1; i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') {
              all_digits = false;
              break;
            }
            producer_id = producer_id * 10 + static_cast<uint32_t>(name[i] - '0');
          }
          if (!all_digits) {
            continue;
          }
          def.id_ = producer_id;
        } else {
          continue;
        }
        instruction.defs_.push_back(std::move(def));
      }
    }

    const size_t index = out_defs.instructions_.size();
    out_defs.instructions_.push_back(std::move(instruction));
    // insert(): if an id or offset repeats, the first line wins.
    out_defs.index_by_id_.insert({id, index});
    out_defs.index_by_offset_.insert({offset, index});
  }

  if (out_defs.instructions_.empty()) {
    std::cerr << "[WARNING] Unable to read the defs of a kernel: the listing names no "
                 "instruction"
              << std::endl;
    return false;
  }

  // Fill the producers' offsets now: a def can name a later instruction (a loop's
  // back edge).
  for (UsedBy& instruction : out_defs.instructions_) {
    for (DefinedBy& def : instruction.defs_) {
      if (def.live_in_) {
        continue;
      }
      const UsedBy* producer = out_defs.FindById(def.id_);
      if (producer != nullptr) {
        def.offset_ = producer->offset_;
      }
    }
  }
  return true;
}

#endif  // PTI_TOOLS_UNITRACE_UTILS_KERNEL_DISASSEMBLER_H_
