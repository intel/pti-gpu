//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef __ITT_ADAPTER_H_
#define __ITT_ADAPTER_H_

#include <ittnotify.h>
#include <ittnotify_config.h>

__itt_global* GetIttGlobalOfCclDomainAdapter();
const __itt_domain* GetIttCclDomainAdapter();

#endif  // __ITT_ADAPTER_H_
