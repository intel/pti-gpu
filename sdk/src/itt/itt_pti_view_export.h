//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @internal
 * @file itt_pti_view_export.h
 * @brief ITT-related function declarations exposed from pti_view library
 *
 * This file declares the only ITT-related functions that are exposed from
 * the pti_view library for internal use between pti_view and pti libraries.
 */

#ifndef SRC_ITT_PTI_VIEW_EXPORT_H_
#define SRC_ITT_PTI_VIEW_EXPORT_H_

#if defined(PTI_CCL_ITT_COMPILE)

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE

#include <ittnotify.h>
#include <ittnotify_config.h>

/**
 * @internal
 * @brief ITT API initialization function exported from pti_view library
 *
 * This function is the only ITT API function exported from pti_view.
 * It is called by the ITT framework to initialize the ITT adapter.
 *
 * @param p Pointer to ITT global structure
 * @param init_groups ITT groups to initialize
 */
ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* p, __itt_group_id init_groups);

/**
 * @internal
 * @brief Retrieve cached ITT initialization parameters
 *
 * Implemented in itt_adapter.cc to retrieve cached ITT init parameters
 * for internal communication between pti_view and pti libraries.
 *
 * @param out_global_ptr Output pointer to receive cached __itt_global pointer
 * @param out_init_groups Output pointer to receive cached init groups
 */
void PtiGetCachedIttInitParams(__itt_global** out_global_ptr, __itt_group_id* out_init_groups);

#endif  // PTI_CCL_ITT_COMPILE
#endif  // SRC_ITT_PTI_VIEW_EXPORT_H_
