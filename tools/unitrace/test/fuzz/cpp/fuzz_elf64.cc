//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzz harness for the ELF64/ZeBin parser chain:
//   elf64_reader.h  -> ReadElf64Sections, FindElf64Section
//   kernel_extractor.h -> ReadGenArchitectureNotes, GetKernelBinary
//   source_resolver.h  -> CollectRelocations, ParseLineUnit, KernelLineTable
//
// Feeds arbitrary bytes as a module binary and exercises every parsing layer.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "elf64_reader.h"
#include "kernel_extractor.h"
#include "source_resolver.h"

// libFuzzer entry point: one input is one module native binary from the driver.
//   data, size  the binary (ELF64/ZeBin), any bytes
// Runs it through every layer unitrace parses a module with: the section table, the
// architecture notes, the kernel binary lookup, the relocations and DWARF line units
// of .debug_line, and KernelLineTable::Build() for the first .text.<kernel>.
// Returns 0; parse errors are expected, a crash or sanitizer report is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Layer 1: section table
  std::vector<Elf64Section> sections;
  Elf64Status status = ReadElf64Sections(data, size, sections);

  if (status != Elf64Status::kOk) {
    return 0;
  }

  // Layer 2: architecture notes
  GenArchitecture arch;
  (void)ReadGenArchitectureNotes(data, size, sections, arch, false);

  // Layer 2b: look up common section names
  (void)FindElf64Section(sections, ".text");
  (void)FindElf64Section(sections, ".debug_line");
  (void)FindElf64Section(sections, ".debug_str");
  (void)FindElf64Section(sections, ".debug_line_str");
  (void)FindElf64Section(sections, ".note.intelgt.compat");

  // Layer 3: try to extract a kernel binary for a dummy kernel name
  KernelBinaryView kernel_view;
  (void)GetKernelBinary(data, size, "fuzz_kernel", kernel_view);

  // Layer 4: if .debug_line exists, exercise the DWARF line-table decoder
  int64_t debug_line_idx = FindElf64Section(sections, ".debug_line");
  if (debug_line_idx >= 0 && sections[debug_line_idx].data_ != nullptr) {
    const Elf64Section &debug_line = sections[debug_line_idx];

    StringTableView debug_str{};
    int64_t ds = FindElf64Section(sections, ".debug_str");
    if (ds >= 0) {
      debug_str.data_ = sections[ds].data_;
      debug_str.size_ = sections[ds].size_;
    }

    StringTableView debug_line_str{};
    int64_t dls = FindElf64Section(sections, ".debug_line_str");
    if (dls >= 0) {
      debug_line_str.data_ = sections[dls].data_;
      debug_line_str.size_ = sections[dls].size_;
    }

    // Collect relocations for all .text.* sections
    for (size_t i = 0; i < sections.size(); ++i) {
      if (sections[i].name_.substr(0, 6) == ".text.") {
        std::string text_name(sections[i].name_);
        Relocations relocations;
        CollectRelocations(sections, static_cast<uint32_t>(debug_line_idx),
                           static_cast<uint32_t>(i), text_name, debug_line,
                           relocations);
      }
    }

    // Parse every line unit
    ByteCursor cursor(debug_line.data_, debug_line.size_);
    while (!cursor.Done()) {
      LineUnit unit;
      bool fatal = false;
      (void)ParseLineUnit(cursor, debug_str, debug_line_str, nullptr, unit, fatal);
      if (fatal) {
        break;
      }
    }
  }

  // Layer 5: full KernelLineTable::Build (exercises the whole chain end-to-end)
  // Try with a few plausible kernel names from sections we found.
  for (const auto &section : sections) {
    if (section.name_.size() > 6 &&
        section.name_.substr(0, 6) == ".text.") {
      std::string kernel_name(section.name_.substr(6));
      KernelLineTable table;
      (void)table.Build(data, size, kernel_name);
      break;
    }
  }

  return 0;
}
