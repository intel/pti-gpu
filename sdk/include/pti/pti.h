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

  PTI_ERROR_INTERNAL = 200  //!< internal error
} pti_result;

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


#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_H_
