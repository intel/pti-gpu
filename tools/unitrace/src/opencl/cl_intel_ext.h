//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_CL_INTEL_EXT_H_
#define PTI_TOOLS_UNITRACE_CL_INTEL_EXT_H_

enum cl_ext_api_id {
  ClExtApiStart = 4321,
  ClExtHostMemAllocINTEL = ClExtApiStart,
  ClExtDeviceMemAllocINTEL,
  ClExtSharedMemAllocINTEL,
  ClExtMemFreeINTEL,
  ClExtGetMemAllocInfoINTEL,
  ClExtSetKernelArgMemPointerINTEL,
  ClExtEnqueueMemcpyINTEL,
  ClExtGetDeviceGlobalVariablePointerINTEL,
  ClExtGetKernelSuggestedLocalWorkSizeINTEL,
  ClExtCreateBufferWithPropertiesINTEL,
  ClExtEnqueueMemsetINTEL,
  ClExtEnqueueMigrateMemINTEL,
  ClExtEnqueueMemAdviseINTEL,
  ClExtEnqueueMemFillINTEL,
  ClExtMemBlockingFreeINTEL,
  ClExtApiEnd 
};

static const char *cl_ext_api[] = {
  "clHostMemAllocINTEL",
  "clDeviceMemAllocINTEL",
  "clSharedMemAllocINTEL",
  "clMemFreeINTEL",
  "clGetMemAllocInfoINTEL",
  "clSetKernelArgMemPointerINTEL",
  "clEnqueueMemcpyINTEL",
  "clGetDeviceGlobalVariablePointerINTEL",
  "clGetKernelSuggestedLocalWorkSizeINTEL",
  "clCreateBufferWithPropertiesINTEL",
  "clEnqueueMemsetINTEL",
  "clEnqueueMigrateMemINTEL",
  "clEnqueueMemAdviseINTEL",
  "clEnqueueMemFillINTEL",
  "clMemBlockingFreeINTEL"
};

#endif /* PTI_TOOLS_UNITRACE_CL_INTEL_EXT_H_ */
