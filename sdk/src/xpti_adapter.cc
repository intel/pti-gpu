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
  // variables to store if XPTI is subscribed by some other subscriber
  // and if this subscriber is likely unitrace
  inline static bool foreign_subscriber_ = false;
  inline static bool likely_unitrace_subscriber_ = false;

  inline static bool Initialize() {
    // respecting another XPTI subscriber:
    // if someone already subscribed (e.g. might be that the app runs under unitrace) -
    // we do not subscribe for XPTI
    std::tie(foreign_subscriber_, likely_unitrace_subscriber_) = utils::IsSubscriberToXPTI();
    if (!foreign_subscriber_) {
      utils::SetEnv("XPTI_SUBSCRIBERS", utils::GetPathToSharedObject(Initialize).c_str());
      utils::SetEnv("XPTI_FRAMEWORK_DISPATCHER", pti::strings::kXptiLibName);
      utils::SetEnv("XPTI_TRACE_ENABLE", "1");

      utils::SetEnv("UR_ENABLE_LAYERS", "UR_LAYER_TRACING");
    }
    return true;
  }

  inline static bool result_ = Initialize();
};

std::pair<bool, bool> IsForeignXPTISubscriber() {
  return {GlobalSyclInitializer::foreign_subscriber_,
          GlobalSyclInitializer::likely_unitrace_subscriber_};
}

void xptiTraceInit(unsigned int major_version, unsigned int minor_version, const char* version_str,
                   const char* stream_name) {
  if (!pti::PtiLibHandler::Instance().xptiTraceInit_) {
    return;
  }
  pti::PtiLibHandler::Instance().xptiTraceInit_(major_version, minor_version, version_str,
                                                stream_name);
}

void xptiTraceFinish(const char* /*stream_name*/) {}
