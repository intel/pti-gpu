//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SAMPLES_C_UTILS_H_
#define SAMPLES_C_UTILS_H_
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pti/pti_callback.h"
#include "pti/pti_view.h"

#define PTI_CHECK_SUCCESS(X)                                                                \
  do {                                                                                      \
    if (X != PTI_SUCCESS) {                                                                 \
      fprintf(stderr, "PTI CALL FAILED: " #X " WITH ERROR %s\n", ptiResultTypeToString(X)); \
      exit(EXIT_FAILURE);                                                                   \
    }                                                                                       \
  } while (0)

void PrintUuidC(uint8_t* uuid, const char* prefix) {
  printf("%s", prefix);
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");
}

void DumpRecordKernel(pti_view_record_kernel* record, bool with_sycl_rec) {
  if (NULL == record) return;

  printf("Kernel Name: %s\n", record->_name);
  printf("               Ze Kernel Append Time: %" PRIu64 " ns\n", record->_append_timestamp);
  printf("               Ze Kernel Submit Time: %" PRIu64 " ns\n", record->_submit_timestamp);
  printf("                Ze Kernel Start Time: %" PRIu64 " ns\n", record->_start_timestamp);
  printf("                  Ze Kernel End Time: %" PRIu64 " ns\n", record->_end_timestamp);
  printf("Kernel Queue Handle: %p\n", record->_queue_handle);
  printf("Kernel Queue ID: %" PRIu64 "\n", record->_sycl_queue_id);
  printf("Kernel CommandList Context Handle: %p\n", record->_context_handle);
  printf("Kernel Id: %" PRIu64 "\n", record->_kernel_id);
  printf("Correlation Id: %u\n", record->_correlation_id);
  printf("Kernel Thread Id: %u\n", record->_thread_id);
  if (with_sycl_rec) {
    printf("         Sycl Kernel Task Begin Time: %" PRIu64 " ns\n",
           record->_sycl_task_begin_timestamp);
    printf("Sycl Kernel EnqueueKernel Begin Time: %" PRIu64 " ns\n",
           record->_sycl_enqk_begin_timestamp);
  }
  printf("Kernel Execution Time: %" PRIu64 " ns\n",
         record->_end_timestamp - record->_start_timestamp);
  printf("Kernel File Name: %s:%" PRIu64 "\n", record->_source_file_name,
         record->_source_line_number);
  printf("Kernel Device: %s\n", record->_pci_address);

  // Print UUID
  printf("Kernel Device UUID: ");
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", record->_device_uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");

  if (with_sycl_rec) {
    printf("Kernel NodeID:InvocationID %" PRIu64 ":%u\n", record->_sycl_node_id,
           record->_sycl_invocation_id);
  }
}

void DumpRecordMemoryCopy(pti_view_record_memory_copy* record) {
  if (NULL == record) return;

  printf("Memory Op: %s\n", record->_name);
  printf("Memory Device: %s\n", record->_pci_address);

  // Print UUID
  printf("Memory Device UUID: ");
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", record->_device_uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");

  printf("Memory Op Execution Time: %" PRIu64 " ns\n",
         record->_end_timestamp - record->_start_timestamp);
  printf("               Memory Op Append Time: %" PRIu64 " ns\n", record->_append_timestamp);
  printf("               Memory Op Submit Time: %" PRIu64 " ns\n", record->_submit_timestamp);
  printf("                Memory Op Start Time: %" PRIu64 " ns\n", record->_start_timestamp);
  printf("                  Memory Op End Time: %" PRIu64 " ns\n", record->_end_timestamp);
  printf("Memory Op Queue Handle: %p\n", record->_queue_handle);
  printf("Memory Op Queue ID: %" PRIu64 "\n", record->_sycl_queue_id);
  printf("Memory Op CommandList Context Handle: %p\n", record->_context_handle);
  printf("Memory Op Id: %" PRIu64 "\n", record->_mem_op_id);
  printf("Memory Bytes Copied: %" PRIu64 "\n", record->_bytes);
  printf("Memory Op Thread Id: %u\n", record->_thread_id);
  printf("Correlation Id: %u\n", record->_correlation_id);
  printf("Memory Copy Type: %s\n", ptiViewMemcpyTypeToString(record->_memcpy_type));
  printf("Memory Copy Source: %s\n", ptiViewMemoryTypeToString(record->_mem_src));
  printf("Memory Copy Destination: %s\n", ptiViewMemoryTypeToString(record->_mem_dst));
}

