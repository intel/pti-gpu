//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "overhead_kinds.h"

#ifdef PTI_OVERHEAD_TRACKING_ENABLED

namespace overhead {

// Single definition of overhead_collection_enabled - all compilation units share this one instance
std::atomic<bool> overhead_collection_enabled{false};

// Single definition of overhead callback
OnZeOverheadFinishCallback ocallback_{nullptr};

}  // namespace overhead

#endif  // PTI_OVERHEAD_TRACKING_ENABLED
