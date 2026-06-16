
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNICONTROL_H_
#define PTI_TOOLS_UNITRACE_UNICONTROL_H_

#include "shared_memory.h"
#ifndef _WIN32
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else  /* _WIN32 */
#include <windows.h>
#endif /* _WIN32 */
#include <atomic>
#include <iostream>

extern char **environ;

#define UNITRACE_METRIC_SAMPLING_CONTROL "unitrace_metric_sampling_control"
#define UNITRACE_METRIC_FLUSH_CONTROL "unitrace_metric_flush_control"

enum TemporalControlState {
  TEMPORAL_RESUMED = 0,
  TEMPORAL_PAUSED = 1,
  TEMPORAL_STOPPED = 2
};

struct TemporalControl {
  TemporalControlState state_;
  char padding_[1024 - sizeof(TemporalControlState)];
};

// Shared-memory control used by the parent unitrace process to know when its
// instrumented child processes have flushed their metric data, using a simple
// pair of counters instead of a per-PID slot table:
//
//   flush_started_   monotonic count of children that have BEGUN flushing since
//                    the session stopped (only ever incremented). It opens the
//                    readiness gate: until at least one child has started there
//                    is nothing to wait for.
//   flush_completed_ number of children that have COMPLETED flushing
//
// A child only touches these counters when it actually flushes: on observing
// the session stop it increments flush_started_(SignalChildFlushStarted), and
// decrements flush_completed_ (SignalChildDataReady) after the flushing is
// completed. A child that never observes the stop (e.g. a non-GPU process 
// that inherited the tool via LD_PRELOAD but issues no L0/CL calls
// never touches the counters, so it is naturally invisible and cannot
// stall the parent -- no registration or per-PID bookkeeping is needed.
//
// Readiness: flush_started_ == flush_completed_ (see CheckChildDataReadyState).
// Because a child observes the stop only on its next collection check, the
// parent applies a short start-of-flush grace (kChildFlushStartGraceMs_) before
// trusting an early ready result, so a fast sibling that already finished does
// not cause the parent to skip a child that has not begun flushing yet (see
// IsChildDataReadyTrusted).
//
// Tradeoff vs. the per-PID slot table: a child killed mid-flush increments
// flush_started_ but never increments flush_completed_, so readiness never goes
// true and // the parent falls back to its bounded timeout. The counters carry
// no per-PID identity, so there is no liveness-based reaping to recover such a
// child early; this simplicity is the intended cost.
struct MetricFlushControl {
  std::atomic<int32_t> flush_started_;
  std::atomic<int32_t> flush_completed_;
};


class UniController{
  public:
    typedef void (*SessionStoppedCallback)(void);

    static void SetSessionStoppedCallback(SessionStoppedCallback callback) {
      session_stopped_callback_ = callback;
    }

    static void CreateTemporalControl(const char* session) {
      SharedMemoryReturnStatus ret = session_shm_.Create(session, sizeof(TemporalControl));
      if (ret == SHM_FAILED) {
        exit(-1);
      }
      if (ret == SHM_ALREADY_EXISTS) {
        // The mapping already existed (Windows: named mapping still held by
        // another process).  Only warn if it wasn't properly stopped.
        if (((TemporalControl*)session_shm_.GetPtr())->state_ != TEMPORAL_STOPPED) {
          std::cerr << "[WARNING] Session " << session << " was not stopped before reusing" << std::endl;
        }
      }
      ((TemporalControl*)session_shm_.GetPtr())->state_ = TEMPORAL_RESUMED;
    }

    static void AttachTemporalControlWrite(const char *session) {
      if (session_shm_.AttachWrite(session, sizeof(TemporalControl)) == SHM_FAILED) {
        exit(-1);
      }
    }

    static void AttachTemporalControlRead(const char *session) {
      SharedMemoryReturnStatus ret = session_shm_.AttachRead(session, sizeof(TemporalControl));
      if (ret == SHM_FAILED) {
        exit(-1);
      }
      if (ret == SHM_STOPPED) {
        temporal_control_stopped_ = true;
        return;
      }
    }

    static void ReleaseTemporalControl(void) {
      if (session_shm_.Release() == SHM_FAILED) {
        std::cerr << "[WARNING] ReleaseTemporalControl Failed" << std::endl;
      }
    }

