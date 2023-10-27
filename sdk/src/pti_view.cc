#include "pti_view.h"

#include "internal_helper.h"
#include "view_handler.h"

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewEnable(pti_view_kind view_kind) {
  try {
    if (!(IsPtiViewKindEnum(view_kind))) {
      return pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    return Instance().Enable(view_kind);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewDisable(pti_view_kind view_kind) {
  try {
    if (!(IsPtiViewKindEnum(view_kind))) {
      return pti_result::PTI_ERROR_BAD_ARGUMENT;
    }
    return Instance().Disable(view_kind);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewSetCallbacks(pti_fptr_buffer_requested fptr_bufferRequested,
                               pti_fptr_buffer_completed fptr_bufferCompleted) {
  try {
    return Instance().RegisterBufferCallbacks(fptr_bufferRequested, fptr_bufferCompleted);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewGetNextRecord(uint8_t* buffer, size_t valid_bytes,
                                pti_view_record_base** record) {
  try {
    return GetNextRecord(buffer, valid_bytes, record);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiFlushAllViews() {
  try {
    return Instance().FlushBuffers();
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewPushExternalCorrelationId(pti_view_external_kind external_kind,
                                            uint64_t external_id) {
  try {
    return Instance().PushExternalKindId(external_kind, external_id);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

//
// TODO: parse different exception types, analyse caught exception and return
// different error code.
//
pti_result ptiViewPopExternalCorrelationId(pti_view_external_kind external_kind,
                                           uint64_t* p_external_id) {
  try {
    return Instance().PopExternalKindId(external_kind, p_external_id);
  } catch (const std::overflow_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::runtime_error& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (const std::exception& e) {
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}
