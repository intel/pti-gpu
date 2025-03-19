//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNIKERNEL_H
#define PTI_TOOLS_UNITRACE_UNIKERNEL_H

#include <level_zero/layers/zel_tracing_api.h>

#include <atomic>
#include <cstring>
#include <map>
#include <stack>
#include <string>

#include "pti/pti_view.h"
#include "utils.h"

class UniCorrId {
 public:
  static uint32_t GetUniCorrId(void) {
    return unique_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
  }

 private:
  inline static std::atomic<uint32_t> unique_id_ = 1;  // start with 1
};

class UniKernelId {
 public:
  static uint64_t GetKernelId(void) {
    return kernel_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
  }

 private:
  inline static std::atomic<uint64_t> kernel_id_ = 1;  // start with 1
};

#define GET_MEMCPY_TYPE(SRC_TYPE, DST_TYPE, RESULT_TYPE)                   \
  if (src_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_##SRC_TYPE && \
      dst_type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_##DST_TYPE) { \
    return pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_##RESULT_TYPE;       \
  }

/**
 * \internal
 * Represets the route of memory copy/fill command
 */
struct ZeMemoryCommandRoute {
  pti_view_memory_type src_type;
  pti_view_memory_type dst_type;
  bool peer_2_peer;
  ZeMemoryCommandRoute()
      : src_type(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY),
        dst_type(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY),
        peer_2_peer(false) {}
  char GetChar(pti_view_memory_type type) const {
    return type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY   ? 'M'
           : type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST   ? 'H'
           : type == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE ? 'D'
                                                                       : 'S';
  }
  std::string StringifyTypesCompact() const {
    std::string str = "";
    str += GetChar(src_type) + std::string("2") + GetChar(dst_type);
    return str;
  }
  std::string StringifyPeer2PeerCompact() const {
    std::string str = "";
    str += peer_2_peer ? " - P2P" : std::string("");
    return str;
  }
  pti_view_memcpy_type GetMemcpyType() const {
    GET_MEMCPY_TYPE(MEMORY, MEMORY, M2M);
    GET_MEMCPY_TYPE(MEMORY, HOST, M2H);
    GET_MEMCPY_TYPE(MEMORY, DEVICE, M2D);
    GET_MEMCPY_TYPE(MEMORY, SHARED, M2S);

    GET_MEMCPY_TYPE(HOST, MEMORY, H2M);
    GET_MEMCPY_TYPE(HOST, HOST, H2H);
    GET_MEMCPY_TYPE(HOST, DEVICE, H2D);
    GET_MEMCPY_TYPE(HOST, SHARED, H2S);

    GET_MEMCPY_TYPE(DEVICE, HOST, D2H);
    GET_MEMCPY_TYPE(DEVICE, DEVICE, D2D);
    GET_MEMCPY_TYPE(DEVICE, SHARED, D2S);
    GET_MEMCPY_TYPE(DEVICE, MEMORY, D2M);

    GET_MEMCPY_TYPE(SHARED, DEVICE, S2D);
    GET_MEMCPY_TYPE(SHARED, SHARED, S2S);
    GET_MEMCPY_TYPE(SHARED, MEMORY, S2M);
    GET_MEMCPY_TYPE(SHARED, HOST, S2H);

    return pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2M;
  }
};

struct ZeKernelCommandExecutionRecord {
  uint64_t sycl_node_id_;
  uint64_t sycl_queue_id_ = PTI_INVALID_QUEUE_ID;
  uint32_t sycl_invocation_id_;
  uint64_t sycl_task_begin_time_;
  uint64_t sycl_enqk_begin_time_;
  std::string source_file_name_;
  const char* sycl_function_name_ = nullptr;
  uint32_t source_line_number_;

  uint64_t kid_;
  uint32_t cid_;
  uint32_t tid_;
  uint32_t pid_;
  int32_t tile_;
  uint64_t append_time_;
  uint64_t submit_time_;
  uint64_t start_time_;
  uint64_t end_time_;
  ze_pci_ext_properties_t pci_prop_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;

