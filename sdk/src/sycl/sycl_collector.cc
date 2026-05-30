#include "sycl_collector.h"

#include <cstring>
#include <xpti/xpti_trace_framework.hpp>

// NOLINTNEXTLINE
XPTI_CALLBACK_API void xptiTraceInit(unsigned int major_version, unsigned int minor_version,
                                     const char* version_str, const char* stream_name) {
  XptiStreamRegistrationHandler::Instance().InitializeStream(major_version, minor_version,
                                                             version_str, stream_name);
}

// NOLINTNEXTLINE
XPTI_CALLBACK_API void xptiTraceFinish(const char* /*stream_name*/) {}

#ifdef XPTI_HAS_STREAM_DETAIL_LEVEL
XPTI_CALLBACK_API bool xptiQuerySubscriberStreamDetailLevel(const char* stream_name,
                                                            xpti::stream_detail_level_t* level) {
  // The framework calls this to query our desired detail level
  if (!level) return false;

  if (stream_name && std::strcmp(stream_name, "sycl") == 0) {
    // Request BASIC level for minimal overhead on the sycl stream
    *level = xpti::stream_detail_level_t::XPTI_STREAM_DETAIL_LEVEL_BASIC;
  } else {
    // Use NORMAL (default) for other streams
    *level = xpti::stream_detail_level_t::XPTI_STREAM_DETAIL_LEVEL_NORMAL;
  }
  return true;
}
#endif

// clang-format off
extern "C" {
  void
#if (defined(_WIN32) || defined(_WIN64))
  __declspec(dllexport)
#else
  __attribute__((visibility("default")))
#endif
  PtiSetXPTIEnvironmentDetails(bool is_foreign_subscriber,
                               bool is_likely_unitrace_subscriber) {
    SyclCollector::foreign_subscriber_ = is_foreign_subscriber;
    SyclCollector::likely_unitrace_subscriber_ = is_likely_unitrace_subscriber;
  }
}
// clang-format on

#if (defined(_WIN32) || defined(_WIN64))

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD fwdReason, LPVOID /*lpvReserved*/) {
  switch (fwdReason) {
    case DLL_PROCESS_ATTACH: {
      break;
    }
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      framework_finalized = true;
      break;
    case DLL_PROCESS_DETACH:
      break;
  }

  return TRUE;
}

#else  // Linux (possibly macOS?)

// NOLINTNEXTLINE
__attribute__((destructor)) static void framework_fini() { framework_finalized = true; }

#endif
