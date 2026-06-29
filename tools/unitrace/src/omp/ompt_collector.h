//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_OMPT_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_OMPT_COLLECTOR_H_

#include <cstdint>
#include <stack>
#include <string>
#include <omp-tools.h>
#include "unievent.h"

#define UNI_OMPT_CHECK_COLLECTOR() do { if (!ompt_collector) return; } while (0)

#define UNI_OMPT_FOREACH_ENTRY(macro)                                          \
  macro(ompt_set_callback)              macro(ompt_get_callback)               \
  macro(ompt_get_state)                 macro(ompt_get_task_info)              \
  macro(ompt_get_task_memory)           macro(ompt_get_thread_data)            \
  macro(ompt_get_parallel_info)         macro(ompt_get_unique_id)              \
  macro(ompt_finalize_tool)             macro(ompt_get_num_procs)              \
  macro(ompt_get_num_places)            macro(ompt_get_place_proc_ids)         \
  macro(ompt_get_place_num)             macro(ompt_get_partition_place_nums)   \
  macro(ompt_get_proc_id)               macro(ompt_enumerate_states)           \
  macro(ompt_enumerate_mutex_impls)

// OMPT routines initialized by the look-up function.
// Use these routines to access more information maintained by the runtime.
#define UNI_OMPT_ENTRY_DECL(name) static name##_t name;
UNI_OMPT_FOREACH_ENTRY(UNI_OMPT_ENTRY_DECL)

// Initialize OMPT entries that can be used to query OpenMP information
static void init_ompt_entries(ompt_function_lookup_t lookup) {
  #define UNI_OMPT_ENTRY_SET(name)                                             \
    name = reinterpret_cast<name##_t>(lookup(#name));
  UNI_OMPT_FOREACH_ENTRY(UNI_OMPT_ENTRY_SET)
}

// Event offset or ID used to map an event ID to its corresponding string.
enum class OmptEventID {
  // Offset and flag value can be used to identify an individual event for 0--87
  SyncRegion = 0,
  Work = 16,
  MutexHold = 32,
  MutexWait = 40,
  MutexInit = 48,
  MutexDestroy = 56,
  Dispatch = 64,
  Cancel = 72,
  // Individual names are required starting from 88
  // Other events
  ParallelRegion = 88,
  TeamsRegion,
  ImplicitTask,
  NestLockOwned,
  Masked,
  Flush,
  TaskGeneration,
  TaskRegion,
  TaskDependence,
};

