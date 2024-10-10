//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file elf_parser.cpp
 * @brief Implementation for the elf_parser.c and elf_parser.cpp interfaces.
 */

#include "elf_parser.h"

#include <string.h>

#include <iomanip>
#include <iostream>

#include "elf_parser.hpp"
#include "pti_assert.h"
#include "section_debug_abbrev.hpp"
#include "section_debug_info.hpp"
#include "section_debug_line.hpp"

#define TEXT_PREFIX ".text."

using namespace elf_parser;

pti_result ptiElfParserCreate(const uint8_t* data, uint32_t size, elf_parser_handle_t* parser) {
  if (data == nullptr || size == 0) return PTI_ERROR_BAD_ARGUMENT;

  ElfParser* parser_ = new ElfParser(data, size);

  if (parser_ == nullptr) return PTI_ERROR_INTERNAL;
  *parser = reinterpret_cast<elf_parser_handle_t*>(parser_);

  if (!reinterpret_cast<ElfParser*>(*parser)->IsValid()) {
    delete parser_;
    return PTI_ERROR_INTERNAL;
  }
  return PTI_SUCCESS;
}

pti_result ptiElfParserDestroy(elf_parser_handle_t* parser) {
  if (parser == nullptr) return PTI_ERROR_BAD_ARGUMENT;

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(*parser);
  if (parser_->IsValid() == false) {
    return PTI_ERROR_BAD_ARGUMENT;
  }
  delete parser_;
  *parser = nullptr;
  return PTI_SUCCESS;
}

pti_result ptiElfParserIsValid(elf_parser_handle_t parser, bool* is_valid) {
  if (is_valid == nullptr) return PTI_ERROR_BAD_ARGUMENT;

  if (parser == nullptr) {
    *is_valid = false;
    return PTI_ERROR_BAD_ARGUMENT;
  }

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(parser);

  *is_valid = parser_->IsValid();
  return PTI_SUCCESS;
}

pti_result ptiElfParserGetKernelNames(elf_parser_handle_t parser, uint32_t num_entries,
                                      const char** names, uint32_t* num_names) {
  if (parser == nullptr) return PTI_ERROR_BAD_ARGUMENT;

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(parser);
  if (parser_->IsValid() == false) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  return parser_->GetKernelNames(num_entries, names, num_names);
}

pti_result ptiElfParserGetSourceMapping(elf_parser_handle_t parser, uint32_t kernel_index,
                                        uint32_t num_entries, SourceMapping* mappings,
                                        uint32_t* num_mappings) {
  if (parser == nullptr) return PTI_ERROR_BAD_ARGUMENT;

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(parser);
  if (parser_->IsValid() == false || kernel_index >= parser_->GetKernelNames().size()) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  auto mapping_ = parser_->GetSourceMappingMatrix(kernel_index);

  const uint32_t mapping_size = mapping_.size();
  if (num_mappings != nullptr) {
    *num_mappings = mapping_size;
  }
  if (mapping_size == 0) {
    return PTI_DEBUG_INFO_NOT_FOUND;
  }

  if (mappings == nullptr) {
    return PTI_SUCCESS;
  }

  if (num_entries == 0) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  const uint32_t entries_to_copy = std::min(num_entries, static_cast<uint32_t>(mapping_.size()));

  for (uint32_t i = 0; i < entries_to_copy; i++) {
    mappings[i] = mapping_[i];
  }

  return PTI_SUCCESS;
}

pti_result ptiElfParserGetBinaryPtr(elf_parser_handle_t parser, uint32_t kernel_index,
                                    const uint8_t** binary, uint32_t* binary_size,
                                    uint64_t* kernel_address) {
  if (parser == nullptr || binary_size == nullptr || binary == nullptr) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(parser);
  if (parser_->IsValid() == false) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  if (kernel_index >= parser_->GetKernelNames().size()) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  auto binary_ = parser_->GetKernelBinary(kernel_index);

  if (binary_.first == nullptr || binary_.second == 0) {
    *binary_size = 0;
    return PTI_ERROR_INTERNAL;
  }

  if (kernel_address != nullptr) {
    *kernel_address = parser_->GetKernelAddress(kernel_index);
  }

  *binary_size = binary_.second;
  *binary = binary_.first;
  return PTI_SUCCESS;
}

