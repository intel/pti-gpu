//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "client.h"

#include <level_zero/ze_api.h>

#include <cstdlib>
#include <iostream>
#include <sycl/sycl.hpp>

#include "pti/pti_callback.h"
#include "samples_utils.h"

/**
 * This file implements a sample collector tool that:
 * - Has two subscribers to PTI Callback Domains related to appending and dispatching GPU
 * operations.
 *
 * - The subscribers trace the workload and report its progress from the callbacks.
 *
 * - Of the two subscribers, the second one subscribes only to operation completion.
 *   This demonstrates that multiple subscribers can co-exist.
 *
 * - The first subscriber implements measurement of the first GPU kernel duration in GPU cycles.
 *   It does this by appending commands to write global timestamps before and after the kernel,
 *   and reading these timestamps upon kernel completion.
 *
 * For simplicity, there is no code for multi-thread synchronization, as well as other tool
 * aspects are ommitted. The sample workload uses a single thread.
 * If it is modified to run multiple threads, the synchronization is the first thing
 * that should be added to the tool.
 */

/// Tool resources

namespace {

pti_callback_subscriber_handle subscriber1 = nullptr;
pti_callback_subscriber_handle subscriber2 = nullptr;

ze_event_handle_t global_time_stamp_start_event = nullptr;
ze_event_handle_t global_time_stamp_end_event = nullptr;
void *buff_start = nullptr;
void *buff_end = nullptr;

uint64_t g_profiled_kernel_id = 0;

}  // namespace

/// Forward declaration of tool functions required for PTI initialization

void CallbackCommon(pti_callback_domain domain, pti_api_group_id driver_group_id,
                    uint32_t driver_api_id, pti_backend_ctx_t backend_context, void *cb_data,
                    void *user_data, void **instance_user_data);

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size);

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size);

/// Start and Stop profiling

void StartProfiling() {
  // At the moment when subscribe for ptiCallback-s - enable at least one ptiView
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));

  // Initializing two pti Subscribers and setting for both of them the same callback function
  // Note, that as user data we pass to each subscriber its own address
  PTI_CHECK_SUCCESS(ptiCallbackSubscribe(&subscriber1, CallbackCommon, &subscriber1));
  std::cout << "Initialized Subscriber: " << subscriber1 << std::endl;
  PTI_CHECK_SUCCESS(ptiCallbackSubscribe(&subscriber2, CallbackCommon, &subscriber2));
  std::cout << "Initialized Subscriber: " << subscriber2 << std::endl;

  // Enabling for each subscriber the domains of interest
  // Subscriber1 will get notifications about GPU Operation Appended and Completed
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber1, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1));
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber1, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1));
  // Subscriber2 will get notifications only GPU Operation Completed only
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber2, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1));
}

void StopProfiling() {
  PTI_CHECK_SUCCESS(ptiCallbackUnsubscribe(subscriber1));
  PTI_CHECK_SUCCESS(ptiCallbackUnsubscribe(subscriber2));

  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));

  PTI_CHECK_SUCCESS(ptiFlushAllViews());
}

/// Tool working functions

bool IsToolResourcesInitialized() {
  return global_time_stamp_start_event != nullptr && global_time_stamp_end_event != nullptr &&
         buff_start != nullptr && buff_end != nullptr;
}

/**
 * @brief Initialization is lazy and happens only once
 */
bool InitToolResources(ze_context_handle_t context, ze_device_handle_t device) {
  static bool ready = false;
  ze_event_pool_handle_t event_pool = nullptr;

  if (!ready) {
    // Pool with 2 events - to be appended them (only) at the first kernel append callback pair
    ze_event_pool_desc_t event_pool_desc = {.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
                                            .pNext = nullptr,
                                            .flags = ZE_EVENT_POOL_FLAG_IPC |
                                                     ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP |
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                            .count = 2};

    auto status = zeEventPoolCreate(context, &event_pool_desc, 1, &device, &event_pool);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeEventPoolCreate failed with error code: " << status << '\n';
      return false;
    }

    // Memory buffers where to write timestamps
    ze_device_mem_alloc_desc_t device_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0,
                                              0};
    ze_host_mem_alloc_desc_t host_desc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};

    status = zeMemAllocShared(context, &device_desc, &host_desc, 8, 64, device, &buff_start);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeMemAllocShared failed with error code: " << status << '\n';
      return false;
    }
    status = zeMemAllocShared(context, &device_desc, &host_desc, 8, 64, device, &buff_end);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeMemAllocShared failed with error code: " << status << '\n';
      return false;
    }

    // Create two events from the pool. They would signal that timestamps written
    ze_event_desc_t event_desc = {.stype = ZE_STRUCTURE_TYPE_EVENT_DESC,
                                  .pNext = nullptr,
                                  .index = 0,
                                  .signal = ZE_EVENT_SCOPE_FLAG_HOST,  // Event is signaled on host
                                  .wait = ZE_EVENT_SCOPE_FLAG_HOST};   // Event is waited on host
    status = zeEventCreate(event_pool, &event_desc, &global_time_stamp_start_event);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeEventCreate failed with error code: " << status << '\n';
      return false;
    }
    event_desc.index = 1;
    status = zeEventCreate(event_pool, &event_desc, &global_time_stamp_end_event);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeEventCreate failed with error code: " << status << '\n';
      return false;
    }
    ready = true;
    return true;
  }
  return false;
}

