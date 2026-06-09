//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef INCLUDE_PTI_H_
#define INCLUDE_PTI_H_

#include "pti/pti_export.h"
#include "pti/pti_version.h"

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/* @brief Maximum device UUID size in bytes. */
#define PTI_MAX_DEVICE_UUID_SIZE  16

/**
 * @brief ABI compatibility/static assert helper for public PTI types
 *
 * If any of these assertions fail, it indicates an ABI-breaking change
 * in a public PTI API type or definition.
 */
#if defined(__cplusplus)
#define PTI_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define PTI_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
/* Fallback for pre-C11: generates a compile error if condition is false */
#define PTI_STATIC_ASSERT_CONCAT_(a, b) a##b
#define PTI_STATIC_ASSERT_CONCAT(a, b) PTI_STATIC_ASSERT_CONCAT_(a, b)
#define PTI_STATIC_ASSERT(cond, msg) \
    typedef char PTI_STATIC_ASSERT_CONCAT(pti_static_assertion_, __LINE__)[(cond) ? 1 : -1]
#endif

/**
 * @brief Return/Error codes
 */
typedef enum {
  PTI_SUCCESS = 0,                        //!< success
  PTI_STATUS_END_OF_BUFFER = 1,           //!< end of buffer reached, e.g., in ptiViewGetNextRecord
  PTI_ERROR_NOT_IMPLEMENTED = 2,          //!< functionality not implemented
  PTI_ERROR_BAD_ARGUMENT = 3,             //!< invalid argument
  PTI_ERROR_NO_CALLBACKS_SET = 4,         //!< error due to no callbacks set via ptiViewSetCallbacks
  PTI_ERROR_EXTERNAL_ID_QUEUE_EMPTY = 5,  //!< empty external ID-queue while working with
                                          //!< PTI_VIEW_EXTERNAL_CORRELATION
  PTI_ERROR_BAD_TIMESTAMP = 6,            //!< error in timestamp conversion, might be related with the user
                                          //!< provided TimestampCallback
  PTI_ERROR_BAD_API_ID = 7,               //!< invalid api_id when enable/disable runtime/driver specific api_id
  PTI_ERROR_NO_GPU_VIEWS_ENABLED = 8,     //!< at least one GPU view must be enabled for kernel tracing

  PTI_ERROR_DRIVER = 50,                  //!< unknown driver error
  PTI_ERROR_TRACING_NOT_INITIALIZED = 51,  //!< installed driver requires tracing enabling with
                                           //!< setting environment variable ZE_ENABLE_TRACING_LAYER
                                           //!< to 1
  PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED = 52,  //!< no Local profiling support in the installed
                                                    //!< driver

  PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED = 100, //!< metrics collection not running
  PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED = 101, //!< metrics collection not stopped
  PTI_ERROR_METRICS_COLLECTION_NOT_PAUSED = 102, //!< metrics collection not paused
  PTI_ERROR_METRICS_COLLECTION_ALREADY_PAUSED = 103, //!< metrics collection already paused
  PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED = 104, //!< metrics collection already running
  PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION = 105, //!< bad metrics collection configuration
  PTI_ERROR_METRICS_NO_DATA_COLLECTED = 106, //!< Calculate called on empty collection

  PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND = 151, //!< One or more requested metrics not found across all metrics groups
  PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP = 152, //!< When requested metrics cannot be collected within one metrics group

  PTI_ERROR_METRICS_SCOPE_OUT_OF_MEMORY = 153, //!< Out of memory during metrics scope operation
  PTI_ERROR_METRICS_SCOPE_COLLECTION_BUFFER_TOO_SMALL = 154, //!< Size of the buffer is not enough 
                                                             //!< to fit even one Metrics Scope collection
  PTI_ERROR_METRICS_SCOPE_INSUFFICIENT_BUFFER = 155, //!< Insufficient buffer for metrics scope operation
  PTI_ERROR_METRICS_SCOPE_INVALID_COLLECTION_BUFFER = 156,  //!< Invalid collection buffer in metrics scope
  PTI_WARN_METRICS_SCOPE_PARTIAL_BUFFER = 157,  //!< Warning: partial buffer populated in metrics scope
  PTI_WARN_BUFFER_NOT_FINALIZED = 158, //!< Warning: current buffer not finalized, cannot create new buffer
  PTI_ERROR_METRICS_SCOPE_DEVICE_TYPE_NOT_UNIFORM = 159, //!< Requested devices are not the same type

  PTI_ERROR_INTERNAL = 200,  //!< internal error

  PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED = 250,  //!< another PC sampling handle is already enabled
  PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED = 251,  //!< handle is not in the required configuration state
  PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED = 252,  //!< handle is already configured - not in the initial state for configuration
  PTI_ERROR_PC_SAMPLING_ALREADY_STARTED = 253,  //!< collection is already running
  PTI_ERROR_PC_SAMPLING_NOT_STARTED = 254,  //!< collection has not started yet
  PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED = 255,  //!< collection is already stopped
  PTI_ERROR_PC_SAMPLING_NOT_STOPPED = 256,  //!< collection is not stopped yet
  PTI_ERROR_PC_SAMPLING_UNSUPPORTED = 257,  //!< PC sampling is not supported on the current system configuration (e.g. no devices with EUStallSampling support found)

  PTI_RESULT_FORCE_UINT32 = 0x7fffffff
} pti_result;

PTI_STATIC_ASSERT(sizeof(pti_result) == sizeof(uint32_t), "pti_result enum should be equal to size of uint32_t");

/**
 * @brief Helper function to return stringified enum members for pti_result
 *
 * @return const char*
 */
PTI_EXPORT const char* ptiResultTypeToString(pti_result result_value);


/**
 * @brief Abstraction for backend-specific objects.
 *
 * Level Zero is currently the only supported backend. However, these types will attempt to serve other backends.
 * In case the other backend supported - the same types will serve it.
 */

typedef void* pti_device_handle_t;  //!< Device handle

typedef void* pti_backend_ctx_t;    //!< Backend context handle

typedef void* pti_backend_queue_t;  //!< Backend queue handle

typedef void* pti_backend_evt_t;    //!< Backend event handle

typedef void* pti_backend_command_list_t; //!< Backend command list handle

typedef void* pti_backend_kernel_t; //!< Backend kernel object handle

typedef void* pti_backend_module_t; //!< Backend module object handle


#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_H_
