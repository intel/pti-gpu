//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef INCLUDE_PTI_VIEW_H_
#define INCLUDE_PTI_VIEW_H_

#include <level_zero/layers/zel_tracing_api.h>
#include <stddef.h>
#include <stdint.h>

#include "pti/pti.h"
#include "pti/pti_export.h"

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief const defines.
 */
#define PTI_MAX_DEVICE_UUID_SIZE 16                         //!< Size of uuid array.
#define PTI_MAX_PCI_ADDRESS_SIZE 16                         //!< Size of pci address array.
#define PTI_INVALID_QUEUE_ID 0xFFFFFFFFFFFFFFFF-1           //!< For oneAPI versions earlier than 2024.1.1 -- UINT64_MAX-1

/**
 * @brief Kinds of software and hardware operations to be tracked and viewed,
 * passed to ptiViewEnable/ptiViewDisable
 */
typedef enum _pti_view_kind {
  PTI_VIEW_INVALID = 0,                   //!< Invalid
  PTI_VIEW_DEVICE_GPU_KERNEL = 1,         //!< Device kernels
  PTI_VIEW_DEVICE_CPU_KERNEL = 2,         //!< Host (CPU) kernels
  PTI_VIEW_LEVEL_ZERO_CALLS = 3,          //!< Level-Zero APIs tracing
  PTI_VIEW_OPENCL_CALLS = 4,              //!< OpenCL APIs tracing
  PTI_VIEW_COLLECTION_OVERHEAD = 5,       //!< Collection overhead
  PTI_VIEW_SYCL_RUNTIME_CALLS = 6,        //!< SYCL runtime API tracing
  PTI_VIEW_EXTERNAL_CORRELATION = 7,      //!< Correlation of external operations
  PTI_VIEW_DEVICE_GPU_MEM_COPY = 8,       //!< Memory copies between Host and Device
  PTI_VIEW_DEVICE_GPU_MEM_FILL = 9,       //!< Device memory fills
  PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P = 10,  //!< Peer to Peer Memory copies between Devices.
} pti_view_kind;

/**
 * @brief Memory types
 */
typedef enum _pti_view_memory_type {
  PTI_VIEW_MEMORY_TYPE_MEMORY = 0,  //!< Unknown memory type
  PTI_VIEW_MEMORY_TYPE_HOST = 1,    //!< Host memory
  PTI_VIEW_MEMORY_TYPE_DEVICE = 2,  //!< Device memory
  PTI_VIEW_MEMORY_TYPE_SHARED = 3,  //!< Shared memory
} pti_view_memory_type;

/**
 * @brief Memory copy types
 * where M=Memory, D=Device, H=Host, S=Shared
 */
typedef enum _pti_view_memcpy_type {
  PTI_VIEW_MEMCPY_TYPE_M2M = 0,     //!< Memory to Memory type
  PTI_VIEW_MEMCPY_TYPE_M2H = 1,     //!< Memory to Host type
  PTI_VIEW_MEMCPY_TYPE_M2D = 2,     //!< Memory to Device type
  PTI_VIEW_MEMCPY_TYPE_M2S = 3,     //!< Memory to Shared type

  PTI_VIEW_MEMCPY_TYPE_H2M = 4,     //!< Host to Memory type
  PTI_VIEW_MEMCPY_TYPE_H2H = 5,     //!< Host to Host type
  PTI_VIEW_MEMCPY_TYPE_H2D = 6,     //!< Host to Device type
  PTI_VIEW_MEMCPY_TYPE_H2S = 7,     //!< Host to Shared type

  PTI_VIEW_MEMCPY_TYPE_D2M = 8,     //!< Device to Memory type
  PTI_VIEW_MEMCPY_TYPE_D2H = 9,     //!< Device to Host type
  PTI_VIEW_MEMCPY_TYPE_D2D = 10,    //!< Device to Device type
  PTI_VIEW_MEMCPY_TYPE_D2S = 11,    //!< Device to Shared type

  PTI_VIEW_MEMCPY_TYPE_S2M = 12,    //!< Shared to Memory type
  PTI_VIEW_MEMCPY_TYPE_S2H = 13,    //!< Shared to Host type
  PTI_VIEW_MEMCPY_TYPE_S2D = 14,    //!< Shared to Device type
  PTI_VIEW_MEMCPY_TYPE_S2S = 15,    //!< Shared to Shared type
} pti_view_memcpy_type;

