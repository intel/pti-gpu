
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
#endif /* _WIN32 */
#include <iostream>

extern char **environ;

#define UNITRACE_METRIC_SAMPLING_CONTROL "unitrace_metric_sampling_control"

enum TemporalControlState {
  TEMPORAL_RESUMED = 0,
  TEMPORAL_PAUSED = 1,
  TEMPORAL_STOPPED = 2
};

struct TemporalControl {
  TemporalControlState state_;
  char padding_[1024 - sizeof(TemporalControlState)];
};


class UniController{
  public:
    static void CreateTemporalControl(const char* session) {
      if (session_shm_.Create(session, sizeof(TemporalControl)) == SHM_FAILED) {
        exit(-1);
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

    static bool IsMetricSamplingEnabled(void) {
      if (session_shm_.GetPtr() != nullptr) {
        return (((TemporalControl*)session_shm_.GetPtr())->state_ == TEMPORAL_RESUMED);
      }

      if (conditional_collection_) {
        if (metric_sample_shm_.GetPtr() != nullptr) {
          return (((TemporalControl*)metric_sample_shm_.GetPtr())->state_ == TEMPORAL_RESUMED);
        }
      }
      return true;
    }

    static bool IsCollectionEnabled(void) {
      if (temporal_control_stopped_) {
        // session is stopped
        return false;
      }

      if (session_shm_.GetPtr() != nullptr) {
        return (((TemporalControl*)session_shm_.GetPtr())->state_ == TEMPORAL_RESUMED);
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

    inline static SharedMemory session_shm_;
    // shared memory to interact between child and parent process for metric sampling.
    // In application control can't be done using environment variable for metric sampling.
    // the sampling thread is owned by the parent process.
    inline static SharedMemory metric_sample_shm_;
};

#endif // PTI_TOOLS_UNITRACE_UNICONTROL_H_



