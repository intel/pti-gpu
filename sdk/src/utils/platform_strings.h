//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef UTILS_PLATFORM_STRINGS_H_
#define UTILS_PLATFORM_STRINGS_H_

#include <string_view>

#include "platform_config.h"
#include "utils.h"

#if !defined(PTI_XPTI_FRAMEWORK_DISPATCHER_NAME)
#if defined(_WIN32)
#define PTI_XPTI_FRAMEWORK_DISPATCHER_NAME_STRING "xptifw.dll"
#else
#define PTI_XPTI_FRAMEWORK_DISPATCHER_NAME_STRING "libxptifw.so"
#endif
#else
#define PTI_XPTI_FRAMEWORK_DISPATCHER_NAME_STRING TOSTRING(PTI_XPTI_FRAMEWORK_DISPATCHER_NAME)
#endif

#if !defined(PTI_VIEW_CORE_LIB_NAME)
#if defined(_WIN32)
#define PTI_VIEW_CORE_LIB_NAME_STRING "pti.dll"
#else
#define PTI_VIEW_CORE_LIB_NAME_STRING "libpti.so"
#endif
#else
#define PTI_VIEW_CORE_LIB_NAME_STRING TOSTRING(PTI_VIEW_CORE_LIB_NAME)
#endif

#if !defined(PTI_PLATFORM_SEPARATOR)
#if defined(_WIN32)
#define PTI_PLATFORM_SEPARATOR "\\"
#else
#define PTI_PLATFORM_SEPARATOR "/"
#endif
#endif

#if !defined(PTI_MODULE_SUBDIR)
#define PTI_MODULE_SUBDIR "pti"
#endif

namespace pti {
namespace strings {
inline static constexpr const char* const kXptiLibName = PTI_XPTI_FRAMEWORK_DISPATCHER_NAME_STRING;
inline static constexpr const char* const kPtiViewLib = PTI_VIEW_CORE_LIB_NAME_STRING;
inline static constexpr std::string_view kOsPathSeparator = PTI_PLATFORM_SEPARATOR;
inline static constexpr std::string_view kModuleSubdir = PTI_MODULE_SUBDIR;
}  // namespace strings
}  // namespace pti
#endif  // UTILS_PLATFORM_STRINGS_H_
