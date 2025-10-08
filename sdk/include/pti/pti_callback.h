//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_CALLBACK_H_
#define PTI_CALLBACK_H_

#include <stdint.h>

#include "pti/pti.h"
#include "pti/pti_view.h"

/**
 * This file contains APIs that are so far experimental in PTI.
 * APIs and data structures in this file are work-in-progress and subject to change!
 * All content in this file concerns the Callback API.
 *
 * The Callback API is useful for many purposes,
 * including the implementation of `MetricsScope` functionality that needs to subscribe to
 * domains such as kernel append to a command list, and potentially other domains.
 * The `MetricsScope` API is under development and is the first (internal) user of the Callback API.
 */


/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _pti_callback_subscriber* pti_callback_subscriber_handle;

typedef enum _pti_callback_domain {
  PTI_CB_DOMAIN_INVALID                            = 0,
  PTI_CB_DOMAIN_DRIVER_CONTEXT_CREATED             = 1, //!< Not implemented yet
                                                     //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED

  PTI_CB_DOMAIN_DRIVER_MODULE_LOADED               = 2, //!< Not implemented yet
                                                     //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED

  PTI_CB_DOMAIN_DRIVER_MODULE_UNLOADED             = 3, //!< Not implemented yet
                                                     //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED

  PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED      = 4, //!< Synchronous callback
                                                     //!< This also serves as PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED
                                                     //!< when appended to Immediate Command List,
                                                     //!< which means no separate callback PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED

  PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED    = 5, //!< Not implemented yet
                                                     //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED

  PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED     = 6, //!< Asynchronous callback, always has only EXIT phase of some API,
                                                      //!< where completed operations are collected and reported

  PTI_CB_DOMAIN_DRIVER_HOST_SYNCHRONIZATION        = 7, //!< Not implemented yet
                                                     //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED

  PTI_CB_DOMAIN_DRIVER_API                         = 1023, //!< Not implemented yet,
                                                        //!< attempt to enable it will return PTI_ERROR_NOT_IMPLEMENTED
                                                        //!< Callback created for all Driver APIs
  // below domains to inform user about PTI internal events
  PTI_CB_DOMAIN_INTERNAL_THREADS                = 1024, //!< Not implemented yet
  PTI_CB_DOMAIN_INTERNAL_EVENT                  = 1025, //!< Not implemented yet

  PTI_CB_DOMAIN_MAX                             = 0x7fffffff
} pti_callback_domain;

typedef enum _pti_callback_phase {
  PTI_CB_PHASE_INVALID                 = 0,
  PTI_CB_PHASE_API_ENTER               = 1,
  PTI_CB_PHASE_API_EXIT                = 2,
  PTI_CB_PHASE_INTERNAL_THREAD_START   = 3,
  PTI_CB_PHASE_INTERNAL_THREAD_END     = 4,
  PTI_CB_PHASE_INTERNAL_EVENT          = 5,

  PTI_CB_PHASE_MAX                     = 0x7fffffff
} pti_callback_phase;

typedef enum _pti_backend_command_list_type {
  PTI_BACKEND_COMMAND_LIST_TYPE_UNKNOWN   = (1<<0),
  PTI_BACKEND_COMMAND_LIST_TYPE_IMMEDIATE = (1<<1),
  PTI_BACKEND_COMMAND_LIST_TYPE_MUTABLE   = (1<<2),

  PTI_BACKEND_COMMAND_LIST_TYPE_MAX       = 0x7fffffff
} pti_backend_command_list_type;

/**
 * A user can subscribe to notifications about non-standard situations from PTI
 * when it collects or processes the data
 */
typedef enum _pti_internal_event_type {
  PTI_INTERNAL_EVENT_TYPE_INFO       = 0,
  PTI_INTERNAL_EVENT_TYPE_WARNING    = 1, // one or a few records data inconsistencies, or other
                                          // collection is safe to continue
  PTI_INTERNAL_EVENT_TYPE_CRITICAL   = 2, // critical error after which further collected data are invalid

  PTI_INTERNAL_EVENT_TYPE_MAX        = 0x7fffffff
} pti_internal_event_type;

typedef enum _pti_gpu_operation_kind {
  PTI_GPU_OPERATION_KIND_INVALID         = 0,
  PTI_GPU_OPERATION_KIND_KERNEL          = 1,
  PTI_GPU_OPERATION_KIND_MEMORY          = 2,
  PTI_GPU_OPERATION_KIND_OTHER           = 3,

  PTI_GPU_OPERATION_KIND_MAX             = 0x7fffffff
} pti_gpu_operation_kind;

typedef struct _pti_gpu_op_details {
  pti_gpu_operation_kind             _operation_kind; //<! Kind of the operation: kernel, mem op
  uint64_t                           _operation_id;   //<! GPU kernel or memory operation instance ID,
                                                      //<! uniquely throughout the process
  uint64_t                           _kernel_handle;  //!< a handle uniquely identifying kernel object as
                                                      //!< contained in the module at the specific offset
                                                      //!< it will be zero in case of not implemented yet or
                                                      //!< for memory operations
  const char*                        _name;           //!< symbolic name of a kernel or memcpy operation
} pti_gpu_op_details;