void DumpRecordMemoryCopyP2p(pti_view_record_memory_copy_p2p* record) {
  if (NULL == record) return;

  printf("Memory Op: %s\n", record->_name);
  printf("Memory Source Device: %s\n", record->_src_pci_address);
  printf("Memory Destination Device: %s\n", record->_dst_pci_address);

  // Print source UUID
  printf("Memory Source Device UUID: ");
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", record->_src_uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");

  // Print destination UUID
  printf("Memory Destination Device UUID: ");
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", record->_dst_uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");

  printf("Memory Op Execution Time: %" PRIu64 " ns\n",
         record->_end_timestamp - record->_start_timestamp);
  printf("               Memory Op Append Time: %" PRIu64 " ns\n", record->_append_timestamp);
  printf("               Memory Op Submit Time: %" PRIu64 " ns\n", record->_submit_timestamp);
  printf("                Memory Op Start Time: %" PRIu64 " ns\n", record->_start_timestamp);
  printf("                  Memory Op End Time: %" PRIu64 " ns\n", record->_end_timestamp);
  printf("Memory Op Queue Handle: %p\n", record->_queue_handle);
  printf("Memory Op Queue ID: %" PRIu64 "\n", record->_sycl_queue_id);
  printf("Memory Op CommandList Context Handle: %p\n", record->_context_handle);
  printf("Memory Op Id: %" PRIu64 "\n", record->_mem_op_id);
  printf("Memory Bytes Copied: %" PRIu64 "\n", record->_bytes);
  printf("Memory Op Thread Id: %u\n", record->_thread_id);
  printf("Correlation Id: %u\n", record->_correlation_id);
  printf("Memory Copy Type: %s\n", ptiViewMemcpyTypeToString(record->_memcpy_type));
  printf("Memory Copy Source: %s\n", ptiViewMemoryTypeToString(record->_mem_src));
  printf("Memory Copy Destination: %s\n", ptiViewMemoryTypeToString(record->_mem_dst));
}

void DumpRecordMemoryFill(pti_view_record_memory_fill* record) {
  if (NULL == record) return;

  printf("Memory Op: %s\n", record->_name);
  printf("Memory Device: %s\n", record->_pci_address);

  // Print UUID
  printf("Memory Device UUID: ");
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    printf("%02x", record->_device_uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) printf("-");
  }
  printf("\n");

  printf("Memory Op Execution Time: %" PRIu64 " ns\n",
         record->_end_timestamp - record->_start_timestamp);
  printf("               Memory Op Append Time: %" PRIu64 " ns\n", record->_append_timestamp);
  printf("               Memory Op Submit Time: %" PRIu64 " ns\n", record->_submit_timestamp);
  printf("               Memory Op Start Time: %" PRIu64 " ns\n", record->_start_timestamp);
  printf("                  Memory Op End Time: %" PRIu64 " ns\n", record->_end_timestamp);
  printf("Memory Op Queue Handle: %p\n", record->_queue_handle);
  printf("Memory Op Queue ID: %" PRIu64 "\n", record->_sycl_queue_id);
  printf("Memory Op CommandList Context Handle: %p\n", record->_context_handle);
  printf("Memory Op Id: %" PRIu64 "\n", record->_mem_op_id);
  printf("Memory Op Thread Id: %u\n", record->_thread_id);
  printf("Memory Bytes Transfered: %" PRIu64 "\n", record->_bytes);
  printf("Memory Value for Set: %" PRIu64 "\n", record->_value_for_set);
  printf("Correlation Id: %u\n", record->_correlation_id);
  printf("Memory Fill Type: %u\n", record->_mem_type);
}

