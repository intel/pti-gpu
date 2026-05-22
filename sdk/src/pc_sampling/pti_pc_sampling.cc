//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti/pti_pc_sampling.h"

pti_result ptiPcSamplingEnable(pti_pc_sampling_handle_t*) { return PTI_ERROR_NOT_IMPLEMENTED; }

pti_result ptiPcSamplingConfigure(pti_pc_sampling_handle_t, const pti_device_handle_t*, size_t,
                                  uint32_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingQueryCollectionBufferSize(pti_pc_sampling_handle_t, size_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingSetCollectionBufferSize(pti_pc_sampling_handle_t, size_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingStartCollection(pti_pc_sampling_handle_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingStopCollection(pti_pc_sampling_handle_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetStallReasons(pti_pc_sampling_handle_t,
                                        pti_pc_sampling_stall_reason_info_t*, size_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetProfiledDevices(pti_pc_sampling_handle_t, pti_device_handle_t*,
                                           size_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetObservedKernelHandles(pti_pc_sampling_handle_t, pti_device_handle_t,
                                                 uint64_t*, size_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetObservedKernelInfo(pti_pc_sampling_handle_t, pti_device_handle_t,
                                              uint64_t, pti_pc_sampling_kernel_info_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetSamplesPerInstruction(pti_pc_sampling_handle_t, pti_device_handle_t,
                                                 uint64_t, pti_pc_sampling_instruction_t*, size_t,
                                                 uint64_t*, size_t) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingGetDeviceStatus(pti_pc_sampling_handle_t, pti_device_handle_t,
                                        pti_pc_sampling_device_status_t*) {
  return PTI_ERROR_NOT_IMPLEMENTED;
}

pti_result ptiPcSamplingDisable(pti_pc_sampling_handle_t) { return PTI_ERROR_NOT_IMPLEMENTED; }
