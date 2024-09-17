//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "section_debug_info.hpp"

#include <cstring>

#include "leb128.h"
#include "pti_assert.h"

using namespace elf_parser;

DebugInfoParser::DebugInfoParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {
  if (data_ == nullptr || size_ < sizeof(uint32_t)) {
    return;
  }

  unit_length_from_beginning_ = *(reinterpret_cast<const uint32_t*>(data));
  if (unit_length_from_beginning_ > 0xfffffff0) {
    if (unit_length_from_beginning_ != 0xffffffff ||
        size_ < (sizeof(uint32_t) /*0xffffffff*/ + sizeof(uint64_t) /*unit_length*/ +
                 sizeof(uint16_t) /*version*/)) {
      return;
    }
    bitness_ = 64;
    unit_length_from_beginning_ =
        *(reinterpret_cast<const uint64_t*>(data + sizeof(uint32_t))) + sizeof(uint64_t);
    version_ = *(reinterpret_cast<const uint16_t*>(data + sizeof(uint32_t) + sizeof(uint64_t)));
  } else {
    if (size_ < sizeof(uint32_t) /*unit_length*/ + sizeof(uint16_t) /*version*/) {
      return;
    }
    bitness_ = 32;
    unit_length_from_beginning_ += sizeof(uint32_t);  // length field not included sizeof itself
    version_ = *(reinterpret_cast<const uint16_t*>(data + sizeof(uint32_t)));
  }

  if (unit_length_from_beginning_ > size_) {
    return;
  }
  if (version_ != DWARF_VERSION5 && version_ != DWARF_VERSION4) {
    return;
  }

  if (version_ == DWARF_VERSION4) {
    switch (bitness_) {
      case 32:
        ProcessDwarf4Header<Dwarf4CompUnitHeader32>();
        break;
      case 64:
        ProcessDwarf4Header<Dwarf4CompUnitHeader64>();
        break;
      default:
        return;
    }
  } else if (version_ == DWARF_VERSION5) {
    switch (bitness_) {
      case 32:
        ProcessDwarf5Header<Dwarf5CompUnitHeader32>();
        break;
      case 64:
        ProcessDwarf5Header<Dwarf5CompUnitHeader64>();
        break;
      default:
        return;
    }
  } else {
    return;
  }
}

const char* DebugInfoParser::GetCompDir(const DwarfCompUnitMap& comp_unit_map) {
  if (!IsValid()) {
    return nullptr;
  }

  const uint8_t* ptr = data_ + data_offset_;

  bool done = false;
  uint32_t abbrev_number = 0;
  ptr = utils::leb128::Decode32(ptr, abbrev_number, done);
  PTI_ASSERT(done);

  // Assumed the first TAG is DW_TAG_compile_unit
  auto it = comp_unit_map.find(abbrev_number);
  PTI_ASSERT(it != comp_unit_map.end());
  const std::vector<DwarfAttribute>& attribute_list = it->second;

  /// TODO: Add support for other TAGs
  for (size_t i = 0; i < attribute_list.size(); ++i) {
    switch (attribute_list[i].form) {
      case DW_FORM_addr:
        // case DW_FORM_strp:
        // case DW_FORM_ref_addr:
        ptr += address_size_;
        break;
      case DW_FORM_data1:
        // case DW_FORM_ref1:
        // case DW_FORM_flag:
        ptr += sizeof(uint8_t);
        break;
      case DW_FORM_data2:
        // case DW_FORM_ref2:
        ptr += sizeof(uint16_t);
        break;
      case DW_FORM_data4:
        // case DW_FORM_ref4:
        ptr += sizeof(uint32_t);
        break;
      case DW_FORM_data8:
        // case DW_FORM_ref8:
        // case DW_FORM_ref_sig8:
        ptr += sizeof(uint64_t);
        break;
        // case DW_FORM_data16:
        //   ptr += sizeof(uint128_t);
        break;
      case DW_FORM_string: {
        const char* value = reinterpret_cast<const char*>(ptr);
        ptr += strlen(value) + 1 /*NULL*/;
        if (attribute_list[i].attribute == DW_AT_comp_dir) {
          // comp_dir = value;
          return value;
        }
        break;
      }
      case DW_FORM_sec_offset:
        PTI_ASSERT(attribute_list[i].attribute == DW_AT_stmt_list);
        ptr += sizeof(uint32_t);
        break;
        // case DW_FORM_block1:
        // case DW_FORM_block2:
        // case DW_FORM_block4:
        // case DW_FORM_block:
        // case DW_FORM_sdata:
        // case DW_FORM_udata
        // case DW_FORM_ref_udata:

      default:
        PTI_ASSERT(0);  // Not implemented
        break;
    }
  }

  return nullptr;
}

template <typename Dwarf4CompUnitHeaderT>
void DebugInfoParser::ProcessDwarf4Header() {
  if (size_ < sizeof(Dwarf4CompUnitHeaderT)) {
    return;
  }
  if (bitness_ != 32 && bitness_ != 64) {
    return;
  }

  const Dwarf4CompUnitHeaderT* header = reinterpret_cast<const Dwarf4CompUnitHeaderT*>(data_);

  if (header->version != DWARF_VERSION4) {
    return;
  }

  data_offset_ = sizeof(Dwarf4CompUnitHeaderT);

  if (size_ < data_offset_) {
    return;
  }

  debug_abbrev_offset_ = header->debug_abbrev_offset;

  address_size_ = header->address_size;

  is_valid_ = true;
  return;
}

template <typename Dwarf5CompUnitHeaderT>
void DebugInfoParser::ProcessDwarf5Header() {
  if (size_ < sizeof(Dwarf5CompUnitHeaderT)) {
    return;
  }
  if (bitness_ != 32 && bitness_ != 64) {
    return;
  }

  const Dwarf5CompUnitHeaderT* header = reinterpret_cast<const Dwarf5CompUnitHeaderT*>(data_);

  if (header->version != DWARF_VERSION5) {
    return;
  }

  switch (header->type) {
    case 1:  // DW_UT_compile
    case 3:  // DW_UT_partial
      data_offset_ = sizeof(Dwarf5CompUnitHeaderT);
      break;
    case 2:  // DW_UT_type
    case 6:  // DW_UT_split_type
      data_offset_ =
          sizeof(Dwarf5CompUnitHeaderT) + 8 /*type_signature*/ + bitness_ / 8 /*type_offset*/;
      break;
    case 4:  // DW_UT_skeleton
    case 5:  // DW_UT_split_compile
      data_offset_ = sizeof(Dwarf5CompUnitHeaderT) + 8 /*dwo_id*/;
      break;
    default:
      return;
      break;
  }

  if (size_ < data_offset_) {
    return;
  }

  debug_abbrev_offset_ = header->debug_abbrev_offset;

  address_size_ = header->address_size;

  is_valid_ = true;
  return;
}
