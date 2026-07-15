//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// API(s) supported by the visitor extension. These are Append calls required
// to generate records while rebuilding an instrumented version of the user's
// command list. While they map closely to core API(s) and other lists in PTI,
// they not exactly 1:1. E.g., EventReset is needed due to bug in some versions
// of the driver, etc... The comments are the spec version where the API was
// introduced.
APPEND_COMMAND(LaunchKernel)                // L0 spec 1.0
APPEND_COMMAND(LaunchKernelWithArguments)   // L0 spec 1.14
APPEND_COMMAND(LaunchKernelWithParameters)  // L0 spec 1.14
APPEND_COMMAND(LaunchCooperativeKernel)     // L0 spec 1.0
APPEND_COMMAND(LaunchKernelIndirect)        // L0 spec 1.0
APPEND_COMMAND(MemoryCopy)                  // L0 spec 1.0
APPEND_COMMAND(MemoryFill)                  // L0 spec 1.0
APPEND_COMMAND(MemoryCopyRegion)            // L0 spec 1.0
APPEND_COMMAND(MemoryCopyFromContext)       // L0 spec 1.0
APPEND_COMMAND(ImageCopy)                   // L0 spec 1.0
APPEND_COMMAND(ImageCopyRegion)             // L0 spec 1.0
APPEND_COMMAND(ImageCopyToMemory)           // L0 spec 1.0
APPEND_COMMAND(ImageCopyFromMemory)         // L0 spec 1.0
APPEND_COMMAND(ImageCopyToMemoryExt)        // L0 spec 1.3
APPEND_COMMAND(ImageCopyFromMemoryExt)      // L0 spec 1.3
APPEND_COMMAND(EventReset)                  // L0 spec 1.0
APPEND_COMMAND(Barrier)                     // L0 spec 1.0
APPEND_COMMAND(MemoryRangesBarrier)         // L0 spec 1.0