void GPUKernelAppendOnEnter(ze_context_handle_t context, ze_device_handle_t device,
                            ze_command_list_handle_t command_list, bool &write_appended) {
  write_appended = false;
  std::cout << "Initialize tool resources to Write Global timestamps around the kernel"
            << std::endl;
  // Lazy initialization, returns true only once
  auto res = InitToolResources(context, device);
  if (res) {
    std::cout << " -----> Appending Write Global timestamp before the kernel" << std::endl;
    res = zeCommandListAppendWriteGlobalTimestamp(command_list, static_cast<uint64_t *>(buff_start),
                                                  global_time_stamp_start_event, 0, nullptr);

    if (res == ZE_RESULT_SUCCESS) {
      std::cout << "        Appended Write Global timestamp" << std::endl;
      write_appended = true;
    } else {
      std::cout << "zeCommandListAppendWriteGlobalTimestamp failed with error code: " << res
                << std::endl;
    }
  } else {
    std::cout << "Data not (re-)initialized" << std::endl;
  }
}

void GPUKernelAppendOnExit(ze_command_list_handle_t command_list, bool &write_appended) {
  write_appended = false;
  if (!IsToolResourcesInitialized()) {
    std::cout << "Tool resources not initialized. Cannot append write timestamp after the kernel"
              << std::endl;
    return;
  }
  std::cout << " <----- Appending Write Global timestamp after the kernel" << std::endl;
  auto res = zeCommandListAppendWriteGlobalTimestamp(
      command_list, static_cast<uint64_t *>(buff_end), global_time_stamp_end_event, 0, nullptr);
  if (res == ZE_RESULT_SUCCESS) {
    std::cout << "        Appended Write Global timestamp" << std::endl;
    write_appended = true;
  } else {
    std::cout << "zeCommandListAppendWriteGlobalTimestamp failed with error code: " << res
              << std::endl;
  }
}

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
              << " . Unexpected for this sample! Will not proceed with appending "
              << "Global Timestamp write" << std::endl;
    return;
  }

  pti_gpu_op_details *gpu_op_details =
      static_cast<pti_gpu_op_details *>(gpu_op_data->_operation_details);

  bool is_op_kernel = (gpu_op_details->_operation_kind == PTI_GPU_OPERATION_KIND_KERNEL);

  if (gpu_op_data->_phase == PTI_CB_PHASE_API_ENTER) {
    *instance_user_data = static_cast<void *>(nullptr);
    if (is_op_kernel) {
      bool write_appended = false;
      GPUKernelAppendOnEnter(static_cast<ze_context_handle_t>(backend_context),
                             static_cast<ze_device_handle_t>(gpu_op_data->_device_handle),
                             static_cast<ze_command_list_handle_t>(gpu_op_data->_cmd_list_handle),
                             write_appended);
      if (write_appended) {
        // remember operation_id which duration is being profiled
        g_profiled_kernel_id = gpu_op_details->_operation_id;
        // Store End event - to indicate for the pair EXIT Callback that
        // it need to append another Write Global Timestamp
        *instance_user_data = static_cast<void *>(&global_time_stamp_end_event);
      }
    }
  } else if (gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT) {
    if (is_op_kernel && (*instance_user_data != nullptr)) {
      bool write_appended = false;
      GPUKernelAppendOnExit(static_cast<ze_command_list_handle_t>(gpu_op_data->_cmd_list_handle),
                            write_appended);
    }
  } else {
    std::cout << "Unexpected phase: " << gpu_op_data->_phase << std::endl;
  }
}

uint64_t ReadTimestamp(void *buff) {
  // Copy buffer from device to host
  uint64_t timestamp = *static_cast<uint64_t *>(buff);
  return timestamp;
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

  if (IsToolResourcesInitialized()) {
    // Checking the events status first - prior reading timestamps
    auto result1 = zeEventQueryStatus(global_time_stamp_start_event);
    auto result2 = zeEventQueryStatus(global_time_stamp_end_event);
    if (result1 == ZE_RESULT_SUCCESS && result2 == ZE_RESULT_SUCCESS) {
      std::cout << "Writes of Global Time Stamp signaled." << std::endl;
      // Reading timestamps
      auto start_time_stamp = ReadTimestamp(buff_start);
      auto end_time_stamp = ReadTimestamp(buff_end);
      std::cout << "Kernel with _kernel_id: " << g_profiled_kernel_id
                << ", start TS: " << start_time_stamp << ", end TS: " << end_time_stamp
                << ", duration: " << (end_time_stamp - start_time_stamp) << " cycles \n";

      // Resetting the events and will not use them anymore
      result1 = zeEventHostReset(global_time_stamp_start_event);
      if (result1 != ZE_RESULT_SUCCESS) {
        std::cout << "zeEventHostReset for Start event failed with error code: " << result1
                  << std::endl;
      }
      result2 = zeEventHostReset(global_time_stamp_end_event);
      if (result2 != ZE_RESULT_SUCCESS) {
        std::cout << "zeEventHostReset for End event failed with error code: " << result2
                  << std::endl;
      }

    } else if (result1 == ZE_RESULT_SUCCESS && result2 == ZE_RESULT_NOT_READY) {
      std::cout << "Global Timestamp End event is NOT READY.";
    } else if (result1 == ZE_RESULT_NOT_READY && result2 == ZE_RESULT_NOT_READY) {
      std::cout << "Global Timestamp Start and End event are NOT READY."
                   "It could be that they already processed and reset. "
                << std::endl;
    } else {
      std::cout << "zeEventQueryStatus for Start event failed with error code: " << result1
                << std::endl;
      std::cout << "zeEventQueryStatus for End event failed with error code: " << result2
                << std::endl;
    }
  }
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
        pti_view_record_kernel *rec = reinterpret_cast<pti_view_record_kernel *>(ptr);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Kernel Record" << '\n';
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
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