static const char *GetOmptEventName(uint32_t event_id) {
  // Maintain ID-to-string map for all OMP events
  static const char *all_ompt_events[] = {
    // OmptEventID::SyncRegion + 0
    "",
    "OMP::sync_region_barrier",
    "OMP::sync_region_barrier_implicit",
    "OMP::sync_region_barrier_explicit",
    "OMP::sync_region_barrier_implementation",
    "OMP::sync_region_taskwait",
    "OMP::sync_region_taskgroup",
    "OMP::sync_region_reduction",
    "OMP::sync_region_barrier_implicit_workshare",
    "OMP::sync_region_barrier_implicit_parallel",
    "OMP::sync_region_barrier_teams",
    "", "", "", "", "",
    // OmptEventID::Work + 0
    "",
    "OMP::work_loop",
    "OMP::work_sections",
    "OMP::work_single_executor",
    "OMP::work_single_other",
    "OMP::work_workshare",
    "OMP::work_distribute",
    "OMP::work_taskloop",
    "OMP::work_scope",
    "", // + 9
    "OMP::work_loop_static",
    "OMP::work_loop_dynamic",
    "OMP::work_loop_guided",
    "OMP::work_loop_other",
    "", "",
    // OmptEventID::MutexHold + 0
    "",
    "OMP::lock_hold",
    "OMP::test_lock_hold",
    "OMP::nest_lock_hold",
    "OMP::test_nest_lock_hold",
    "OMP::critical_hold",
    "OMP::atomic_hold",
    "OMP::ordered_hold",
    // OmptEventID::MutexWait + 0
    "",
    "OMP::lock_wait",
    "OMP::test_lock_wait",
    "OMP::nest_lock_wait",
    "OMP::test_nest_lock_wait",
    "OMP::critical_wait",
    "OMP::atomic_wait",
    "OMP::ordered_wait",
    // OmptEventID::MutexInit + 0
    "",
    "OMP::lock_init",
    "OMP::test_lock_init",
    "OMP::nest_lock_init",
    "OMP::test_nest_lock_init",
    "OMP::critical_init",
    "OMP::atomic_init",
    "OMP::ordered_init",
    // OmptEventID::MutexDestroy + 0
    "",
    "OMP::lock_destroy",
    "OMP::test_lock_destroy",
    "OMP::nest_lock_destroy",
    "OMP::test_nest_lock_destroy",
    "OMP::critical_destroy",
    "OMP::atomic_destroy",
    "OMP::ordered_destroy",
    // OmptEventID::Dispatch + 0
    "",
    "OMP::dispatch_iteration",
    "OMP::dispatch_section",
    "OMP::dispatch_ws_loop_chunk",
    "OMP::dispatch_task_loop_chunk",
    "OMP::dispatch_distribute_chunk",
    "", "",
    // OmptEventID::Cancel + 0
    "OMP::cancel_parallel_activated",
    "OMP::cancel_parallel_detected",
    "OMP::cancel_parallel_discarded_task",
    "OMP::cancel_parallel",
    "OMP::cancel_sections_activated",
    "OMP::cancel_sections_detected",
    "OMP::cancel_sections_discarded_task",
    "OMP::cancel_sections",
    "OMP::cancel_loop_activated",
    "OMP::cancel_loop_detected",
    "OMP::cancel_loop_discarded_task",
    "OMP::cancel_loop",
    "OMP::cancel_taskgroup_activated",
    "OMP::cancel_taskgroup_detected",
    "OMP::cancel_taskgroup_discarded_task",
    "OMP::cancel_taskgroup",
    // OmptEventID::ParallelRegion
    "OMP::parallel_region",
    "OMP::teams_region",
    "OMP::implicit_task",
    "OMP::nest_lock_owned",
    "OMP::masked",
    "OMP::flush",
    "OMP::task_generation",
    "OMP::task_region",
    "OMP::task_dependence",
  };
  return all_ompt_events[event_id];
}

class OmptCollector {
  using OmptLoggingCallback =
      void (*)(uint64_t, uint64_t, uint64_t, EVENT_TYPE, OmpArgs *);
  /// Chrome logging callback
  OmptLoggingCallback callback_ = nullptr;
public:
  /// Create and return a new OMPT collector
  static OmptCollector *Create(OmptLoggingCallback callback = nullptr) {
    auto *collector = new OmptCollector(callback);
    if (!collector)
      std::cerr << "[WARNING] Unable to create OpenMP tracer" << std::endl;
    return collector;
  }
  /// Get thread-private stack of OMPT records
  static std::stack<ompt_record_ompt_t> &GetRecords() {
    static thread_local std::stack<ompt_record_ompt_t> records{};
    return records;
  }
  /// Construct with the specified logger callback
  OmptCollector(OmptLoggingCallback callback) : callback_(callback) {}
  /// Log the specified information
  void Log(uint64_t event_id, uint64_t start_ts, uint64_t end_ts,
           OmpArgs *args = nullptr) {
    if (callback_)
      callback_(event_id, start_ts, end_ts, EVENT_COMPLETE, args);
  }
  /// Log with start time and current time, and pop the current record.
  void Log(uint64_t event_id, uint64_t start_ts, OmpArgs *args = nullptr) {
    if (callback_) {
      callback_(event_id, start_ts, UniTimer::GetHostTimestamp(),
                EVENT_COMPLETE, args);
      GetRecords().pop();
    }
  }
  /// Mark an event without endpoints
  void Mark(uint64_t event_id, OmpArgs *args = nullptr) {
    if (callback_) {
      uint64_t now = UniTimer::GetHostTimestamp();
      callback_(event_id, now, now, EVENT_MARK, args);
    }
  }
  /// Push the OMPT record to the thread-private stack
  void Push(ompt_record_ompt_t &record) { GetRecords().push(record); }
  /// Pop the OMPT record from the stack
  void Pop() { GetRecords().pop(); }
  /// Read the most recent OMPT record pushed to the stack
  ompt_record_ompt_t &Top() { return GetRecords().top(); }
};

