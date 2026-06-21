//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_ITT_ITT_METADATA_H_
#define SRC_ITT_ITT_METADATA_H_

#include <ittnotify.h>

#include <atomic>

namespace itt_metadata {

// String constants for metadata identification
constexpr const char *kSendSizeString = "send_size";
constexpr const char *kRecvSizeString = "recv_size";
constexpr const char *kCommIdString = "comm_id";

// Singleton accessor functions for atomic string handles
// Using functions ensures single definition across translation units
inline std::atomic<__itt_string_handle *> &GetSendSizeHandle() {
  static std::atomic<__itt_string_handle *> handle{nullptr};
  return handle;
}

inline std::atomic<__itt_string_handle *> &GetRecvSizeHandle() {
  static std::atomic<__itt_string_handle *> handle{nullptr};
  return handle;
}

inline std::atomic<__itt_string_handle *> &GetCommIdHandle() {
  static std::atomic<__itt_string_handle *> handle{nullptr};
  return handle;
}

}  // namespace itt_metadata

#endif  // SRC_ITT_ITT_METADATA_H_