    static void TemporalPause(const char *session) {
      AttachTemporalControlWrite(session);
      if (session_shm_.GetPtr() != nullptr) {
        if (((TemporalControl*)session_shm_.GetPtr())->state_ == TEMPORAL_STOPPED) {
          return;
        }
        ((TemporalControl*)session_shm_.GetPtr())->state_ = TEMPORAL_PAUSED;
        std::cerr << "[INFO] Session " << session << " is paused" << std::endl;
      }
    }

    static void TemporalResume(const char *session) {
      AttachTemporalControlWrite(session);
      if (session_shm_.GetPtr() != nullptr) {
        if (((TemporalControl*)session_shm_.GetPtr())->state_ == TEMPORAL_STOPPED) {
          return;
        }
        ((TemporalControl*)session_shm_.GetPtr())->state_ = TEMPORAL_RESUMED;
        std::cerr << "[INFO] Session " << session << " is resumed" << std::endl;
      }
    }

    static void TemporalStop(const char *session) {
      AttachTemporalControlWrite(session);
      if (session_shm_.GetPtr() != nullptr) {
        ((TemporalControl*)session_shm_.GetPtr())->state_ = TEMPORAL_STOPPED;
        std::cerr << "[INFO] Session " << session << " is stopped and can no longer be paused or resumed" << std::endl;
      }
      // Release (unmap + unlink) the shared memory so a new session with the
      // same name can be created immediately.  The profiled process already has
      // the TEMPORAL_STOPPED value; its mapping remains valid until it munmaps.
      session_shm_.Release();
    }

    static void CreateMetricSamplingControl() {
      if (metric_sample_shm_.Create(UNITRACE_METRIC_SAMPLING_CONTROL, sizeof(TemporalControl), true) == SHM_FAILED) {
        exit(-1);
      }

      if (!utils::GetEnv("UNITRACE_StartPaused").empty()) {
        ((TemporalControl*)metric_sample_shm_.GetPtr())->state_ = TEMPORAL_PAUSED;
      } else {
        ((TemporalControl*)metric_sample_shm_.GetPtr())->state_ = TEMPORAL_RESUMED;
      }
    }

    static void ReleaseMetricSamplingControl() {
      if (metric_sample_shm_.GetPtr() != nullptr) {
        metric_sample_shm_.Release();
      }
    }

    static void CreateMetricFlushControl() {
      if (metric_flush_shm_.Create(UNITRACE_METRIC_FLUSH_CONTROL, sizeof(MetricFlushControl), true) == SHM_FAILED) {
        exit(-1);
      }
      auto* ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      ctl->flush_started_.store(0, std::memory_order_release);
      ctl->flush_completed_.store(0, std::memory_order_release);
    }

    static void ReleaseMetricFlushControl() {
      if (metric_flush_shm_.GetPtr() != nullptr) {
        metric_flush_shm_.Release();
      }
    }

    // Called by the in-process tool (child) when it observes the session stop
    // and is about to flush its metric data, BEFORE the flush runs. Increments
    // flush_started_ (monotonic; opens the readiness gate) 
    // The matching increment of flush_completed_
    // happens in SignalChildDataReady() once the flush completes. A child that
    // never observes the stop never calls this, so it stays invisible to the
    // parent's readiness check.
    static void SignalChildFlushStarted() {
      auto* ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      if (ctl == nullptr) {
        if (metric_flush_shm_.AttachWrite(UNITRACE_METRIC_FLUSH_CONTROL, sizeof(MetricFlushControl)) != SHM_SUCCESS) {
          return;
        }
        ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      }
      ctl->flush_started_.fetch_add(1, std::memory_order_acq_rel);
    }

    // Called by the in-process tool (child) when its session stops and metric
    // data has been flushed. Increments flush_completed_, balancing the
    // increment done by SignalChildFlushStarted().
    static void SignalChildDataReady() {
      auto* ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      if (ctl == nullptr) {
        if (metric_flush_shm_.AttachWrite(UNITRACE_METRIC_FLUSH_CONTROL, sizeof(MetricFlushControl)) != SHM_SUCCESS) {
          return;
        }
        ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      }
      ctl->flush_completed_.fetch_add(1, std::memory_order_acq_rel);
    }

