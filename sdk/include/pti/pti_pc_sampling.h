//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_H_
#define PTI_PC_SAMPLING_H_

#include <stddef.h>
#include <stdint.h>

#include "pti/pti.h"

/* clang-format off */
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * This file defines the PTI PC Sampling (GPU EU Stall Sampling) API for collecting statistical
 * per-instruction active or categorized stall samples.
 * 
 * This API is experimental and subject to change without deprecation in future releases.
 *
 * Configuration:
 *   1. Call ptiPcSamplingEnable to create a collection handle
 *   2. Call ptiPcSamplingConfigure to select the devices to profile and the sampling period.
 *   3. Call ptiPcSamplingQueryCollectionBufferSize to query the recommended collection buffer size (optional)
 *   4. Call ptiPcSamplingSetCollectionBufferSize to set the collection buffer size (optional)
 *
 * Collection:
 *   5. Call ptiPcSamplingStartCollection to begin sampling
 *   6. Call ptiPcSamplingStopCollection to end sampling
 *
 * Data retrieval (called only after ptiPcSamplingStopCollection):
 *   7. Call ptiPcSamplingGetStallReasons to obtain stall reason information shared by all profiled devices (optional)
 *   8. Call ptiPcSamplingGetProfiledDevices to obtain profiled devices
 *   9. Call ptiPcSamplingGetDeviceStatus to obtain per-device collection status (optional)
 *  10. For each profiled device, call ptiPcSamplingGetObservedKernelHandles to obtain kernel handles with samples
 *  11. For each kernel handle, call ptiPcSamplingGetObservedKernelInfo to obtain kernel metadata
 *  12. For each kernel handle, call ptiPcSamplingGetSamplesPerInstruction to collect instruction-level sampling data
 *
 * Cleanup:
 *  13. Call ptiPcSamplingDisable to free all PTI-owned resources
 *
 * Device support:
 *   - Current implementations may support collection on only a single device.
 *   - Future multi-device support is expected to provide two flows:
 *     1. The caller explicitly selects devices to profile.
 *     2. The caller passes NULL for the device array and PTI profiles devices
 *        that execute compute workloads.
 *
 * Memory ownership:
 *   - The caller allocates and frees all caller-owned buffers.
 *   - All const char* pointers returned by PTI remain valid until PTI library
 *     teardown, even if reached through PTI-owned structures (like pti_pc_sampling_source_info_t)
 *     that are valid until ptiPcSamplingDisable.
 *   - PTI-managed per-device collection buffers remain valid until
 *     ptiPcSamplingDisable.
 *
 * Array count semantics:
 *   Many functions in this API follow a two-call pattern: first query the
 *   required output count, then call again with a caller-allocated buffer.
 *   The second call should ideally use the same count returned by the first
 *   call. If a different count is supplied:
 *   - If the supplied count is smaller than required, PTI fills only that many
 *     result entries.
 *   - If the supplied count is larger than required, PTI updates only the
 *     output count to the actual number of result entries.
 *
 */

typedef struct _pti_pc_sampling_handle_t* pti_pc_sampling_handle_t;

/**
 * @brief Stall reason name and description entry
 */
typedef struct _pti_pc_sampling_stall_reason_info_t {
  size_t  _struct_size;                           //!< Size of this structure; user settable; required
  const char*  _name;                             //!< Reason name; owned by PTI
  const char*  _description;                      //!< Reason description; owned by PTI
} pti_pc_sampling_stall_reason_info_t;

/**
 * @brief Source information for a single instruction.
 * Owned by PTI; valid until ptiPcSamplingDisable.
 */
typedef struct _pti_pc_sampling_source_info_t {
  const char*  _file_path;                        //!< Source file path; null if unavailable; owned by PTI
  uint64_t  _file_line;                           //!< Source line number; 0 if unavailable
} pti_pc_sampling_source_info_t;

/**
 * @brief Per-instruction sampling metrics
 */
typedef struct _pti_pc_sampling_instruction_t {
  uint64_t  _instruction_offset;                  //!< Instruction offset within kernel
  pti_pc_sampling_source_info_t*  _source_info;   //!< Source information; null if unavailable; owned by PTI
} pti_pc_sampling_instruction_t;

/**
 * @brief Kernel information returned by ptiPcSamplingGetObservedKernelInfo
 */