OmptCollector *ompt_collector = nullptr;

// Callback for parallel region begin
static void on_ompt_callback_parallel_begin(
    // ompt_data_t arguments represent data reserved for tool use. Tool can use
    // this place to attach information to the specific OpenMP instance (e.g.,
    // parallel region, task, thread).
    // tool data attached to the encountering task
    ompt_data_t *encountering_task_data,
    // procedure frame information for the encountering task
    const ompt_frame_t *encountering_task_frame,
    // tool data location to be used by the tool for the region
    ompt_data_t *parallel_data,
    // number of threads or teams requested by the user
    unsigned int requested_parallelism,
    // flag that indicates if the region is for a team (parallel region) or
    // a league (teams region)
    int flags,
    // return address of the routine that implements the region
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  ompt_record_ompt_t omptrec;
  omptrec.type = ompt_callback_parallel_begin;
  omptrec.time = UniTimer::GetHostTimestamp();
  auto &parbegin = omptrec.record.parallel_begin;
  parbegin.requested_parallelism = requested_parallelism;
  parbegin.flags = flags;
  parbegin.codeptr_ra = codeptr_ra;
  ompt_collector->Push(omptrec);
}

// Callback for parallel region end
static void on_ompt_callback_parallel_end(
    ompt_data_t *parallel_data,
    ompt_data_t *encountering_task_data,
    int flags,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto &omptrec = ompt_collector->Top();
  if (omptrec.type != ompt_callback_parallel_begin) {
    std::cerr << "[WARNING] Unexpected OMPT record\n";
    return;
  }
  auto &parbegin = omptrec.record.parallel_begin;
  bool is_team = parbegin.flags & ompt_parallel_team;
  auto event_id = static_cast<uint64_t>(is_team ? OmptEventID::ParallelRegion
                                                : OmptEventID::TeamsRegion);
  ompt_collector->Log(event_id, omptrec.time);
}

// Callback for synchronization regions
static void on_ompt_callback_sync_region(
    // type of the synchronizing resgion
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    // tool data from the enclosing parallel/teams region
    ompt_data_t *parallel_data,
    // tool data from the current task
    ompt_data_t *task_data,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  if (endpoint == ompt_scope_begin) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_sync_region;
    omptrec.time = UniTimer::GetHostTimestamp();
    auto &syncbegin = omptrec.record.sync_region;
    syncbegin.kind = kind;
    syncbegin.endpoint = endpoint;
    syncbegin.codeptr_ra = codeptr_ra;
    ompt_collector->Push(omptrec);
  } else {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_sync_region ||
        omptrec.record.sync_region.endpoint != ompt_scope_begin) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::SyncRegion) + kind;
    ompt_collector->Log(event_id, omptrec.time);
  }
}

// Callback for sync region due to reduction activity
static void on_ompt_callback_reduction(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra) {
  on_ompt_callback_sync_region(kind, endpoint, parallel_data, task_data,
                               codeptr_ra);
}

// Callback for work sharing constructs
static void on_ompt_callback_work(
    ompt_work_t work_type,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  if (endpoint == ompt_scope_begin) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_work;
    omptrec.time = UniTimer::GetHostTimestamp();
    auto &workbegin = omptrec.record.work;
    workbegin.work_type = work_type;
    workbegin.endpoint = endpoint;
    workbegin.count = count;
    workbegin.codeptr_ra = codeptr_ra;
    ompt_collector->Push(omptrec);
  } else {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_work ||
        omptrec.record.work.endpoint != ompt_scope_begin) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::Work) + work_type;
    ompt_collector->Log(event_id, omptrec.time);
  }
}

