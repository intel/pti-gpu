//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef INCLUDE_PTI_CALLBACK_H_
#define INCLUDE_PTI_CALLBACK_H_

#include <stddef.h>
#include <stdint.h>

#include "pti/pti.h"
#include "pti/pti_driver_levelzero_api_ids.h"
#include "pti/pti_export.h"
#include "pti/pti_view.h"

/**
 *  This file contains the draft definitions of the callback API.
 *  It is subject of discussion and changes.
 *  As soon as interested parties agreed on the basic terms -
 *  the plan is to do POC implementation of the API.
 *
 *  The tool that using PTI will be able to subscribe to the driver and PTI events.
 *  Driver or PTI functions other than
 *
 *    const char* ptiResultTypeToString(pti_result result_value);
 *    pti_result  ptiViewGetApiIdName(pti_api_group_id type, uint32_t unique_id, const char** name);
 *     pti_result  ptiCallbackGetExternalCorrelationIds(...)
 *    < other to be indicated there >
 *
 *  must not be called from the callbacks.
 *
 *  All data passed into Callback functions are valid only during the callback.
 *  No assumptions should be made if there data would be valid after the callback.
 */

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Callback domains (that are not INTERNAL) inform user about important events about
 * GPU context creating, GPU operation launch, GPU host synchronization.
 * These events delivered through the callbacks of a number of driver APIs.
 *
 * Each domain corresponds to a group of APIs and will be extended if new APIs are added
 * to the driver. This way - it will be transparent for a user and
 * do not require to setup callback to individual APIs.
 *
 * However, if there is a need, a user can subscribe to individual APIs if needed,
 * via ptiCallbackEnableDriverAPI function.
 *
 * The subscriptions via Domain API and via individual API are treated as different subscriptions and
 * so a user might have two callbacks for the same API - one for the domain and one for the individual API.
 * The callback will be called for the domain and for the individual API.
 */
typedef enum _pti_callback_domain {
  PTI_DOMAIN_UNNAMED                         = 0, // Callback created for specific API ID
  PTI_DOMAIN_DRIVER_CONTEXT_CREATE           = 1,
  PTI_DOMAIN_DRIVER_GPU_OPERATION_LAUNCH     = 2,
  PTI_DOMAIN_DRIVER_HOST_SYNCHRONIZATION     = 3,
  // below domains to inform user about PTI internal events
  PTI_DOMAIN_INTERNAL_THREADS                = 20,
  PTI_DOMAIN_INTERNAL_EVENT                  = 21,

  PTI_DOMAIN_MAX                             = 0x7fffffff
} pti_callback_domain;

typedef enum _pti_callback_site {
  PTI_SITE_INVALID                 = 0,
  PTI_SITE_DRIVER_API_ENTER        = 1,
  PTI_SITE_DRIVER_API_EXIT         = 2,
  PTI_SITE_INTERNAL_THREAD_START   = 3,
  PTI_SITE_INTERNAL_THREAD_END     = 4,
  PTI_SITE_INTERNAL_EVENT          = 5,

  PTI_SITE_MAX                     = 0x7fffffff
} pti_callback_site;

typedef enum _pti_backend_submit_type {
  PTI_BACKEND_SUBMIT_TYPE_INVALID   = 0,
  PTI_BACKEND_SUBMIT_TYPE_IMMEDIATE = 1,
  PTI_BACKEND_SUBMIT_TYPE_QUEUE     = 2,

  PTI_BACKEND_SUBMIT_TYPE_MAX       = 0x7fffffff
} pti_backend_submit_type;

/**
 * INTERNAL DOMAINs Callback inform user about PTI internal operations and could be used to
 * characterize internal overhead of the collection or data processing, or some non-standard situations.
 *
 * Most of PTI work happens in the application threads in the callbacks to runtime and driver functions.
 * But in some cases PTI creates its own threads to do some work:
 * as of today - to deliver data to the user and to collect GPU data hadware metrics.
 */
typedef enum _pti_thread_purpose {
  PTI_THREAD_PURPOSE_INVALID         = 0,
  PTI_THREAD_PURPOSE_SERVICE         = 1,
  PTI_THREAD_PURPOSE_DATA_DELIVERY   = 2,
  PTI_THREAD_PURPOSE_DATA_COLLECTION = 3, // for example, thread that collects GPU HW metrics

  PTI_THREAD_PURPOSE_MAX             = 0x7fffffff
} pti_thread_purpose;

/**
 * A user can subscribe to notifications about non-standard situation from PTI
 * when it collects or processes the data
 */
typedef enum _pti_internal_event_type {
  PTI_INTERNAL_EVENT_TYPE_INFO       = 0,
  PTI_INTERNAL_EVENT_TYPE_WARNING    = 1, // one or few records data inconsistences, or other
                                          // collection is safe to continue
  PTI_INTERNAL_EVENT_TYPE_CRITICAL   = 2, // critical error after which further collected data are invalid

  PTI_INTERNAL_EVENT_TYPE_MAX        = 0x7fffffff
} pti_internal_event_type;

typedef enum _pti_operation_kind {
  PTI_OPERATION_KIND_INVALID         = 0,
  PTI_OPERATION_KIND_KERNEL          = 1,
  PTI_OPERATION_KIND_MEMCPY          = 2,
  PTI_OPERATION_KIND_MEM_FILL        = 3,

  PTI_OPERATION_KIND_MAX             = 0x7fffffff
} pti_operation_kind;

