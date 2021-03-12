//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "ze_correlator.h"

thread_local uint64_t ZeCorrelator::kernel_id_ = 0;