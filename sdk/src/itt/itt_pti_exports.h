//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef SRC_ITT_PTI_EXPORTS_H_
#define SRC_ITT_PTI_EXPORTS_H_

#include <ittnotify.h>
#include <ittnotify_config.h>

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* p, __itt_group_id init_groups);

#endif  // SRC_ITT_PTI_EXPORTS_H_
