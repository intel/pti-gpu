//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_DEBUG_ABBREV_PARSER_H_
#define PTI_UTILS_DEBUG_ABBREV_PARSER_H_

#include "dwarf.h"
#include "leb128.h"
#include "pti_assert.h"

class DebugAbbrevParser {
 public:
  DebugAbbrevParser(const uint8_t* data, uint32_t size) :
      data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ == 0) {
      return false;
    }

    return true;
  }

  DwarfCompUnitMap GetCompUnitMap() const {
    if (!IsValid()) {
      return DwarfCompUnitMap();
    }

    DwarfCompUnitMap comp_unit_map;

    const uint8_t* ptr = data_;
    while (ptr < data_ + size_) {
      bool done = false;
      uint32_t abbrev_number = 0;
      ptr = utils::leb128::Decode32(ptr, abbrev_number, done);
      PTI_ASSERT(done);
      if (abbrev_number == 0) {
        break;
      } else {
        PTI_ASSERT(ptr < data_ + size_);
      }

      uint32_t tag = 0;
      ptr = utils::leb128::Decode32(ptr, tag, done);
      PTI_ASSERT(done);
      PTI_ASSERT(ptr < data_ + size_);

      ++ptr; // has_children

      std::vector<DwarfAttribute>* attribute_list = nullptr;
      if (tag == DW_TAG_compile_unit) {
        PTI_ASSERT(comp_unit_map.count(abbrev_number) == 0);
        attribute_list = &(comp_unit_map[abbrev_number]);
      }

      uint32_t attribute = 0, form = 0;
      do {
        ptr = utils::leb128::Decode32(ptr, attribute, done);
        PTI_ASSERT(done);
        PTI_ASSERT(ptr < data_ + size_);

        ptr = utils::leb128::Decode32(ptr, form, done);
        PTI_ASSERT(done);
        PTI_ASSERT(ptr < data_ + size_);

        if (attribute == 0 || form == 0) {
          PTI_ASSERT(attribute == 0);
          PTI_ASSERT(form == 0);
        } else {
          if (attribute_list != nullptr) {
            attribute_list->push_back({attribute, form});
          }
        }
      } while (attribute != 0 || form != 0);
    }

    return comp_unit_map;
  }

 private:
  const uint8_t* data_;
  uint32_t size_;
};

#endif // PTI_UTILS_DEBUG_ABBREV_PARSER_H_