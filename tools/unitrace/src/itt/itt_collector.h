//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_ITT_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_ITT_COLLECTOR_H_

#include <chrono>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <stack>
#include "unicontrol.h"
#include <tuple>

static std::string rank_mpi = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
typedef void (*OnIttLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts);

struct ittFunction {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ittFunction& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ittFunction& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};
using ittFunctionInfoMap = std::map<std::string, ittFunction>;

ittFunctionInfoMap ccl_function_info_map;
std::mutex lock_func_info;

void AddFunctionTime(const std::string& name, uint64_t time) {
  if (name.rfind("oneCCL::", 0) == 0) {
    const std::lock_guard<std::mutex> lock(lock_func_info);
    if (ccl_function_info_map.count(name) == 0) {
      ccl_function_info_map[name] = {time, time, time, 1};
    } else {
      ittFunction& function = ccl_function_info_map[name];
      function.total_time += time;
      if (time < function.min_time) {
        function.min_time = time;
      }
      if (time > function.max_time) {
        function.max_time = time;
      }
      ++function.call_count;
    }
  }
}

class IttCollector {
 public: // Interface

  static IttCollector *Create(OnIttLoggingCallback callback = nullptr) {
    IttCollector* collector = new IttCollector(callback);

    if (collector == nullptr) {
      std::cerr << "[WARNING] Unable to create ITT tracer" << std::endl;
    }

    return collector;
  }

  void EnableCclSummary() {is_itt_ccl_summary_ = true;}
  bool IsCclSummaryOn() { return is_itt_ccl_summary_; }

  void EnableChromeLogging() { is_itt_chrome_logging_on_ = true;}
  bool IsEnableChromeLoggingOn() { return is_itt_chrome_logging_on_; }

  ~IttCollector() {
  }

  IttCollector(const IttCollector& copy) = delete;
  IttCollector& operator=(const IttCollector& copy) = delete;

  void Log(const char *name, uint64_t start_ts, uint64_t end_ts) {
    if (callback_) {
      callback_(name, start_ts, end_ts);
    }
  }

  std::string CclSummaryReport() const {
    const uint32_t kFunctionLength = 10;
    const uint32_t kCallsLength = 12;
    const uint32_t kTimeLength = 20;
    const uint32_t kPercentLength = 12;

    std::set< std::pair<std::string, ittFunction>,
              utils::Comparator > sorted_list(
        ccl_function_info_map.begin(), ccl_function_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kFunctionLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    // return early if there is not API in the list
    if (total_duration == 0) {
      return "";
    }
    std::string str;
    str += "************************************************************\n";
    str += "*  Process ID : " + std::to_string(utils::GetPid()) + " | Rank ID : " + rank_mpi + "\n";
    str += "************************************************************\n";

    str += std::string(std::max(int(max_name_length - sizeof("Function") + 1), 0), ' ') + "Function, " +
      std::string(std::max(int(kCallsLength - sizeof("Calls") + 1), 0), ' ') + "Calls, " +
      std::string(std::max(int(kTimeLength - sizeof("Time (ns)") + 1), 0), ' ') + "Time (ns), " +
      std::string(std::max(int(kPercentLength - sizeof("Time (%)") + 1), 0), ' ') + "Time (%), " +
      std::string(std::max(int(kTimeLength - sizeof("Average (ns)") + 1), 0), ' ') + "Average (ns), " +
      std::string(std::max(int(kTimeLength - sizeof("Min (ns)") + 1), 0), ' ') + "Min (ns), " +
      std::string(std::max(int(kTimeLength - sizeof("Max (ns)") + 1), 0), ' ') + "Max (ns)\n";

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = (100.0f * duration / total_duration);
      str += std::string(std::max(int(max_name_length - function.length()), 0), ' ') + function + ", " +
              std::string(std::max(int(kCallsLength - std::to_string(call_count).length()), 0), ' ') + std::to_string(call_count) + ", " +
              std::string(std::max(int(kTimeLength - std::to_string(duration).length()), 0), ' ') + std::to_string(duration) + ", " +

              std::string(std::max(int(kPercentLength - std::to_string(percent_duration).length()), 0), ' ') +
              std::to_string(percent_duration) + ", " +
              std::string(std::max(int(kTimeLength - std::to_string(avg_duration).length()), 0), ' ') + std::to_string(avg_duration) + ", " +
              std::string(std::max(int(kTimeLength - std::to_string(min_duration).length()), 0), ' ') + std::to_string(min_duration) + ", " +
              std::string(std::max(int(kTimeLength - std::to_string(max_duration).length()), 0), ' ') + std::to_string(max_duration) + "\n";
    }
    return str;
  }