typedef struct _pti_pc_sampling_kernel_info_t {
  size_t  _struct_size;                           //!< Size of this structure; user settable; required
  pti_device_handle_t  _device;                   //!< Device on which the kernel ran
  uint64_t  _kernel_handle;                       //!< Unique kernel identifier
  const char*  _kernel_name;                      //!< Kernel name; owned by PTI
  size_t  _reason_count;                          //!< Number of stall reasons with samples for this kernel
  size_t  _instructions_with_samples_count;       //!< Count of instructions that have samples for this kernel
  uint64_t* _aggregated_samples;                  //!< Array of size _reason_count aggregating stall-sample data for this kernel
} pti_pc_sampling_kernel_info_t;

/**
 * @brief Per-device PC Sampling collection status
 */
typedef struct _pti_pc_sampling_device_status_t {
  size_t  _struct_size;                           //!< Size of this structure; user settable; required
  pti_device_handle_t  _device;                   //!< Device for which the status is reported
  uint32_t  _samples_dropped;                     //!< 1 if samples were dropped for the device; 0 otherwise
  uint64_t  _total_sample_count;                  //!< Total number of collected samples for the device
  uint64_t  _total_pc_count;                      //!< Total number of sampled PCs for the device
} pti_pc_sampling_device_status_t;

/**
 * @brief Create a PC Sampling collection handle
 *
 * @param[out] handle            Pointer to store the created handle
 *
 * @return PTI_SUCCESS on successful handle creation
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL
 * @return PTI_ERROR_PC_SAMPLING_ALREADY_ENABLED if another PC sampling handle is still enabled
 * @return PTI_ERROR_INTERNAL if PTI cannot allocate the collection handle
 * @return PTI_ERROR_PC_SAMPLING_UNSUPPORTED if the PC Sampling is not supported on the current system
 */
pti_result PTI_EXPORT
ptiPcSamplingEnable(pti_pc_sampling_handle_t* handle);

/**
 * @brief Configure PC Sampling for a collection handle
 * 
 * @param[in] handle               Collection handle
 * @param[in] devices              Device filter; NULL profiles all available devices
 * @param[in] device_count         Number of entries in devices; ignored when devices is NULL
 * @param[in] sampling_period_ns   Sampling period in nanoseconds for all selected devices; 0 makes PTI use the default period of 100000 ns
 *
 * @return PTI_SUCCESS on successful configuration
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL
 * @return PTI_ERROR_PC_SAMPLING_ALREADY_CONFIGURED if handle is already configured or otherwise not in the initial state required for configuration
 * @return PTI_ERROR_NOT_IMPLEMENTED if a device-filtered configuration is requested
 */
pti_result PTI_EXPORT
ptiPcSamplingConfigure(pti_pc_sampling_handle_t handle,
                       const pti_device_handle_t* devices,
                       size_t device_count,
                       uint32_t sampling_period_ns);

/**
 * @brief Query the recommended PC Sampling collection buffer size
 *
 * @param[in]  handle          Collection handle
 * @param[out] buffer_size     Pointer to store the collection buffer size in bytes; cannot be NULL
 *
 * @return PTI_ERROR_NOT_IMPLEMENTED
 */
pti_result PTI_EXPORT
ptiPcSamplingQueryCollectionBufferSize(pti_pc_sampling_handle_t handle,
                                       size_t* buffer_size);

/**
 * @brief Set the PC Sampling collection buffer size
 *
 * @param[in] handle          Collection handle
 * @param[in] buffer_size     Collection buffer size in bytes for all configured devices
 *
 * @return PTI_ERROR_NOT_IMPLEMENTED
 */
pti_result PTI_EXPORT
ptiPcSamplingSetCollectionBufferSize(pti_pc_sampling_handle_t handle,
                                     size_t buffer_size);

/**
 * @brief Start PC Sampling collection
 *
 * @param[in] handle             Collection handle
 *
 * @return PTI_SUCCESS when collection is started
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL
 * @return PTI_ERROR_PC_SAMPLING_NOT_CONFIGURED if collection is not in the configured state yet
 * @return PTI_ERROR_PC_SAMPLING_ALREADY_STARTED if collection is already running
 * @return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED if collection was already stopped
 */
pti_result PTI_EXPORT
ptiPcSamplingStartCollection(pti_pc_sampling_handle_t handle);

/**
 * @brief Stop collection
 *
 * Ends the sampling session and makes post-stop query APIs valid. Raw data
 * loading and aggregation are deferred until a later query requests them.
 *
 * @param[in] handle             Collection handle
 *
 * @return PTI_SUCCESS when collection is successfully stopped
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL
 * @return PTI_ERROR_PC_SAMPLING_NOT_STARTED if collection has not started yet
 * @return PTI_ERROR_PC_SAMPLING_ALREADY_STOPPED if collection was already stopped
 */
