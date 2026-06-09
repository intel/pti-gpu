//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef CLIENT_H_
#define CLIENT_H_

#include <vector>

#include "pti/pti.h"

// Start metrics scope collection.
// Empty `device_handles` enables auto-detect mode (all uniform GPUs are profiled).
// Non-empty `device_handles` profiles only the listed devices in explicit mode.
void StartProfiling(const std::vector<pti_device_handle_t>& device_handles = {});

void StopProfiling();

#endif  // CLIENT_H_
