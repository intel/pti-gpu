//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef INCLUDE_PTI_METRICS_H_
#define INCLUDE_PTI_METRICS_H_

#include <stdint.h>
#include <stdlib.h>

#include "pti/pti.h"

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/*****************************************************************************
                                 DEVICE
*****************************************************************************/

typedef void* pti_device_handle_t;  //!< Abstraction of device within PTI

typedef struct _pti_pci_properties_t {
  uint8_t _domain;
  uint8_t _bus;
  uint8_t _device;
  uint8_t _function;
} pti_pci_properties_t;

typedef struct _pti_device_properties_t {
  pti_device_handle_t _handle;                    //!< Internal handle to device
  pti_pci_properties_t _address;                  //!< PCI device properties
  const char* _model_name;                        //!< Model name of the device
  uint8_t _uuid[PTI_MAX_DEVICE_UUID_SIZE];        //!< Universal unique identifier
  /* we can also get number of sub devices, serial number (char[]), board number(char[]), brand name(char[]),
  vendor name(char[]), driver version(char[]), device type and flags */
} pti_device_properties_t;


/*****************************************************************************
                                 METRIC
*****************************************************************************/

typedef void* pti_metric_handle_t; //!< Abstraction of a metric handle within PTI

/// @brief Supported value types
typedef enum _pti_metric_value_type {
  PTI_METRIC_VALUE_TYPE_UINT32 = 0,              //!< 32-bit unsigned-integer
  PTI_METRIC_VALUE_TYPE_UINT64 = 1,              //!< 64-bit unsigned-integer
  PTI_METRIC_VALUE_TYPE_FLOAT32 = 2,             //!< 32-bit floating-point
  PTI_METRIC_VALUE_TYPE_FLOAT64 = 3,             //!< 64-bit floating-point
  PTI_METRIC_VALUE_TYPE_BOOL8 = 4,               //!< 8-bit boolean
  PTI_METRIC_VALUE_TYPE_STRING = 5,              //!< C string
  PTI_METRIC_VALUE_TYPE_UINT8 = 6,               //!< 8-bit unsigned-integer
  PTI_METRIC_VALUE_TYPE_UINT16 = 7,              //!< 16-bit unsigned-integer

  PTI_METRIC_VALUE_TYPE_FORCE_UINT32 = 0x7fffffff
} pti_metric_value_type;

/// @brief Metric types
typedef enum _pti_metric_type {
  PTI_METRIC_TYPE_DURATION = 0,                  //!< duration
  PTI_METRIC_TYPE_EVENT = 1,                     //!< event
  PTI_METRIC_TYPE_EVENT_WITH_RANGE = 2,          //!< event with range
  PTI_METRIC_TYPE_THROUGHPUT = 3,                //!< throughput
  PTI_METRIC_TYPE_TIMESTAMP = 4,                 //!< timestamp
  PTI_METRIC_TYPE_FLAG = 5,                      //!< flag
  PTI_METRIC_TYPE_RATIO = 6,                     //!< ratio
  PTI_METRIC_TYPE_RAW = 7,                       //!< raw
  PTI_METRIC_TYPE_IP = 0x7ffffffe,               //!< instruction pointer

  PTI_METRIC_TYPE_FORCE_UINT32 = 0x7fffffff

} pti_metric_type;

typedef union _pti_value_t {
    uint32_t ui32;                               //!< 32-bit unsigned-integer
    uint64_t ui64;                               //1< 64-bit unsigned-integer
    float fp32;                                  //!< 32-bit floating-point
    double fp64;                                 //!< 64-bit floating-point
    bool b8;                                     //!< 8-bit boolean
} pti_value_t;

typedef struct _pti_metric_properties_t {
  pti_metric_handle_t _handle;                   //!< Internal handle for the metric
  const char* _name;                             //!< Name of the metric
  const char* _description;                      //!< Description of the metric
  /* Can also get Component and tier number*/
  pti_metric_type _metric_type;                  //!< Type of the metric
  pti_metric_value_type _value_type;             //!< Value type of the metric
  const char* _units;                            //!< The units of the metric result values
} pti_metric_properties_t;

/*******************************************************************************
                                 METRIC GROUP
*******************************************************************************/

typedef void* pti_metrics_group_handle_t; //!< Abstraction of a metric group handle within PTI

