
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNICONTROL_H
#define PTI_TOOLS_UNITRACE_UNICONTROL_H

#ifndef _WIN32
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif /* _WIN32 */

#include <iostream>

extern char **environ;

#define TEMPORAL_CONTROL_SESSION_NAME_MAX 256
#define TEMPORAL_CONTROL_SESSION_PREFIX "/uctrl"

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
    static void CreateTemporalControl(const char *session) {
#ifndef _WIN32
      // create shared memory object
      if (strlen(session) > (TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1)) {
        std::cerr << "[ERROR] Session identifier is too long (maximum " << TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1 << " }" << std::endl;
        exit(-1);
      }

      strcpy(temporal_control_name_, TEMPORAL_CONTROL_SESSION_PREFIX);
      strcat(temporal_control_name_, session);

      temporal_control_handle_ = shm_open(temporal_control_name_, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (temporal_control_handle_ == -1) {
        temporal_control_handle_ = shm_open(temporal_control_name_, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (temporal_control_handle_ == -1) {
          std::cerr << "[ERROR] Failed to create shared memory for session " << session << " (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }
        else {
          std::cerr << "[WARNING] Session " << session << " was not stopped before reusing" << std::endl;
        }
      }

      // set size of shared memory
      if (ftruncate(temporal_control_handle_, sizeof(TemporalControl)) == -1) {
        std::cerr << "[ERROR] Failed to set temporal control size" << std::endl;
        exit(-1);
      }

      // map shared memory
      temporal_control_ptr_ = (TemporalControl *)mmap(0, sizeof(TemporalControl), PROT_READ | PROT_WRITE, MAP_SHARED, temporal_control_handle_, 0);
      if (temporal_control_ptr_ == MAP_FAILED) {
        std::cerr << "[ERROR] Failed to map shared memory (" << strerror(errno) << ")" << std::endl;
        exit(-1);
      }

      temporal_control_ptr_->state_ = TEMPORAL_RESUMED; // default is TEMPORAL_RESUMED
#endif /* _WIN32 */
    }

    static void AttachTemporalControlWrite(const char *session) {
#ifndef _WIN32
      if (temporal_control_ptr_ == nullptr) {
        // open shared memory object
        if (strlen(session) > (TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1)) {
          std::cerr << "[ERROR] Session identifier is too long (maximum " << TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1 << " }" << std::endl;
          exit(-1);
        }

        strcpy(temporal_control_name_, TEMPORAL_CONTROL_SESSION_PREFIX);
        strcat(temporal_control_name_, session);
        temporal_control_handle_ = shm_open(temporal_control_name_, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (temporal_control_handle_ == -1) {
          std::cerr << "[ERROR] Session " << session << " does not exist or cannot be opened (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }

        // map shared memory
        temporal_control_ptr_ = (TemporalControl *)mmap(0, sizeof(TemporalControl), PROT_READ | PROT_WRITE, MAP_SHARED, temporal_control_handle_, 0);
        if (temporal_control_ptr_ == MAP_FAILED) {
          std::cerr << "[ERROR] Failed to map shared memory (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }
      }
#endif /* _WIN32 */
    }

    static void AttachTemporalControlRead(const char *session) {
#ifndef _WIN32
      if (temporal_control_ptr_ == nullptr) {
        // open shared memory object
        if (strlen(session) > (TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1)) {
          std::cerr << "[ERROR] Session identifier is too long (maximum " << TEMPORAL_CONTROL_SESSION_NAME_MAX - strlen(TEMPORAL_CONTROL_SESSION_PREFIX) - 1 << " }" << std::endl;
          exit(-1);
        }

        strcpy(temporal_control_name_, TEMPORAL_CONTROL_SESSION_PREFIX);
        strcat(temporal_control_name_, session);
        temporal_control_handle_ = shm_open(temporal_control_name_, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (temporal_control_handle_ == -1) {
          std::cerr << "[WARNING] Session " << session << " is already stopped" << std::endl;
          temporal_control_stopped_ = true;
          return;
        }

        // map shared memory
        temporal_control_ptr_ = (TemporalControl *)mmap(0, sizeof(TemporalControl), PROT_READ, MAP_SHARED, temporal_control_handle_, 0);
        if (temporal_control_ptr_ == MAP_FAILED) {
          std::cerr << "[ERROR] Failed to map shared memory (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }
      }
#endif /* _WIN32 */
    }

    static void ReleaseTemporalControl(void) {
#ifndef _WIN32
      if (temporal_control_ptr_ != nullptr) {
        // unmap shared memory
        if (munmap((void *)temporal_control_ptr_, sizeof(TemporalControl)) != 0) {
          std::cerr << "[ERROR] Failed to unmap shared memory (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }
        if (close(temporal_control_handle_) != 0) {
          std::cerr << "[ERROR] Failed to close shared memory descriptor (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }

        // unlink the shared memory
        if (shm_unlink(temporal_control_name_) != 0) {
          std::cerr << "[ERROR] Failed to unlink shared memory (" << strerror(errno) << ")" << std::endl;
          exit(-1);
        }
      }
#endif /* _WIN32 */
    }

    static void TemporalPause(const char *session) {
      AttachTemporalControlWrite(session);
      if (temporal_control_ptr_ != nullptr) {
        temporal_control_ptr_->state_ = TEMPORAL_PAUSED;
        std::cerr << "[INFO] Session " << session << " is paused" << std::endl;
      }	
    }

    static void TemporalResume(const char *session) {
      AttachTemporalControlWrite(session);
      if (temporal_control_ptr_ != nullptr) {
        temporal_control_ptr_->state_ = TEMPORAL_RESUMED;
        std::cerr << "[INFO] Session " << session << " is resumed" << std::endl;
      }	
    }

    static void TemporalStop(const char *session) {
      AttachTemporalControlWrite(session);
      if (temporal_control_ptr_ != nullptr) {
        temporal_control_ptr_->state_ = TEMPORAL_STOPPED;
        ReleaseTemporalControl();
        std::cerr << "[INFO] Session " << session << " is stopped and can no longer be paused or resumed" << std::endl;
      }	
    }

    static bool IsCollectionEnabled(void) {
      if (temporal_control_stopped_) {
        // session is stopped
        return false;
      }

      if (temporal_control_ptr_ != nullptr) {
        return (temporal_control_ptr_->state_ == TEMPORAL_RESUMED);
      }

      if (conditional_collection_) {
        if (itt_paused_) {
          return false;
        }

        if (environ != nullptr) {
          char *env;
          char *value = nullptr;
          constexpr int len = sizeof("PTI_ENABLE_COLLECTION") - 1;	// do not count trailing '\0' 
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
    }
    static void IttResume(void) {
      itt_paused_ = false;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
    }
  private:
    inline static bool conditional_collection_ = (utils::GetEnv("UNITRACE_StartPaused") == "1") ? true : false;
    inline static bool itt_paused_ = false;
    inline static TemporalControl *temporal_control_ptr_ = nullptr;
    inline static int temporal_control_handle_ = -1;
    inline static char temporal_control_name_[TEMPORAL_CONTROL_SESSION_NAME_MAX] = {0};
    // when the session is stopped, the shared memory will be removed so processes started afterwards
    // gets null value for temporal_control_ptr_ 
    // but temporal_control_ptr_ can also be null if the session is unnamed
    // this flag is true when temporal_control_ptr_ is null but the named session is stopped
    // so subsequent processes are not profiled 
    inline static bool temporal_control_stopped_ = false;
};
    
#endif // PTI_TOOLS_UNITRACE_UNICONTROL_H
