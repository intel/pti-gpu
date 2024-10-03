//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "pti_lib_handler.h"
#include "utils/platform_strings.h"
#include "utils/utils.h"

class GlobalSyclInitializer {
 public:
  inline static bool Initialize() {
    utils::SetEnv("XPTI_SUBSCRIBERS", utils::GetPathToSharedObject(Initialize).c_str());
    utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", pti::strings::kXptiLibName);
    utils::SetEnv("XPTI_TRACE_ENABLE", "1");
    return true;
  }

  inline static bool result_ = Initialize();
};

void xptiTraceInit(unsigned int major_version, unsigned int minor_version, const char* version_str,
                   const char* stream_name) {
  if (!pti::PtiLibHandler::Instance().xptiTraceInit_) {
    return;
  }
  pti::PtiLibHandler::Instance().xptiTraceInit_(major_version, minor_version, version_str,
                                                stream_name);
}

void xptiTraceFinish(const char* /*stream_name*/) {}