 private: // Implementation

  IttCollector(OnIttLoggingCallback callback) : callback_(callback) {
  }

 private: // Data
  OnIttLoggingCallback callback_ = nullptr;
  bool is_itt_ccl_summary_ = false;
  bool is_itt_chrome_logging_on_ = false;
};

static IttCollector *itt_collector = nullptr;

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE
#include "ittnotify.h"
#include "ittnotify_config.h"

struct ThreadTaskDescriptor {
  char domain[512];
  char name[512];
  uint64_t start_time;
};

thread_local std::stack<ThreadTaskDescriptor> task_desc;

thread_local std::map<__itt_event, uint64_t> event_desc;

static std::vector<std::string> itt_events;
static int num_itt_events = 0;

static __itt_global *itt_global = NULL;

static void fill_func_ptr_per_lib(__itt_global* p)
{
  __itt_api_info* api_list = (__itt_api_info*)p->api_list_ptr;

  for (int i = 0; api_list[i].name != NULL; i++) {
    *(api_list[i].func_ptr) = (void*)__itt_get_proc(p->lib, api_list[i].name);
    if (*(api_list[i].func_ptr) == NULL)
    {
      *(api_list[i].func_ptr) = api_list[i].null_func;
    }
  }
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* p, __itt_group_id init_groups)
{
  if (p != NULL) {
    fill_func_ptr_per_lib(p);
    itt_global = p;
  }
}

ITT_EXTERN_C __itt_domain* ITTAPI __itt_domain_create(const char *name)
{
  if (itt_global == NULL) {
    return NULL;
  }

  __itt_domain *h_tail = NULL, *h = NULL;

  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = NULL, h = itt_global->domain_list; h != NULL; h_tail = h, h = h->next) {
    if (h->nameA != NULL && !__itt_fstrcmp(h->nameA, name)) break;
  }
  if (h == NULL) {
    NEW_DOMAIN_A(itt_global, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global->mutex));

  return h;
}

ITT_EXTERN_C __itt_string_handle* ITTAPI __itt_string_handle_create(const char* name)
{
  if (itt_global == NULL) {
    return NULL;
  }
  __itt_string_handle *h_tail = NULL, *h = NULL;

  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = NULL, h = itt_global->string_list; h != NULL; h_tail = h, h = h->next) {
    if (h->strA != NULL && !__itt_fstrcmp(h->strA, name)) break;
  }
  if (h == NULL) {
    NEW_STRING_HANDLE_A(itt_global, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global->mutex));

  return h;
}

ITT_EXTERN_C void ITTAPI __itt_pause(void)
{
  UniController::IttPause();
}

ITT_EXTERN_C void ITTAPI __itt_pause_scoped(__itt_collection_scope scope)
{
}

ITT_EXTERN_C void ITTAPI __itt_resume(void)
{
  UniController::IttResume();
}

