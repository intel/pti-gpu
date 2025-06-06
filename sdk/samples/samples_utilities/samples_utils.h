//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SAMPLES_UTILS_H_
#define SAMPLES_UTILS_H_
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "pti/pti_view.h"

namespace samples_utils {
#define PTI_THROW(X)                                                                      \
  do {                                                                                    \
    if (X != pti_result::PTI_SUCCESS) {                                                   \
      std::string error_msg =                                                             \
          "PTI CALL FAILED: " #X " WITH ERROR: " + std::string{ptiResultTypeToString(X)}; \
      throw std::runtime_error(error_msg);                                                \
    }                                                                                     \
  } while (0)

#define PTI_CHECK_SUCCESS(X)                                                            \
  do {                                                                                  \
    if (X != pti_result::PTI_SUCCESS) {                                                 \
      std::cerr << "PTI CALL FAILED: " #X << " WITH ERROR " << ptiResultTypeToString(X) \
                << std::endl;                                                           \
      std::exit(EXIT_FAILURE);                                                          \
    }                                                                                   \
  } while (0)

inline constexpr auto kDefaultPtiBufferAlignment = std::align_val_t{1};

template <typename T>
[[nodiscard]] inline T* AlignedAlloc(std::size_t size, std::align_val_t align) {
  try {
    return static_cast<T*>(::operator new(size, align));
  } catch (const std::bad_alloc& e) {
    std::cerr << "Alloc failed " << e.what() << '\n';
    return nullptr;
  }
}

template <typename T>
inline void AlignedDealloc(T* buf_ptr, std::align_val_t align) {
  try {
    ::operator delete(buf_ptr, align);
  } catch (...) {
    std::cerr << "DeAlloc failed, abort" << '\n';
    std::abort();
  }
}

template <typename T>
[[nodiscard]] inline T* AlignedAlloc(std::size_t size) {
  return AlignedAlloc<T>(size, kDefaultPtiBufferAlignment);
}

template <typename T>
inline void AlignedDealloc(T* buf_ptr) {
  return AlignedDealloc<T>(buf_ptr, kDefaultPtiBufferAlignment);
}

template <typename... T>
constexpr std::size_t ValidateTimestamps(T... args) {
  using TimestampType = std::common_type_t<T...>;
  constexpr auto count = sizeof...(args);
  static_assert(count > 1, "Must provide more than one timestamp to validate");
  std::size_t found_issues = 0;
  TimestampType prev_stamp = 0;

  // Use fold expressions to find issues with timestamps
  // https://en.cppreference.com/w/cpp/language/fold
  // this could probably be simplified if we do not care about the number of
  // timestamp issues (we might be able to remove the lambda).
  (
      [&] {
        auto next_stamp = args;
        if (!(prev_stamp <= next_stamp)) {
          found_issues++;
        }
        prev_stamp = next_stamp;
      }(),
      ...);
  return found_issues;
}

//
// Returns:    True  - if a_list passed in is a monotonically increasing sequence.
//             False - if not.
// Assumption: Operator <= is well defined for this type already.
//
template <typename T>
inline bool isMonotonic(std::initializer_list<T> a_list) {
  bool current_state = true;
  T previous = a_list.begin()[0];
  for (auto item : a_list) {
    current_state = current_state && (previous <= item);
    if (!current_state) return false;
    previous = item;
  }
  return true;
}

inline std::string stringify_uuid(uint8_t* uuid, std::string additional_string) {
  std::stringstream sstream;
  sstream << additional_string;
  sstream << std::hex << std::setfill('0');
  for (uint32_t i = 1; i <= PTI_MAX_DEVICE_UUID_SIZE; ++i) {
    sstream << std::setw(2);
    sstream << static_cast<uint16_t>(uuid[PTI_MAX_DEVICE_UUID_SIZE - i]);
    if (i == 4 || i == 6 || i == 8 || i == 10) sstream << "-";
  }
  sstream << std::setfill(' ') << std::dec;
  return sstream.str();
}

inline void print_uuid(uint8_t* uuid, std::string additional_string) {
  std::cout << stringify_uuid(uuid, std::move(additional_string)) << std::endl;
}

inline void DumpRecord(pti_view_record_kernel* record) {
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
  std::cout << "Kernel Queue ID: " << record->_sycl_queue_id << '\n';
  std::cout << "Kernel CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Kernel Id: " << std::dec << record->_kernel_id << '\n';
  std::cout << "Correlation Id: " << std::dec << record->_correlation_id << '\n';
  std::cout << "Kernel Thread Id: " << std::dec << record->_thread_id << '\n';
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

inline void DumpRecord(pti_view_record_memory_copy* record) {
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
  std::cout << "Memory Op Queue ID: " << record->_sycl_queue_id << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id: " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Bytes Copied: " << std::dec << record->_bytes << '\n';
  std::cout << "Memory Op Thread Id: " << std::dec << record->_thread_id << '\n';
  std::cout << "Correlation Id: " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Copy Type: " << std::dec << ptiViewMemcpyTypeToString(record->_memcpy_type)
            << '\n';
  std::cout << "Memory Copy Source: " << std::dec << ptiViewMemoryTypeToString(record->_mem_src)
            << '\n';
  std::cout << "Memory Copy Destination: " << std::dec
            << ptiViewMemoryTypeToString(record->_mem_dst) << '\n';
}

inline void DumpRecord(pti_view_record_memory_copy_p2p* record) {
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
  std::cout << "Memory Op Queue ID: " << record->_sycl_queue_id << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id: " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Bytes Copied: " << std::dec << record->_bytes << '\n';
  std::cout << "Memory Op Thread Id: " << std::dec << record->_thread_id << '\n';
  std::cout << "Correlation Id: " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Copy Type: " << std::dec << ptiViewMemcpyTypeToString(record->_memcpy_type)
            << '\n';
  std::cout << "Memory Copy Source: " << std::dec << ptiViewMemoryTypeToString(record->_mem_src)
            << '\n';
  std::cout << "Memory Copy Destination: " << std::dec
            << ptiViewMemoryTypeToString(record->_mem_dst) << '\n';
}

inline void DumpRecord(pti_view_record_memory_fill* record) {
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
  std::cout << "Memory Op Queue ID: " << record->_sycl_queue_id << '\n';
  std::cout << "Memory Op CommandList Context Handle: " << record->_context_handle << '\n';
  std::cout << "Memory Op Id: " << std::dec << record->_mem_op_id << '\n';
  std::cout << "Memory Op Thread Id: " << std::dec << record->_thread_id << '\n';
  std::cout << "Memory Bytes Transfered: " << record->_bytes << '\n';
  std::cout << "Memory Value for Set: " << record->_value_for_set << '\n';
  std::cout << "Correlation Id: " << std::dec << record->_correlation_id << '\n';
  std::cout << "Memory Fill Type: " << std::dec << record->_mem_type << '\n';
}

inline void DumpRecord(pti_view_record_api* record) {
  if (NULL == record) return;
  const char* api_name = nullptr;
  PTI_THROW(ptiViewGetApiIdName(record->_api_group, record->_api_id, &api_name));
  std::cout << "Api Function Name: " << api_name << '\n';
  std::cout << "Api Function CBID: " << record->_api_id << '\n';
  std::cout << "Api Start Time: " << record->_start_timestamp << '\n';
  std::cout << "  Api End Time: " << record->_end_timestamp << '\n';
  std::cout << "Api Process Id: " << record->_process_id << '\n';
  std::cout << "Api Thread Id: " << record->_thread_id << '\n';
  std::cout << "Api Correlation Id: " << record->_correlation_id << '\n';
}

inline void DumpRecord(pti_view_record_synchronization* record) {
  if (NULL == record) return;
  switch (record->_synch_type) {
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_EXECUTION: {
      std::cout << "Barrier Synch Type: Execution Barrier\n";
    }; break;
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_MEMORY: {
      std::cout << "Barrier Synch Type: Memory Coherency Barrier\n";
    }; break;
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_FENCE: {
      std::cout << "Fence Synch Type: Execution\n";
    }; break;
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_EVENT: {
      std::cout << "Event Synch Type: Host\n";
    }; break;
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_LIST: {
      std::cout << "CommandList Synch Type: Host\n";
    }; break;
    case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_QUEUE: {
      std::cout << "CommandQueue Synch Type: Host\n";
    }; break;
    default:
      break;
  }
  std::cout << "Synch Start Time: " << record->_start_timestamp << '\n';
  std::cout << "  Synch End Time: " << record->_end_timestamp << '\n';
  std::cout << "  Synch Duration: " << record->_end_timestamp - record->_start_timestamp << "ns\n";
  std::cout << "Synch Thread Id: " << record->_thread_id << '\n';
  std::cout << "Synch Correlation Id: " << record->_correlation_id << '\n';
  std::cout << "Synch BE Queue Handle: " << record->_queue_handle << '\n';
  std::cout << "Synch BE Context Handle: " << record->_context_handle << '\n';
  std::cout << "Synch BE Event Handle: " << record->_event_handle << '\n';
  std::cout << "Synch BE Number Wait Events: " << record->_number_wait_events << '\n';
  std::cout << "Synch Api Function CBID: " << record->_api_id << '\n';
  std::cout << "Synch Api Group ID: " << record->_api_group << '\n';
  std::cout << "Synch Api Return Code: " << record->_return_code << '\n';
  const char* api_name = nullptr;
  PTI_THROW(
      ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, record->_api_id, &api_name));
  std::cout << "Synch Api Function Name: " << api_name << '\n';
}

inline void DumpRecord(pti_view_record_overhead* record) {
  if (NULL == record) return;
  std::cout << "Overhead Kind : " << ptiViewOverheadKindToString(record->_overhead_kind) << '\n';
  std::cout << "Overhead Time Duration(ns): " << record->_overhead_duration_ns << '\n';
  std::cout << "Overhead Count: " << record->_overhead_count << '\n';
  std::cout << "Overhead Start Timestamp(ns): " << record->_overhead_start_timestamp_ns << '\n';
  std::cout << "Overhead End Timestamp(ns): " << record->_overhead_end_timestamp_ns << '\n';
  std::cout << "Overhead ThreadId: " << record->_overhead_thread_id << '\n';
  // std::cout << "Overhead API Responsible : "
  //<< record->_overhead_api_name << '\n';
}

inline void DumpRecord(pti_view_record_external_correlation* record) {
  if (NULL == record) return;
  std::cout << "External Correlation Kind : " << record->_external_kind << '\n';
  std::cout << "Correlation Id: " << record->_correlation_id << '\n';
  std::cout << "External Id: " << record->_external_id << '\n';
}

inline pti_backend_queue_t GetLevelZeroBackendQueue(sycl::queue& queue) {
  pti_backend_queue_t backend_queue = nullptr;
  auto queue_type = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(queue);

  if (auto* ptr_queue_handle = std::get_if<ze_command_list_handle_t>(&queue_type)) {
    backend_queue = static_cast<pti_backend_queue_t>(*ptr_queue_handle);
  } else if (auto* ptr_queue_handle = std::get_if<ze_command_queue_handle_t>(&queue_type)) {
    backend_queue = static_cast<pti_backend_queue_t>(*ptr_queue_handle);
  }
  return backend_queue;
}

}  // namespace samples_utils
#endif