    // Returns:
    //   -1 : no child has started flushing
    //    0 : one or more children have started flusheing but not all of them
    //        have finished flushing. 
    //    1 : one or more children have started flusheing and all of them have
    //        finished flushing
    // If a child is killed mid-flush, flush_completed_ will not be bumped and 0
    // is returned and the parent falls back to its bounded timeout.
    // 
    static int CheckChildDataReadyState() {
      auto* ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      if (ctl == nullptr) {
        if (metric_flush_shm_.AttachRead(UNITRACE_METRIC_FLUSH_CONTROL, sizeof(MetricFlushControl)) != SHM_SUCCESS) {
          return false;
        }
        ctl = (MetricFlushControl*)metric_flush_shm_.GetPtr();
      }
      
      auto started = ctl->flush_started_.load(std::memory_order_acquire);
      auto completed = ctl->flush_completed_.load(std::memory_order_acquire);
      if (started == 0) {
        return -1;
      }
      if (started == completed) {
        return 1;
      }
      return 0; 
    }

    // Grace period (ms) the parent waits after a session stop before it trusts
    // an CheckChildDataReady(State) == 1 result. This closes the start-of-flush race:
    // with multiple GPU children, a fast child can begin and finish flushing
    // (flush_started_ == 1, flush_completed_ == 0) before a slower sibling has
    // observed the stop and incremented the counters. Because the counters carry
    // no notion of "how many children will participate", the parent cannot tell
    // the slow child apart from "all done"; waiting out the grace gives every
    // child time to begin flushing (and thus raise flush_completed_) first.
    static constexpr int kChildFlushStartGraceMs_ = 500;

    // Combines CheckChildDataReadyState() with the start-of-flush grace: readiness is
    // trusted only when it holds AND the grace has elapsed since the parent
    // began waiting (elapsed_ms). 
    static bool IsChildDataReadyTrusted(int elapsed_ms) {
      auto state = CheckChildDataReadyState();
      if ((state != -1) && (state == 0)) {
        return false;
      }
      // No child ever flushed or all children completed flushing in elapsed_time
      return (elapsed_ms >= kChildFlushStartGraceMs_);
    }

    // Read-only check of the named session's state. Returns true if the
    // session shared memory is mapped and currently TEMPORAL_STOPPED. Unlike
    // IsMetricSamplingEnabled()/IsCollectionEnabled() this does NOT fire the
    // session-stopped callback, so it is safe to poll from the parent process.
    static bool IsSessionStopped(void) {
      if (session_shm_.GetPtr() == nullptr) {
        return false;
      }
      return (((TemporalControl*)session_shm_.GetPtr())->state_ == TEMPORAL_STOPPED);
    }

    static bool IsMetricSamplingEnabled(void) {
      if (session_shm_.GetPtr() != nullptr) {
        TemporalControlState state = ((TemporalControl*)session_shm_.GetPtr())->state_;
        if (state == TEMPORAL_STOPPED) {
          // Named session was stopped. Fire the session-stopped callback once
          // so the owner of the metric sampling thread (the parent process)
          // can react to the stop. The callback is invoked at most once across
          // all sampling threads via the atomic exchange guard.
          if (!session_stopped_notified_.exchange(true)) {
            if (session_stopped_callback_) {
              session_stopped_callback_();
            }
          }
          return false;
        }
        return (state == TEMPORAL_RESUMED);
      }

      if (conditional_collection_) {
        if (metric_sample_shm_.GetPtr() != nullptr) {
          return (((TemporalControl*)metric_sample_shm_.GetPtr())->state_ == TEMPORAL_RESUMED);
        }
      }
      return true;
    }

