//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SAMPLES_UTILS_H_
#define SAMPLES_UTILS_H_
#include <iomanip>
#include <iostream>

#include "pti_view.h"

namespace samples_utils {

std::string stringify_uuid(uint8_t* uuid, std::string additional_string) {
  std::stringstream sstream;
  sstream << additional_string;
  sstream << std::hex << std::setfill('0');
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    sstream << std::setw(2);
    sstream << static_cast<uint16_t>(uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i==4 || i==6 || i==8 || i==10) sstream << "-";
  }
  sstream << std::setfill(' ') << std::dec;
  return sstream.str();
}

void print_uuid(uint8_t* uuid, std::string additional_string) {
  std::cout << stringify_uuid(uuid,additional_string) << std::endl;
}

void dump_record(pti_view_record_kernel* record) {
  if (NULL == record) return;

  std::cout << "Kernel Name: " << record->_name << '\n';
  std::cout << "               Ze Kernel Append Time: " << record->_append_timestamp << " ns"
            << '\n';
  std::cout << "               Ze Kernel Submit Time: " << record->_submit_timestamp << " ns"
            << '\n';
  std::cout << "                Ze Kernel Start Time: " << record->_start_timestamp << " ns"
            << '\n';
  std::cout << "                  Ze Kernel End Time: " << record->_end_timestamp << " ns" << '\n';
  std::cout << "Kernel Queue Handle: " << record->_queue_handle << '\n';
  std::cout << "Kernel CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Kernel Id : " << std::dec << record->_kernel_id << '\n';
  std::cout << "Correlation Id : " << std::dec << record->_correlation_id << '\n';
  std::cout << "Kernel Thread Id : " << std::dec << record->_thread_id << '\n';
  std::cout << "         Sycl Kernel Task Begin Time: " << std::dec
            << record->_sycl_task_begin_timestamp << " ns" << '\n';
  std::cout << "Sycl Kernel EnqueueKernel Begin Time: " << std::dec
            << record->_sycl_enqk_begin_timestamp << " ns" << '\n';
  std::cout << "Kernel Execution Time: " << std::dec
            << record->_end_timestamp - record->_start_timestamp << " ns" << '\n';
  std::cout << "Kernel File Name: " << record->_source_file_name << ":"
            << record->_source_line_number << '\n';
  std::cout << "Kernel Device: " << record->_pci_address << '\n';
  print_uuid(record->_device_uuid, "Kernel Device UUID: ");
  std::cout << "Kernel NodeID:InvocationID " << record->_sycl_node_id << ':'
            << record->_sycl_invocation_id << '\n';
}

void dump_record(pti_view_record_memory_copy* record) {
  if (NULL == record) return;

  std::cout << "Memory Op: " << record->_name << '\n';
  std::cout << "Memory Device: " << record->_pci_address << '\n';
  print_uuid(record->_device_uuid, "Memory Device UUID: ");
  std::cout << "Memory Op Execution Time: " << std::dec
            << record->_end_timestamp - record->_start_timestamp << " ns" << '\n';
  std::cout << "               Memory Op Append Time: " << std::dec << record->_append_timestamp
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << std::dec << record->_submit_timestamp
            << " ns" << '\n';
  std::cout << "                Memory Op Start Time: " << std::dec << record->_start_timestamp
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << std::dec << record->_end_timestamp
            << " ns" << '\n';
  std::cout << "Memory Op Queue Handle: " << record->_queue_handle << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id : " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Bytes Copied: " << std::dec << record->_bytes << '\n';
  std::cout << "Memory Op Thread Id : " << std::dec << record->_thread_id << '\n';
  std::cout << "Correlation Id : " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Copy Type: " << std::dec << record->_memcpy_type << '\n';
  std::cout << "Memory Copy Source: " << std::dec << record->_mem_src << '\n';
  std::cout << "Memory Copy Destination: " << std::dec << record->_mem_dst << '\n';
}