pti_result ptiElfParserGetGfxCore(/*IN*/ elf_parser_handle_t parser, /*OUT*/ uint32_t* gfx_core) {
  if (parser == nullptr || gfx_core == nullptr) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  ElfParser* parser_ = reinterpret_cast<ElfParser*>(parser);
  if (parser_->IsValid() == false) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  *gfx_core = parser_->GetGfxCore();

  return PTI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// ElfParser implementation

ElfParser::ElfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {
  if (!ElfParser::IsValid(data_, size_)) {
    return;
  }

  const ElfHeaderIndent* headerIdent = reinterpret_cast<const ElfHeaderIndent*>(data_);

  switch (headerIdent->class_) {
    case ELFCLASS32: {
      this->address_width_ = 32;
      if (!Init<ElfHeader32, ElfSectionHeader32, SymtabEntry32>()) {
        return;
      };
      break;
    }
    case ELFCLASS64: {
      this->address_width_ = 64;
      if (!Init<ElfHeader64, ElfSectionHeader64, SymtabEntry64>()) {
        return;
      };
      break;
    }
    default:
      return;
  }
  this->initialized_ = true;
}

bool ElfParser::IsValid(const uint8_t* data, uint32_t size) {
  if (data == nullptr || size < sizeof(ElfHeaderIndent)) return false;

  const ElfHeaderIndent* headerIdent = reinterpret_cast<const ElfHeaderIndent*>(data);
  if (headerIdent->mag0 != ELF_MAGIC_NUMBER || headerIdent->mag1 != 'E' ||
      headerIdent->mag2 != 'L' || headerIdent->mag3 != 'F')
    return false;

  if (!(headerIdent->class_ == ELFCLASS32 || headerIdent->class_ == ELFCLASS64)) return false;

  if (headerIdent->data != ELFDATA2LSB) return false;  // Only little-endian is supported

  switch (headerIdent->class_) {
    case ELFCLASS32: {
      if (!IsValidHeader<ElfHeader32, ElfSectionHeader32>(data, size)) return false;
      break;
    }
    case ELFCLASS64: {
      if (!IsValidHeader<ElfHeader64, ElfSectionHeader64>(data, size)) return false;
      break;
    }
    default:
      return false;
  }
  return true;
}

bool ElfParser::IsValid() const {
  return this->initialized_ && ElfParser::IsValid(this->data_, this->size_);
}

pti_result ElfParser::GetKernelNames(uint32_t num_entries, const char** names,
                                     uint32_t* num_names) const {
  if (num_names != nullptr) {
    *num_names = kernel_names_.size();
  }

  if (names == nullptr) {
    return PTI_SUCCESS;
  }

  if (num_entries == 0) {
    return PTI_ERROR_BAD_ARGUMENT;
  }

  uint32_t entries_to_copy = std::min(num_entries, static_cast<uint32_t>(kernel_names_.size()));

  for (uint32_t i = 0; i < entries_to_copy; i++) {
    names[i] = kernel_names_[i];
  }

  return PTI_SUCCESS;
}

std::vector<std::string> ElfParser::GetKernelNames() const {
  std::vector<std::string> kernel_names;
  for (auto kernel_name : kernel_names_) {
    kernel_names.push_back(kernel_name);
  }
  return kernel_names;
}

std::pair<const uint8_t*, uint32_t> ElfParser::GetKernelBinary(uint32_t kernel_index) const {
  if (kernel_index >= kernel_names_.size()) {
    return std::pair<const uint8_t*, uint32_t>(nullptr, 0);
  }

  return this->GetSectionData((std::string(TEXT_PREFIX) + kernel_names_[kernel_index]).c_str());
}

std::pair<const uint8_t*, uint32_t> ElfParser::GetKernelBinary(std::string kernel_name) const {
  return this->GetSectionData((std::string(TEXT_PREFIX) + kernel_name).c_str());
}

uint64_t ElfParser::GetKernelAddress(uint32_t kernel_index) const {
  if (kernel_index >= kernel_names_.size()) {
    return -1;
  }
  const auto& section =
      GetSection((std::string(TEXT_PREFIX) + kernel_names_[kernel_index]).c_str());
  if (section.data == nullptr) {
    return -1;
  }

  return section.addr;
};

uint64_t ElfParser::GetKernelAddress(std::string kernel_name) const {
  const auto& section = GetSection(kernel_name.c_str());
  if (section.data == nullptr) {
    return -1;
  }

  return section.addr;
};

uint32_t ElfParser::GetKernelIndex(std::string kernel_name) const {
  for (uint32_t i = 0; i < kernel_names_.size(); i++) {
    if (kernel_name.compare(kernel_names_[i]) == 0) {
      return i;
    }
  }
  return -1;
}

uint32_t ElfParser::GetGfxCore() const {
  const auto& section = GetSection(".note.intelgt.compat");
  if (section.data == nullptr) {
    return 0;  // IGFX_UNKNOWN_CORE
  }

  const uint32_t name_len = strlen("IntelGT");
  for (uint32_t i = 0; i < section.size;) {
    const auto* noteHeader = reinterpret_cast<const ElfNote*>(section.data + i);
    if (noteHeader->type == NT_INTELGT_GFXCORE_FAMILY &&
        noteHeader->name_size == name_len + 1  // including null terminator
        && noteHeader->desc_size == sizeof(uint32_t) &&
        strncmp(reinterpret_cast<const char*>(noteHeader) + sizeof(ElfNote), "IntelGT", name_len) ==
            0) {
      uint32_t core = *reinterpret_cast<const uint32_t*>(
          reinterpret_cast<const uint8_t*>(noteHeader) + sizeof(ElfNote) + name_len + 1);
      return core;
    }
    i += sizeof(ElfNote) + noteHeader->name_size + noteHeader->desc_size;
    if (i % 4 != 0) i += 4 - i % 4;  // padding
  }
  return 0;  // IGFX_UNKNOWN_CORE
}

std::vector<SourceMapping> ElfParser::GetSourceMappingMatrix(uint32_t kernel_index) {
  if (source_mapping_.find(kernel_index) == source_mapping_.end()) {
    source_mapping_[kernel_index] = GetSourceMappingNonCached(kernel_index);
  }
  return source_mapping_[kernel_index];
}

std::map<uint64_t, SourceMapping> ElfParser::GetSourceMapping(uint32_t kernel_index) {
  auto mapping = this->GetSourceMappingMatrix(kernel_index);
  if (mapping.size() == 0) {
    return {};
  }

  uint64_t min_address = mapping.front().address;
  uint64_t max_address = mapping.back().address;

  std::map<uint64_t, SourceMapping> mapping_out;

  uint32_t index = 0;
  for (uint64_t address = min_address; address < max_address; address += MIN_INSTRUCTION_SIZE) {
    if (mapping_out.find(address) != mapping_out.end()) {  // unexpected
      return {};
    }

    while (mapping[index].address < address && index < mapping.size()) {
      index++;
    }

    mapping_out.emplace(address, mapping[index]);
    auto lastElement = --mapping_out.end();
    lastElement->second.address = address;
  }

  return mapping_out;
}

std::map<uint64_t, SourceMapping> ElfParser::GetSourceMapping(std::string kernel_name) {
  return ElfParser::GetSourceMapping(GetKernelIndex(std::move(kernel_name)));
}

template <typename Header, typename SectionHeader>
bool ElfParser::IsValidHeader(const uint8_t* data, uint32_t size) {
  if (data == nullptr) return false;

  if (size < sizeof(Header)) return false;

  const Header* header = reinterpret_cast<const Header*>(data);
  if (header->machine != EM_INTELGT || header->version == 0 || header->shoff == 0 ||
      header->shentsize == 0u || header->shentsize != sizeof(SectionHeader) ||
      header->shoff +
              static_cast<uint64_t>(header->shentsize) * static_cast<uint64_t>(header->shnum) >
          size)
    return false;

  const SectionHeader* sectionHeader = reinterpret_cast<const SectionHeader*>(data + header->shoff);

  for (uint i = 0; i < header->shnum; i++) {
    if (sectionHeader[i].offset + sectionHeader[i].size > size) return false;
  }

  return true;
}

template <typename Header, typename SectionHeader, typename SymtabEntryT>
bool ElfParser::Init() {
  if (!ElfParser::IsValid(data_, size_)) {
    return false;
  }

  /* header */
  const Header* header = reinterpret_cast<const Header*>(data_);

  /* read strtab + kernel names */
  strtab_records_.clear();
  const SectionHeader* sectionHeader =
      reinterpret_cast<const SectionHeader*>(data_ + header->shoff);
  const SectionHeader* strtabSectionHeader = &sectionHeader[header->shstrndx];
  const char* strtab_section_cstr =
      reinterpret_cast<const char*>(&data_[strtabSectionHeader->offset]);

  if (strtabSectionHeader->offset + strtabSectionHeader->size >= size_) {
    return false;
  }

  uint string_start = 0;
  for (uint i = 0; i < strtabSectionHeader->size; i++) {
    if (strtab_section_cstr[i] == '\0') {
      i++;
      const char* section_name_cstr = &strtab_section_cstr[string_start];
      strtab_records_.emplace(string_start, section_name_cstr);

      std::string section_name(&strtab_section_cstr[string_start]);
      if (strlen(TEXT_PREFIX) <= strlen(section_name_cstr) &&
          strncmp(section_name_cstr, TEXT_PREFIX, strlen(TEXT_PREFIX)) == 0) {
        kernel_names_.push_back(&strtab_section_cstr[string_start] + strlen(TEXT_PREFIX));
        kernel_name_offset_map_.emplace(string_start, kernel_names_.size() - 1);
      }
      string_start = i;
    }
  }

  for (uint i = 0; i < header->shnum; i++) {
    sections_.emplace(
        strtab_records_[sectionHeader[i].name],
        Section{data_ + sectionHeader[i].offset, sectionHeader[i].size, sectionHeader[i].addr,
                sectionHeader[i].name, strtab_records_[sectionHeader[i].name]});
  }

  auto section_symtab = this->GetSectionData(".symtab");
  if (section_symtab.first == nullptr || section_symtab.second == 0) {
    return false;
  }

  const SymtabEntryT* entries = reinterpret_cast<const SymtabEntryT*>(section_symtab.first);
  for (uint i = 0; i * sizeof(SymtabEntryT) < section_symtab.second; i++) {
    SymtabEntry entry;
    entry.name = entries[i].name;
    entry.info = entries[i].info;
    entry.bind = entry.info >> 4;
    entry.type = entry.info & 0xf;
    entry.other = entries[i].other;
    entry.shndx = entries[i].shndx;
    entry.value = entries[i].value;
    entry.size = entries[i].size;
    symtab_.push_back(entry);
  }

  return true;
}

std::vector<SourceMapping> ElfParser::GetSourceMappingNonCached(uint32_t kernel_index) const {
  if (kernel_index >= kernel_names_.size()) {
    return {};
  }

  // get from rela debug info the relocation for kernel_index
  std::vector<RelaEntry> rela_debug_info =
      this->GetRelaForKernel(".rela.debug_info", kernel_index, true);

  // get section debug info
  auto section_debug_info = this->GetSectionData(".debug_info");
  if (section_debug_info.first == nullptr) {
    return {};
  }

  // get unit which is related to kernel_index
  std::unordered_map<const uint8_t* /*unit pointer*/, uint32_t /*unit size*/> debug_info_units;

  for (uint32_t i = 0; i < section_debug_info.second;) {
    const uint8_t* ptr = section_debug_info.first + i;
    auto ddip = DebugInfoParser(ptr, section_debug_info.second - i);
    if (!ddip.IsValid()) {
      return {};
    }
    uint32_t size = ddip.GetUnitLength();
    if (size == -1) {
      return {};
    }

    for (const auto& rela : rela_debug_info) {
      if (rela.offset > i && rela.offset < i + size /* in boundaries */) {
        debug_info_units[ptr] = size;
      }
    }
    i += size;
  }

  if (debug_info_units.size() !=
      1) {  // more than one unit for kernel found - cannot handle such case
    return {};
  }
  auto debug_info_unit = debug_info_units.begin();

  // get comp dir related to kernel_index
  auto ddip = DebugInfoParser(debug_info_unit->first, debug_info_unit->second);
  if (!ddip.IsValid()) {
    return {};
  }

  // get debug abbrev
  auto section_debug_abbrev = this->GetSectionData(".debug_abbrev");
  if (section_debug_abbrev.first == nullptr) {
    return {};
  }

  uint64_t debug_abbrev_offset = ddip.GetDebugAbbrevOffset();

  DebugAbbrevParser dap(section_debug_abbrev.first + debug_abbrev_offset,
                        section_debug_abbrev.second - debug_abbrev_offset);
  if (!dap.IsValid()) {
    return {};
  }

  const char* comp_dir = ddip.GetCompDir(dap.GetCompUnitMap());
  if (comp_dir == nullptr) {
    return {};
  }

  // get from section rela debug line the relocation for kernel_index
  std::vector<RelaEntry> rela_debug_line =
      this->GetRelaForKernel(".rela.debug_line", kernel_index, true);

  // get section debug info
  auto section_debug_line = this->GetSectionData(".debug_line");
  if (section_debug_line.first == nullptr) {
    return {};
  }

  // get unit which is related to kernel_index
  std::unordered_map<uint64_t /*offset*/, uint64_t /*unit size*/> debug_line_units;
  for (uint64_t offset = 0; offset < section_debug_line.second;) {
    const uint8_t* ptr = section_debug_line.first + offset;
    auto ddip = DwarfDebugLineParser(section_debug_line.first, offset,
                                     section_debug_line.second - offset, this->address_width_);
    if (!ddip.IsValid()) {
      return {};
    }
    uint64_t size = ddip.GetUnitLength();
    if (size == -1) {
      return {};
    }

    for (const auto& rela : rela_debug_line) {
      if (rela.offset > offset && rela.offset < offset + size /* in boundaries */
          && debug_line_units.find(offset) == debug_line_units.end()) {
        debug_line_units[offset] = size;
      }
    }
    offset += size;
  }

  if (debug_line_units.size() !=
      1) {  // more than one unit for kernel found - cannot handle such case
    return {};
  }
  auto debug_line_unit = debug_line_units.begin();

  DwarfDebugLineParser dlp(section_debug_line.first, debug_line_unit->first,
                           debug_line_unit->second, this->address_width_);
  if (!dlp.IsValid()) {
    return {};
  }

  auto mapping = dlp.GetMapping(comp_dir);
  if (mapping.size() == 0) {
    return {};
  }
  return mapping;
}

const ElfParser::Section ElfParser::GetSection(const char* section_name) const {
  if (section_name == nullptr) {  // assert instead?
    return Section{nullptr, 0, 0, 0, 0};
  }
  auto section = sections_.find(section_name);
  if (section == sections_.end()) {
    return Section{nullptr, 0, 0, 0, 0};
  }
  return section->second;
}

std::pair<const uint8_t*, uint32_t> ElfParser::GetSectionData(const char* section_name) const {
  const auto& section = GetSection(section_name);
  if (section.data == nullptr) {
    return std::pair<const uint8_t*, uint32_t>(nullptr, 0);
  }
  return std::make_pair(section.data, section.size);
}

RelaEntry ElfParser::ConstructRelaEntry(RelaEntry64 entry) {
  RelaEntry result;
  result.offset = entry.offset;
  result.info = entry.info;
  result.sym = entry.info >> 32;
  result.type = entry.info & 0xffffffff;
  result.addend = entry.addend;
  return result;
}

RelaEntry ElfParser::ConstructRelaEntry(RelaEntry32 entry) {
  RelaEntry result;
  result.offset = entry.offset;
  result.info = entry.info;
  result.sym = entry.info >> 8;
  result.type = entry.info & 0xff;
  result.addend = entry.addend;
  return result;
}

template <typename RelaEntryT>
inline std::vector<RelaEntry> ElfParser::GetRelaForKernelBitness(const char* section_name,
                                                                 uint32_t kernel_index,
                                                                 bool zero_addend) const {
  auto section_rela = this->GetSectionData(section_name);
  if (section_rela.first == nullptr || section_rela.second == 0) {
    return std::vector<RelaEntry>();
  }

  std::vector<RelaEntry> rela;

  const RelaEntryT* entries = reinterpret_cast<const RelaEntryT*>(section_rela.first);
  for (uint i = 0; i * sizeof(entries[i]) < section_rela.second; i++) {
    RelaEntry rela_entry = ElfParser::ConstructRelaEntry(entries[i]);
    if (zero_addend && rela_entry.addend != 0) {
      continue;
    }
    auto kernel_id_it = kernel_name_offset_map_.find(symtab_[rela_entry.sym].name);
    if (kernel_id_it == kernel_name_offset_map_.end()) {
      continue;
    }
    if (kernel_id_it->second != kernel_index) continue;
    rela.push_back(rela_entry);
  }

  return rela;
}

std::vector<RelaEntry> ElfParser::GetRelaForKernel(const char* section_name, uint32_t kernel_index,
                                                   bool zero_addend) const {
  switch (this->address_width_) {
    case 32:
      return this->GetRelaForKernelBitness<RelaEntry32>(section_name, kernel_index, zero_addend);
    case 64:
      return this->GetRelaForKernelBitness<RelaEntry64>(section_name, kernel_index, zero_addend);
  }

  return std::vector<RelaEntry>();
}
