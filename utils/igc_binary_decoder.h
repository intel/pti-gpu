//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#pragma once

#include <memory.h>

#include <igc/ocl_igc_shared/executable_format/patch_list.h>
#include <igdgmm/inc/common/igfxfmid.h>

#include "gen_binary_decoder.h"

using namespace iOpenCL;

class IgcBinaryDecoder {
 public:
  IgcBinaryDecoder(const std::vector<uint8_t>& binary) : binary_(binary) {}

  std::vector<Instruction> Disassemble(const std::string& kernel_name) {
    if (!IsValidHeader()) {
      return std::vector<Instruction>();
    }

    const SProgramBinaryHeader* header =
      reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    iga_gen_t arch = GetArch(header->Device);
    if (arch == IGA_GEN_INVALID) {
      return std::vector<Instruction>();
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(header) +
      sizeof(SProgramBinaryHeader) + header->PatchListSize;
    for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
      const SKernelBinaryHeaderCommon* kernel_header =
        reinterpret_cast<const SKernelBinaryHeaderCommon*>(ptr);

      ptr += sizeof(SKernelBinaryHeaderCommon);
      const char* name = (const char*)ptr;

      ptr += kernel_header->KernelNameSize;
      if (kernel_name == name) {
        std::vector<uint8_t> raw_binary(kernel_header->KernelHeapSize);
        memcpy(raw_binary.data(), ptr,
               kernel_header->KernelHeapSize * sizeof(uint8_t));
        GenBinaryDecoder decoder(raw_binary, arch);
        return decoder.Disassemble();
      }

      ptr += kernel_header->PatchListSize +
        kernel_header->KernelHeapSize +
        kernel_header->GeneralStateHeapSize +
        kernel_header->DynamicStateHeapSize +
        kernel_header->SurfaceStateHeapSize;
    }

    return std::vector<Instruction>();
  }

 private:
  bool IsValidHeader() {
    if (binary_.size() < sizeof(SProgramBinaryHeader)) {
      return false;
    }

    const SProgramBinaryHeader* header =
      reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    if (header->Magic != MAGIC_CL) {
      return false;
    }

    return true;
  }

  static iga_gen_t GetArch(uint32_t device) {
    switch (device) {
      case IGFX_GEN8_CORE:
        return IGA_GEN8;
      case IGFX_GEN9_CORE:
        return IGA_GEN9p5;
      case IGFX_GEN11_CORE:
      case IGFX_GEN11LP_CORE:
        return IGA_GEN11;
      case IGFX_GEN12_CORE:
      case IGFX_GEN12LP_CORE:
        return IGA_GEN12p1;
      case IGFX_XE_HP_CORE:
        return IGA_XE_HP;
      case IGFX_XE_HPG_CORE:
        return IGA_XE_HPG;
      case IGFX_XE_HPC_CORE:
        return IGA_XE_HPC;
      default:
        break;
    }
    return IGA_GEN_INVALID;
  }

private:
    std::vector<uint8_t> binary_;
};