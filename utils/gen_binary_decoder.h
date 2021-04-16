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
  int32_t offset;
  std::string text;
};

class GenBinaryDecoder {
 public:
  GenBinaryDecoder(const std::vector<uint8_t>& binary, iga_gen_t arch)
      : kernel_view_(arch, binary.data(), binary.size()) {}

  bool IsValid() const {
    return kernel_view_.decodeSucceeded();
  }

  std::vector<Instruction> Disassemble() {
    if (!IsValid()) {
      return std::vector<Instruction>();
    }

    std::vector<Instruction> instruction_list;

    char text[MAX_STR_SIZE] = { 0 };
    int32_t offset = 0, size = 0;
    while (true) {
      size = kernel_view_.getInstSize(offset);
      if (size == 0) {
        break;
      }

      size_t lenght = kernel_view_.getInstSyntax(offset, text, MAX_STR_SIZE);
      PTI_ASSERT(lenght > 0);
      instruction_list.push_back({offset, text});

      offset += size;
    }

    return instruction_list;
  }

 private:
  KernelView kernel_view_;
};

#endif // PTI_UTILS_GEN_BINARY_DECODER_H_