void dump_record(pti_view_record_memory_copy_p2p* record) {
  if (NULL == record) return;

  std::cout << "Memory Op: " << record->_name << '\n';
  std::cout << "Memory Source Device: " << record->_src_pci_address << '\n';
  std::cout << "Memory Destination Device: " << record->_dst_pci_address << '\n';
  print_uuid(record->_src_uuid, "Memory Source Device UUID: ");
  print_uuid(record->_dst_uuid, "Memory Destination Device UUID: ");
  std::cout << "Memory Op Execution Time: " << std::dec
            << record->_end_timestamp - record->_start_timestamp << " ns" << '\n';
  std::cout << "               Memory Op Append Time: " << std::dec << record->_append_timestamp
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << std::dec << record->_submit_timestamp
            << " ns" << '\n';
  std::cout << "                Memory Op Start Time: " << std::dec << record->_start_timestamp
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << std::dec << record->_end_timestamp
            << " ns" << '\n';
  std::cout << "Memory Op Queue Handle: " << record->_queue_handle << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id : " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Bytes Copied: " << std::dec << record->_bytes << '\n';
  std::cout << "Memory Op Thread Id : " << std::dec << record->_thread_id << '\n';
  std::cout << "Correlation Id : " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Copy Type: " << std::dec << record->_memcpy_type << '\n';
  std::cout << "Memory Copy Source: " << std::dec << record->_mem_src << '\n';
  std::cout << "Memory Copy Destination: " << std::dec << record->_mem_dst << '\n';
}

void dump_record(pti_view_record_memory_fill* record) {
  if (NULL == record) return;

  std::cout << "Memory Op: " << record->_name << '\n';
  std::cout << "Memory Device: " << record->_pci_address << '\n';
  print_uuid(record->_device_uuid, "Memory Device UUID: ");
  std::cout << "Memory Op Execution Time: " << std::dec
            << record->_end_timestamp - record->_start_timestamp << " ns" << '\n';
  std::cout << "               Memory Op Append Time: " << std::dec << record->_append_timestamp
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << std::dec << record->_submit_timestamp
            << " ns" << '\n';
  std::cout << "                Memory Op Start Time: " << std::dec << record->_start_timestamp
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << std::dec << record->_end_timestamp
            << " ns" << '\n';
  std::cout << "Memory Op Queue Handle: " << record->_queue_handle << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id : " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Op Thread Id : " << std::dec << record->_thread_id << '\n';
  std::cout << "Memory Bytes Transfered: " << record->_bytes << '\n';
  std::cout << "Memory Value for Set: " << record->_value_for_set << '\n';
  std::cout << "Correlation Id : " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Fill Type: " << std::dec << record->_mem_type << '\n';
}

void dump_record(pti_view_record_sycl_runtime* record) {
  if (NULL == record) return;
  std::cout << "Sycl Function Name: " << record->_name << '\n';
  std::cout << "Sycl Start Time: " << record->_start_timestamp << '\n';
  std::cout << "Sycl End Time: " << record->_end_timestamp << '\n';
  std::cout << "Sycl Process Id: " << record->_process_id << '\n';
  std::cout << "Sycl Thread Id: " << record->_thread_id << '\n';
  std::cout << "Sycl Correlation Id: " << record->_correlation_id << '\n';
}

void dump_record(pti_view_record_overhead* record) {
  if (NULL == record) return;
  std::cout << "Overhead Kind : " << record->_overhead_kind << '\n';
  std::cout << "Overhead Time Duration(ns): " << record->_overhead_duration_ns << '\n';
  std::cout << "Overhead Count: " << record->_overhead_count << '\n';
  std::cout << "Overhead Start Timestamp(ns): " << record->_overhead_start_timestamp_ns << '\n';
  std::cout << "Overhead End Timestamp(ns): " << record->_overhead_end_timestamp_ns << '\n';
  std::cout << "Overhead ThreadId : " << record->_overhead_thread_id << '\n';
  // std::cout << "Overhead API Responsible : "
  //<< record->_overhead_api_name << '\n';
}

void dump_record(pti_view_record_external_correlation* record) {
  if (NULL == record) return;
  std::cout << "External Correlation Kind : " << record->_external_kind << '\n';
  std::cout << "Correlation Id: " << record->_correlation_id << '\n';
  std::cout << "External Id: " << record->_external_id << '\n';
}
}  // namespace samples_utils
#endif
