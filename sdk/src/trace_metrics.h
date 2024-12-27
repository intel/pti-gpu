/*
 * Copyright (C) 2024 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they were
 * provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit this
 * software or the related documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 */

#ifndef PTI_TRACE_METRICS_H_
#define PTI_TRACE_METRICS_H_
#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

namespace external {
namespace L0 {
//////////////////////////////////////////////////////////////////////////////
/// @brief Handle of metric tracer's object
// Note: this is available in the latest loader
typedef struct _zet_metric_tracer_exp_handle_t *zet_metric_tracer_exp_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of metric decoder's object
// Note: this is available in the latest loader
typedef struct _zet_metric_decoder_exp_handle_t *zet_metric_decoder_exp_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of metric calculate operation
typedef struct _zet_intel_metric_calculate_operation_exp_handle_t
    *zet_intel_metric_calculate_operation_exp_handle_t;

inline constexpr uint32_t ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EXP_TRACER_BASED = ZE_BIT(2);

// Note: this is available in the latest loader
inline constexpr auto ZET_STRUCTURE_TYPE_METRIC_TRACER_EXP_DESC =
    static_cast<zet_structure_type_t>(0x00010008);

inline constexpr auto ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP =
    static_cast<zet_structure_type_t>(0x00010009);

// Note: this is available in the latest loader
typedef struct _zet_metric_tracer_exp_desc_t {
  zet_structure_type_t stype;
  const void *pNext;
  uint32_t notifyEveryNBytes;
} zet_metric_tracer_exp_desc_t;

// Note: this is available in the latest loader
typedef struct _zet_metric_entry_exp_t {
  zet_value_t value;
  uint64_t timeStamp;
  uint32_t metricIndex;
  ze_bool_t onSubdevice;
  uint32_t subdeviceId;
} zet_metric_entry_exp_t;

// These are internal only as of 12/24
typedef enum _zet_intel_metric_calculate_operation_exp_flag_t {
  ZET_INTEL_METRIC_CALCULATE_OPERATION_EXP_FLAG_MIN = 0,
  ZET_INTEL_METRIC_CALCULATE_OPERATION_EXP_FLAG_MAX,
  ZET_INTEL_METRIC_CALCULATE_OPERATION_EXP_FLAG_AVERAGE,
  ZET_INTEL_METRIC_CALCULATE_OPERATION_EXP_FLAG_SUM,
  ZET_INTEL_METRIC_CALCULATE_OPERATION_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_calculate_operation_exp_flag_t;

typedef enum _zet_intel_metric_calculate_result_status_exp_t {
  ZET_INTEL_METRIC_CALCULATE_EXP_RESULT_VALID = 0,
  ZET_INTEL_METRIC_CALCULATE_EXP_RESULT_INVALID,
  ZET_INTEL_METRIC_CALCULATE_EXP_RESULT_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_calculate_result_status_exp_t;

typedef enum _zet_intel_metric_timestamp_mode_exp_t {
  ZET_INTEL_METRIC_TIMESTAMP_EXP_MODE_RELATIVE = 0,
  ZET_INTEL_METRIC_TIMESTAMP_EXP_MODE_ABSOLUTE,
  ZET_INTEL_METRIC_TIMESTAMP_EXP_MODE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_timestamp_mode_exp_t;

typedef struct _zet_intel_metric_result_exp_t {
  zet_value_t value;
  zet_intel_metric_calculate_result_status_exp_t resultStatus;
} zet_intel_metric_result_exp_t;

typedef struct _zet_intel_metric_calculate_time_window_exp_t {
  uint64_t windowStart;
  zet_intel_metric_timestamp_mode_exp_t timestampMode;
  uint64_t windowSize;
} zet_intel_metric_calculate_time_window_exp_t;

typedef struct _zet_intel_metric_calculate_exp_desc_t {
  zet_structure_type_t stype;
  const void *pNext;
  uint32_t metricGroupCount;
  zet_metric_group_handle_t *phMetricGroups;
  uint32_t metricCount;
  zet_metric_handle_t *phMetrics;
  uint32_t timeWindowsCount;
  zet_intel_metric_calculate_time_window_exp_t *pCalculateTimeWindows;
  uint64_t timeAggregationWindow;
  zet_intel_metric_calculate_operation_exp_flag_t operation;
  uint64_t startingTime;
} zet_intel_metric_calculate_exp_desc_t;

typedef struct _zet_intel_metric_decoded_buffer_exp_properties_t {
  zet_structure_type_t stype;
  const void *pNext;
  uint64_t minTimeStamp;
  uint64_t maxTimeStamp;
} zet_intel_metric_decoded_buffer_exp_properties_t;

// These symbols are only available in later versions of the loader
ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricTracerCreateExp(
    zet_context_handle_t context_handle, zet_device_handle_t device_handle, uint32_t,
    zet_metric_group_handle_t *metric_group_handle, zet_metric_tracer_exp_desc_t *tracer_desc,
    ze_event_handle_t event_handle, zet_metric_tracer_exp_handle_t *tracer_handle);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerDestroyExp(zet_metric_tracer_exp_handle_t hMetricTracer);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerEnableExp(zet_metric_tracer_exp_handle_t hMetricTracer, bool synchronous);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerDisableExp(zet_metric_tracer_exp_handle_t hMetricTracer, bool synchronous);

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricTracerReadDataExp(
    zet_metric_tracer_exp_handle_t hMetricTracer, size_t *pRawDataSize, uint8_t *pRawData);

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricDecoderCreateExp(
    zet_metric_tracer_exp_handle_t hMetricTracer, zet_metric_decoder_exp_handle_t *phMetricDecoder);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderDestroyExp(zet_metric_decoder_exp_handle_t hMetricDecoder);

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricTracerDecodeExp(
    zet_metric_decoder_exp_handle_t hMetricDecoder, size_t *pRawDataSize, const uint8_t *pRawData,
    uint32_t metricCount, zet_metric_handle_t *phMetric, uint32_t *pMetricEntriesCount,
    zet_metric_entry_exp_t *pMetricEntries);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderGetDecodableMetricsExp(zet_metric_decoder_exp_handle_t hMetricDecoder,
                                       uint32_t *pCount, zet_metric_handle_t *phMetrics);

// These symbols are available internally only
ZE_APIEXPORT ze_result_t ZE_APICALL zetIntelMetricCalculateOperationCreateExp(
    zet_context_handle_t context_handle, zet_device_handle_t device_handle,
    zet_intel_metric_calculate_exp_desc_t *calculate_desc,
    zet_intel_metric_calculate_operation_exp_handle_t *calculate_op_handle);

ZE_APIEXPORT ze_result_t ZE_APICALL zetIntelMetricCalculateOperationDestroyExp(
    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation);

ZE_APIEXPORT ze_result_t ZE_APICALL zetIntelMetricCalculateGetReportFormatExp(
    zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle, uint32_t *metric_count,
    zet_metric_handle_t *metrics_handles);

ZE_APIEXPORT ze_result_t ZE_APICALL zetIntelMetricDecodeCalculateMultipleValuesExp(
    zet_metric_decoder_exp_handle_t decoder_handle, size_t *raw_data_size, const uint8_t *raw_data,
    zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle, uint32_t *set_count,
    uint32_t *report_count_per_set, uint32_t *metric_report_count,
    zet_intel_metric_result_exp_t *metric_results);

ZE_APIEXPORT ze_result_t ZE_APICALL zetIntelMetricDecodeToBinaryBufferExp(
    zet_metric_decoder_exp_handle_t decoder_handle, size_t *raw_data_size, const uint8_t *raw_data,
    zet_intel_metric_calculate_operation_exp_handle_t calculate_op_handle,
    zet_intel_metric_decoded_buffer_exp_properties_t *decoded_buffer_props,
    size_t *decoded_buffer_size, uint8_t *decoded_buffer);

}  // namespace L0
}  // namespace external
#endif  // PTI_TRACE_METRICS_H_