pti_result PTI_EXPORT
ptiPcSamplingStopCollection(pti_pc_sampling_handle_t handle);

/**
 * @brief Retrieve reason names and descriptions common across all profiled devices
 *
 * This query is independent of collection lifecycle and may be called immediately after
 * ptiPcSamplingEnable succeeds.
 * 
 * Usage: 1- Call ptiPcSamplingGetStallReasons(handle, NULL, reason_count) to discover the required count; the required count will be written to reason_count.
 *        2- Allocate reasons buffer of size sizeof(pti_pc_sampling_stall_reason_info_t) * (*reason_count) and set _struct_size on each element.
 *        3- Call ptiPcSamplingGetStallReasons(handle, reasons, reason_count) again to get the reason information written to the supplied buffer.
 *
 * @param[in]     handle         Collection handle
 * @param[in,out] reasons        Caller-allocated array of *reason_count elements; set to NULL to query the required count
 * @param[in,out] reason_count   In: size of reasons array; Out: required or actual number of reasons; cannot be NULL
 *
 * @return PTI_SUCCESS after successful retrieval of the stall-reason count or entries
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL or reason_count is NULL
 * @return PTI_ERROR_INTERNAL if stall-reason metadata cannot be derived from the configured metric group
 */
pti_result PTI_EXPORT
ptiPcSamplingGetStallReasons(pti_pc_sampling_handle_t handle,
                             pti_pc_sampling_stall_reason_info_t* reasons,
                             size_t* reason_count);

/**
 * @brief Get profiled devices with observed kernels
 *
 * Usage: 1- Call ptiPcSamplingGetProfiledDevices(handle, NULL, device_count) to discover the required count; the required count will be written to device_count.
 *        2- Allocate devices buffer of size sizeof(pti_device_handle_t) * (*device_count).
 *        3- Call ptiPcSamplingGetProfiledDevices(handle, devices, device_count) again to get the profiled device handles.
 *
 * Note that this function will return all devices that could have samples.
 * If there were kernels visible to PTI but no samples were collected for them (e.g. due to short execution time),
 * those devices will still be returned by this function.
 *
 * @param[in]     handle         Collection handle
 * @param[in,out] devices        Caller-allocated array of *device_count elements; set to NULL to query the required count
 * @param[in,out] device_count   In: size of devices array; Out: required or actual number of devices; cannot be NULL
 *
 * @return PTI_SUCCESS after successful retrieval of profiled device information for a stopped collection
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL or device_count is NULL
 * @return PTI_ERROR_PC_SAMPLING_NOT_STOPPED if collection has not reached the stopped state yet
 */
pti_result PTI_EXPORT
ptiPcSamplingGetProfiledDevices(pti_pc_sampling_handle_t handle,
                                pti_device_handle_t* devices,
                                size_t* device_count);

/**
 * @brief Retrieve observed kernel handles for a profiled device
 *
 * Usage: 1- Call ptiPcSamplingGetObservedKernelHandles(handle, device, NULL, kernel_count) to discover the required number of kernel handles.
 *        2- Allocate kernel_handles buffer of size sizeof(uint64_t) * (*kernel_count).
 *        3- Call ptiPcSamplingGetObservedKernelHandles(handle, device, kernel_handles, kernel_count) again to get the kernel handles written to the supplied buffer.
 *
 * @param[in]     handle           Collection handle
 * @param[in]     device           Profiled device handle
 * @param[in,out] kernel_handles   Caller-allocated array of *kernel_count elements; set to NULL to query the required count
 * @param[in,out] kernel_count     In: size of kernel_handles array; Out: required or actual number of kernel handles; cannot be NULL
 *
 * @return PTI_SUCCESS after successful retrieval of the kernel-handle count or entries
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL, kernel_count is NULL, or device does not match the configured device
 * @return PTI_ERROR_PC_SAMPLING_NOT_STOPPED if collection has not reached the stopped state yet
 * @return PTI_ERROR_INTERNAL if cannot collect the kernel-handle count or entries
 */
pti_result PTI_EXPORT
ptiPcSamplingGetObservedKernelHandles(pti_pc_sampling_handle_t handle,
                                      pti_device_handle_t device,
                                      uint64_t* kernel_handles,
                                      size_t* kernel_count);

