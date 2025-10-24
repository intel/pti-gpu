//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_API_PTI_METRICS_SCOPE_H_
#define SRC_API_PTI_METRICS_SCOPE_H_

#include <stddef.h>
#include <stdint.h>

#include "pti/pti.h"
#include "pti/pti_metrics.h"

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * This file contains the PTI MetricsScope API that enables collecting GPU hardware metrics for
 * individual GPU kernels submitted by the application.
 * Such collection might be more practical for users than time-based collection.
 *
 * MetricsScope operates in three phases:
 * - Configuration
 * - Collection
 * - Metrics evaluation
 *
 * Configuration flow:
 *   1. Call ptiMetricsScopeEnable to create a scope collection handle
 *   2. Call ptiMetricsGetDevices to get available devices (defined in pti_metrics.h)
 *   3. Call ptiMetricsScopeConfigure to pass desired metrics for a specific device
 *   4. Call ptiMetricsScopeQueryCollectionBufferSize to query the estimated buffer size
 *   5. Call ptiMetricsScopeSetCollectionBufferSize to set the desired collection buffer size
 *
 * Collection is initiated by calling ptiMetricsScopeStartCollection
 * and ended by calling ptiMetricsScopeStopCollection.
 *
 * For the Collection and Metrics evaluation phases, there are 2 types of buffers that MetricsScope deals with:
 *  Collection buffers - buffers that accumulate raw data during collection;
 *                       mostly black boxes for users;
 *                       one buffer contains data for one specific device;
 *                       owned by PTI;
 *                       buffer size (same for all) is setup by the user during the configuration phase;
 *                       PTI allocates buffers on demand during collection
 *
 *  Metrics buffers    - buffers where PTI populates requested metrics after collection has stopped,
 *                       taking collection buffer(s) as input;
 *                       owned by the user;
 *                       contain a set of uniform records of pti_metrics_scope_record_t type
 */

typedef struct _pti_scope_collection_handle_t* pti_scope_collection_handle_t;

/**
 * @brief MetricsScope collection modes
 */
typedef enum _pti_metrics_scope_mode_t {
    PTI_METRICS_SCOPE_INVALID_MODE = 0,          //!< Invalid mode
    PTI_METRICS_SCOPE_AUTO_KERNEL = 1,           //!< Automatic per GPU kernel collection
    PTI_METRICS_SCOPE_USER = 2,                  //!< User-controlled scope (not implemented at the moment)

    PTI_METRICS_SCOPE_MODE_FORCE_UINT32 = 0x7fffffff
} pti_metrics_scope_mode_t;

/**
 * @brief Collection Buffer properties structure
 */
typedef struct _pti_metrics_scope_collection_buffer_properties_t {
    size_t              _struct_size;                   //!< To be set by the user prior to passing a pointer to this structure to the PTI API
    pti_device_handle_t _device_handle;                 //!< Device for which data is contained in the buffer
    size_t              _num_scopes;                    //!< Number of scopes/records in the buffer
    size_t              _buffer_size;                   //!< Size of the collection buffer used in bytes
    uint64_t            _host_time_first_scope_append;  //!< Currently not populated
    uint64_t            _host_time_last_scope_append;   //!< Currently not populated
    const char*         _metric_group_name;             //!< Pointer to Metrics Group name; can be used for informational purposes;
                                                        //!< valid until ptiMetricsScopeDisable is called
} pti_metrics_scope_collection_buffer_properties_t;

/**
 * @brief Scope record structure
 *
 * Its size depends on the number of requested metrics.
 * These records are uniformly populated in the Metrics Buffer
 */
typedef struct _pti_metrics_scope_record_t {
    uint64_t            _kernel_id;                  //!< Unique identifier for the kernel instance
    pti_backend_queue_t _queue;                      //!< Command queue handle
    const char*         _kernel_name;                //!< Pointer to kernel name,
                                                     //!< guaranteed to be valid for the lifetime of the parent MetricsScope
    pti_value_t*        _metrics_values;             //!< Array of metric values
} pti_metrics_scope_record_t;

/**
 * @brief Scope record metadata structure that describes the metrics stored in pti_metrics_scope_record_t
 */
typedef struct _pti_metrics_scope_record_metadata_t {
    size_t                  _struct_size;        //!< To be set by the user prior to passing a pointer to this structure to the PTI API
    size_t                  _metrics_count;      //!< Number of metrics per scope record
                                                 //!< and size of each of the three following arrays
    pti_metric_value_type*  _value_types;        //!< Array of metric value types
                                                 //!< valid until ptiMetricsScopeDisable is called
    const char**            _metric_names;       //!< Array of metric names
                                                 //!< valid until ptiMetricsScopeDisable is called
    const char**            _metric_units;       //!< Array of metric units
                                                 //!< valid until ptiMetricsScopeDisable is called
} pti_metrics_scope_record_metadata_t;

/**
 * @brief Allocate and initialize the scope collection handle
 * Usage: Call this function first to create a scope collection handle before configuring metrics collection
 *
 * @param[out] scope_collection_handle       Pointer to store the scope collection handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeEnable(pti_scope_collection_handle_t* scope_collection_handle);

/**
 * @brief Configure MetricsScope collection
 *
 * @param[in] scope_collection_handle        Scope collection handle obtained from ptiMetricsScopeEnable
 * @param[in] collection_mode                Collection mode;
 *                                           currently only PTI_METRICS_SCOPE_AUTO_KERNEL is supported
 * @param[in] devices_to_profile             Array of device handles for target devices;
 *                                           currently only one device per MetricsScope is supported
 * @param[in] device_count                   Number of devices in the devices_to_profile array
 * @param[in] metric_names                   Array of metric names to collect
 * @param[in] metric_count                   Number of metric names in the metric_names array
 *
 * @return pti_result
 *
 */
