//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <fstream>
#include "utils.h"
#include "logger.h"

// JSON is the default; protobuf only when --output-format=protobuf and built in.
// Resolved once (queried per record on the flush path); the format is fixed for
// the process lifetime.
inline bool UseProtobufOutput() {
#if BUILD_WITH_PERFETTO
  static const bool use_protobuf = (utils::GetEnv("UNITRACE_OutputFormat") == "protobuf");
  return use_protobuf;
#else  /* BUILD_WITH_PERFETTO */
  return false;
#endif /* BUILD_WITH_PERFETTO */
}

inline const char* TimelineTraceExt() {
  return UseProtobufOutput() ? "pftrace" : "json";
}

enum LoggerType {
  LOGGER_TYPE_LEGACY_SHARED_TRACE = 0,
  LOGGER_TYPE_TRACE_HOST_TIMING,
  LOGGER_TYPE_TRACE_DEVICE_TIMING,
  LOGGER_TYPE_TRACE_DEVICE_SUBMISSION,
  LOGGER_TYPE_CHROME_TRACE_UNITRACE,
  LOGGER_TYPE_METRICS_SAMPLING,
  LOGGER_TYPE_METRICS_SAMPLING_TEMP,
  LOGGER_TYPE_METRICS_QUERY,
  LOGGER_TYPE_METRICS_QUERY_TEMP,
  // The kernel index: one record per kernel naming it, the base address the driver
  // gave it, its command id and which dumped module binary holds its ISA. The extent
  // is not in the record - the reader derives it from the binary. See
  // ZeMetricProfiler::ReadModuleAndKernelProps, which documents the handoff. The
  // binaries themselves are separate files named by the records, since there is one
  // per module rather than one per device.
  LOGGER_TYPE_KPROPS,
  // No type for the disassembly artifacts: one is named after the kernel it holds,
  // not after a device, so it never fit GenerateLogFileName's (type, device_id)
  // shape. See GetDisasmFileName, which names them instead.
  LOGGER_TYPE_KTIME,
  LOGGER_TYPE_CONFIG,
  LOGGER_TYPE_TRACE_CALL_LOGGING,
  LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT,
  LOGGER_TYPE_TRACE_DEVICE_TIMELINE,
  LOGGER_TYPE_KMD_TRACE
};

class LegacyLoggerFactory;
class ResultDirLoggerFactory;

class LoggerFactory {
public:
    // Returns a singleton instance (do not delete the pointer)
    static LoggerFactory* Create(uint32_t app_id = (std::numeric_limits<uint32_t>::max)());

    bool IsLegacy() const;
    bool IsResultDir() const;
    const std::string& GetAppName() const { return app_name_; }
    const std::string& GetRank() const { return rank_; }

    // Directory for the per-kernel assembly files, "asm" beside the run's other output.
    //
    // These are results, not intermediates, so they deliberately do not go to
    // UNITRACE_DataDir: TearDown() removes that directory wholesale at exit, which
    // would delete them before anyone could read them. They land beside the
    // sampling report that references them instead - the same process writes both.
    //
    // Created on first use, so callers should hoist the call out of a per-kernel
    // loop rather than calling it per file.
    std::string GetDisasmDirPath() const;

    // Name of one kernel's disassembly artifact in disasm_dir:
    //   <disasm_dir>/<device_id>.<pid>.<kernel_id>.<ext>
    //
    // Nothing in the name says what these are, the directory they sit in saying it once
    // for all of them. It is the same name in either mode, since the directory is already
    // per-run in both (see GetDisasmDirPath) - there is nothing left here for a dot or an
    // underscore to keep apart.
    //
    // The directory is passed in so a per-kernel loop does not re-create it per file.
    //
    // The pid is part of the name because the ranks of one MPI run share this directory
    // and a kernel command id is only unique within a process. Without it one rank's
    // artifacts would silently overwrite another's with the wrong kernel's disassembly.
    std::string GetDisasmFileName(const std::string& disasm_dir, int32_t device_id, uint32_t pid,
                                  uint64_t kernel_id, const std::string& ext) const;

    // Base name of one dumped module binary in the data directory:
    //   [.]module[._]<pid>.<index>.bin
    //
    // A base name, not a path, because the writer and the reader reach the data
    // directory differently. Both call this rather than either building the name
    // itself: a kernel record carries only (pid, index), so this is the whole of
    // what turns a record into the file holding its ISA, and a disagreement between
    // the two sides would surface as a kernel that silently cannot be disassembled.
    //
    // No device id in the name. The index is unique within the process that assigned
    // it, and the pid separates the ranks of one MPI run, which is the only reason
    // these names have to be qualified at all - so a module never has to be
    // associated with a device just to be written.
    std::string GetModuleBinaryFileName(uint32_t pid, uint32_t index) const;

