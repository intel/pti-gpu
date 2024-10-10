//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file elf_parser.hpp
 * @brief This file contains the declaration of the C++ parser for GPU ELF files.
 */

#ifndef PTI_ELF_PARSER_HPP_
#define PTI_ELF_PARSER_HPP_

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "elf_parser_def.hpp"
#include "elf_parser_mapping.h"

namespace elf_parser {

class ElfParser {
 public:
  /**
   * @brief Constructs an ElfParser object.
   * This constructor initializes an ElfParser object with the provided ELF binary data and size.
   * memory is managed by caller, but should be available until ElfParser object is destroyed.
   * ElfParser object does not copy the data.
   *
   * @param data A pointer to the ELF binary data.
   * @param size The size of the ELF binary data.
   */
  ElfParser(const uint8_t* data, uint32_t size);
  ~ElfParser() = default;

  // copy ctor and move constructors
  ElfParser(const ElfParser& other) = delete;
  ElfParser& operator=(const ElfParser& other) = delete;
  ElfParser(ElfParser&& other) = delete;
  ElfParser& operator=(ElfParser& other) = delete;

  static bool IsValid(const uint8_t* data, uint32_t size);
  bool IsValid() const;

  /**
   * @brief Retrieves the names of all the kernels in the ELF file. Provides number of found kernels
   * and pointers to kernel names in the original data. If names == nullptr set kernel_num to kernel
   * number. Otherwise (names != nullptr) fill names array with pointers to null terminated strings
   * in original binary - kernel names. names array (array of pointers) should be allocated and
   * freed by caller. kernel name strings are located in elf binary data and will be freed with the
   * original binary data.
   *
   */
  pti_result GetKernelNames(uint32_t num_entries, const char** names, uint32_t* num_names) const;
  std::vector<std::string> GetKernelNames() const;

  /**
   * @brief Retrieves the pointer to the binary data for the specified kernel in original Elf data.
   * Provides the size of the binary data for the specified kernel.
   * The binary data is located in the original binary data and will be freed with the original
   * binary data.
   */
  std::pair<const uint8_t* /*ptr*/, uint32_t /*size*/> GetKernelBinary(uint32_t kernel_index) const;
  std::pair<const uint8_t* /*ptr*/, uint32_t /*size*/> GetKernelBinary(
      std::string kernel_name) const;
  uint64_t GetKernelAddress(uint32_t kernel_index) const;
  uint64_t GetKernelAddress(std::string kernel_name) const;

  uint32_t GetKernelIndex(std::string kernel_name) const;

  uint32_t GetGfxCore() const;

  /**
   * @brief Retrieves the source mapping data for the specified kernel in dwarf state machine matrix
   * format. Returns emtpy vector in case of error
   */
  std::vector<SourceMapping> GetSourceMappingMatrix(uint32_t kernel_index);

  /**
   * @brief Get the Source Mapping object, map provided for addresses with step of
   * MIN_INSTRUCTION_SIZE (8 bytes) For performance sensetive cases, use GetSourceMappingMatrix
   * instead. In case of error returns empty map
   * @return std::map<uint64_t, SourceMapping> - assembly<->source mapping
   */
  std::map<uint64_t /*address*/, SourceMapping> GetSourceMapping(uint32_t kernel_index);
  std::map<uint64_t /*address*/, SourceMapping> GetSourceMapping(std::string kernel_name);

  std::vector<SourceMapping> GetSourceMappingNonCached(uint32_t kernel_index) const;

 private:
  struct Section {
    const uint8_t* data;
    const uint64_t size;
    const uint64_t addr;
    const uint32_t name_offset;
    const char* name;
  };

  template <typename Header, typename SectionHeader>
  static bool IsValidHeader(const uint8_t* data, uint32_t size);
  template <typename Header, typename SectionHeader, typename SymtabEntryT>
  bool Init();

  // returns Section structure, which describes elf section with name section_name
  const Section GetSection(const char* section_name) const;
  // returns pointer to section data and size of section in bytes
  std::pair<const uint8_t* /*ptr*/, uint32_t /*size*/> GetSectionData(
      const char* section_name) const;
  std::vector<RelaEntry> GetRelaForKernel(const char* section_name, uint32_t kernel_index,
                                          bool zero_addend = true) const;

  template <typename RelaEntryT>
  std::vector<RelaEntry> GetRelaForKernelBitness(const char* section_name, uint32_t kernel_index,
                                                 bool zero_addend) const;

  static RelaEntry ConstructRelaEntry(RelaEntry64 entry);
  static RelaEntry ConstructRelaEntry(RelaEntry32 entry);

  const uint8_t* data_;
  const uint32_t size_;
  uint32_t address_width_ = 0;

  std::unordered_map<uint32_t, const char*> strtab_records_;
  std::vector<const char*> kernel_names_;
  std::unordered_map<uint32_t, uint32_t> kernel_name_offset_map_;
  std::vector<SymtabEntry> symtab_;
  std::unordered_map<std::string, const Section> sections_;
  std::unordered_map<uint32_t, std::vector<SourceMapping>> source_mapping_;

  bool initialized_ = false;
};

}  // namespace elf_parser

#endif  // PTI_ELF_PARSER_HPP_