ITT_EXTERN_C void ITTAPI __itt_resume_scoped(__itt_collection_scope scope)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin(const __itt_domain *domain, __itt_id taskid, __itt_id parentid, __itt_string_handle *name) {
  if (!UniController::IsCollectionEnabled()) {
    return;
  }

  if (!itt_collector->IsCclSummaryOn() && !itt_collector->IsEnableChromeLoggingOn()) {
    return;
  }

  ThreadTaskDescriptor desc;
  if (domain && domain->nameA) {
    strncpy(desc.domain, domain->nameA, sizeof(desc.domain) - 2);
  }
  else {
    desc.domain[0] = 0;
  }
  if (name && name->strA) {
    strncpy(desc.name, name->strA, sizeof(desc.name) - 2);
  }
  else {
    desc.name[0] = 0;
  }

  desc.start_time = UniTimer::GetHostTimestamp();
  task_desc.push(desc);
}

ITT_EXTERN_C void ITTAPI __itt_task_end(const __itt_domain *domain)
{
  if (!UniController::IsCollectionEnabled()) {
    return;
  }

  if (!itt_collector->IsCclSummaryOn() && !itt_collector->IsEnableChromeLoggingOn()) {
    return;
  }

  if (!task_desc.empty() && !strcmp(task_desc.top().domain, domain->nameA)) {
    char task[1057];

    snprintf(task, 1056, "%s::%s", task_desc.top().domain, task_desc.top().name);

    std::string name = task_desc.top().domain;
    name+="::";
    name+=task_desc.top().name;
    auto start = task_desc.top().start_time;
    auto end = UniTimer::GetHostTimestamp();

    if (itt_collector->IsCclSummaryOn()) {
      AddFunctionTime(name, end-start);
    }
    if (itt_collector->IsEnableChromeLoggingOn()) {
      itt_collector->Log(task, start, end);
    }
    task_desc.pop();
  }
}

ITT_EXTERN_C __itt_event ITTAPI __itt_event_create(const char *name, int namelen)
{
  if (itt_global == NULL) {
    return -1;
  }

  __itt_mutex_lock(&(itt_global->mutex));
  itt_events.push_back(std::string(name, namelen));
  num_itt_events = itt_events.size();
  int i = num_itt_events - 1;
  __itt_mutex_unlock(&(itt_global->mutex));

  return (__itt_event)i;
}

ITT_EXTERN_C int ITTAPI __itt_event_start(__itt_event event) {
  if (!UniController::IsCollectionEnabled()) {
    return __itt_error_success;
  }
  
  if (!itt_collector->IsCclSummaryOn() && !itt_collector->IsEnableChromeLoggingOn()) {
    return __itt_error_success;
  }

  if ((event < 0) || (event >= num_itt_events)) { // let it race
    return __itt_error_no_symbol;	// which error code to return?
  }

  uint64_t start = UniTimer::GetHostTimestamp();
  event_desc.insert({event, start});

  return __itt_error_success;
}

ITT_EXTERN_C int ITTAPI __itt_event_end(__itt_event event) {
  if (!UniController::IsCollectionEnabled()) {
    return __itt_error_success;
  }

  if (!itt_collector->IsCclSummaryOn() && !itt_collector->IsEnableChromeLoggingOn()) {
    return __itt_error_success;
  }
  if ((event < 0) || (event >= num_itt_events)) { // let it race
    return __itt_error_no_symbol;	// which error code to return?
  }

  auto it = event_desc.find(event);
  if (it != event_desc.end()) {
    uint64_t start = it->second;
    event_desc.erase(it);
    // trust the event has already been created
    //__itt_mutex_lock(&(itt_global->mutex));

    auto end = UniTimer::GetHostTimestamp();
    if (itt_collector->IsCclSummaryOn()) {
      AddFunctionTime(itt_events[event], end-start);
    }
    if (itt_collector->IsEnableChromeLoggingOn()) {
      itt_collector->Log(itt_events[event].c_str(), start, end);
    }
    //__itt_mutex_unlock(&(itt_global->mutex));
    return __itt_error_success;
  }

  return __itt_error_no_symbol;		// which error code to return?
}


