//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_ELF_PARSER_MAPPING_H_
#define PTI_ELF_PARSER_MAPPING_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct SourceMapping {
  uint32_t file_id;
  const char* file_path;  // pointer to the file path in the original data
  const char* file_name;  // pointer to the file name in the original data
  uint64_t address;       // address in the binary in 64-bit format
  uint32_t line;
  uint32_t column;
};

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // PTI_ELF_PARSER_MAPPING_H_
