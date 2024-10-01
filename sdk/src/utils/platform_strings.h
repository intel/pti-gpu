//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef UTILS_PLATFORM_STRINGS_H_
#define UTILS_PLATFORM_STRINGS_H_

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
#define PTI_VIEW_CORE_LIB_NAME_STRING "pti" "-" TOSTRING(PTI_VERSION_MAJOR) "-" TOSTRING(PTI_VERSION_MINOR) ".dll"
#else
#define PTI_VIEW_CORE_LIB_NAME_STRING "libpti.so" "." TOSTRING(PTI_VERSION_MAJOR) "." TOSTRING(PTI_VERSION_MINOR)
#endif
#else
#define PTI_VIEW_CORE_LIB_NAME_STRING TOSTRING(PTI_VIEW_CORE_LIB_NAME)
#endif


namespace pti {
namespace strings {
inline static constexpr const char* const kXptiLibName = PTI_XPTI_FRAMEWORK_DISPATCHER_NAME_STRING;
inline static constexpr const char* const kPtiViewLib = PTI_VIEW_CORE_LIB_NAME_STRING;
}  // namespace utils
}  // namespace pti
#endif  // UTILS_PLATFORM_STRINGS_H_