/**
 *  @brief External correlation kinds
 */
typedef enum _pti_view_external_kind {
  PTI_VIEW_EXTERNAL_KIND_INVALID = 0,   //!< Invalid external kind
  PTI_VIEW_EXTERNAL_KIND_UNKNOWN = 1,   //!< Unknown external kind
  PTI_VIEW_EXTERNAL_KIND_CUSTOM_0 = 2,  //!< Custom external kind
  PTI_VIEW_EXTERNAL_KIND_CUSTOM_1 = 3,  //!< Custom external kind
  PTI_VIEW_EXTERNAL_KIND_CUSTOM_2 = 4,  //!< Custom external kind
  PTI_VIEW_EXTERNAL_KIND_CUSTOM_3 = 5,  //!< Custom external kind
} pti_view_external_kind;

/**
 *  @brief Collection Overhead kinds
 */
typedef enum _pti_view_overhead_kind { 
  PTI_VIEW_OVERHEAD_KIND_INVALID = 0,        //!< Invalid overhead kind
  PTI_VIEW_OVERHEAD_KIND_UNKNOWN = 1,        //!< Unknown overhead kind
  PTI_VIEW_OVERHEAD_KIND_RESOURCE = 2,       //!< Overhead due to a resource 
  PTI_VIEW_OVERHEAD_KIND_BUFFER_FLUSH = 3,   //!< Overhead due to a buffer flush
  PTI_VIEW_OVERHEAD_KIND_DRIVER = 4,         //!< Overhead due to driver
  PTI_VIEW_OVERHEAD_KIND_TIME = 5,           //!< Overhead due to L0 api processing time
} pti_view_overhead_kind;

/**
 * @brief Base View record type
 */
typedef struct pti_view_record_base {
  pti_view_kind _view_kind;                   //!< Record View kind
} pti_view_record_base;

/**
 * @brief Device Compute kernel View record type
 */
typedef struct pti_view_record_kernel {
  pti_view_record_base _view_kind;                  //!< Base record
  ze_command_queue_handle_t _queue_handle;          //!< Device back-end queue handle
  ze_context_handle_t _context_handle;              //!< Context handle
  const char* _name;                                //!< Kernel name
  const char* _source_file_name;                    //!< Kernel source file,
                                                    //!< null if no information
  uint64_t _source_line_number;                     //!< Kernel beginning source line number,
                                                    //!< 0 if no information
  uint64_t _kernel_id;                              //!< Kernel instance ID,
                                                    //!< unique among all device kernel instances
  uint32_t _correlation_id;                         //!< ID that correlates this record with records
                                                    //!< of other Views
  uint32_t _thread_id;                              //!< Thread ID of Function call
  char _pci_address[PTI_MAX_PCI_ADDRESS_SIZE];      //!< Device pci_address
  uint8_t _device_uuid[PTI_MAX_DEVICE_UUID_SIZE];   //!< Device uuid
  uint64_t _append_timestamp;                       //!< Timestamp of kernel appending to
                                                    //!< back-end command list, ns
  uint64_t _start_timestamp;                        //!< Timestamp of kernel start on device, ns
  uint64_t _end_timestamp;                          //!< Timestamp of kernel completion on device, ns
  uint64_t _submit_timestamp;                       //!< Timestamp of kernel command list submission
                                                    //!< of device, ns
  uint64_t _sycl_task_begin_timestamp;              //!< Timestamp of kernel submission from SYCL layer,
                                                    //!< ns
  uint64_t _sycl_enqk_begin_timestamp;              //!< Timestamp of enqueue kernel from SYCL layer, ns
  uint64_t _sycl_node_id;                           //!< SYCL Node ID
  uint64_t _sycl_queue_id;                          //!< Device front-end queue id
  uint32_t _sycl_invocation_id;                     //!< SYCL Invocation ID
} pti_view_record_kernel;

