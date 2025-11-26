//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "client.h"

#include <level_zero/ze_api.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sycl/sycl.hpp>

#include "pti/pti_callback.h"
#include "samples_utils.h"

/**
 * This file demonstrate usage of PTI Callback Subscriber and
 * External Correlation called from within Append callbacks
 * The sample workload uses a single thread.
 */

namespace {

pti_callback_subscriber_handle subscriber = nullptr;

std::atomic<uint64_t> external_correlation_id = 0;

}  // namespace

//
// Forward declaration of tool functions required for PTI initialization
//
void CallbackCommon(pti_callback_domain domain, pti_api_group_id driver_group_id,
                    uint32_t driver_api_id, pti_backend_ctx_t backend_context, void *cb_data,
                    void *user_data, void **instance_user_data);

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size);

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size);

//
// Start and Stop profiling
//
void StartProfiling() {
  // At the moment when subscribe for ptiCallback-s -
  // need to enable at least one ptiView for GPU operations
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));

  // Demonstrating here how to use External Correlation in Subscriber Callbacks
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DRIVER_API));
  PTI_CHECK_SUCCESS(ptiViewEnableDriverApiClass(1, pti_api_class::PTI_API_CLASS_GPU_OPERATION_CORE,
                                                pti_api_group_id::PTI_API_GROUP_LEVELZERO));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION));

  // Initializing Subscriber and setting the callback function
  // As user data we pass to subscriber its own address
  PTI_CHECK_SUCCESS(ptiCallbackSubscribe(&subscriber, CallbackCommon, &subscriber));
  std::cout << "Initialized Subscriber: " << subscriber << std::endl;

  // Enabling for each subscriber domains of interest
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1));
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1));
}

void StopProfiling() {
  PTI_CHECK_SUCCESS(ptiCallbackUnsubscribe(subscriber));

  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DRIVER_API));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION));

  PTI_CHECK_SUCCESS(ptiFlushAllViews());
}

//
// Functions used in Callbacks
//
void CallbackGPUOperationAppend([[maybe_unused]] pti_callback_domain domain,
                                pti_api_group_id driver_group_id, uint32_t driver_api_id,
                                [[maybe_unused]] pti_backend_ctx_t backend_context, void *cb_data,
                                void *user_data, void **instance_user_data) {
  std::cout << "In " << __func__
            << " Subscriber: " << *(reinterpret_cast<pti_callback_subscriber_handle *>(user_data))
            << std::endl;

  samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                  user_data, instance_user_data);

  if (cb_data == nullptr) {
    std::cerr << "CallbackGPUOperationAppend: callback_data is null. Unexpected" << std::endl;
    return;
  }
  pti_callback_gpu_op_data *gpu_op_data = static_cast<pti_callback_gpu_op_data *>(cb_data);
  if (gpu_op_data->_operation_details == nullptr) {
    std::cerr << "CallbackGPUOperationAppend: pti_gpu_op_details is null. Unexpected" << std::endl;
    return;
  }

  if (gpu_op_data->_operation_count != 1) {
    std::cout << "WARNING: Operation count is not 1, it is: " << gpu_op_data->_operation_count
              << " . Unexpected for this sample! Will not proceed with Push/Pop "
              << "of External Correlation " << std::endl;
    return;
  }

  if (gpu_op_data->_phase == PTI_CB_PHASE_API_ENTER) {
    *instance_user_data = static_cast<void *>(nullptr);
    external_correlation_id.fetch_add(1);
    auto result = ptiViewPushExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, external_correlation_id);
    std::cout << "Pushing External Correlation Id: " << external_correlation_id
              << ", Result: " << result << std::endl;
  } else if (gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT) {
    uint64_t local_external_correlation_id = 0ULL;
    auto result = ptiViewPopExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &local_external_correlation_id);
    std::cout << "Popped External Correlation Id: " << local_external_correlation_id
              << ", Result: " << result << std::endl;
  } else {
    std::cout << "Unexpected phase: " << gpu_op_data->_phase << std::endl;
  }
}

void CallbackGPUOperationCompletion([[maybe_unused]] pti_callback_domain domain,
                                    pti_api_group_id driver_group_id, uint32_t driver_api_id,
                                    [[maybe_unused]] pti_backend_ctx_t backend_context,
                                    void *cb_data, void *user_data,
                                    [[maybe_unused]] void **instance_user_data) {
  std::cout << "In " << __func__
            << " Subscriber: " << *(reinterpret_cast<pti_callback_subscriber_handle *>(user_data))
            << std::endl;

  samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                  user_data, instance_user_data);
}

void CallbackCommon(pti_callback_domain domain, pti_api_group_id driver_group_id,
                    uint32_t driver_api_id, pti_backend_ctx_t backend_context, void *cb_data,
                    void *user_data, void **instance_user_data) {
  switch (domain) {
    case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED:
      CallbackGPUOperationAppend(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                 user_data, instance_user_data);
      break;
    case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED:
      CallbackGPUOperationCompletion(domain, driver_group_id, driver_api_id, backend_context,
                                     cb_data, user_data, instance_user_data);
      break;
    default: {
      std::cout << "In " << __func__ << " (default case)" << std::endl;
      samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context,
                                      cb_data, user_data, instance_user_data);
      break;
    }
  }
  std::cout << std::endl;
}

//
// PTI Reports Buffer functions
//
void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
  constexpr auto kRequestedRecordCount = 5'000'000ULL;
  constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

  *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = kRequestedBufferSize;
}

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (valid_buf_size) {
      samples_utils::AlignedDealloc(buf);
    }
    return;
  }
  pti_view_record_base *ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      std::cout << "Reached End of buffer" << '\n';
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        auto *rec = reinterpret_cast<pti_view_record_kernel *>(ptr);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        std::cout << "Found Kernel Record" << '\n';
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        auto *rec = reinterpret_cast<pti_view_record_memory_copy *>(ptr);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        std::cout << "Found Memory Copy Record" << '\n';
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        auto *rec = reinterpret_cast<pti_view_record_memory_fill *>(ptr);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        std::cout << "Found Memory Fill Record" << '\n';
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        break;
      }
      case pti_view_kind::PTI_VIEW_DRIVER_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        std::cout << "Found Driver API Record" << '\n';
        auto *rec = reinterpret_cast<pti_view_record_api *>(ptr);
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        std::cout << "Found External Correlation Record" << '\n';
        auto *rec = reinterpret_cast<pti_view_record_external_correlation *>(ptr);
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------\n";
        break;
      }
      default: {
        std::cerr << "We don't expect this kind of record in this sample. Kind: "
                  << static_cast<int>(ptr->_view_kind) << '\n';
        break;
      }
    }
  }
  samples_utils::AlignedDealloc(buf);
}
