//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNIKERNEL_H
#define PTI_TOOLS_UNITRACE_UNIKERNEL_H

#include <iostream>

#include <level_zero/layers/zel_tracing_api.h>

class UniKernelId{
  public:
    static uint64_t GetKernelId(void) {
        return kernel_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
    }

  private:
    inline static std::atomic<uint64_t> kernel_id_ = 1;	//start with 1
};
    
class UniKernelInstanceId{
  public:
    static uint64_t GetKernelInstanceId(void) {
        return kernel_instance_id_.fetch_add(1, std::memory_order::memory_order_relaxed);
    }

  private:
    inline static std::atomic<uint64_t> kernel_instance_id_ = 1;	//start with 1
};
    
enum FLOW_DIR {
  FLOW_NUL = 0,
  FLOW_D2H = 1,
  FLOW_H2D = 2,
};

struct ZeKernelCommandExecutionRecord {
  uint64_t kid_;
  uint64_t tid_;
  uint64_t kernel_command_id_;
  uint64_t start_time_;
  uint64_t end_time_;
  ze_device_handle_t device_;
  size_t mem_size_;
  int32_t tile_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;
  ze_group_count_t group_count_;
  bool implicit_scaling_;
};

#endif // PTI_TOOLS_UNITRACE_UNIKERNEL_H