/**
 * @brief SYCL runtime API View record type
 */
typedef struct pti_view_record_sycl_runtime{
  pti_view_record_base _view_kind;  //!< Base record
  const char* _name;                //!< SYCL runtime function name
  uint64_t _start_timestamp;        //!< Function enter timestamp, ns
  uint64_t _end_timestamp;          //!< Function exit timestamp, ns
  uint32_t _process_id;             //!< Process ID of function call
  uint32_t _thread_id;              //!< Thread ID of function call
  uint32_t _correlation_id;         //!< ID that correlates this record with records of other Views
} pti_view_record_sycl_runtime;

/**
 * @brief Memory Copy Operation View record type
 */
typedef struct pti_view_record_memory_copy {
  pti_view_record_base _view_kind;                  //!< Base record
  pti_view_memcpy_type _memcpy_type;                //!< Memory copy type
  pti_view_memory_type _mem_src;                    //!< Memory type
  pti_view_memory_type _mem_dst;                    //!< Memory type
  ze_command_queue_handle_t _queue_handle;          //!< Device back-end queue handle
  ze_context_handle_t _context_handle;              //!< Context handle
  const char* _name;                                //!< Back-end API name making a memory copy
  char _pci_address[PTI_MAX_PCI_ADDRESS_SIZE];      //!< Source or Destination Device pci_address
  uint8_t _device_uuid[PTI_MAX_DEVICE_UUID_SIZE];   //!< Source or Destination Device uuid
  uint64_t _mem_op_id;                              //!< Memory operation ID, unique among
                                                    //!< all memory operations instances
  uint32_t _correlation_id;                         //!< ID that correlates this record with records
                                                    //!< of other Views
  uint32_t _thread_id;                              //!< Thread ID from which operation submitted
  uint64_t _append_timestamp;                       //!< Timestamp of memory copy appending to
                                                    //!< back-end command list, ns
  uint64_t _start_timestamp;                        //!< Timestamp of memory copy start on device, ns
  uint64_t _end_timestamp;                          //!< Timestamp of memory copy completion on device, ns
  uint64_t _submit_timestamp;                       //!< Timestamp of memory copy command list submission
                                                    //!< to device, ns
  uint64_t _bytes;                                  //!< number of bytes copied
  uint64_t _sycl_queue_id;                          //!< Device front-end queue id
} pti_view_record_memory_copy;

/**
 * @brief Peer to Peer Memory Copy Operation View record type
 */
typedef struct pti_view_record_memory_copy_p2p {
  pti_view_record_base _view_kind;                  //!< Base record
  pti_view_memcpy_type _memcpy_type;                //!< Memory copy type
  pti_view_memory_type _mem_src;                    //!< Memory type
  pti_view_memory_type _mem_dst;                    //!< Memory type
  ze_command_queue_handle_t _queue_handle;          //!< Device back-end queue handle
  ze_context_handle_t _context_handle;              //!< Context handle
  const char* _name;                                //!< Back-end API name making a memory copy
  char _src_pci_address[PTI_MAX_PCI_ADDRESS_SIZE];  //!< Source Device pci_address
  char _dst_pci_address[PTI_MAX_PCI_ADDRESS_SIZE];  //!< Destination Device pci_address
  uint8_t _src_uuid[PTI_MAX_DEVICE_UUID_SIZE];      //!< Source Device uuid
  uint8_t _dst_uuid[PTI_MAX_DEVICE_UUID_SIZE];      //!< Destination Device uuid
  uint64_t _mem_op_id;                              //!< Memory operation ID, unique among
                                                    //!< all memory operations instances
  uint32_t _correlation_id;                         //!< ID that correlates this record with records
                                                    //!< of other Views
  uint32_t _thread_id;                              //!< Thread ID from which operation submitted
  uint64_t _append_timestamp;                       //!< Timestamp of memory copy appending to
                                                    //!< back-end command list, ns
  uint64_t _start_timestamp;                        //!< Timestamp of memory copy start on device, ns
  uint64_t _end_timestamp;                          //!< Timestamp of memory copy completion on device, ns
  uint64_t _submit_timestamp;                       //!< Timestamp of memory copy command list submission
                                                    //!< to device, ns
  uint64_t _bytes;                                  //!< number of bytes copied
  uint64_t _sycl_queue_id;                          //!< Device front-end queue id
} pti_view_record_memory_copy_p2p;