typedef struct _pti_gpu_op_launch_detail {
  pti_operation_kind                 _operation_kind; // kind of the operation: kernel, mem op
  uint32_t                           _correlation_id; // correlation IDs of operation append
  const char*                        _kernel_name;    // type of the memcpy operation
} pti_gpu_op_launch_detail;

typedef struct _pti_apicall_callback_data {
  pti_callback_domain       _domain;            // domain of the callback
  pti_backend_submit_type   _submit_type;       // Immediate command list, Command Queue execute,..
  pti_callback_site         _site;              // ENTER or EXIT
  uint32_t                  _return_code;       // will be valid only for L0 API EXIT, for others will be nullptr
  uint32_t                  _correlation_id;    // ID that corresponds to the same call reported by View API records
  uint32_t                  _operation_count;   // number of operations submitted to the GPU,
                                                // non-zero only for DOMAIN_DRIVER_GPU_OPERATION_LAUNCH,
                                                // or API related to GPU operation submission
  pti_gpu_op_launch_detail* _operation_details; // not Null only for DOMAIN_DRIVER_GPU_OPERATION_LAUNCH,
                                                // or API related to GPU operation submission.
} pti_apicall_callback_data;

typedef struct _pti_internal_callback_data {
  pti_callback_domain  _domain;        // domain of the callback
  pti_callback_site    _site;          // THREAD START/END or INTERNAL EVENT
  uint32_t             _detail;       // depending on the domain should be casted/interpreted
                                       // as a purpose of an internal PTI thread or
                                       // pti_internal_event_type
  const char*          _message;       // explains details
} pti_internal_callback_data;

typedef void (*pti_callback_function)(
  pti_callback_domain  domain,
  pti_api_group_id     driver_api_group_id, // driver API group ID, keep it to distinguish between L0 and OpenCL
                                            // although the current implementation is only for L0
  uint32_t             driver_api_id,
  pti_backend_ctx_t    backend_context,     // Driver (L0) level context handle
  void*                cb_data, // depending on the domain it should be type-casted to the pointer
                               // to either pti_apicall_callback_data or pti_internal_callback_data
  void*                user_data);

typedef void* pti_callback_subscriber; // Subscriber handle, current implementation supports only one subscriber
                                       // per process, but it is not a limitation of the API

/**
 * @brief Initiate Callback subscriber
 *
 * @param subscriber - subscriber handle
 * @param callback   - pointer to the callback function
 * @param user_data  - user data to be passed to the callback function
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackSubscribe(pti_callback_subscriber* subscriber,
                     pti_callback_function    callback,
                     void* user_data);

/**
 * @brief Unsubscribe Callback subscriber, this will disable all callbacks and
 *        destroy subscriber handle
 */
pti_result PTI_EXPORT
ptiCallbackUnsubscribe(pti_callback_subscriber subscriber);

/**
 * @brief Enables callbacks on specific domain
 *
 * @param subscriber - subscriber handle
 * @param domain     - domain to enable
 * @param enter_cb   - indicate if callback called on enter/start: 0-no, 1-yes; used only for domains with 2 sites
 * @param exit_cb    - indicates if callback called on exit/end: 0-no, 1-yes; used only for domains with 2 sites
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackEnableDomain(pti_callback_subscriber subscriber,
                        pti_callback_domain  domain,
                        uint32_t enter_cb,
                        uint32_t exit_cb);

/**
 * @brief Disables callbacks on specific domain
 */
pti_result PTI_EXPORT
ptiCallbackDisableDomain(pti_callback_subscriber subscriber,
                         pti_callback_domain  domain);

/**
 * @brief Disables all callbacks for the subscriber, all domains, APIs
 */
pti_result PTI_EXPORT
ptiCallbackDisableAll(pti_callback_subscriber subscriber);

/**
 * @brief Enables callbacks on specific driver API
 *
 * @param subscriber - subscriber handle
 * @param driver_api_group_id - driver API group ID, for now only PTI_API_GROUP_LEVELZERO supported
 * @param driver_api_id - driver API ID
 * @param enter_cb   - indicate if callback called on enter/start: 0-no, 1-yes; used only for domains with 2 sites
 * @param exit_cb    - indicates if callback called on exit/end: 0-no, 1-yes; used only for domains with 2 sites
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackEnableDriverAPI(pti_callback_subscriber subscriber,
                           pti_api_group_id driver_api_group_id,
                           uint32_t driver_api_id,
                           uint32_t enter_cb,
                           uint32_t exit_cb);

/**
 * @brief Disables callbacks on specific driver API
 */
pti_result PTI_EXPORT
ptiCallbackDisableDriverAPI(pti_callback_subscriber subscriber,
                            pti_api_group_id driver_api_group_id,
                            uint32_t driver_api_id);

/**
 * @brief Returns external correlation IDs that are on top of the stack at the Callback point
 *
 * Usage: 1 - pass external_kind and external_id as nullptr to get number of IDs available
 *            on the top of stack at the Callback point
 *        2 - call this API again where number is number of IDs to return,
 *            external_kind and external_id should have enough memory allocated to hold number of IDs
 *            that will be copied to that memory
 *
 * @param[in,out] number    - pointer to retrieve the number of external correlation IDs on the stack
 *                            or to ask for the number of IDs to return
 * @param[in] external_kind - pointer to store kinds of the external correlation IDs
 * @param[in] external_id   - pointer to store external correlation IDs
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackGetExternalCorrelationIds(uint32_t* number,
                                    pti_view_external_kind* external_kind,
                                    uint64_t* external_id);
#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_CALLBACK_H_