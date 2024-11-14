//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UTILS_PTI_FILESYSTEM_H_
#define UTILS_PTI_FILESYSTEM_H_

#include "platform_config.h"

#if defined(PTI_EXPERIMENTAL_FILESYSTEM)
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

namespace pti {
namespace utils {
#if defined(PTI_EXPERIMENTAL_FILESYSTEM)
namespace filesystem = std::experimental::filesystem;
#else
namespace filesystem = std::filesystem;
#endif
}  // namespace utils
}  // namespace pti

#endif  // UTILS_PTI_FILESYSTEM_H_