/**
 * @brief Device Memory Fill operation View record type
 */
typedef struct pti_view_record_memory_fill {
  pti_view_record_base _view_kind;                  //!< Base record
  pti_view_memory_type _mem_type;                   //!< Type of memory filled
  ze_command_queue_handle_t _queue_handle;          //!< Device back-end queue handle
  ze_context_handle_t _context_handle;              //!< Context handle
  const char* _name;                                //!< Back-end API name making a memory fill
  char _pci_address[PTI_MAX_PCI_ADDRESS_SIZE];      //!< Device pci_address
  uint8_t _device_uuid[PTI_MAX_DEVICE_UUID_SIZE];   //!< Device uuid
  uint64_t _mem_op_id;                              //!< Memory operation ID,
                                                    //!< unique among all memory operations instances
  uint32_t _correlation_id;                         //!< ID provided by user, marking some external
                                                    //!< to PTI operations
  uint32_t _thread_id;                              //!< Thread ID from which operation submitted
  uint64_t _append_timestamp;                       //!< Timestamp of memory fill appending
                                                    //!< to back-end command list, ns
  uint64_t _start_timestamp;                        //!< Timestamp of memory fill start on device, ns
  uint64_t _end_timestamp;                          //!< Timestamp of memory fill completion on device, ns
  uint64_t _submit_timestamp;                       //!< Timestamp of memory fill command list submission
                                                    //!< to device, ns
  uint64_t _bytes;                                  //!< Number of bytes filled
  uint64_t _value_for_set;                          //!< Value filled
  uint64_t _sycl_queue_id;                          //!< Device front-end queue id
} pti_view_record_memory_fill;

/**
 * @brief External Correlation View record type
 */
typedef struct pti_view_record_external_correlation {
  pti_view_record_base _view_kind;          //!< Base record
  uint32_t _correlation_id;                 //!< ID that correlates this record with records
                                            //!< of other Views
  uint64_t _external_id;                    //!< ID provided by user, marking an external
                                            //!< to PTI operation
  pti_view_external_kind _external_kind;
} pti_view_record_external_correlation;


/**
 * @brief Overhead View record type
 */
typedef struct pti_view_record_overhead {
  pti_view_record_base _view_kind;          //!< Base record
  uint64_t _overhead_start_timestamp_ns;    //!< Overhead observation start timestamp, ns
  uint64_t _overhead_end_timestamp_ns;      //!< Overhead observation end timestamp, ns
  uint64_t _overhead_thread_id;             //!< Thread ID of where the overhead observed
  uint64_t _overhead_count;                 //!< number of views in the overhead region
  uint64_t _overhead_duration_ns;           //!< Cumulative duration of the overhead over
                                            //!< the observation region, could be less than
                                            //!< interval between the observation region
                                            //!< start and the end
  pti_view_overhead_kind  _overhead_kind;   //!< Type of overhead
} pti_view_record_overhead;


/**
 * @brief Function pointer for buffer completed
 *
 * @param buffer
 * @param buffer_size_in_bytes
 * @param used_bytes
 * @return void
 */
typedef void (*pti_fptr_buffer_completed)(unsigned char* buffer,
                                             size_t buffer_size_in_bytes,
                                             size_t used_bytes);