// Callback for mutex operations
static void on_ompt_callback_mutex_acquire(
    // type of the associated OpenMP activity
    ompt_mutex_t kind,
    // synchronization hint provided when the mutex was initialized
    unsigned int hint,
    // mutex implementation type
    unsigned int impl,
    // identifier for the associated mutex
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  ompt_record_ompt_t omptrec;
  omptrec.type = ompt_callback_mutex_acquire;
  omptrec.time = UniTimer::GetHostTimestamp();
  omptrec.record.mutex_acquire.kind = kind;
  omptrec.record.mutex_acquire.hint = hint;
  omptrec.record.mutex_acquire.impl = impl;
  omptrec.record.mutex_acquire.wait_id = wait_id;
  omptrec.record.mutex_acquire.codeptr_ra = codeptr_ra;
  ompt_collector->Push(omptrec);
}

static void on_ompt_callback_mutex_acquired(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto &omptrec = ompt_collector->Top();
  if (omptrec.type != ompt_callback_mutex_acquire) {
    std::cerr << "[WARNING] Unexpected OMPT record\n";
    return;
  }
  uint64_t now = UniTimer::GetHostTimestamp();
  auto event_id = static_cast<uint64_t>(OmptEventID::MutexWait) + kind;
  ompt_collector->Log(event_id, omptrec.time, now);
  // Reuse existing record while only updating the timestamp.
  omptrec.time = now;
}

static void on_ompt_callback_mutex_released(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto &omptrec = ompt_collector->Top();
  if (omptrec.type != ompt_callback_mutex_acquire) {
    std::cerr << "[WARNING] Unexpected OMPT record\n";
    return;
  }
  auto event_id = static_cast<uint64_t>(OmptEventID::MutexHold) + kind;
  ompt_collector->Log(event_id, omptrec.time);
}

static void on_ompt_callback_lock_init(
    ompt_mutex_t kind,
    unsigned int hint,
    unsigned int impl,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto event_id = static_cast<uint64_t>(OmptEventID::MutexInit) + kind;
  ompt_collector->Mark(event_id);
}

static void on_ompt_callback_lock_destroy(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto event_id = static_cast<uint64_t>(OmptEventID::MutexDestroy) + kind;
  ompt_collector->Mark(event_id);
}

static void on_ompt_callback_nest_lock(
    ompt_scope_endpoint_t endpoint,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  // Region for nest-lock-owned event
  if (endpoint == ompt_scope_begin) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_nest_lock;
    omptrec.time = UniTimer::GetHostTimestamp();
    omptrec.record.nest_lock.endpoint = endpoint;
    omptrec.record.nest_lock.wait_id = wait_id;
    omptrec.record.nest_lock.codeptr_ra = codeptr_ra;
  } else {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_nest_lock ||
        omptrec.record.nest_lock.endpoint != ompt_scope_begin) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::NestLockOwned);
    ompt_collector->Log(event_id, omptrec.time);
  }
}

// Callback for implicit task
static void on_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    // number of threads (parallel region) or number of teams (teams region)
    unsigned int actual_parallelism,
    // thread number or the team number of the current thread
    unsigned int index,
    // flag that indicates if the task is an initial task or implicit task
    int flags) {
  UNI_OMPT_CHECK_COLLECTOR();
  if (endpoint == ompt_scope_begin) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_implicit_task;
    omptrec.time = UniTimer::GetHostTimestamp();
    omptrec.record.implicit_task.endpoint = endpoint;
    omptrec.record.implicit_task.actual_parallelism = actual_parallelism;
    omptrec.record.implicit_task.index = index;
    omptrec.record.implicit_task.flags = flags;
    ompt_collector->Push(omptrec);
  } else {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_implicit_task ||
        omptrec.record.implicit_task.endpoint != ompt_scope_begin) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::ImplicitTask);
    OmpArgs args;
    args.ompt = omptrec;
    ompt_collector->Log(event_id, omptrec.time, &args);
  }
}

// Callback for masked region
static void on_ompt_callback_masked(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  if (endpoint == ompt_scope_begin) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_masked;
    omptrec.time = UniTimer::GetHostTimestamp();
    omptrec.record.masked.endpoint = endpoint;
    omptrec.record.masked.codeptr_ra = codeptr_ra;
    ompt_collector->Push(omptrec);
  } else {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_masked ||
        omptrec.record.masked.endpoint != ompt_scope_begin) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::Masked);
    ompt_collector->Log(event_id, omptrec.time);
  }
}

