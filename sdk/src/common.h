//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef PTI_COMMON_H_
#define PTI_COMMON_H_

#include <cstddef>

struct PtiCbMemoryData {
  size_t bytes_xfered;
  size_t value_set;
};

#endif  // PTI_COMMON_H_