void DumpRecordApi(pti_view_record_api* record) {
  if (NULL == record) return;

  const char* api_name = NULL;
  pti_result result = ptiViewGetApiIdName(record->_api_group, record->_api_id, &api_name);
  if (result != PTI_SUCCESS) {
    printf("Error getting API name: %s\n", ptiResultTypeToString(result));
    return;
  }

  printf("Api Function Name: %s\n", api_name);
  printf("Api Function CBID: %u\n", record->_api_id);
  printf("Api Start Time: %" PRIu64 " ns\n", record->_start_timestamp);
  printf("  Api End Time: %" PRIu64 " ns\n", record->_end_timestamp);
  printf("Api Process Id: %u\n", record->_process_id);
  printf("Api Thread Id: %u\n", record->_thread_id);
  printf("Api Correlation Id: %u\n", record->_correlation_id);
}

void DumpRecordSynchronization(pti_view_record_synchronization* record) {
  if (NULL == record) return;

  switch (record->_synch_type) {
    case PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_EXECUTION:
      printf("Barrier Synch Type: Execution Barrier\n");
      break;
    case PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_MEMORY:
      printf("Barrier Synch Type: Memory Coherency Barrier\n");
      break;
    case PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_FENCE:
      printf("Fence Synch Type: Execution\n");
      break;
    case PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_EVENT:
      printf("Event Synch Type: Host\n");
      break;
    case PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_LIST:
      printf("CommandList Synch Type: Host\n");
      break;
    case PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_QUEUE:
      printf("CommandQueue Synch Type: Host\n");
      break;
    default:
      break;
  }

  printf("Synch Start Time: %" PRIu64 "\n", record->_start_timestamp);
  printf("  Synch End Time: %" PRIu64 "\n", record->_end_timestamp);
  printf("  Synch Duration: %" PRIu64 " ns\n", record->_end_timestamp - record->_start_timestamp);
  printf("Synch Thread Id: %u\n", record->_thread_id);
  printf("Synch Correlation Id: %u\n", record->_correlation_id);
  printf("Synch BE Queue Handle: %p\n", record->_queue_handle);
  printf("Synch BE Context Handle: %p\n", record->_context_handle);
  printf("Synch BE Event Handle: %p\n", record->_event_handle);
  printf("Synch BE Number Wait Events: %u\n", record->_number_wait_events);
  printf("Synch Api Function CBID: %u\n", record->_api_id);
  printf("Synch Api Group ID: %u\n", record->_api_group);
  printf("Synch Api Return Code: %u\n", record->_return_code);

  const char* api_name = NULL;
  pti_result result = ptiViewGetApiIdName(PTI_API_GROUP_LEVELZERO, record->_api_id, &api_name);
  if (result == PTI_SUCCESS) {
    printf("Synch Api Function Name: %s\n", api_name);
  }
}

void DumpRecordOverhead(pti_view_record_overhead* record) {
  if (NULL == record) return;

  printf("Overhead Kind : %s\n", ptiViewOverheadKindToString(record->_overhead_kind));
  printf("Overhead Time Duration(ns): %" PRIu64 "\n", record->_overhead_duration_ns);
  printf("Overhead Count: %" PRIu64 "\n", record->_overhead_count);
  printf("Overhead Start Timestamp(ns): %" PRIu64 "\n", record->_overhead_start_timestamp_ns);
  printf("Overhead End Timestamp(ns): %" PRIu64 "\n", record->_overhead_end_timestamp_ns);
  printf("Overhead ThreadId: %u\n", record->_overhead_thread_id);
}

void DumpRecordExternalCorrelation(pti_view_record_external_correlation* record) {
  if (NULL == record) return;

  printf("External Correlation Kind : %u\n", record->_external_kind);
  printf("Correlation Id: %u\n", record->_correlation_id);
  printf("External Id: %" PRIu64 "\n", record->_external_id);
}

bool IsMonotonicUint64(const uint64_t* array, size_t count) {
  if (array == NULL || count <= 1) {
    return true;
  }

  for (size_t i = 1; i < count; ++i) {
    if (array[i - 1] > array[i]) {
      return false;
    }
  }
  return true;
}
#endif  // SAMPLES_C_UTILS_H_