    // The pid out of a per-device file name written by another process:
    //   [.]<type>[._]<device_id>.<pid>.txt  ->  <pid>
    //
    // The counterpart of the app_id_ that GenerateLogFileName puts there, for the
    // reader of a file it did not write. A .kprops file is written by one process and
    // its records do not repeat that process's id per kernel (see
    // ZeMetricProfiler::ReadModuleAndKernelProps), so this is where their pid comes
    // from - and the pid is what tells one rank's module binaries and disassembly
    // artifacts from another's.
    //
    // Static and free of the naming scheme's leading punctuation: it reads only the
    // trailing ".<pid>.txt", which both schemes share, so the caller does not have to
    // know which factory wrote the file it found. Returns 0 if the name does not end
    // that way, which no file this searches for can.
    static uint32_t GetFileNamePid(const std::string& path);

    std::shared_ptr<Logger> GetLogger(LoggerType type, bool lazy_flush = false, bool lock_free = false, bool binary = false) const {
        int32_t device_id = -1;
        return GetDeviceLogger(type, device_id, lazy_flush, lock_free, binary);
    }

    LoggerFactory(const LoggerFactory&) = delete;
    LoggerFactory& operator=(const LoggerFactory&) = delete;
    virtual ~LoggerFactory() = default;

    virtual std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false, bool binary = false) const  = 0;
    virtual std::string GenerateLogFileName(LoggerType type, int32_t device_id = -1) const = 0;
#ifdef _WIN32
    virtual std::pair<std::string, std::string> SearchRawMetricFiles(uint32_t pid) const = 0;
#endif // _WIN32
    virtual std::vector<std::string> SearchFilesByType(LoggerType type, int32_t device_id) const = 0;

protected:
    static std::string ComputeAppName(void);
    explicit LoggerFactory(uint32_t app_id);
    void CreateDirectory(const std::string& dir) const;
    std::shared_ptr<Logger> GetLoggerImpl(LoggerType type, int32_t device_id, bool lazy_flush, bool lock_free, bool binary = false) const;
    void RemoveDirectoryIfEmpty(const std::string& dir) const;
    void SetAppId(uint32_t app_id) {app_id_ = app_id;}
    std::string GetDataDirPath(bool warn = true) const {
        std::string data_dir = utils::GetEnv("UNITRACE_DataDir");
        if (data_dir.empty() && warn) {
          std::cerr << "[ERROR] Data directory is missing or not specified." << std::endl;
          exit(-1); // Bail out if data directory is not specified at this point.
        }
        return data_dir;
    }

    uint32_t app_id_;
    const std::string app_name_;
    const std::string rank_;
    std::string dir_path_; // Directory path for output, empty by default
    // Set by GetDisasmDirPath() to the directory it created, so that a subclass can
    // clean it up without calling that method again and re-creating it.
    mutable std::string disasm_dir_path_;
    mutable std::mutex mutex_;
    mutable std::map<std::tuple<LoggerType, int32_t, bool>, std::shared_ptr<Logger>> loggers_;
};

class LegacyLoggerFactory : public LoggerFactory {
public:
    LegacyLoggerFactory(uint32_t app_id, const std::string& log_file);

    std::string GetLogFileName(const std::string& logfile, uint32_t pid = (std::numeric_limits<uint32_t>::max)()) const;
    std::string GetMetricsFileName(const std::string& log_file, uint32_t app_id) const;

    std::string GetChromeTraceFileName() const;
    std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false, bool binary = false) const override;
    std::string GenerateLogFileName(LoggerType type, int32_t device_id = -1) const override;
#ifdef _WIN32
    std::pair<std::string, std::string> SearchRawMetricFiles(uint32_t pid) const override;
#endif // _WIN32
    std::vector<std::string> SearchFilesByType(LoggerType type, int32_t device_id) const override;
    void AdjustLoggerTypeAndDeviceId(LoggerType& type, int32_t& device_id) const;

private:
    const std::string log_file_;
};

class ResultDirLoggerFactory : public LoggerFactory {
public:
    ResultDirLoggerFactory(uint32_t app_id);
    ~ResultDirLoggerFactory() override;

    std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false, bool binary = false) const override;
    std::string GenerateLogFileName(LoggerType type, int32_t device_id = -1) const override;
#ifdef _WIN32
    std::pair<std::string, std::string> SearchRawMetricFiles(uint32_t pid) const override;
#endif // _WIN32
    std::vector<std::string> SearchFilesByType(LoggerType type, int32_t device_id) const override;
    std::string GetTraceLogFileName(LoggerType type) const;

private:
    std::string result_dir_;
    std::string metrics_dir_path_;
};
