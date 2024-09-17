//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_GEN_BINARY_DECODER_H_
#define PTI_UTILS_GEN_BINARY_DECODER_H_

#include <vector>
#include <string>

#include <iga/kv.hpp>

#include "utils.h"

struct Instruction {
  uint64_t offset;
  std::string text;
};

typedef uint32_t gfx_core_family_t;

class GenBinaryDecoder {
 public:
  GenBinaryDecoder(const std::vector<uint8_t>& binary, iga_gen_t arch)
      : kernel_view_(arch, binary.data(), binary.size()) {}
  GenBinaryDecoder(const uint8_t* data, uint32_t size, iga_gen_t arch)
      : kernel_view_(arch, data, size) {}

  bool IsValid() const {
    return kernel_view_.decodeSucceeded();
  }

  std::vector<Instruction> Disassemble() {
    if (!IsValid()) {
      return std::vector<Instruction>();
    }

    std::vector<Instruction> instruction_list;

    char text[MAX_STR_SIZE] = { 0 };
    uint64_t offset = 0, size = 0;
    while (true) {
      size = kernel_view_.getInstSize(offset);
      if (size == 0) {
        break;
      }

      size_t length = kernel_view_.getInstSyntax(offset, text, MAX_STR_SIZE);
      PTI_ASSERT(length > 0);
      instruction_list.push_back({offset, text});

      offset += size;
    }

    return instruction_list;
  }

  /**
   * @brief Convert GFX core family enum to iga_gen_t enum.
   */
  static iga_gen_t GfxCoreToIgaGen(gfx_core_family_t type) {
    // "inc/common/igfxfmid.h": GFXCORE_FAMILY
    // "iga/iga.h" iga_gen_t
    switch (type) {
      case 12: // GFXCORE_FAMILY::IGFX_GEN9_CORE:
        return IGA_GEN9;
      case 13: // GFXCORE_FAMILY::IGFX_GEN10_CORE:
      case 14: // GFXCORE_FAMILY::IGFX_GEN10LP_CORE:
        return IGA_GEN10;
      case 15: // GFXCORE_FAMILY::IGFX_GEN11_CORE:
      case 16: // GFXCORE_FAMILY::IGFX_GEN11LP_CORE:
        return IGA_GEN11;
      case 17: // GFXCORE_FAMILY::IGFX_GEN12_CORE:
      case 18: // GFXCORE_FAMILY::IGFX_GEN12LP_CORE:
        return IGA_GEN12p1;
      case 0x0c05: // GFXCORE_FAMILY::IGFX_XE_HP_CORE:
        return IGA_XE_HP;
      case 0x0c07: // GFXCORE_FAMILY::IGFX_XE_HPG_CORE:
        return IGA_XE_HPG;
      case 0x0c08: // GFXCORE_FAMILY::IGFX_XE_HPC_CORE:
        return IGA_XE_HPC;
      default:
        return IGA_GEN_INVALID;
    }
  }

 private:
  KernelView kernel_view_;
};

#endif // PTI_UTILS_GEN_BINARY_DECODER_H_