  ze_command_queue_handle_t queue_ = nullptr;
  ze_device_handle_t device_ = nullptr;    // in case of memcpy -- represents source
  ze_context_handle_t context_ = nullptr;  // in case of memcpy -- represents source
  ze_event_handle_t event_ = nullptr;      // event used for host synchronization.

  ZeMemoryCommandRoute route_;
  ze_device_handle_t dst_device_;  // in case of memcpy -- represents destination (nullptr else)
  ze_pci_ext_properties_t dst_pci_prop_;  // in case of memcpy -- represents destination

  uint8_t src_device_uuid[PTI_MAX_DEVICE_UUID_SIZE];
  uint8_t dst_device_uuid[PTI_MAX_DEVICE_UUID_SIZE];

  bool implicit_scaling_;
  std::string name_;
  const char* sycl_func_name_;
  size_t bytes_xfered_;
  size_t value_set_;

  uint32_t callback_id_;
  uint64_t api_start_time_;
  uint64_t api_end_time_;
  uint64_t num_wait_events_ = 0;  // tracks wait event count for synchronization activity commands
  ze_result_t result_;
};

// This structure and thread_local object enables collectors to avoid retrieving pid and tid
// multiple times. Especially taking into account that getting tid is syscall and thus expensive.
// Collectors just read this structure to get pid and tid.
struct PidTidInfo {
  uint32_t pid;
  uint32_t tid;
};

inline thread_local PidTidInfo thread_local_pid_tid_info = {utils::GetPid(), utils::GetTid()};

// clang-format off
// Below table highlights when we will emit special record.  Special records are in api records with hybrid api_groups
// PTI_VIEW_RUNTIME_API		PTI_VIEW_DEVICE_GPU_KERNEL	PTI_VIEW_DRIVER_API	Generate Special Hybrid Rec.
//------------------------------------------------------------------------------------------------------------------------
// on                                   on                              off                      if no sycl rec present:yes
// off                                  on                              on                       no
// on                                   on                              on                       no
// on                                   off                             on                       no
// off                                  off                             on                       no
// off                                  on                              off                      no
// on                                   off                             off                      no
// off                                  off                             off                      no
//------------------------------------------------------------------------------------------------------------------------
// clang-format on
struct SpecialCallsData {
  uint32_t sycl_rec_present = 0;  // sycl runtime rec is not present.
  bool zecall_disabled = true;    // zecalls disabled?
};

//
// TODO -- refactor 2nd level callbacks so that callbacks are not bound to
// ZeKernelCommandExecutionRecord but flexible to handle various smaller structures -- for now
// side-back smaller structres via void*data in the callbacks.
//
inline thread_local ZeKernelCommandExecutionRecord
    overhead_data;  // Placeholder till we refactor the 2nd level callbacks.

struct ExternalCorrIdKey {
  pti_view_external_kind _external_kind;
};

struct ExternalKeyCompare {
  bool operator()(const ExternalCorrIdKey& lhs, const ExternalCorrIdKey& rhs) const {
    return (std::memcmp((char*)(&lhs), (char*)(&rhs), sizeof(ExternalCorrIdKey)) < 0);
  }
};

struct OverheadKindKey {
  pti_view_overhead_kind _overhead_kind;
};

struct OverheadKeyCompare {
  bool operator()(const OverheadKindKey& lhs, const OverheadKindKey& rhs) const {
    return (std::memcmp((char*)(&lhs), (char*)(&rhs), sizeof(OverheadKindKey)) < 0);
  }
};

inline thread_local ZeKernelCommandExecutionRecord sycl_data_mview;
inline thread_local ZeKernelCommandExecutionRecord sycl_data_kview;
inline thread_local std::map<ExternalCorrIdKey, std::stack<pti_view_record_external_correlation>,
                             ExternalKeyCompare>
    map_ext_corrid_vectors;
inline thread_local std::map<OverheadKindKey, pti_view_record_overhead, OverheadKeyCompare>
    map_overhead_per_kind;

// Tracks on a per thread basis if a L0 view_kind has been activated/enabled.
inline thread_local std::map<pti_view_kind, bool> map_view_kind_enabled;
#endif  // PTI_TOOLS_UNITRACE_UNIKERNEL_H