typedef struct _pti_callback_gpu_op_data {
  pti_callback_domain             _domain;              //!< domain of the callback
  pti_backend_command_list_type   _cmd_list_properties; //!< immediate, mutable,..
  pti_backend_command_list_t      _cmd_list_handle;     //!< Device back-end command list handle,
                                                        //!< could be nullptr if unknown or
                                                        //!< when several operations with different command lists
                                                        //!< reported together
  pti_backend_queue_t             _queue_handle;        //!< Device back-end queue handle,
                                                        //!< could be nullptr if unknown
                                                        //!< when several operations with different command lists
                                                        //!< reported together
  pti_device_handle_t             _device_handle;       //!< Device handle
  pti_callback_phase              _phase;               //!< PTI_CB_PHASE_API_ENTER/EXIT
  uint32_t                        _return_code;         //!< will be valid only for L0 API EXIT, for others will be zero
  uint32_t                  _correlation_id;    //!< ID that corresponds to the same call reported by View API records
  uint32_t                  _operation_count;   //!< number of operations appended or dispatched to the GPU
  pti_gpu_op_details*       _operation_details; //!< pointer to details of operation(s) appended, dispatched or completed
} pti_callback_gpu_op_data;

typedef struct _pti_internal_callback_data {
  pti_callback_domain  _domain;       //!< domain of the callback
  pti_callback_phase   _phase;        //!< THREAD START/END or INTERNAL EVENT
  uint32_t             _detail;       //!< depending on the domain should be casted/interpreted
                                      //!< as a purpose of an internal PTI thread or
                                      //!< pti_internal_event_type
  const char*          _message;      //!< explains details
} pti_internal_callback_data;

typedef void (*pti_callback_function)(
  pti_callback_domain  domain,
  pti_api_group_id     driver_api_group_id, //!< driver API group ID, keep it to distinguish between L0 and OpenCL
                                            //!< although the current implementation is only for L0
  uint32_t             driver_api_id,
  pti_backend_ctx_t    backend_context,     //!< Driver (L0) level context handle
  void*                cb_data, //!< depending on the domain, it should be type-casted to the pointer
                                //!< to either pti_callback_gpu_op_data, pti_internal_callback_data,
                                //!< or to other types to be defined
  void*                global_user_data,    //!< Any global data defined by user returned
                                            //!< to every callback from a same subscriber
  void**         instance_user_data);       //!< Data that could be passed between ENTER and EXIT
                                            //!< phases of one API call

/**
 * Callback API functions
 * None of the PTI API functions should be called from within a Callback function.
 * Exceptions are helper functions that return character representations of enums.
 */

/**
 * @brief Initialize Callback subscriber
 *
 * @param subscriber - subscriber handle
 * @param callback   - pointer to the callback function
 * @param user_data  - user data to be passed to the callback function
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackSubscribe(pti_callback_subscriber_handle* subscriber,
                     pti_callback_function    callback,
                     void* user_data);

/**
 * @brief Unsubscribe Callback subscriber. This unsubscribes from all domains, disables the callback,
 *        cleans up all resources related to the subscriber handle, and invalidates the handle.
 */
pti_result PTI_EXPORT
ptiCallbackUnsubscribe(pti_callback_subscriber_handle subscriber);

/**
 * @brief Enables callbacks on specific domain
 *
 * @param subscriber - subscriber handle
 * @param domain     - domain to enable
 * @param enter_cb   - indicate if callback called on enter/start: 0-no, 1-yes; used only for domains with 2 phases
 * @param exit_cb    - indicates if callback is called on exit/end: 0-no, 1-yes; used only for domains with 2 phases
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackEnableDomain(pti_callback_subscriber_handle subscriber,
                        pti_callback_domain  domain,
                        uint32_t enter_cb,
                        uint32_t exit_cb);

/**
 * @brief Disables callbacks for specific domain
 */
pti_result PTI_EXPORT
ptiCallbackDisableDomain(pti_callback_subscriber_handle subscriber,
                         pti_callback_domain  domain);

/**
 * @brief Disables the callback of the subscriber for all domains
 */
pti_result PTI_EXPORT
ptiCallbackDisableAllDomains(pti_callback_subscriber_handle subscriber);

/**
 * @brief Helper function to return stringified enum members for pti_callback_domain
 *
 * @return const char*
 */
PTI_EXPORT const char* ptiCallbackDomainTypeToString(pti_callback_domain domain);

/**
 * @brief Helper function to return stringified enum members for pti_callback_phase
 *
 * @return const char*
 */
PTI_EXPORT const char* ptiCallbackPhaseTypeToString(pti_callback_phase phase);

#if defined(__cplusplus)
}
#endif
#endif  // PTI_CALLBACK_H_