// Callback for task creation
static void on_ompt_callback_task_create(
    // tool data from the generating task
    ompt_data_t *encountering_task_data,
    // frame information from the generating task
    const ompt_frame_t *encountering_task_frame,
    // tool data location to be used by the tool for the generated task
    ompt_data_t *new_task_data,
    // task flags that contain the attributes of the generated task
    int flags,
    // flag that indicates if the new task has dependence information
    int has_dependences,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  OmpArgs args;
  args.ompt.type = ompt_callback_task_create;
  args.ompt.record.task_create.new_task_id = ompt_get_unique_id();
  new_task_data->value = args.ompt.record.task_create.new_task_id;
  auto event_id = static_cast<uint64_t>(OmptEventID::TaskGeneration);
  ompt_collector->Mark(event_id, &args);
}

// Callback for task schedule
static void on_ompt_callback_task_schedule(
  // tool data from the task that arrived at the task scheduling point
  ompt_data_t *prior_task_data,
  // status of the prior task
  ompt_task_status_t prior_task_status,
  // tool data from the task resumed at the task scheduling point
  ompt_data_t *next_task_data) {
  // For now, only trace task start/finish
  UNI_OMPT_CHECK_COLLECTOR();
  if (prior_task_status == ompt_task_switch) {
    ompt_record_ompt_t omptrec;
    omptrec.type = ompt_callback_task_schedule;
    omptrec.time = UniTimer::GetHostTimestamp();
    omptrec.record.task_schedule.prior_task_status = prior_task_status;
    ompt_collector->Push(omptrec);
  } else if (prior_task_status == ompt_task_complete) {
    auto &omptrec = ompt_collector->Top();
    if (omptrec.type != ompt_callback_task_schedule ||
        omptrec.record.task_schedule.prior_task_status != ompt_task_switch) {
      std::cerr << "[WARNING] Unexpected OMPT record\n";
      return;
    }
    auto event_id = static_cast<uint64_t>(OmptEventID::TaskRegion);
    ompt_collector->Log(event_id, omptrec.time);
  }
}

static void on_ompt_callback_dispatch(
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    ompt_dispatch_t kind,
    ompt_data_t instance) {
  UNI_OMPT_CHECK_COLLECTOR();
  OmpArgs args;
  args.ompt.type = ompt_callback_dispatch;
  args.ompt.record.dispatch.kind = kind;
  switch (kind) {
  case ompt_dispatch_iteration:
  case ompt_dispatch_section:
    // Logical iteration number or pointer to section
    args.ompt.record.dispatch.instance = instance;
    break;
  case ompt_dispatch_ws_loop_chunk:
  case ompt_dispatch_taskloop_chunk:
  case ompt_dispatch_distribute_chunk: {
    // Iteration space [<Start>:<Count>]
    auto *chunk = static_cast<ompt_dispatch_chunk_t *>(instance.ptr);
    args.data[0] = chunk->start;
    args.data[1] = chunk->iterations;
    args.ompt.record.dispatch.instance = ompt_data_none;
    break;
  }
  default:
    ; // Nothing to do
  }
  auto event_id = static_cast<uint64_t>(OmptEventID::Dispatch) + kind;
  ompt_collector->Mark(event_id, &args);
}

static void on_ompt_callback_sync_region_wait(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra) {
  // sync-region events and sync-region-wait events occur together in most
  // regions, so we may not obtain extra information from sync-region-wait.
}

static void on_ompt_callback_flush(
    ompt_data_t *thread_data,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  auto event_id = static_cast<uint64_t>(OmptEventID::Flush);
  ompt_collector->Mark(event_id);
}

static void on_ompt_callback_cancel(
    ompt_data_t *task_data,
    int flags,
    const void *codeptr_ra) {
  UNI_OMPT_CHECK_COLLECTOR();
  // ID to string mapping is hard-coded here.
  auto event_id = static_cast<uint64_t>(OmptEventID::Cancel);
  if (flags & ompt_cancel_parallel)
    ;
  else if (flags & ompt_cancel_sections)
    event_id += 4;
  else if (flags & ompt_cancel_loop)
    event_id += 8;
  else if (flags & ompt_cancel_taskgroup)
    event_id += 12;
  if (flags & ompt_cancel_activated)
    ;
  else if (flags & ompt_cancel_detected)
    event_id += 1;
  else if (flags & ompt_cancel_discarded_task)
    event_id += 2;
  else
    event_id += 3;
  ompt_collector->Mark(event_id);
}

