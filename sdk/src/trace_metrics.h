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
typedef struct _zet_metric_tracer_exp_handle_t *zet_metric_tracer_exp_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of metric decoder's object
typedef struct _zet_metric_decoder_exp_handle_t *zet_metric_decoder_exp_handle_t;

inline constexpr uint32_t ZET_METRIC_SAMPLING_TYPE_EXP_FLAG_TRACER_BASED = ZE_BIT(2);
inline constexpr auto ZET_STRUCTURE_TYPE_METRIC_TRACER_EXP_DESC =
    static_cast<zet_structure_type_t>(0x00010008);

typedef struct _zet_metric_tracer_exp_desc_t {
  zet_structure_type_t stype;
  const void *pNext;
  uint32_t notifyEveryNBytes;
} zet_metric_tracer_exp_desc_t;

typedef struct _zet_metric_entry_exp_t {
  zet_value_t value;
  uint64_t timeStamp;
  uint32_t metricIndex;
  ze_bool_t onSubdevice;
  uint32_t subdeviceId;
} zet_metric_entry_exp_t;

typedef enum _zex_metric_calculate_operation_exp_t {
  ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_MIN = 0,
  ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_MAX,
  ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_AVERAGE,
  ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FLAG_SUM,
  ZET_ENUM_EXP_METRIC_CALCULATE_OPERATION_FORCE_UINT32 = 0x7fffffff
} zex_metric_calculate_operation_exp_t;

typedef enum _zex_metric_calculate_result_status_exp_t {
  ZET_ENUM_EXP_METRIC_CALCULATE_RESULT_VALID = 0,
  ZET_ENUM_EXP_METRIC_CALCULATE_RESULT_INVALID,
  ZET_ENUM_EXP_METRIC_CALCULATE_RESULT_FORCE_UINT32 = 0x7fffffff
} zex_metric_calculate_result_status_exp_t;

typedef enum _zex_metric_timestamp_mode_t {
  ZET_METRIC_TIMESTAMP_MODE_RELATIVE = 0,
  ZET_METRIC_TIMESTAMP_MODE_ABSOLUTE,
  ZET_METRIC_TIMESTAMP_MODE_FORCE_UINT32 = 0x7fffffff
} zex_metric_timestamp_mode_t;

typedef struct _zex_metric_result_exp_t {
  zet_value_t value;
  zex_metric_calculate_result_status_exp_t resultStatus;
} zex_metric_result_exp_t;

typedef struct _zex_metric_calculate_time_window_t {
  uint64_t windowStart;
  zex_metric_timestamp_mode_t timestampMode;
  uint64_t windowSize;
} zex_metric_calculate_time_window_t;

typedef enum _zet_structure_exp_type_t {
  ZET_STRUCTURE_TYPE_METRIC_TRACER_DESC_EXP = 0x00010007,
  ZET_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP = 0x00010008
} zet_structure_exp_type_t;

typedef struct _zex_metric_calculate_exp_desc_t {
  zet_structure_exp_type_t stype;
  const void *pNext;
  uint32_t metricGroupCount;
  zet_metric_group_handle_t *phMetricGroups;
  uint32_t metricCount;
  zet_metric_handle_t *phMetrics;
  uint32_t timeWindowsCount;
  zex_metric_calculate_time_window_t *pCalculateTimeWindows;
  uint64_t timeAggregationWindow;
  zex_metric_calculate_operation_exp_t operation;
} zex_metric_calculate_exp_desc_t;

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricTracerCreateExp(
    zet_context_handle_t hContext, zet_device_handle_t hDevice, uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups, zet_metric_tracer_exp_desc_t *desc,
    ze_event_handle_t hNotificationEvent, zet_metric_tracer_exp_handle_t *phMetricTracer);

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

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricDecoderDecodeExp(
    zet_metric_decoder_exp_handle_t hMetricDecoder, size_t *pRawDataSize, const uint8_t *pRawData,
    uint32_t metricCount, zet_metric_handle_t *phMetric, uint32_t *pMetricEntriesCount,
    zet_metric_entry_exp_t *pMetricEntries);

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderGetDecodableMetricsExp(zet_metric_decoder_exp_handle_t hMetricDecoder,
                                       uint32_t *pCount, zet_metric_handle_t *phMetrics);

ZE_APIEXPORT ze_result_t ZE_APICALL zexMetricDecodeCalculateMultipleValuesExp(
    zet_metric_decoder_exp_handle_t hMetricDecoder, size_t *prawDataSize, const uint8_t *pRawData,
    zex_metric_calculate_exp_desc_t *pCalculateDesc, uint32_t *pSetCount,
    uint32_t *pMetricResultsCountPerSet, uint32_t *pTotalMetricResultsCount,
    zex_metric_result_exp_t *pMetricResults);

}  // namespace L0
}  // namespace external
#endif  // PTI_TRACE_METRICS_H_
