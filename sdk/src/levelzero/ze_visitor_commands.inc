//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// API(s) supported by the visitor extension. These are Append calls required
// to generate records while rebuilding an instrumented version of the user's
// command list. While they map closely to core API(s) and other lists in PTI,
// they not exactly 1:1. E.g., EventReset is needed due to bug in some versions
// of the driver, etc...
// APPEND_COMMAND(name, major, minor): major.minor is the lowest driver-reported
// spec version or the actual spec version that supports the command.

APPEND_COMMAND(LaunchKernel, 1, 0)
APPEND_COMMAND(LaunchKernelWithArguments, 1, 14)
APPEND_COMMAND(LaunchKernelWithParameters, 1, 14)
APPEND_COMMAND(LaunchCooperativeKernel, 1, 0)
APPEND_COMMAND(LaunchKernelIndirect, 1, 0)
APPEND_COMMAND(MemoryCopy, 1, 0)
APPEND_COMMAND(MemoryFill, 1, 0)
APPEND_COMMAND(MemoryCopyWithParameters, 1, 15)
APPEND_COMMAND(MemoryFillWithParameters, 1, 15)
APPEND_COMMAND(MemoryCopyRegion, 1, 0)
APPEND_COMMAND(MemoryCopyFromContext, 1, 0)
APPEND_COMMAND(ImageCopy, 1, 0)
APPEND_COMMAND(ImageCopyRegion, 1, 0)
APPEND_COMMAND(ImageCopyToMemory, 1, 0)
APPEND_COMMAND(ImageCopyFromMemory, 1, 0)
APPEND_COMMAND(ImageCopyToMemoryExt, 1, 3)
APPEND_COMMAND(ImageCopyFromMemoryExt, 1, 3)
APPEND_COMMAND(EventReset, 1, 0)
APPEND_COMMAND(Barrier, 1, 0)
APPEND_COMMAND(MemoryRangesBarrier, 1, 0)