/**
 * @brief Retrieve kernel metadata for one observed kernel
 *
 * Caller allocates kernel_info, must set _struct_size, and allocates an
 * _aggregated_samples array of size reason_count from ptiPcSamplingGetStallReasons.
 * If _aggregated_samples is NULL, it is ignored and not populated.
 *
 * @param[in]     handle          Collection handle
 * @param[in]     device          Profiled device handle
 * @param[in]     kernel_handle   Kernel handle returned by ptiPcSamplingGetObservedKernelHandles
 * @param[in,out] kernel_info     Caller-allocated kernel info structure;
 *
 * @return PTI_SUCCESS after successful retrieval of the observed-kernel metadata
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL, device does not match the configured device, kernel_info is NULL, or kernel_info->_struct_size is too small
 * @return PTI_ERROR_PC_SAMPLING_NOT_STOPPED if collection has not reached the stopped state yet
 * @return PTI_ERROR_BAD_ARGUMENT if kernel_handle was not observed during the collection
 * @return PTI_ERROR_INTERNAL if cannot collect the observed-kernel metadata
 */
pti_result PTI_EXPORT
ptiPcSamplingGetObservedKernelInfo(pti_pc_sampling_handle_t handle,
                                   pti_device_handle_t device,
                                   uint64_t kernel_handle,
                                   pti_pc_sampling_kernel_info_t* kernel_info);

/**
 * @brief Retrieve instruction-level PC Sampling data for one observed kernel on a profiled device
 *
 * Called after ptiPcSamplingGetObservedKernelInfo. The caller allocates:
 *   - instruction_buffer for kernel_info._instructions_with_samples_count elements
 *   - samples_buffer for kernel_info._instructions_with_samples_count * kernel_info._reason_count elements
 *
 * The caller can access the sample count for instruction i and stall reason j with:
 * samples_buffer[i * kernel_info._reason_count + j]
 *
 * @param[in]     handle                   Collection handle
 * @param[in]     device                   Profiled device handle
 * @param[in]     kernel_handle            Kernel handle returned by ptiPcSamplingGetObservedKernelHandles
 * @param[out]    instruction_buffer       Caller-allocated instruction buffer
 * @param[in]     instruction_buffer_count Size of instruction_buffer array
 * @param[out]    samples_buffer           Caller-allocated flattened sample-count buffer
 * @param[in]     samples_buffer_count     Size of samples_buffer array
 *
 * @return PTI_SUCCESS after successful retrieval of the persisted instruction offsets and sample counts
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL, device is NULL, samples_buffer is NULL, instruction_buffer is NULL,
 *                                or device does not match the configured device
 * @return PTI_ERROR_PC_SAMPLING_NOT_STOPPED if collection has not reached the stopped state yet
 * @return PTI_ERROR_INTERNAL if cannot collect the persisted instruction offsets and sample counts
 */
pti_result PTI_EXPORT
ptiPcSamplingGetSamplesPerInstruction(pti_pc_sampling_handle_t handle,
                                      pti_device_handle_t device,
                                      uint64_t kernel_handle,
                                      pti_pc_sampling_instruction_t* instruction_buffer,
                                      size_t instruction_buffer_count,
                                      uint64_t* samples_buffer,
                                      size_t samples_buffer_count);

/**
 * @brief Retrieve PC Sampling collection status for a profiled device.
 *
 * The returned status reports whether samples were dropped and provides
 * per-device totals for collected samples and sampled PCs.
 *
 * @param[in]     handle            Collection handle
 * @param[in]     device            Profiled device handle
 * @param[in,out] device_status     Caller-allocated device status structure; set _struct_size
 *
 * @return PTI_SUCCESS after successful status retrieval
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL, device is NULL, device_status is NULL,
 *                                or device does not match the configured PC sampling device
 * @return PTI_ERROR_PC_SAMPLING_NOT_STOPPED if collection has not reached the stopped state yet
 * @return PTI_ERROR_INTERNAL if cannot collect device status
 */
pti_result PTI_EXPORT
ptiPcSamplingGetDeviceStatus(pti_pc_sampling_handle_t handle,
                             pti_device_handle_t device,
                             pti_pc_sampling_device_status_t* device_status);

/**
 * @brief Disable collection and free PTI resources associated with this handle
 *
 * @param[in] handle    Collection handle
 *
 * @return PTI_SUCCESS on successful cleanup
 * @return PTI_ERROR_BAD_ARGUMENT if handle is NULL
 */
pti_result PTI_EXPORT
ptiPcSamplingDisable(pti_pc_sampling_handle_t handle);

#if defined(__cplusplus)
}
#endif

#endif  // PTI_PC_SAMPLING_H_