ITT_EXTERN_C void ITTAPI __itt_marker(const __itt_domain *domain, __itt_id id, __itt_string_handle *name, __itt_scope scope)
{
  if (!UniController::IsCollectionEnabled()) {
    return;
  }

  if (!itt_collector->IsEnableChromeLoggingOn()) {
    return;
  }

  char marker[1025];

  if (domain && domain->nameA) {
    if (name && name->strA) {
      if ((id.d1 != __itt_null.d1) || (id.d2 != __itt_null.d2) || (id.d3 != __itt_null.d3)) {
        snprintf(marker, 1024, "%s::%s::%lld::%lld::%lld", domain->nameA, name->strA, id.d1, id.d2, id.d3);
      }
      else {
        snprintf(marker, 1024, "%s::%s", domain->nameA, name->strA);
      }
    }
    else {
      if ((id.d1 != __itt_null.d1) || (id.d2 != __itt_null.d2) || (id.d3 != __itt_null.d3)) {
        snprintf(marker, 1024, "%s::%lld::%lld::%lld", domain->nameA, id.d1, id.d2, id.d3);
      }
      else {
        snprintf(marker, 1024, "%s", domain->nameA);
      }
    }
  }
  else {
    if (name && name->strA) {
      if ((id.d1 != __itt_null.d1) || (id.d2 != __itt_null.d2) || (id.d3 != __itt_null.d3)) {
        snprintf(marker, 1024, "%s::%lld::%lld::%lld", name->strA, id.d1, id.d2, id.d3);
      }
      else {
        snprintf(marker, 1024, "%s", name->strA);
      }
    }
    else {
      if ((id.d1 != __itt_null.d1) || (id.d2 != __itt_null.d2) || (id.d3 != __itt_null.d3)) {
        snprintf(marker, 1024, "%lld::%lld::%lld", id.d1, id.d2, id.d3);
      }
      else {
        snprintf(marker, 1024, "UNNAMED_MARKER");
      }
    }
  }
    
  uint64_t ts = UniTimer::GetHostTimestamp();
  itt_collector->Log(marker, ts, ts);
}

// Need these empty stubs to make sure symbols are resolved in case any of these symbols are present in target application
ITT_EXTERN_C void ITTAPI __itt_detach(void)
{
}