/**
 * @brief Function pointer for buffer requested
 *
 * @param buffer_ptr
 * @param buffer_size_in_bytes
 * @return void
 */
typedef void (*pti_fptr_buffer_requested)(unsigned char** buffer_ptr,
                                             size_t* buffer_size_in_bytes);


/**
 * @brief Sets callback to user buffer management functions implemented
 * by a user
 *
 * @param fptr_bufferRequested
 * @param fptr_bufferCompleted
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiViewSetCallbacks(pti_fptr_buffer_requested fptr_bufferRequested,
                       pti_fptr_buffer_completed fptr_bufferCompleted);

/**
 * @brief Enables View of specific group of operations
 *
 * @param view_kind
 * @return pti_result
 */
pti_result PTI_EXPORT ptiViewEnable(pti_view_kind view_kind);

/**
 * @brief Disables View of specific group of operations
 *
 * @param view_kind
 * @return pti_result
 */
pti_result PTI_EXPORT ptiViewDisable(pti_view_kind view_kind);

/**
 * @brief Returns if GPU Local view is supported by the installed driver
 *
 * @return pti_result
*/
pti_result PTI_EXPORT ptiViewGPULocalAvailable();

/**
 * @brief Flushes all view records by calling bufferCompleted callback
 *
 * @return pti_result
 */
pti_result PTI_EXPORT ptiFlushAllViews();

/**
 * @brief Gets next view record in buffer.
 *
 * @param buffer The buffer initially provided by pti_fptr_buffer_requested
 * user function and now passd to pti_fptr_buffer_completed
 * @param valid_bytes Size of portion of the buffer filled with view records
 * @param record Current view record
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiViewGetNextRecord(uint8_t* buffer, size_t valid_bytes,
                       pti_view_record_base** record);

/**
 * @brief Pushes ExternelCorrelationId kind and id for generation of external correlation records
 *
 * @param external_kind
 * @param external_id
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiViewPushExternalCorrelationId(pti_view_external_kind external_kind, uint64_t external_id);

/**
 * @brief Pops ExternelCorrelationId kind and id for generation of external correlation records
 *
 * @param external_kind
 * @param external_id
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiViewPopExternalCorrelationId(pti_view_external_kind external_kind, uint64_t* p_external_id);

/**
 * @brief Helper function to return stringified enum types for pti_view_overhead_kind.
 *
 * @return const char*
 */
PTI_EXPORT const char*
ptiViewOverheadKindToString( pti_view_overhead_kind type );

/**
 * @brief Helper function to return stringified enum types for pti_view_memory_type.
 *
 * @return const char*
 */
PTI_EXPORT const char*
ptiViewMemoryTypeToString( pti_view_memory_type type );

/**
 * @brief Helper function to return stringified enum types for pti_view_memcpy_type.
 *
 * @return const char*
 */
PTI_EXPORT const char*
ptiViewMemcpyTypeToString( pti_view_memcpy_type type );

/**
 * @brief Returns current pti host timestamp in nanoseconds. The timestamp is in the same domain as view records timestamps.
 *
 * @return uint64_t
 */
PTI_EXPORT uint64_t
ptiViewGetTimestamp();


/**
 * @brief User provided timestamping function.
 *        This will be used to obtain host timestamps when user registers using the ptiViewSetTimestampCallback.
 *        It is expected that this function will return timestamps in nano seconds.
 */
typedef uint64_t (*pti_fptr_get_timestamp)( void );

/**
 * @brief Sets callback to user provided timestamping function.  This will replace the default pti host timestamper.
 *        Multiple callbacks that set differing timestamp function, through the session; will result in differing
 *        timestamp domains in the view record buffer.
 *
 * @return pti_result
 */
pti_result PTI_EXPORT
ptiViewSetTimestampCallback(pti_fptr_get_timestamp fptr_timestampRequested);

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDE_PTI_VIEW_H_
