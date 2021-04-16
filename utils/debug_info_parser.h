//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_DEBUG_INFO_PARSER_H_
#define PTI_UTILS_DEBUG_INFO_PARSER_H_

#include "dwarf.h"
#include "leb128.h"
#include "pti_assert.h"

class DebugInfoParser {
 public:
  DebugInfoParser(const uint8_t* data, uint32_t size) :
      data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Dwarf32CompUnitHeader)) {
      return false;
    }

    const Dwarf32CompUnitHeader* header =
      reinterpret_cast<const Dwarf32CompUnitHeader*>(data_);
    if (header->version != DWARF_VERSION) {
      return false;
    }

    return true;
  }

  std::string GetCompDir(const DwarfCompUnitMap& comp_unit_map) {
    if (!IsValid()) {
      return std::string();
    }

    const uint8_t* comp_unit_ptr = data_;
    while (comp_unit_ptr < data_ + size_) {
      const Dwarf32CompUnitHeader* header =
        reinterpret_cast<const Dwarf32CompUnitHeader*>(comp_unit_ptr);
      const uint8_t* ptr = comp_unit_ptr + sizeof(Dwarf32CompUnitHeader);
      comp_unit_ptr += sizeof(uint32_t);

      bool done = false;
      uint32_t abbrev_number = 0;
      ptr = utils::leb128::Decode32(ptr, abbrev_number, done);
      PTI_ASSERT(done);
      PTI_ASSERT(abbrev_number > 0);

      // Assumed the first TAG is DW_TAG_compile_unit
      auto it = comp_unit_map.find(abbrev_number);
      PTI_ASSERT(it != comp_unit_map.end());
      const std::vector<DwarfAttribute>& attribute_list = it->second;

      const char* comp_dir = nullptr;
      for (size_t i = 0; i < attribute_list.size(); ++i) {
        switch (attribute_list[i].form) {
          case DW_FORM_addr:
            ptr += header->address_size;
            break;
          case DW_FORM_data1:
            ptr += sizeof(uint8_t);
            break;
          case DW_FORM_data2:
            ptr += sizeof(uint16_t);
            break;
          case DW_FORM_data4:
            ptr += sizeof(uint32_t);
            break;
          case DW_FORM_data8:
            ptr += sizeof(uint64_t);
            break;
          case DW_FORM_string: {
            const char* value = reinterpret_cast<const char*>(ptr);
            ptr += strlen(value) + 1;
            if (attribute_list[i].attribute == DW_AT_comp_dir) {
              comp_dir = value;
            }
            break;
          }
          case DW_FORM_sec_offset:
            PTI_ASSERT(attribute_list[i].attribute == DW_AT_stmt_list);
            ptr += sizeof(uint32_t);
            break;
          default:
            PTI_ASSERT(0); // Not implemented
            break;
        }
      }

      if (comp_dir != nullptr) {
        return comp_dir;
      }

      comp_unit_ptr += header->unit_length;
    }

    return std::string();
  }

 private:
  const uint8_t* data_;
  uint32_t size_;
};

#endif // PTI_UTILS_DEBUG_INFO_PARSER_H_