typedef enum _pti_metrics_group_type {
  PTI_METRIC_GROUP_TYPE_EVENT_BASED = 0b0001,    //!< Event based sampling (Query)
  PTI_METRIC_GROUP_TYPE_TIME_BASED = 0b0010,     //!< Time based sampling (Stream)
  PTI_METRIC_GROUP_TYPE_TRACE_BASED = 0b0100,    //!< Trace based sampling (Trace)

  PTI_METRIC_GROUP_TYPE_FORCE_UINT32 = 0x7fffffff
} pti_metrics_group_type;

typedef struct _pti_metrics_group_properties_t {
  pti_metrics_group_handle_t _handle;            //!< Internal handle for the metric group
  pti_metrics_group_type _type;                  //!< Sampling type of the metric group
  uint32_t _domain;                              //!< Hardware domain responsible for collecting the metric group
  uint32_t _metric_count;                        //!< Number of metrics in the metric group
  pti_metric_properties_t* _metric_properties;   //!< Convenience pointer to buffer where metric properties in the metric group can be saved
                                                 // The pointer is initialized to null. User is responsible for allocating buffer of size _metric_count
                                                 // and calling the ptiMetricsGetMetricsProperties function to get the metric properties polulated
                                                 // This pointer does not need to be used, it is part of the pti_metrics_group_preperties_t for convinience
                                                 // only. It is usefull when traversing the buffer of metric group properties and then metric properties in
                                                 // each group without needing to keep additional maps. User may choose to use a different pointer for metric
                                                 // properties.
  const char* _name;                             //!< Name of the metric group
  const char* _description;                      //!< Description of the metric group
} pti_metrics_group_properties_t;

