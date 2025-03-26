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
#include "pti/pti_export.h"
#include "pti/pti_view.h"
#include "pti/pti_driver_levelzero_api_ids.h"

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
 */

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

typedef enum _pti_callback_domain {
  PTI_DOMAIN_INVALID                         = 0,
  PTI_DOMAIN_DRIVER_CONTEXT_CREATE           = 1,
  PTI_DOMAIN_DRIVER_GPU_OPERATION_LAUNCH     = 2,
  PTI_DOMAIN_DRIVER_HOST_SYNCHRONIZATION     = 3,
  // below domains to inform user about PTI internal events
  PTI_DOMAIN_INTERNAL_THREADS                = 100,
  PTI_DOMAIN_INTERNAL_CRITICAL               = 101,
} pti_callback_domain;

typedef enum _pti_callback_site {
  PTI_SITE_INVALID            = 0,
  PTI_DRIVER_API_ENTER        = 1,
  PTI_DRIVER_API_EXIT         = 2,
  PTI_INTERNAL_THREAD_START   = 3,
  PTI_INTERNAL_THREAD_END     = 4,
  PTI_INTERNAL_CRITICAL_EVENT = 5,
} pti_callback_site;

/**
 * Most of PTI work happens in the application threads in the callbacks to runtime and driver functions.
 * In some cases PTI creates its own threads to do some work:
 * as of today - to deliver data to the user and to collect GPU data hadware metrcis.
 */
typedef enum _pti_thread_purpose {
  PTI_THREAD_INVALID = 0,
  PTI_THREAD_SERVICE = 1,
  PTI_THREAD_DATA_DELIVERY = 2,
  PTI_THREAD_DATA_COLLECTION = 3,      // for example, thread that collects GPU HW metrics
} pti_thread_purpose;

typedef struct _pti_apicall_callback_data {
  pti_callback_domain  _domain;        // domain of the callback
  pti_callback_site    _site;          // ENTER or EXIT
  uint32_t             _thread_id;     // thread ID
  uint32_t             _driver_api_id;
  pti_backend_ctx_t    _backend_context_handle; // Driver (L0) level context handle
  const char*          _kernel_name;   // valid only for DRIVER APIs related to GPU kernel submission
  uint32_t             _return_code;   // will be valid only for L0 API EXIT, for others will be nullptr
  uint32_t             _correlation_id; // ID that corresponds to the same call reported by View API records
  uint64_t*            _local_data;    // user data passed between ENTER and EXIT
} pti_apicall_callback_data;

typedef struct _pti_internal_callback_data {
  pti_callback_domain  _domain;        // domain of the callback
  pti_callback_site    _site;          // THREAD START/END or CRITICAL EVENT
  uint32_t             _purpose;       // depending on the domain should be casted and interpreted
                                       // as a purpose of the internal PTI thread or
                                       // pti_result for the critical event
  uint32_t             _thread_id;     // thread ID
} pti_internal_callback_data;

typedef void (*pti_callback_function)(
  pti_callback_domain  domain,
  void*                cb_data, // depending on the domain it should be type-casted to the pointer
                               // to either pti_apicall_callback_data or pti_internal_callback_data
  void*                user_data);

typedef void* pti_callback_subscriber;   //!< Subscriber handle

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
 * Usage: 1 - pass external_kind and external_id as nullptr to get number of IDs availible
 *            on the top of stack at the Callback point
 *        2 - call this API again where number is number of IDs to return,
 *            external_kind and external_id should have enough memory allocated to hold number of IDs
 *            that will be copied to that memory
 *
 * @param[in,out] number    - number of the external correlation IDs
 * @param[in] external_kind - kind of the external correlation ID
 * @param[in] external_id   - external correlation ID
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiCallbackGetExternalCorrelationIds(uint32_t number,
                                    pti_view_external_kind* external_kind,
                                    uint64_t* external_id);
#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_CALLBACK_H_