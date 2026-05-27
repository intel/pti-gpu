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
#include <fstream>
#include "utils.h"
#include "logger.h"

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
  LOGGER_TYPE_KPROPS,
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
    const std::string& GetRank() const { return rank_; }

    std::shared_ptr<Logger> GetLogger(LoggerType type, bool lazy_flush = false, bool lock_free = false) const {
        int32_t device_id = -1;
        return GetDeviceLogger(type, device_id, lazy_flush, lock_free);
    }

    LoggerFactory(const LoggerFactory&) = delete;
    LoggerFactory& operator=(const LoggerFactory&) = delete;
    virtual ~LoggerFactory() = default;

    virtual std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false) const  = 0;
    virtual std::string GenerateLogFileName(LoggerType type, int32_t device_id = -1) const = 0;
#ifdef _WIN32
    virtual std::pair<std::string, std::string> SearchRawMetricFiles(uint32_t pid) const = 0;
#endif // _WIN32
    virtual std::vector<std::string> SearchFilesByType(LoggerType type, int32_t device_id) const = 0;

protected:
    static std::string GetAppName(void);
    explicit LoggerFactory(uint32_t app_id);
    void CreateDirectory(const std::string& dir) const;
    std::shared_ptr<Logger> GetLoggerImpl(LoggerType type, int32_t device_id, bool lazy_flush, bool lock_free) const;
    void SetAppId(uint32_t app_id) {app_id_ = app_id;}

    uint32_t app_id_;
    const std::string app_name_;
    const std::string rank_;
    std::string dir_path_; // Directory path for output, empty by default
    std::string data_dir_path_; // path for temporary files
    mutable std::mutex mutex_;
    mutable std::map<std::pair<LoggerType, int32_t>, std::shared_ptr<Logger>> loggers_;
};

class LegacyLoggerFactory : public LoggerFactory {
public:
    LegacyLoggerFactory(uint32_t app_id, const std::string& log_file);

    std::string GetLogFileName(const std::string& logfile, uint32_t pid = (std::numeric_limits<uint32_t>::max)()) const;
    std::string GetMetricsFileName(const std::string& log_file, uint32_t app_id) const;

    std::string GetChromeTraceFileName() const;
    std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false) const override;
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

    std::shared_ptr<Logger> GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush = false, bool lock_free = false) const override;
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
