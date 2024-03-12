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

struct ZeKernelCommandExecutionRecord {
  uint64_t sycl_node_id_;
  uint32_t sycl_invocation_id_;
  uint64_t sycl_task_begin_time_;
  uint64_t sycl_enqk_begin_time_;
  std::string source_file_name_;
  const char* sycl_function_name_ = nullptr;
  uint32_t source_line_number_;

  uint64_t kid_;
  uint32_t cid_;
  uint64_t tid_;
  uint64_t pid_;
  int32_t tile_;
  uint64_t append_time_;
  uint64_t submit_time_;
  uint64_t start_time_;
  uint64_t end_time_;
  ze_pci_ext_properties_t pci_prop_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;

  ze_command_queue_handle_t queue_;
  ze_device_handle_t device_;    // in case of memcpy -- represents source
  ze_context_handle_t context_;  // in case of memcpy -- represents source

  ze_device_handle_t dst_device_;  // in case of memcpy -- represents destination (nullptr else)
  ze_pci_ext_properties_t dst_pci_prop_;  // in case of memcpy -- represents destination

  uint8_t src_device_uuid[PTI_MAX_DEVICE_UUID_SIZE];
  uint8_t dst_device_uuid[PTI_MAX_DEVICE_UUID_SIZE];

  bool implicit_scaling_;
  std::string name_;
  const char* sycl_func_name_;
  size_t bytes_xfered_;
  size_t value_set_;
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

#endif  // PTI_TOOLS_UNITRACE_UNIKERNEL_H
