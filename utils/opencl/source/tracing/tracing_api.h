/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*
 * Copied from commit ca7d47598a63959d42750c62a2981e08ffa392a1
 * of https://github.com/intel/compute-runtime.git.
 */

#pragma once

#include "opencl/source/tracing/tracing_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
    Function creates a tracing handle object
    \param[in] device Device to create tracing handle for
    \param[in] callback User-defined callback that will be called along with
                        traced API function
    \param[in] userData Pointer to any data user would like to pass into the
                        callback, can be zero
    \param[out] handle Tracing handle object that describes current tracing
                       session
    \return Status code for current operation

    Thread Safety: yes
*/
cl_int CL_API_CALL clCreateTracingHandleINTEL(cl_device_id device, cl_tracing_callback callback, void *userData, cl_tracing_handle *handle);

/*!
    Function allows to specify which target API call should be traced.
    By default function will NOT be traced
    \param[in] handle Tracing handle object
    \param[in] fid Target function identifier
    \param[in] enable Flag to enable/disable tracing for target function
    \return Status code for current operation

    Thread Safety: no
*/
cl_int CL_API_CALL clSetTracingPointINTEL(cl_tracing_handle handle, cl_function_id fid, cl_bool enable);

/*!
    Function destroys the tracing handle object and releases all the associated
    resources
    \param[in] handle Tracing handle object
    \return Status code for current operation

    Thread Safety: no
*/
cl_int CL_API_CALL clDestroyTracingHandleINTEL(cl_tracing_handle handle);

/*!
    Function enables the tracing process for the handle. Multiple handles
    can be enabled at a time
    \param[in] handle Tracing handle object
    \return Status code for current operation

    Thread Safety: yes
*/
cl_int CL_API_CALL clEnableTracingINTEL(cl_tracing_handle handle);

/*!
    Function disables the tracing process for the handle. It will wait until
    all currently running callbacks are done
    \param[in] handle Tracing handle object
    \return Status code for current operation

    Thread Safety: yes
*/
cl_int CL_API_CALL clDisableTracingINTEL(cl_tracing_handle handle);

/*!
    Function requests the tracing state for the handle
    \param[in] handle Tracing handle object
    \param[out] enable Returns TRUE if tracing handle is in use and
                       FALSE otherwise
    \return Status code for current operation

    Thread Safety: yes
*/
cl_int CL_API_CALL clGetTracingStateINTEL(cl_tracing_handle handle, cl_bool *enable);

#ifdef __cplusplus
}
#endif