ITT_EXTERN_C __itt_pt_region ITTAPI __itt_pt_region_create(const char *name)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_thread_set_name(const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_thread_ignore(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_suppress_push(unsigned int mask)
{
}

ITT_EXTERN_C void ITTAPI __itt_suppress_pop(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_suppress_mark_range(__itt_suppress_mode_t mode, unsigned int mask, void * address, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_suppress_clear_range(__itt_suppress_mode_t mode, unsigned int mask, void * address, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_create (void *addr, const char *objtype, const char *objname, int attribute)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_rename(void *addr, const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_destroy(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_prepare(void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_cancel(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_acquired(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_sync_releasing(void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_fsync_prepare(void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_fsync_cancel(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_fsync_acquired(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_fsync_releasing(void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_site_begin(__itt_model_site *site, __itt_model_site_instance *instance, const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_site_beginA(const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_site_beginAL(const char *name, size_t siteNameLen)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_site_end  (__itt_model_site *site, __itt_model_site_instance *instance)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_site_end_2(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_task_begin(__itt_model_task *task, __itt_model_task_instance *instance, const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_task_beginA(const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_task_beginAL(const char *name, size_t taskNameLen)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_iteration_taskA(const char *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_iteration_taskAL(const char *name, size_t taskNameLen)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_task_end  (__itt_model_task *task, __itt_model_task_instance *instance)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_task_end_2(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_acquire(void *lock)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_acquire_2(void *lock)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_release(void *lock)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_release_2(void *lock)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_record_allocation  (void *addr, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_record_deallocation(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_induction_uses(void* addr, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_reduction_uses(void* addr, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_observe_uses(void* addr, size_t size)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_clear_uses(void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_disable_push(__itt_model_disable x)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_disable_pop(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_model_aggregate_task(size_t x)
{
}

ITT_EXTERN_C __itt_heap_function ITTAPI __itt_heap_function_create(const char* name, const char* domain)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_heap_allocate_begin(__itt_heap_function h, size_t size, int initialized)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_allocate_end(__itt_heap_function h, void** addr, size_t size, int initialized)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_free_begin(__itt_heap_function h, void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_free_end(__itt_heap_function h, void* addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_reallocate_begin(__itt_heap_function h, void* addr, size_t new_size, int initialized)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_reallocate_end(__itt_heap_function h, void* addr, void** new_addr, size_t new_size, int initialized)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_internal_access_begin(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_internal_access_end(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_record_memory_growth_begin(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_record_memory_growth_end(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_reset_detection(unsigned int reset_mask)
{
}

ITT_EXTERN_C void ITTAPI __itt_heap_record(unsigned int record_mask)
{
}

ITT_EXTERN_C void ITTAPI __itt_id_create(const __itt_domain *domain, __itt_id id)
{
}

ITT_EXTERN_C void ITTAPI __itt_id_destroy(const __itt_domain *domain, __itt_id id)
{
}

ITT_EXTERN_C __itt_timestamp ITTAPI __itt_get_timestamp(void)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_region_begin(const __itt_domain *domain, __itt_id id, __itt_id parentid, __itt_string_handle *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_region_end(const __itt_domain *domain, __itt_id id)
{
}

ITT_EXTERN_C void ITTAPI __itt_frame_begin_v3(const __itt_domain *domain, __itt_id *id)
{
}

ITT_EXTERN_C void ITTAPI __itt_frame_end_v3(const __itt_domain *domain, __itt_id *id)
{
}

ITT_EXTERN_C void ITTAPI __itt_frame_submit_v3(const __itt_domain *domain, __itt_id *id, __itt_timestamp begin, __itt_timestamp end)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_group(const __itt_domain *domain, __itt_id id, __itt_id parentid, __itt_string_handle *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_fn(const __itt_domain *domain, __itt_id taskid, __itt_id parentid, void* fn)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_overlapped(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_end_overlapped(const __itt_domain *domain, __itt_id taskid)
{
}

ITT_EXTERN_C void ITTAPI __itt_metadata_add(const __itt_domain *domain, __itt_id id, __itt_string_handle *key, __itt_metadata_type type, size_t count, void *data)
{
}

ITT_EXTERN_C void ITTAPI __itt_metadata_str_add(const __itt_domain *domain, __itt_id id, __itt_string_handle *key, const char *data, size_t length)
{
}

ITT_EXTERN_C void ITTAPI __itt_metadata_add_with_scope(const __itt_domain *domain, __itt_scope scope, __itt_string_handle *key, __itt_metadata_type type, size_t count, void *data)
{
}

ITT_EXTERN_C void ITTAPI __itt_metadata_str_add_with_scope(const __itt_domain *domain, __itt_scope scope, __itt_string_handle *key, const char *data, size_t length)
{
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_to_current(const __itt_domain *domain, __itt_relation relation, __itt_id tail)
{
}

ITT_EXTERN_C void ITTAPI __itt_relation_add(const __itt_domain *domain, __itt_id head, __itt_relation relation, __itt_id tail)
{
}

ITT_EXTERN_C __itt_clock_domain* ITTAPI __itt_clock_domain_create(__itt_get_clock_info_fn fn, void* fn_data)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_clock_domain_reset(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_id_create_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id id)
{
}

ITT_EXTERN_C void ITTAPI __itt_id_destroy_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id id)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id taskid,      __itt_id parentid, __itt_string_handle* name)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_fn_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id taskid, __itt_id parentid, void* fn)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_end_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp)
{
}

ITT_EXTERN_C __itt_counter ITTAPI __itt_counter_create(const char *name, const char *domain)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc(__itt_counter id)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_delta(__itt_counter id, unsigned long long value)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec(__itt_counter id)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_delta(__itt_counter id, unsigned long long value)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_v3(const __itt_domain *domain, __itt_string_handle *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_delta_v3(const __itt_domain *domain, __itt_string_handle *name, unsigned long long delta)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_v3(const __itt_domain *domain, __itt_string_handle *name)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_delta_v3(const __itt_domain *domain, __itt_string_handle *name, unsigned long long delta)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_set_value(__itt_counter id, void *value_ptr)
{
}

ITT_EXTERN_C void ITTAPI __itt_counter_set_value_ex(__itt_counter id, __itt_clock_domain *clock_domain, unsigned long long timestamp, void *value_ptr)
{
}

ITT_EXTERN_C __itt_counter ITTAPI __itt_counter_create_typed(const char *name, const char *domain, __itt_metadata_type type)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_counter_destroy(__itt_counter id)
{
}

ITT_EXTERN_C void ITTAPI __itt_marker_ex(const __itt_domain *domain,  __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id id, __itt_string_handle *name, __itt_scope scope)
{
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_to_current_ex(const __itt_domain *domain,  __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_relation relation, __itt_id tail)
{
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_ex(const __itt_domain *domain,  __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id head, __itt_relation relation, __itt_id tail)
{
}

ITT_EXTERN_C __itt_track_group* ITTAPI __itt_track_group_create(__itt_string_handle* name, __itt_track_group_type track_group_type)
{
  return 0;
}

ITT_EXTERN_C __itt_track* ITTAPI __itt_track_create(__itt_track_group* track_group, __itt_string_handle* name, __itt_track_type track_type)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_set_track(__itt_track* track)
{
}

ITT_EXTERN_C int ITTAPI __itt_av_save(void *data, int rank, const int *dimensions, int type, const char *filePath, int columnOrder)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_enable_attach(void)
{
}

ITT_EXTERN_C void ITTAPI __itt_module_load(void *start_addr, void *end_addr, const char *path)
{
}

ITT_EXTERN_C void ITTAPI __itt_module_unload(void *addr)
{
}

ITT_EXTERN_C void ITTAPI __itt_module_load_with_sections(__itt_module_object* module_obj)
{
}

ITT_EXTERN_C void ITTAPI __itt_module_unload_with_sections(__itt_module_object* module_obj)
{
}

ITT_EXTERN_C __itt_histogram* ITTAPI __itt_histogram_create(const __itt_domain* domain, const char* name, __itt_metadata_type x_type, __itt_metadata_type y_type)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_histogram_submit(__itt_histogram* hist, size_t length, void* x_data, void* y_data)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_overlapped_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id taskid, __itt_id parentid, __itt_string_handle* name)
{
}

ITT_EXTERN_C void ITTAPI __itt_task_end_overlapped_ex(const __itt_domain* domain, __itt_clock_domain* clock_domain, unsigned long long timestamp, __itt_id taskid)
{
}

ITT_EXTERN_C __itt_mark_type ITTAPI __itt_mark_create(const char *name)
{
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark(__itt_mark_type mt, const char *parameter)
{
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_global(__itt_mark_type mt, const char *parameter)
{
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_off(__itt_mark_type mt)
{
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_global_off(__itt_mark_type mt)
{
  return 0;
}

ITT_EXTERN_C __itt_caller ITTAPI __itt_stack_caller_create(void)
{
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_stack_caller_destroy(__itt_caller id)
{
}

ITT_EXTERN_C void ITTAPI __itt_stack_callee_enter(__itt_caller id)
{
}

ITT_EXTERN_C void ITTAPI __itt_stack_callee_leave(__itt_caller id)
{
}


#endif // PTI_TOOLS_UNITRACE_ITT_COLLECTOR_H_
