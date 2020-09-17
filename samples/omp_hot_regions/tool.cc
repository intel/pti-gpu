//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <stack>
#include <thread>

#include <omp-tools.h>

#include "pti_assert.h"

const char* kLine =
  "+---------------------------------------"
  "---------------------------------------+";
const char* kHeader =
  "| Region ID  |     Region Type    | Call"
  " Count | Avg Time, ms | Total Time, ms |";

enum RegionType {
  REGION_TYPE_PARALLEL = 0,
  REGION_TYPE_TARGET,
  REGION_TYPE_TRANSFER_TO_DEVICE,
  REGION_TYPE_TRANSFER_FROM_DEVICE
};

struct RegionInfo {
  float time;
  unsigned call_count;
  RegionType type;
};

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using RegionMap = std::map<uint64_t, RegionInfo>;

class ToolContext;
static ToolContext* context = nullptr;
static thread_local std::stack<TimePoint> time_point;

// Internal Tool Functionality ////////////////////////////////////////////////

class ToolContext {
 public:
  ToolContext() {}

  void UpdateRegionMap(uint64_t id, RegionInfo region) {
    const std::lock_guard<std::mutex> lock(lock_);

    if (region_map_.count(id) == 0) {
      region_map_[id] = region;
    } else {
      region_map_[id].time += region.time;
      region_map_[id].call_count += region.call_count;
      PTI_ASSERT(region_map_[id].type == region.type);
    }
  }

  const RegionMap& GetRegionMap() const {
    return region_map_;
  }

 private:
  RegionMap region_map_;
  std::mutex lock_;
};

static void PushTimestamp() {
  time_point.push(std::chrono::steady_clock::now());
}

static float PopTimestamp() {
  PTI_ASSERT(time_point.size() > 0);
  TimePoint start = time_point.top();
  time_point.pop();
  TimePoint end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;
  return time.count();
}

static void RegionBegin() {
  PushTimestamp();
}

static void RegionEnd(RegionType type, const void* codeptr_ra) {
  PTI_ASSERT(context != nullptr);
  float time = PopTimestamp();
  uint64_t id = reinterpret_cast<uint64_t>(codeptr_ra);
  id += type;
  context->UpdateRegionMap(id, {time, 1, type});
}

static void ParallelBegin(ompt_data_t* task_data,
                   const ompt_frame_t* task_frame,
                   ompt_data_t* parallel_data,
                   unsigned int requested_parallelism,
                   int flags,
                   const void* codeptr_ra) {
  RegionBegin();
}

static void ParallelEnd(ompt_data_t* parallel_data,
                 ompt_data_t* task_data,
                 int flags,
                 const void* codeptr_ra) {
  RegionEnd(REGION_TYPE_PARALLEL, codeptr_ra);
}

static void Target(ompt_target_t kind,
            ompt_scope_endpoint_t endpoint,
            int device_num,
            ompt_data_t* task_data,
            ompt_id_t target_id,
            const void* codeptr_ra) {
  if (kind != ompt_target) {
    return;
  }

  if (endpoint == ompt_scope_begin) {
    RegionBegin();
  } else {
    RegionEnd(REGION_TYPE_TARGET, codeptr_ra);
  }
}

static void TargetDataOp(ompt_scope_endpoint_t endpoint,
                  ompt_id_t target_id,
                  ompt_id_t host_op_id,
                  ompt_target_data_op_t optype,
                  void *src_addr,
                  int src_device_num,
                  void *dest_addr,
                  int dest_device_num,
                  size_t bytes,
                  const void *codeptr_ra) {
  if (optype == ompt_target_data_transfer_to_device ||
      optype == ompt_target_data_transfer_from_device) {
    if (endpoint == ompt_scope_begin) {
      RegionBegin();
    } else {
      if (optype == ompt_target_data_transfer_to_device) {
        RegionEnd(REGION_TYPE_TRANSFER_TO_DEVICE, codeptr_ra);
      } else if (optype == ompt_target_data_transfer_from_device) {
        RegionEnd(REGION_TYPE_TRANSFER_FROM_DEVICE, codeptr_ra);
      }
    }
  }
}

static int Initialize(ompt_function_lookup_t lookup,
               int initial_device_num,
               ompt_data_t* data) {
  ompt_set_callback_t ompt_set_callback =
    reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
  PTI_ASSERT(ompt_set_callback != nullptr);  

  ompt_set_result_t result = ompt_set_error;

  PTI_ASSERT(context == nullptr);
  context = new ToolContext;
  PTI_ASSERT(context != nullptr);

  result = ompt_set_callback(ompt_callback_parallel_begin,
    reinterpret_cast<ompt_callback_t>(ParallelBegin));
  PTI_ASSERT(result == ompt_set_always);
  result = ompt_set_callback(ompt_callback_parallel_end,
    reinterpret_cast<ompt_callback_t>(ParallelEnd));
  PTI_ASSERT(result == ompt_set_always);

  result = ompt_set_callback(ompt_callback_target,
    reinterpret_cast<ompt_callback_t>(Target));
  PTI_ASSERT(result == ompt_set_always);
  result = ompt_set_callback(ompt_callback_target_data_op,
    reinterpret_cast<ompt_callback_t>(TargetDataOp));
  PTI_ASSERT(result == ompt_set_always);

  return 1;
}

static const char* GetTypeString(RegionType type) {
  switch(type) {
    case REGION_TYPE_PARALLEL:
      return "Parallel";
    case REGION_TYPE_TARGET:
      return "Target";
    case REGION_TYPE_TRANSFER_FROM_DEVICE:
      return "TransferFromDevice";
    case REGION_TYPE_TRANSFER_TO_DEVICE:
      return "TransferToDevice";
    default:
      break;
  }
  return "";
}

static void PrintResults() {
  PTI_ASSERT(context != nullptr);

  const RegionMap& region_map = context->GetRegionMap();
  if (region_map.size() == 0) {
    return;
  }

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  for (auto region : region_map) {
    PTI_ASSERT(region.second.call_count > 0);
    std::cout << "| " << std::hex << std::setw(10) << region.first << " | " <<
      std::setw(18) << GetTypeString(region.second.type) << " | " <<
      std::setw(10) << std::dec << region.second.call_count << " | " <<
      std::setw(12) << std::setprecision(3) << std::fixed <<
      region.second.time / region.second.call_count << " | " <<
      std::setw(14) << region.second.time << " |" << std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

static void Finalize(ompt_data_t* data) {
  if (data->ptr != nullptr) {
    ompt_start_tool_result_t* result =
      static_cast<ompt_start_tool_result_t*>(data->ptr);
    delete result;
  }

  PrintResults();

  PTI_ASSERT(context != nullptr);
  delete context;
}

// Internal Tool Interface ////////////////////////////////////////////////////

ompt_start_tool_result_t* ompt_start_tool(
    unsigned int omp_version, const char* runtime_version) {
  std::cout << "[INFO] OMP Runtime Version: " << runtime_version << std::endl;

  ompt_start_tool_result_t* result = new ompt_start_tool_result_t;
  result->initialize = Initialize;
  result->finalize = Finalize;
  result->tool_data.ptr = result;

  return result;
}