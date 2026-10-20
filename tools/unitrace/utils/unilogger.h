//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef UNITRACE_UTILS_UNILOGGER_H_
#define UNITRACE_UTILS_UNILOGGER_H_

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif /* _WIN32 */
#ifdef __linux__
#include <dirent.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#endif /* __linux__ */

#include "utils/unitrace_assert.h"

class UniLogger {
 public:
  UniLogger(const std::string& filename, bool lazy_flush = false, bool lock_free = false, bool binary = false) {
    if (!filename.empty()) {
      // Binary mode for the Perfetto protobuf stream; text otherwise.
      file_.open(filename, binary ? (std::ios::out | std::ios::binary) : std::ios::out);
      if (!(file_.is_open())) {
        std::cerr << "[ERROR] Failed to open file " << filename << " for writing. Do you have the right permission?" << std::endl;
        exit(-1);
      }
    }
    lazy_flush_ = lazy_flush;
    lock_free_ = lock_free;
    binary_ = binary;
    log_file_name_ = filename;
  }

  UniLogger(const UniLogger& that) = delete;

  ~UniLogger() {
    // Close() is idempotent: if the stream was already closed early (e.g. when
    // a session was stopped, see Close()), this is a no-op and does not touch
    // the (now released) file.
    Close();
  }

  // Flush buffered data and close the file so it is durable immediately, not
  // only at teardown. Later Log()/Flush() calls become no-ops (never fall
  // through to std::cerr). Idempotent: safe from an early stop and the dtor.
  void Close() {
    const std::lock_guard<std::mutex> lock(lock_);
    if (closed_) {
      return;
    }
    closed_ = true;
    if (file_.is_open()) {
      if (file_.tellp() == empty_pos_) {
        // Nothing was written: close without flushing and remove the empty file.
        file_.close();
        std::remove(log_file_name_.c_str());
      }
      else {
        // Data was written: flush it out, then close.
        file_ << std::flush;
        file_.close();
      }
    }
  }

#ifndef _WIN32
  // Fork child: the inherited stream shares the parent's file offset and holds
  // a copy of its unflushed bytes. Disown it WITHOUT flushing (those bytes are
  // the parent's) and open our own file. Single-threaded context: no lock.
  void ReopenAfterFork(const std::string& filename) {
    if (!file_.is_open()) {
      return;  // logging to stderr: nothing to disown
    }
    const std::string inherited_name = log_file_name_;
    // Park the inherited state in a filebuf that is never destroyed, since its
    // destructor would flush the copied bytes. One small leak per fork.
    std::filebuf* orphan = new std::filebuf();
    file_.rdbuf()->swap(*orphan);
    file_.clear();
    ReleaseInheritedDescriptor(inherited_name);

    log_file_name_ = filename;
    closed_ = false;
    empty_pos_ = 0;
    file_.open(filename, binary_ ? (std::ios::out | std::ios::binary) : std::ios::out);
    if (!file_.is_open()) {
      std::cerr << "[ERROR] Failed to open file " << filename << " for writing in forked process " << getpid() << std::endl;
      // Stay silent rather than fall through to the std::cerr path in Log().
      closed_ = true;
    }
  }

  // atfork prepare/after hooks: hold lock_ across fork() so no other thread
  // can leave it locked in the child. The forking thread owns the lock in both
  // processes, so a plain unlock is valid in the parent and the child.
  void LockForFork() { lock_.lock(); }
  void UnlockAfterFork() { lock_.unlock(); }
#endif /* _WIN32 */

  void Log(const std::string& text) {
    // Once closed, drop the write. Do NOT fall through to the std::cerr branch
    // below (that path is only for the "never opened a file" case); a closed
    // file must stay silent, not echo trace data to the terminal.
    if (closed_) {
      return;
    }
    if (file_.is_open()) {
      if (lock_free_) {
        file_ << text;
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
      else {
        const std::lock_guard<std::mutex> lock(lock_);
        file_ << text;
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
    } else {
      std::cerr << text;
      if (!lazy_flush_) {
        std::cerr << std::flush;
      }
    }
  }

  // Binary write overload (raw bytes), used by the Perfetto protobuf emitter.
  void Log(const void* data, size_t len) {
    // Once closed, drop the write (mirrors the string Log above). A late
    // protobuf packet after Close() must not fall through to the std::cerr
    // branch below, which would spray raw binary bytes onto the terminal.
    if (closed_) {
      return;
    }
    const std::streamsize count = static_cast<std::streamsize>(len);
    if (file_.is_open()) {
      if (lock_free_) {
        file_.write(static_cast<const char*>(data), count);
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
      else {
        const std::lock_guard<std::mutex> lock(lock_);
        file_.write(static_cast<const char*>(data), count);
        if (!lazy_flush_) {
          file_ << std::flush;
        }
      }
    } else {
      std::cerr.write(static_cast<const char*>(data), count);
      if (!lazy_flush_) {
        std::cerr << std::flush;
      }
    }
  }

  void Flush() {
    if (closed_) {
      return;
    }
    if (file_.is_open()) {
      if (lock_free_) {
        file_ << std::flush;
      }
      else {
        const std::lock_guard<std::mutex> lock(lock_);
        file_ << std::flush;
      }
    } else {
      std::cerr << std::flush;
    }
  }

  std::string GetLogFileName() const {
    return log_file_name_;
  }

  std::iostream::pos_type GetLogFilePosition() {
    return file_.tellp();
  }

  void SetEmptyPosition() {
    if (file_.is_open()) {
      empty_pos_ = file_.tellp();
    }
  }

  bool IsEmpty() {
    if (file_.is_open()) {
      return file_.tellp() == empty_pos_;
    }
    return true;
  }

  // Returns true if logging to file, false if logging to screen
  bool IsLogToFile() const {
    return file_.is_open();
  }

 private:
#ifndef _WIN32
  // Close the inherited descriptor behind the orphaned filebuf so the child
  // does not hold the parent's file open. The stream API hides the fd; on Linux
  // find it via /proc/self/fd, elsewhere leave it open (nothing writes to it).
  static void ReleaseInheritedDescriptor(const std::string& name) {
#ifdef __linux__
    char target[PATH_MAX];
    if (realpath(name.c_str(), target) == nullptr) {
      return;
    }
    DIR* dir = opendir("/proc/self/fd");
    if (dir == nullptr) {
      return;
    }
    const int dir_fd = dirfd(dir);
    while (struct dirent* ent = readdir(dir)) {
      char* end = nullptr;
      const long fd = strtol(ent->d_name, &end, 10);
      if (end == ent->d_name || *end != '\0' || fd == dir_fd) {
        continue;
      }
      char link[PATH_MAX];
      const ssize_t n = readlinkat(dir_fd, ent->d_name, link, sizeof(link) - 1);
      if (n < 0) {
        continue;
      }
      link[n] = '\0';
      if (strcmp(link, target) == 0) {
        close(static_cast<int>(fd));
        break;
      }
    }
    closedir(dir);
#else  /* __linux__ */
    (void)name;
#endif /* __linux__ */
  }
#endif /* _WIN32 */

  std::string log_file_name_;
  std::mutex lock_;
  std::ofstream file_;
  std::iostream::pos_type empty_pos_ = 0;
  bool lazy_flush_;
  bool lock_free_;	// caller deal with concurrency?
  bool binary_ = false;
  bool closed_ = false;	// true once Close() has flushed and released the file
};

#endif // UNITRACE_UTILS_UNILOGGER_H_