/**
 * @brief Get the properties of all devices on the system on which metric collection can be done
 * usage: 1- Call ptiMetricsGetDevices(NULL, device_count) to discover the required buffer size; the required buffer size will be written to device_count.
 *        2- Allocate devices buffer of size sizeof(pti_device_properties_t) * device_count
 *        3- Call ptiMetricsGetDevices(devices, device_count) to get the properties of the discovered devices written to devices
 *
 * @param[in/out] devices                   Pointer to the array of devices. If NULL, the number of devices is returned in *device_count
 *                                          if not NULL, it should point to the buffer of size: sizeof(pti_device_description_t) * (*device_count)
 * @param[in/out] device_count              Number of devices
 *                                          in - if devices is NULL or if device_count is less than the required minimum buffer size,
 *                                          device_count will contain the required buffer size
 *                                          out - devices buffer size used
 *                                          Note: device_count cannot be NULL
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsGetDevices(pti_device_properties_t* devices,
                       uint32_t* device_count);

/**
 * @brief Get the properties of the metric groups supported by the device
 * Usage: 1- Call ptiMetricsGetMetricGroups(device_handle, null, metrics_group_count) to discover the required buffer size; the required buffer size will be written
 *        to metrics_group_count.
 *        2- Allocate metrics_groups buffer of size sizeof(pti_metrics_group_properties_t) * metrics_group_count
 *        3- Call ptiMetricsGetMetricGroups(device_handle, metrics_groups, metrics_group_count) again to get the properties of the discovered metric groups written
 *           to the supplied buffer
 *
 * @param[in] device_handle                  Device handle
 * @param[in/out] metrics_groups             Buffer where to save metric group properties for the metric groups that can be collected.
 *                                           Set to null to discover the required buffer size
 * @param[in/out] metrics_group_count        If metrics_groups is set to null or metrics_group_count is less than the required minimum buffer size,
 *                                           metrics_group_count will contain the metric group count
 *                                           Otherwise, metrics_group_count will contain actual metric group count
 *                                           Note: metrics_group_count cannot be null
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsGetMetricGroups(pti_device_handle_t device_handle,
                            pti_metrics_group_properties_t* metrics_groups,
                            uint32_t *metrics_group_count);

/**
 * @brief Get properties for all metrics in a metric group.
 * Usage: 1- Get available metric groups on a specified device using ptiMetricsGetMetricGroups
 *        2- In the metric group properties structure of the metric group of interest, allocate buffer _metric_properties of size sizeof(pti_metric_properties_t) * _metric_count
 *        3- call ptiMetricsGetMetricsProperties(metrics_group_handle, _metric_properties) to get the metric properties written to the supplied buffer
 *
 * @param[in] metrics_group_handle           Metric group handle
 * @param[in/out] metrics                    Buffer where to save metric properties for the specified metric group
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsGetMetricsProperties(pti_metrics_group_handle_t metrics_group_handle,
                                pti_metric_properties_t* metrics);

typedef struct _pti_metrics_group_collection_params_t {
  size_t _struct_size;                            //!< [in] Size of the pti_metirc_config_collection_params struct used for backwards compatibility
  pti_metrics_group_handle_t _group_handle;       //!< [in] Metric group handle.
  uint32_t _sampling_interval;                    //!< [in] Set the sampling interval in nsec.
                                                  //!<      This field is applicable for PTI_METRIC_GROUP_TYPE_TIME_BASED metrics groups only.
  uint32_t _time_aggr_window;                     //!< [in] Set the time aggregation window in nsec.
                                                  //!<      This field is applicable for PTI_METRIC_GROUP_TYPE_TRACE_BASED metrics groups only.
} pti_metrics_group_collection_params_t;

/**
 * @brief Configure metric groups of interest.
 * Note: only 1 metric group of type PTI_METRIC_GROUP_TYPE_TIME_BASED can be specified at this time.
 * TODO: add support for multiple metric groups and different types
 *
 * @param[in] device_handle                  Device handle
 * @param[in] metric_config_params           Buffer of input parameters structures. Note: only 1 is supported at this time
 * @param[in] metrics_group_count            Number of configuration structures in the configuration buffer
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsConfigureCollection(pti_device_handle_t device_handle,
                                pti_metrics_group_collection_params_t *metrics_group_collection_params,
                                uint32_t metrics_group_count);

/**
 * @brief Start metrics collection on specified device
 * Note: ptiMetricsConfigureCollection must be called first to configure the metric group(s) of interest
 *
 * @param[in] device_handle                  Device handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsStartCollection(pti_device_handle_t device_handle);

/**
 * @brief Start metrics collection on specified device in paused mode
 * Note: ptiMetricsConfigureCollection must be called first to configure the metric group(s) of interest
 *
 * @param[in] device_handle                  Device handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsStartCollectionPaused(pti_device_handle_t device_handle);

/**
 * @brief Pause metrics collection on specified device
 * Note: Collection  must be started first
 *
 * @param[in] device_handle                  Device handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsPauseCollection(pti_device_handle_t device_handle);

/**
 * @brief Resume metrics collection on specified device
 * Note: Collection must be started and paused
 *
 * @param[in] device_handle                  Device handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsResumeCollection(pti_device_handle_t device_handle);

/**
 * @brief stop metrics collection on specified device
 * Note: ptiMetricsStartCollection must be called first to start the collection
 * This function terminates the collection but does not process the data
 * @param[in] device_handle                  Device handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsStopCollection(pti_device_handle_t device_handle);

/**
 * @brief process and dump collected data on specified device
 * Note: ptiMetricsStopCollection must be called first to process collected data
 * ptiMetricGetCalculatedData can only be called once after the collection is stopped and cannot be called between pause and resume
 *
 * usage: 1- Call ptiMetricGetCalculatedData(device_handle, metrics_group_handle, NULL, metrics_values_count) to discover the required buffer size for
 *           data collected for the specified metric group on on the specified device ;
 *           the required buffer size will be written to value_count in multiples of pti_value_t.
 *        2- Allocate metrics_values_buffer for holding  metrics_values_count values
 *        3- Call ptiMetricGetCalculatedData(device_handle, metrics_group_handle, metrics_values_buffer, metrics_values_count) to get the values written to buffer
 * A sample contains a 64bit value container for each metric in the metric group.
 * based on the metric's value type, the 64bit value container should be converted appropriately.
 *
 * @param[in] device_handle                 Device handle
 * @param[in] metrics_group_handle          Metric Group handle
 * @param[in/out] metrics_values_buffer     Buffer where to save collected samples
 *                                          Set to null to discover the required buffer size
 * @param[in/out] metrics_values_count      If metrics_values_buffer is set to null, metrics_values_count will contain the number of pti_value_t
 *                                          values the buffer needs to be able to hold
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricGetCalculatedData(pti_device_handle_t device_handle,
                          pti_metrics_group_handle_t metrics_group_handle,
                          pti_value_t* metrics_values_buffer,
                          uint32_t* metrics_values_count);

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_METRICS_H_
