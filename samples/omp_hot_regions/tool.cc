//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <chrono>
#include <stack>

#include <omp-tools.h>

#include "omp_region_collector.h"

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
static thread_local std::stack<TimePoint> time_point;

static OmpRegionCollector* collector = nullptr;
static std::chrono::steady_clock::time_point start;

// Internal Tool Functionality ////////////////////////////////////////////////

static void PushTimestamp() {
  time_point.push(std::chrono::steady_clock::now());
}

static uint64_t PopTimestamp() {
  TimePoint end = std::chrono::steady_clock::now();

  PTI_ASSERT(time_point.size() > 0);
  TimePoint start = time_point.top();
  time_point.pop();

  std::chrono::duration<uint64_t, std::nano> time = end - start;
  return time.count();
}

static void ParallelBegin(
    ompt_data_t* task_data, const ompt_frame_t* task_frame,
    ompt_data_t* parallel_data, unsigned int requested_parallelism,
    int flags, const void* codeptr_ra) {
  PushTimestamp();
}

static void ParallelEnd(
    ompt_data_t* parallel_data, ompt_data_t* task_data,
    int flags, const void* codeptr_ra) {
  uint64_t time = PopTimestamp();
  PTI_ASSERT(collector != nullptr);
  collector->AddRegion(
      reinterpret_cast<uint64_t>(codeptr_ra),
      REGION_TYPE_PARALLEL, time, 0);
}

static void Target(
    ompt_target_t kind, ompt_scope_endpoint_t endpoint,
    int device_num, ompt_data_t* task_data,
    ompt_id_t target_id, const void* codeptr_ra) {
  if (kind != ompt_target) {
    return;
  }

  if (endpoint == ompt_scope_begin) {
    PushTimestamp();
  } else {
    uint64_t time = PopTimestamp();
    PTI_ASSERT(collector != nullptr);
    collector->AddRegion(
        reinterpret_cast<uint64_t>(codeptr_ra),
        REGION_TYPE_TARGET, time, 0);
  }
}

static void TargetDataOp(
    ompt_scope_endpoint_t endpoint, ompt_id_t target_id,
    ompt_id_t host_op_id, ompt_target_data_op_t optype,
    void *src_addr, int src_device_num,
    void *dest_addr, int dest_device_num,
    size_t bytes, const void *codeptr_ra) {
  if (optype == ompt_target_data_transfer_to_device ||
      optype == ompt_target_data_transfer_from_device) {
    if (endpoint == ompt_scope_begin) {
      PushTimestamp();
    } else {
      uint64_t time = PopTimestamp();
      PTI_ASSERT(collector != nullptr);
      if (optype == ompt_target_data_transfer_to_device) {
        collector->AddRegion(
            reinterpret_cast<uint64_t>(codeptr_ra),
            REGION_TYPE_TRANSFER_TO_DEVICE, time, bytes);
      } else if (optype == ompt_target_data_transfer_from_device) {
        collector->AddRegion(
            reinterpret_cast<uint64_t>(codeptr_ra),
            REGION_TYPE_TRANSFER_FROM_DEVICE, time, bytes);
      }
    }
  }
}

static void PrintResults() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  PTI_ASSERT(collector != nullptr);
  const RegionMap& region_map = collector->GetRegionMap();
  if (region_map.size() == 0) {
    return;
  }

  uint64_t total_duration = 0;
  for (auto& value : region_map) {
    total_duration += value.second.total_time;
  }

  std::cerr << std::endl;
  std::cerr << "=== OpenMP Timing Results: ===" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Total Execution Time (ns): " << time.count() << std::endl;
  std::cerr << "Total Region Time (ns): " << total_duration << std::endl;
  std::cerr << std::endl;

  if (total_duration > 0) {
    OmpRegionCollector::PrintRegionTable(region_map);
  }

  std::cerr << std::endl;
}

static int Initialize(
    ompt_function_lookup_t lookup,
    int initial_device_num,
    ompt_data_t* data) {
  ompt_set_callback_t ompt_set_callback =
    reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
  if (ompt_set_callback == nullptr) {
    std::cerr << "[WARNING] Unable to create OpenMP region collector" <<
      std::endl;
    return 0;
  }

  ompt_set_result_t result = ompt_set_error;

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

  PTI_ASSERT(collector == nullptr);

  collector = OmpRegionCollector::Create();
  PTI_ASSERT(collector != nullptr);
  start = std::chrono::steady_clock::now();

  return 1;
}

static void Finalize(ompt_data_t* data) {
  if (data->ptr != nullptr) {
    ompt_start_tool_result_t* result =
      static_cast<ompt_start_tool_result_t*>(data->ptr);
    delete result;
  }

  if (collector != nullptr) {
    PrintResults();
    delete collector;
  }
}

// Internal Tool Interface ////////////////////////////////////////////////////

ompt_start_tool_result_t* ompt_start_tool(
    unsigned int omp_version, const char* runtime_version) {
  std::cerr << "[INFO] OMP Runtime Version: " << runtime_version << std::endl;

  ompt_start_tool_result_t* result = new ompt_start_tool_result_t;
  result->initialize = Initialize;
  result->finalize = Finalize;
  result->tool_data.ptr = result;

  return result;
}