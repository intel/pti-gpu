//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_DEFAULT_BUFFER_CALLBACKS_H_
#define SRC_DEFAULT_BUFFER_CALLBACKS_H_

#include <iostream>
#include <memory>

#include "view_record_info.h"

namespace pti {
namespace view {
namespace defaults {
constexpr std::size_t kBufferAlignment = 8;
constexpr std::size_t kDefaultSizeOfBuffer = 1'000 * SizeOfLargestViewRecord();

void DefaultBufferAllocation(unsigned char** buf, std::size_t* buf_size) {
  *buf_size = kDefaultSizeOfBuffer;
  auto* ptr = ::operator new(*buf_size);
  ptr = std::align(kBufferAlignment, sizeof(unsigned char), ptr, *buf_size);
  *buf = static_cast<unsigned char*>(ptr);
  if (!*buf) {
    std::cerr << "Unable to allocate memory for default buffer" << '\n';
    std::abort();
  }
}

void DefaultRecordParser(unsigned char* const buf, [[maybe_unused]] std::size_t buf_size,
                         std::size_t valid_buf_size) {
  if (valid_buf_size) {
    ::operator delete(buf);
  }
}
}  // namespace defaults
}  // namespace view
}  // namespace pti
#endif  // SRC_DEFAULT_BUFFER_CALLBACKS_H