    // defer_callback: when true, report the stopped state WITHOUT synchronously
    // firing the session-stopped callback (and without consuming the one-shot
    // notify guard, so the notification stays armed). Pass true when calling
    // from code that already holds the collector's processing locks: firing
    // OnSessionStopped() there re-enters the collector (Flush ->
    // ProcessAllCommandsSubmitted) and deadlocks on the non-recursive
    // global_device_submissions_mutex_ (std::system_error / EDEADLK). With the
    // notification deferred, it fires later from a safe top-level call site
    // (e.g. the next OnEnter*Append* callback) where no collector lock is held.
    static bool IsCollectionEnabled(bool defer_callback = false) {
      if (temporal_control_stopped_) {
        // Session is stopped. The very first detection may have happened in a
        // deferred context (under a collector lock, where firing the callback
        // synchronously would re-enter and deadlock), which set
        // temporal_control_stopped_ but skipped the notification. If the
        // notification is still pending and this is a safe (non-deferred)
        // top-level call site, fire it now -- otherwise the deferred callback
        // would be lost forever because every later call short-circuits here.
        if (!defer_callback && !session_stopped_notified_.exchange(true)) {
          if (session_stopped_callback_) {
            session_stopped_callback_();
          }
        }
        return false;
      }

      if (session_shm_.GetPtr() != nullptr) {
        TemporalControlState state = ((TemporalControl*)session_shm_.GetPtr())->state_;
        if (state == TEMPORAL_STOPPED) {
          temporal_control_stopped_ = true;
          if (!defer_callback && !session_stopped_notified_.exchange(true)) {
            if (session_stopped_callback_) {
              session_stopped_callback_();
            }
          }
          return false;
        }
        return (state == TEMPORAL_RESUMED);
      }

      if (conditional_collection_) {
        if (metric_sample_shm_.GetPtr() != nullptr) {
          return (((TemporalControl*)metric_sample_shm_.GetPtr())->state_ == TEMPORAL_RESUMED);
        }

        if (itt_paused_) {
          return false;
        }

        if (environ != nullptr) {
          char *env;
          char *value = nullptr;
          constexpr int len = sizeof("PTI_ENABLE_COLLECTION") - 1;  // do not count trailing '\0'
          char **cursor = environ;
          // PTI_ENABLE_COLLECTION is likely at the end if it is set
          while (*cursor) {
            cursor++;
          }
          cursor--;
          for (; (cursor != environ - 1) && ((env = *cursor) != nullptr); cursor--) {
            if ((env[0] == 'P') && (env[1] == 'T') && (env[2] == 'I') && (strncmp(env + 3, "_ENABLE_COLLECTION", len - 3) == 0) && (env[len] == '=')) {
              value = (env + len + 1);
              break;
            }
          }

          if ((value == nullptr) || (*value == '0')) {
             return false;
          }
        }
      }
      return true;
    }

    static void IttPause(void) {
      itt_paused_ = true;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "0");
      // shared memory for metric sampling created by parent process
      // we always need to check in child process if it still exist
      if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
        if (metric_sample_shm_.AttachWrite(UNITRACE_METRIC_SAMPLING_CONTROL, sizeof(TemporalControl)) == SHM_SUCCESS) {
          ((TemporalControl*)metric_sample_shm_.GetPtr())->state_ = TEMPORAL_PAUSED;
        }
      }
    }

    static void IttResume(void) {
      itt_paused_ = false;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
      // shared memory for metric sampling created by parent process
      // we always need to check in child process if it still exist
      if (utils::GetEnv("UNITRACE_KernelMetrics") == "1") {
        if (metric_sample_shm_.AttachWrite(UNITRACE_METRIC_SAMPLING_CONTROL, sizeof(TemporalControl)) == SHM_SUCCESS) {
          ((TemporalControl*)metric_sample_shm_.GetPtr())->state_ = TEMPORAL_RESUMED;
        }
      }
    }

  private:
    inline static bool conditional_collection_ = (utils::GetEnv("UNITRACE_StartPaused") == "1") ? true : false;
    inline static bool itt_paused_ = false;
    // when the session is stopped, the shared memory will be removed so processes started afterwards
    // gets null value for session_shm_.GetPtr()
    // but session_shm_.GetPtr() can also be null if the session is unnamed
    // this flag is true when session_shm_.GetPtr() is null but the named session is stopped
    // so subsequent processes are not profiled
    inline static bool temporal_control_stopped_ = false;
    inline static SessionStoppedCallback session_stopped_callback_ = nullptr;
    inline static std::atomic<bool> session_stopped_notified_{false};

    inline static SharedMemory session_shm_;
    // shared memory to interact between child and parent process for metric sampling.
    // In application control can't be done using environment variable for metric sampling.
    // the sampling thread is owned by the parent process.
    inline static SharedMemory metric_sample_shm_;
    // shared memory for metric flush handshake between parent and child.
    // Used when -k is present to coordinate flushing metric data and computing metrics.
    inline static SharedMemory metric_flush_shm_;
};

#endif // PTI_TOOLS_UNITRACE_UNICONTROL_H_



