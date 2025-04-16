//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "pti/pti.h"
#include "pti/pti_view.h"
#include "pti_lib_handler.h"

static constexpr const char* const kInvalidString = "INVALID";

pti_result ptiViewEnable(pti_view_kind view_kind) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewEnable_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewEnable_(view_kind);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewDisable(pti_view_kind view_kind) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewDisable_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewDisable_(view_kind);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewGPULocalAvailable() {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewGPULocalAvailable_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewGPULocalAvailable_();
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

const char* ptiViewOverheadKindToString(pti_view_overhead_kind type) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return kInvalidString;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewOverheadKindToString_) {
      return kInvalidString;
    }

    return pti::PtiLibHandler::Instance().ptiViewOverheadKindToString_(type);
  } catch (...) {
    return kInvalidString;
  }
}
const char* ptiViewMemoryTypeToString(pti_view_memory_type type) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return kInvalidString;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewMemoryTypeToString_) {
      return kInvalidString;
    }

    return pti::PtiLibHandler::Instance().ptiViewMemoryTypeToString_(type);
  } catch (...) {
    return kInvalidString;
  }
}

const char* ptiViewMemcpyTypeToString(pti_view_memcpy_type type) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return kInvalidString;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewMemcpyTypeToString_) {
      return kInvalidString;
    }

    return pti::PtiLibHandler::Instance().ptiViewMemcpyTypeToString_(type);
  } catch (...) {
    return kInvalidString;
  }
}

pti_result ptiViewSetCallbacks(pti_fptr_buffer_requested fptr_bufferRequested,
                               pti_fptr_buffer_completed fptr_bufferCompleted) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewSetCallbacks_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewSetCallbacks_(fptr_bufferRequested,
                                                               fptr_bufferCompleted);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewGetNextRecord(uint8_t* buffer, size_t valid_bytes,
                                pti_view_record_base** record) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewGetNextRecord_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewGetNextRecord_(buffer, valid_bytes, record);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiFlushAllViews() {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiFlushAllViews_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiFlushAllViews_();
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewPushExternalCorrelationId(pti_view_external_kind external_kind,
                                            uint64_t external_id) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewPushExternalCorrelationId_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewPushExternalCorrelationId_(external_kind,
                                                                            external_id);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewPopExternalCorrelationId(pti_view_external_kind external_kind,
                                           uint64_t* p_external_id) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewPopExternalCorrelationId_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewPopExternalCorrelationId_(external_kind,
                                                                           p_external_id);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

uint64_t ptiViewGetTimestamp() {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return 0;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewGetTimestamp_) {
      return 0;
    }

    return pti::PtiLibHandler::Instance().ptiViewGetTimestamp_();
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewSetTimestampCallback(pti_fptr_get_timestamp fptr_timestampRequested) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewSetTimestampCallback_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewSetTimestampCallback_(fptr_timestampRequested);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewGetApiIdName(pti_api_group_id type, uint32_t unique_id, const char** name) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewGetApiIdName_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewGetApiIdName_(type, unique_id, name);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewEnableDriverApi(uint32_t enable, pti_api_group_id type, uint32_t api_id) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewEnableDriverApi_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewEnableDriverApi_(enable, type, api_id);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}
pti_result ptiViewEnableRuntimeApi(uint32_t enable, pti_api_group_id type, uint32_t api_id) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewEnableRuntimeApi_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewEnableRuntimeApi_(enable, type, api_id);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

pti_result ptiViewEnableDriverApiClass(uint32_t enable, pti_api_class api_class,
                                       pti_api_group_id group) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewEnableDriverApiClass_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewEnableDriverApiClass_(enable, api_class, group);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}
pti_result ptiViewEnableRuntimeApiClass(uint32_t enable, pti_api_class api_class,
                                        pti_api_group_id group) {
  try {
    if (!pti::PtiLibHandler::Instance().ViewAvailable()) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    if (!pti::PtiLibHandler::Instance().ptiViewEnableRuntimeApiClass_) {
      return pti_result::PTI_ERROR_NOT_IMPLEMENTED;
    }

    return pti::PtiLibHandler::Instance().ptiViewEnableRuntimeApiClass_(enable, api_class, group);
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}
