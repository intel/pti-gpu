//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE

#if defined(PTI_CCL_ITT_COMPILE)
#define ITTAPI_CDECL __attribute__((visibility("default")))
#else
#define ITTAPI_CDECL
#endif  // PTI_CCL_ITT_COMPILE

#include <ittnotify.h>
#include <ittnotify_config.h>
#include <spdlog/spdlog.h>

// ITT API stub functions for libpti_view
// These provide symbol resolution without actual implementation

ITT_EXTERN_C void ITTAPI __itt_task_end_internal_callback_info(const __itt_domain *domain
                                                               [[maybe_unused]],
                                                               int64_t mpi_counter [[maybe_unused]],
                                                               size_t src_size [[maybe_unused]],
                                                               size_t dst_size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_event ITTAPI __itt_event_create(const char *name [[maybe_unused]],
                                                   int namelen [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return -1;
}

ITT_EXTERN_C int ITTAPI __itt_event_start(__itt_event event [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return __itt_error_no_symbol;
}

ITT_EXTERN_C void ITTAPI __itt_pause(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C void ITTAPI __itt_pause_scoped(__itt_collection_scope scope [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_resume(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C void ITTAPI __itt_resume_scoped(__itt_collection_scope scope [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_marker(const __itt_domain *domain [[maybe_unused]],
                                      __itt_id id [[maybe_unused]],
                                      __itt_string_handle *name [[maybe_unused]],
                                      __itt_scope scope [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_detach(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C __itt_pt_region ITTAPI __itt_pt_region_create(const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_thread_set_name(const char *name [[maybe_unused]]) {
  SPDLOG_DEBUG("{}(): {}", __FUNCTION__, name);
}

ITT_EXTERN_C void ITTAPI __itt_thread_ignore(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C void ITTAPI __itt_suppress_push(unsigned int mask [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_suppress_pop(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C void ITTAPI __itt_suppress_mark_range(__itt_suppress_mode_t mode [[maybe_unused]],
                                                   unsigned int mask [[maybe_unused]],
                                                   void *address [[maybe_unused]],
                                                   size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_suppress_clear_range(__itt_suppress_mode_t mode [[maybe_unused]],
                                                    unsigned int mask [[maybe_unused]],
                                                    void *address [[maybe_unused]],
                                                    size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_create(void *addr [[maybe_unused]],
                                           const char *objtype [[maybe_unused]],
                                           const char *objname [[maybe_unused]],
                                           int attribute [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_rename(void *addr [[maybe_unused]],
                                           const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_destroy(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_prepare(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_cancel(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_acquired(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_sync_releasing(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_fsync_prepare(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_fsync_cancel(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_fsync_acquired(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_fsync_releasing(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_site_begin(__itt_model_site *site [[maybe_unused]],
                                                __itt_model_site_instance *instance
                                                [[maybe_unused]],
                                                const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_site_beginA(const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_site_beginAL(const char *name [[maybe_unused]],
                                                  size_t siteNameLen [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_site_end(__itt_model_site *site [[maybe_unused]],
                                              __itt_model_site_instance *instance
                                              [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_site_end_2(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_task_begin(__itt_model_task *task [[maybe_unused]],
                                                __itt_model_task_instance *instance
                                                [[maybe_unused]],
                                                const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_task_beginA(const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_task_beginAL(const char *name [[maybe_unused]],
                                                  size_t taskNameLen [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_iteration_taskA(const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_iteration_taskAL(const char *name [[maybe_unused]],
                                                      size_t taskNameLen [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_task_end(__itt_model_task *task [[maybe_unused]],
                                              __itt_model_task_instance *instance
                                              [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_task_end_2(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_acquire(void *lock [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_acquire_2(void *lock [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_release(void *lock [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_lock_release_2(void *lock [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_record_allocation(void *addr [[maybe_unused]],
                                                       size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_record_deallocation(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_induction_uses(void *addr [[maybe_unused]],
                                                    size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_reduction_uses(void *addr [[maybe_unused]],
                                                    size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_observe_uses(void *addr [[maybe_unused]],
                                                  size_t size [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_clear_uses(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_disable_push(__itt_model_disable x [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_disable_pop(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_model_aggregate_task(size_t x [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_heap_function ITTAPI __itt_heap_function_create(const char *name
                                                                   [[maybe_unused]],
                                                                   const char *domain
                                                                   [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_heap_allocate_begin(__itt_heap_function h [[maybe_unused]],
                                                   size_t size [[maybe_unused]],
                                                   int initialized [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_allocate_end(__itt_heap_function h [[maybe_unused]],
                                                 void **addr [[maybe_unused]],
                                                 size_t size [[maybe_unused]],
                                                 int initialized [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_free_begin(__itt_heap_function h [[maybe_unused]],
                                               void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_free_end(__itt_heap_function h [[maybe_unused]],
                                             void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_reallocate_begin(__itt_heap_function h [[maybe_unused]],
                                                     void *addr [[maybe_unused]],
                                                     size_t new_size [[maybe_unused]],
                                                     int initialized [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_reallocate_end(__itt_heap_function h [[maybe_unused]],
                                                   void *addr [[maybe_unused]],
                                                   void **new_addr [[maybe_unused]],
                                                   size_t new_size [[maybe_unused]],
                                                   int initialized [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_internal_access_begin(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_internal_access_end(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_record_memory_growth_begin(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_record_memory_growth_end(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_reset_detection(unsigned int reset_mask [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_heap_record(unsigned int record_mask [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_id_create(const __itt_domain *domain [[maybe_unused]],
                                         __itt_id id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_id_destroy(const __itt_domain *domain [[maybe_unused]],
                                          __itt_id id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_timestamp ITTAPI __itt_get_timestamp(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_region_begin(const __itt_domain *domain [[maybe_unused]],
                                            __itt_id id [[maybe_unused]],
                                            __itt_id parentid [[maybe_unused]],
                                            __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_region_end(const __itt_domain *domain [[maybe_unused]],
                                          __itt_id id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_frame_begin_v3(const __itt_domain *domain [[maybe_unused]],
                                              __itt_id *id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_frame_end_v3(const __itt_domain *domain [[maybe_unused]],
                                            __itt_id *id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_frame_submit_v3(const __itt_domain *domain [[maybe_unused]],
                                               __itt_id *id [[maybe_unused]],
                                               __itt_timestamp begin [[maybe_unused]],
                                               __itt_timestamp end [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_group(const __itt_domain *domain [[maybe_unused]],
                                          __itt_id id [[maybe_unused]],
                                          __itt_id parentid [[maybe_unused]],
                                          __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_fn(const __itt_domain *domain [[maybe_unused]],
                                             __itt_id taskid [[maybe_unused]],
                                             __itt_id parentid [[maybe_unused]],
                                             void *fn [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_overlapped(const __itt_domain *domain [[maybe_unused]],
                                                     __itt_id taskid [[maybe_unused]],
                                                     __itt_id parentid [[maybe_unused]],
                                                     __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_end_overlapped(const __itt_domain *domain [[maybe_unused]],
                                                   __itt_id taskid [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_metadata_str_add(const __itt_domain *domain [[maybe_unused]],
                                                __itt_id id [[maybe_unused]],
                                                __itt_string_handle *key [[maybe_unused]],
                                                const char *data [[maybe_unused]],
                                                size_t length [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_metadata_add_with_scope(const __itt_domain *domain [[maybe_unused]],
                                                       __itt_scope scope [[maybe_unused]],
                                                       __itt_string_handle *key [[maybe_unused]],
                                                       __itt_metadata_type type [[maybe_unused]],
                                                       size_t count [[maybe_unused]],
                                                       void *data [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_metadata_str_add_with_scope(
    const __itt_domain *domain [[maybe_unused]], __itt_scope scope [[maybe_unused]],
    __itt_string_handle *key [[maybe_unused]], const char *data [[maybe_unused]],
    size_t length [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_to_current(const __itt_domain *domain [[maybe_unused]],
                                                       __itt_relation relation [[maybe_unused]],
                                                       __itt_id tail [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_relation_add(const __itt_domain *domain [[maybe_unused]],
                                            __itt_id head [[maybe_unused]],
                                            __itt_relation relation [[maybe_unused]],
                                            __itt_id tail [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_clock_domain *ITTAPI __itt_clock_domain_create(__itt_get_clock_info_fn fn
                                                                  [[maybe_unused]],
                                                                  void *fn_data [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_clock_domain_reset(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_id_create_ex(const __itt_domain *domain [[maybe_unused]],
                                            __itt_clock_domain *clock_domain [[maybe_unused]],
                                            unsigned long long timestamp [[maybe_unused]],
                                            __itt_id id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_id_destroy_ex(const __itt_domain *domain [[maybe_unused]],
                                             __itt_clock_domain *clock_domain [[maybe_unused]],
                                             unsigned long long timestamp [[maybe_unused]],
                                             __itt_id id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_ex(const __itt_domain *domain [[maybe_unused]],
                                             __itt_clock_domain *clock_domain [[maybe_unused]],
                                             unsigned long long timestamp [[maybe_unused]],
                                             __itt_id taskid [[maybe_unused]],
                                             __itt_id parentid [[maybe_unused]],
                                             __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_fn_ex(const __itt_domain *domain [[maybe_unused]],
                                                __itt_clock_domain *clock_domain [[maybe_unused]],
                                                unsigned long long timestamp [[maybe_unused]],
                                                __itt_id taskid [[maybe_unused]],
                                                __itt_id parentid [[maybe_unused]],
                                                void *fn [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_end_ex(const __itt_domain *domain [[maybe_unused]],
                                           __itt_clock_domain *clock_domain [[maybe_unused]],
                                           unsigned long long timestamp [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_counter ITTAPI __itt_counter_create(const char *name [[maybe_unused]],
                                                       const char *domain [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc(__itt_counter id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_delta(__itt_counter id [[maybe_unused]],
                                                 unsigned long long value [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec(__itt_counter id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_delta(__itt_counter id [[maybe_unused]],
                                                 unsigned long long value [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_v3(const __itt_domain *domain [[maybe_unused]],
                                              __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_inc_delta_v3(const __itt_domain *domain [[maybe_unused]],
                                                    __itt_string_handle *name [[maybe_unused]],
                                                    unsigned long long delta [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_v3(const __itt_domain *domain [[maybe_unused]],
                                              __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_dec_delta_v3(const __itt_domain *domain [[maybe_unused]],
                                                    __itt_string_handle *name [[maybe_unused]],
                                                    unsigned long long delta [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_set_value(__itt_counter id [[maybe_unused]],
                                                 void *value_ptr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_counter_set_value_ex(__itt_counter id [[maybe_unused]],
                                                    __itt_clock_domain *clock_domain
                                                    [[maybe_unused]],
                                                    unsigned long long timestamp [[maybe_unused]],
                                                    void *value_ptr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_counter ITTAPI __itt_counter_create_typed(const char *name [[maybe_unused]],
                                                             const char *domain [[maybe_unused]],
                                                             __itt_metadata_type type
                                                             [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_counter_destroy(__itt_counter id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_marker_ex(const __itt_domain *domain [[maybe_unused]],
                                         __itt_clock_domain *clock_domain [[maybe_unused]],
                                         unsigned long long timestamp [[maybe_unused]],
                                         __itt_id id [[maybe_unused]],
                                         __itt_string_handle *name [[maybe_unused]],
                                         __itt_scope scope [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_to_current_ex(
    const __itt_domain *domain [[maybe_unused]], __itt_clock_domain *clock_domain [[maybe_unused]],
    unsigned long long timestamp [[maybe_unused]], __itt_relation relation [[maybe_unused]],
    __itt_id tail [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_relation_add_ex(const __itt_domain *domain [[maybe_unused]],
                                               __itt_clock_domain *clock_domain [[maybe_unused]],
                                               unsigned long long timestamp [[maybe_unused]],
                                               __itt_id head [[maybe_unused]],
                                               __itt_relation relation [[maybe_unused]],
                                               __itt_id tail [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_track_group *ITTAPI
__itt_track_group_create(__itt_string_handle *name [[maybe_unused]],
                         __itt_track_group_type track_group_type [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C __itt_track *ITTAPI __itt_track_create(__itt_track_group *track_group [[maybe_unused]],
                                                    __itt_string_handle *name [[maybe_unused]],
                                                    __itt_track_type track_type [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_set_track(__itt_track *track [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C int ITTAPI __itt_av_save(void *data [[maybe_unused]], int rank [[maybe_unused]],
                                      const int *dimensions [[maybe_unused]],
                                      int type [[maybe_unused]],
                                      const char *filePath [[maybe_unused]],
                                      int columnOrder [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_enable_attach(void) { SPDLOG_TRACE("{}() Adapter", __FUNCTION__); }

ITT_EXTERN_C void ITTAPI __itt_module_load(void *start_addr [[maybe_unused]],
                                           void *end_addr [[maybe_unused]],
                                           const char *path [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_module_unload(void *addr [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_module_load_with_sections(__itt_module_object *module_obj
                                                         [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_module_unload_with_sections(__itt_module_object *module_obj
                                                           [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_histogram *ITTAPI __itt_histogram_create(
    const __itt_domain *domain [[maybe_unused]], const char *name [[maybe_unused]],
    __itt_metadata_type x_type [[maybe_unused]], __itt_metadata_type y_type [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_histogram_submit(__itt_histogram *hist [[maybe_unused]],
                                                size_t length [[maybe_unused]],
                                                void *x_data [[maybe_unused]],
                                                void *y_data [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_begin_overlapped_ex(
    const __itt_domain *domain [[maybe_unused]], __itt_clock_domain *clock_domain [[maybe_unused]],
    unsigned long long timestamp [[maybe_unused]], __itt_id taskid [[maybe_unused]],
    __itt_id parentid [[maybe_unused]], __itt_string_handle *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_task_end_overlapped_ex(const __itt_domain *domain [[maybe_unused]],
                                                      __itt_clock_domain *clock_domain
                                                      [[maybe_unused]],
                                                      unsigned long long timestamp [[maybe_unused]],
                                                      __itt_id taskid [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C __itt_mark_type ITTAPI __itt_mark_create(const char *name [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark(__itt_mark_type mt [[maybe_unused]],
                                   const char *parameter [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_global(__itt_mark_type mt [[maybe_unused]],
                                          const char *parameter [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_off(__itt_mark_type mt [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C int ITTAPI __itt_mark_global_off(__itt_mark_type mt [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C __itt_caller ITTAPI __itt_stack_caller_create(void) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
  return 0;
}

ITT_EXTERN_C void ITTAPI __itt_stack_caller_destroy(__itt_caller id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_stack_callee_enter(__itt_caller id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}

ITT_EXTERN_C void ITTAPI __itt_stack_callee_leave(__itt_caller id [[maybe_unused]]) {
  SPDLOG_TRACE("{}() Adapter", __FUNCTION__);
}
