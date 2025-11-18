//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SAMPLES_UTILS_H_
#define SAMPLES_UTILS_H_
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

#include "pti/pti_callback.h"
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

// Helper class and function: formats an int with thousands separator
// e.g. 1234567890 -> 1'234'567'890
// when manually comparing timestamps in tests
class ApostropheNumpunct : public std::numpunct<char> {
 protected:
  char do_thousands_sep() const override { return '\''; }
  std::string do_grouping() const override { return "\3"; }
};

template <typename T>
inline std::string AposFormat(T value) {
  static_assert(std::is_unsigned<T>::value, "Unsigned integer type required");
  std::ostringstream oss;
  static std::locale apostrophe_locale(std::locale::classic(), new ApostropheNumpunct);
  oss.imbue(apostrophe_locale);
  oss << value;
  return oss.str();
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
  std::cout << "               Ze Kernel Append Time: " << AposFormat(record->_append_timestamp)
            << " ns" << '\n';
  std::cout << "               Ze Kernel Submit Time: " << AposFormat(record->_submit_timestamp)
            << " ns" << '\n';
  std::cout << "                Ze Kernel Start Time: " << AposFormat(record->_start_timestamp)
            << " ns" << '\n';
  std::cout << "                  Ze Kernel End Time: " << AposFormat(record->_end_timestamp)
            << " ns" << '\n';
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
            << AposFormat(record->_end_timestamp - record->_start_timestamp) << " ns" << '\n';
  std::cout << "               Memory Op Append Time: " << AposFormat(record->_append_timestamp)
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << AposFormat(record->_submit_timestamp)
            << " ns" << '\n';
  std::cout << "                Memory Op Start Time: " << AposFormat(record->_start_timestamp)
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << AposFormat(record->_end_timestamp)
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
  std::cout << "               Memory Op Append Time: " << AposFormat(record->_append_timestamp)
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << AposFormat(record->_submit_timestamp)
            << " ns" << '\n';
  std::cout << "                Memory Op Start Time: " << AposFormat(record->_start_timestamp)
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << AposFormat(record->_end_timestamp)
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
  std::cout << "Memory Op Execution Time: "
            << AposFormat(record->_end_timestamp - record->_start_timestamp) << " ns" << '\n';
  std::cout << "               Memory Op Append Time: " << AposFormat(record->_append_timestamp)
            << " ns" << '\n';
  std::cout << "               Memory Op Submit Time: " << AposFormat(record->_submit_timestamp)
            << " ns" << '\n';
  std::cout << "               Memory Op Start Time: " << AposFormat(record->_start_timestamp)
            << " ns" << '\n';
  std::cout << "                  Memory Op End Time: " << AposFormat(record->_end_timestamp)
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
  std::cout << "Api Function Id:   " << record->_api_id << '\n';
  std::cout << "Correlation Id:    " << record->_correlation_id << '\n';
  std::cout << "Api Start Time: " << AposFormat(record->_start_timestamp) << " ns" << '\n';
  std::cout << "  Api End Time: " << AposFormat(record->_end_timestamp) << " ns" << '\n';
  std::cout << "Process Id:     " << record->_process_id << '\n';
  std::cout << "Thread Id:      " << record->_thread_id << '\n';
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
  std::cout << "Synch Api Function Id: " << record->_api_id << '\n';
  std::cout << "Synch Api Group Id:    " << record->_api_group << '\n';
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
  std::cout << "External Correlation Kind: " << record->_external_kind << '\n';
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

inline std::string GetCommandListTypeString(pti_backend_command_list_type cmd_list_type) {
  std::string result;
  if (cmd_list_type & PTI_BACKEND_COMMAND_LIST_TYPE_UNKNOWN) {
    result += " | Unknown";
  }
  if (cmd_list_type & PTI_BACKEND_COMMAND_LIST_TYPE_IMMEDIATE) {
    result += " | Immediate";
  }
  if (cmd_list_type & PTI_BACKEND_COMMAND_LIST_TYPE_MUTABLE) {
    result += " | Mutable";
  }

  if (!result.empty()) {
    result = result.substr(3);  // remove leading " | "
  } else {
    result = "INVALID_VALUE";
  }
  return result;
}

inline std::string GetOperationTypeString(pti_gpu_operation_kind operation_kind) {
  std::string result;
  switch (operation_kind) {
    case (PTI_GPU_OPERATION_KIND_KERNEL):
      result = "Kernel";
      break;
    case (PTI_GPU_OPERATION_KIND_MEMORY):
      result = "Memory";
      break;
    case (PTI_GPU_OPERATION_KIND_OTHER):
      result = "Other";
      break;
    default:
      result = "Unknown";
      break;
  }
  return result;
}

inline void DumpCallbackData(pti_callback_domain domain, pti_api_group_id driver_api_group_id,
                             uint32_t driver_api_id, pti_backend_ctx_t backend_context,
                             void* cb_data, void* global_user_data, void** instance_user_data) {
  std::cout << "=== Callback Data Dump ===" << std::endl;
  std::cout << "Domain: " << ptiCallbackDomainTypeToString(domain)
            << ", Backend Context: " << backend_context << std::endl;

  const char* api_name = nullptr;
  if (PTI_SUCCESS == ptiViewGetApiIdName(driver_api_group_id, driver_api_id, &api_name)) {
    std::cout << "Driver API Group Id/API Id/Name: " << driver_api_group_id << "/" << driver_api_id
              << "/" << api_name << std::endl;
  } else {
    std::cout << "Driver API Group Id/API Id/Name: " << driver_api_group_id << "/" << driver_api_id
              << "/Unknown" << std::endl;
  }
  if (cb_data != nullptr) {
    switch (domain) {
      case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED:
      case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED:
      case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED: {
        pti_callback_gpu_op_data* gpu_op_data = static_cast<pti_callback_gpu_op_data*>(cb_data);
        std::cout << "GPU Operation Data:" << std::endl;
        std::cout << "  Phase: " << ptiCallbackPhaseTypeToString(gpu_op_data->_phase) << std::endl;
        if (domain != PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
          std::cout << "  Command List Handle: " << gpu_op_data->_cmd_list_handle << std::endl;
          std::cout << "  Command List Type:   "
                    << GetCommandListTypeString(gpu_op_data->_cmd_list_properties) << std::endl;
          std::cout << "  Queue Handle:        " << gpu_op_data->_queue_handle << std::endl;
          std::cout << "  Correlation Id:      " << gpu_op_data->_correlation_id << std::endl;
        }
        std::cout << "  Device Handle: " << gpu_op_data->_device_handle << std::endl;
        std::cout << "  Return Code: " << gpu_op_data->_return_code << std::endl;
        std::cout << "  Operation Count: " << gpu_op_data->_operation_count << std::endl;

        if (gpu_op_data->_operation_details != nullptr) {
          pti_gpu_op_details* op_details =
              static_cast<pti_gpu_op_details*>(gpu_op_data->_operation_details);
          std::cout << "  GPU Operations Details:" << std::endl;
          for (uint32_t i = 0; i < gpu_op_data->_operation_count; ++i, ++op_details) {
            std::cout << " -- Operation Kind: "
                      << GetOperationTypeString(op_details->_operation_kind) << std::endl;
            std::cout << "    Operation Id: " << op_details->_operation_id << std::endl;
            std::cout << "    Kernel Handle: " << op_details->_kernel_handle << std::endl;
            if (op_details->_name != nullptr) {
              std::cout << "    Name: " << op_details->_name << std::endl;
            }
          }
        }
        break;
      }
      case PTI_CB_DOMAIN_INTERNAL_THREADS:
      case PTI_CB_DOMAIN_INTERNAL_EVENT: {
        pti_internal_callback_data* internal_data =
            static_cast<pti_internal_callback_data*>(cb_data);
        std::cout << "Internal Callback Data:" << std::endl;
        std::cout << "  Phase: " << ptiCallbackPhaseTypeToString(internal_data->_phase) << " ("
                  << internal_data->_phase << ")" << std::endl;
        std::cout << "  Detail: " << internal_data->_detail << std::endl;
        if (internal_data->_message != nullptr) {
          std::cout << "  Message: " << internal_data->_message << std::endl;
        }
        break;
      }
      default:
        std::cout << "Unknown domain type for callback data" << std::endl;
        break;
    }
  } else {
    std::cout << "Callback data is null" << std::endl;
  }

  if (global_user_data != nullptr) {
    std::cout << "Global User Data: " << global_user_data << std::endl;
  }

  if (instance_user_data != nullptr && *instance_user_data != nullptr) {
    std::cout << "Instance User Data: " << *instance_user_data << std::endl;
  }

  std::cout << "=========================" << std::endl;
}

}  // namespace samples_utils
#endif