static void on_ompt_callback_task_dependence(
    ompt_data_t *first_task_data,
    ompt_data_t *second_task_data) {
  UNI_OMPT_CHECK_COLLECTOR();
  OmpArgs args;
  args.ompt.type = ompt_callback_task_dependence;
  args.ompt.record.task_dependence.src_task_id = first_task_data->value;
  args.ompt.record.task_dependence.sink_task_id = second_task_data->value;
  auto event_id = static_cast<uint64_t>(OmptEventID::TaskDependence);
  ompt_collector->Mark(event_id, &args);
}

// OMPT initialization callback
static int ompt_initialize(ompt_function_lookup_t lookup,
                           int initial_device_num, ompt_data_t *tool_data) {
  init_ompt_entries(lookup);

  if (!ompt_set_callback) {
    std::cerr << "[ERROR] Failed to get ompt_set_callback\n";
    return 0;
  }

  // Register callbacks
#define UNI_OMPT_SET_CALLBACK(callback)                                        \
  ompt_set_callback(callback, (ompt_callback_t)on_##callback)
  UNI_OMPT_SET_CALLBACK(ompt_callback_parallel_begin);
  UNI_OMPT_SET_CALLBACK(ompt_callback_parallel_end);
  UNI_OMPT_SET_CALLBACK(ompt_callback_task_create);
  UNI_OMPT_SET_CALLBACK(ompt_callback_task_schedule);
  UNI_OMPT_SET_CALLBACK(ompt_callback_implicit_task);
  UNI_OMPT_SET_CALLBACK(ompt_callback_sync_region);
  UNI_OMPT_SET_CALLBACK(ompt_callback_mutex_acquire);
  UNI_OMPT_SET_CALLBACK(ompt_callback_mutex_acquired);
  UNI_OMPT_SET_CALLBACK(ompt_callback_mutex_released);
  UNI_OMPT_SET_CALLBACK(ompt_callback_work);
  UNI_OMPT_SET_CALLBACK(ompt_callback_masked);
  UNI_OMPT_SET_CALLBACK(ompt_callback_reduction);
  UNI_OMPT_SET_CALLBACK(ompt_callback_lock_init);
  UNI_OMPT_SET_CALLBACK(ompt_callback_lock_destroy);
  UNI_OMPT_SET_CALLBACK(ompt_callback_nest_lock);
  UNI_OMPT_SET_CALLBACK(ompt_callback_dispatch);
  UNI_OMPT_SET_CALLBACK(ompt_callback_flush);
  UNI_OMPT_SET_CALLBACK(ompt_callback_cancel);
  UNI_OMPT_SET_CALLBACK(ompt_callback_task_dependence);
#if 0
  /// Not enabled - rarely used, simple translation of user directive
  UNI_OMPT_SET_CALLBACK(ompt_callback_error);
  /// Not enabled - duplicate information of sync_region callback in most cases
  UNI_OMPT_SET_CALLBACK(ompt_callback_sync_region_wait);
  /// Not enabled - event from omp_control_tool routine call by users
  UNI_OMPT_SET_CALLBACK(ompt_callback_control_tool);
  /// Not enabled - event from task with depend information
  UNI_OMPT_SET_CALLBACK(ompt_callback_dependences);
  /// Not enabled - thread end occurs after unitrace finishes
  UNI_OMPT_SET_CALLBACK(ompt_callback_thread_begin);
  UNI_OMPT_SET_CALLBACK(ompt_callback_thread_end);
#endif /* 0 */

  return 1; // Success
}

// OMPT finalization callback
static void ompt_finalize(ompt_data_t *tool_data) {}

// OMPT tool registration
ompt_start_tool_result_t *ompt_start_tool(unsigned int omp_version,
                                          const char *runtime_version) {
  static ompt_start_tool_result_t result{ompt_initialize, ompt_finalize, {0}};
  return ompt_collector ? &result : nullptr;
}

#endif // PTI_TOOLS_UNITRACE_OMPT_COLLECTOR_H_