pti_result PTI_EXPORT
ptiMetricsScopeConfigure(pti_scope_collection_handle_t scope_collection_handle,
                         pti_metrics_scope_mode_t collection_mode,
                         pti_device_handle_t* devices_to_profile,
                         uint32_t device_count,
                         const char** metric_names,
                         size_t metric_count);

/**
 * @brief Query the estimated collection buffer size required for collecting metrics for the specified scope count
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] scopes_number                  Number of scopes to estimate collection buffer size
 * @param[out] estimated_buffer_size         Pointer to store the estimated collection buffer size in bytes
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeQueryCollectionBufferSize(pti_scope_collection_handle_t scope_collection_handle,
                                         size_t scopes_number,
                                         size_t* estimated_buffer_size);

/**
 * @brief Set the collection buffer size to be used during collection
 * Note: As soon as the first such buffer is full, PTI will allocate a second one and so on.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] buffer_size                    Size of the collection buffer in bytes for later allocation
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeSetCollectionBufferSize(pti_scope_collection_handle_t scope_collection_handle,
                                       size_t buffer_size);


/**
 * @brief Start metrics scope collection
 *
 * @param[in] scope_collection_handle        Scope collection handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeStartCollection(pti_scope_collection_handle_t scope_collection_handle);

/**
 * @brief Stop metrics scope collection
 *
 * @param[in] scope_collection_handle        Scope collection handle
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeStopCollection(pti_scope_collection_handle_t scope_collection_handle);

/**
 * @brief Disable MetricsScope and free all associated resources
 *
 * @param[in] scope_collection_handle        Scope collection handle to cleanup
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeDisable(pti_scope_collection_handle_t scope_collection_handle);

/**
 * @brief Get the number of collection buffers available
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[out] buffer_count                  Pointer to store the number of available collection buffers
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeGetCollectionBuffersCount(pti_scope_collection_handle_t scope_collection_handle,
                                         size_t* buffer_count);

/**
 * @brief Get the collection buffer of the specified index and its size
 * Note: The size might be handy for future usage when such buffers might be stored by the user
 * for fully offline processing.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] buffer_index                   Index of the collection buffer to retrieve
 * @param[out] buffer                        Pointer to store the collection buffer address
 * @param[out] buffer_size                   Pointer to store the collection buffer size in bytes
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeGetCollectionBuffer(pti_scope_collection_handle_t scope_collection_handle,
                                   size_t buffer_index,
                                   void** buffer,
                                   size_t* buffer_size);

/**
 * @brief Get information about the collection buffer
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] collection_buffer              Collection buffer to query properties for
 * @param[in/out] props                      Pointer to store the collection buffer properties;
 *                                           user must not forget to initialize props->_struct_size prior to the call
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeGetCollectionBufferProperties(
    pti_scope_collection_handle_t scope_collection_handle,
    void* collection_buffer,
    pti_metrics_scope_collection_buffer_properties_t* props);

/**
 * @brief Query for the required metrics buffer size for storing calculated metrics records
 *
 * This function calculates the exact metrics buffer size needed to store all records
 * from a collection buffer, including space for strings and metric values.
 *
 * @param[in] scope_collection_handle           Scope collection handle
 * @param[in] collection_buffer                 Collection buffer to query
 * @param[out] required_metrics_buffer_size     Required metrics buffer size in bytes
 * @param[out] records_count                    Number of records that will be stored
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeQueryMetricsBufferSize(pti_scope_collection_handle_t scope_collection_handle,
                                     void* collection_buffer,
                                     size_t* required_metrics_buffer_size,
                                     size_t* records_count);

/**
 * @brief Calculate metrics from collection data and populate into user-provided metrics buffer
 * Usage:   The user must first call ptiMetricsScopeQueryMetricsBufferSize to determine
 *          the required metrics buffer size, then allocate the metrics buffer and call this function.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] collection_buffer              Collection buffer containing raw metrics data
 * @param[in] metrics_buffer                 User metrics buffer for storing records
 * @param[in] metrics_buffer_size            Size of metrics_buffer in bytes
 * @param[out] records_count                 Number of records that will be/were written to the metrics buffer
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeCalculateMetrics(pti_scope_collection_handle_t scope_collection_handle,
                                void* collection_buffer,
                                void* metrics_buffer,
                                size_t metrics_buffer_size,
                                size_t* records_count);


/**
 * @brief Get metadata for user's metrics buffer in a scope collection
 * This function provides type and unit information for all requested metrics.
 * Call this function to get metadata per scope that applies to all records in all buffers.
 *
 * The metadata structure contains direct pointers to metric information stored within the 
 * scope collection handle. These pointers remain valid until ptiMetricsScopeDisable is called.
 * 
 * Usage:
 *  - User must set metadata->_struct_size = sizeof(pti_metrics_scope_record_metadata_t) before calling
 *  - Function populates metadata->_metrics_count and the three array pointers
 *  - No memory allocation is performed by this function
 *  - Returned pointers reference internal scope collection data (no copying)
 *
 * @param[in] scope_collection_handle    Scope collection handle
 * @param[out] metadata                  Metadata structure to populate
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiMetricsScopeGetMetricsMetadata(pti_scope_collection_handle_t scope_collection_handle,
                                  pti_metrics_scope_record_metadata_t* metadata);


#if defined(__cplusplus)
}
#endif

#endif  // SRC_API_PTI_METRICS_SCOPE_H_
