//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti/pti.h"

#include <spdlog/spdlog.h>

#include <array>

#include "utils/enum_conversion_helper.h"

constexpr const char* const kResultFallback = "INVALID";

inline constexpr static std::array kResultStrTable = {
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_SUCCESS),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_STATUS_END_OF_BUFFER),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_NOT_IMPLEMENTED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_BAD_ARGUMENT),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_NO_CALLBACKS_SET),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_BAD_TIMESTAMP),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_DRIVER),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_TRACING_NOT_INITIALIZED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result,
                                         PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_METRICS_NO_DATA_COLLECTED),
    PTI_ASSOCIATE_ENUM_MEMBER_TO_DEFAULT(pti_result, PTI_ERROR_INTERNAL),
};

constexpr const char* PtiResultTypeToStringImpl(pti_result result_value) {
  switch (result_value) {
    PTI_ENUM_CONVERSION(pti_result, PTI_SUCCESS, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_STATUS_END_OF_BUFFER, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_NOT_IMPLEMENTED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_BAD_ARGUMENT, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_NO_CALLBACKS_SET, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_BAD_TIMESTAMP, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_DRIVER, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_TRACING_NOT_INITIALIZED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_METRICS_NO_DATA_COLLECTED, kResultStrTable)
    PTI_ENUM_CONVERSION(pti_result, PTI_ERROR_INTERNAL, kResultStrTable)
  }
  return kResultFallback;
}

const char* ptiResultTypeToString(pti_result result_value) {
  try {
    return PtiResultTypeToStringImpl(result_value);
  } catch (...) {
    return kResultFallback;
  }
}
