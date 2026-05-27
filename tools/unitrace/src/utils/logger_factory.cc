//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "logger_factory.h"

#if CXX_STD_FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::experimental::filesystem
#else /* CXX_STD_FILESYSTEM_EXPERIMENTAL */
#include <filesystem>
#define CXX_STD_FILESYSTEM_NAMESPACE std::filesystem
#endif /* CXX_STD_FILESYSTEM_EXPERIMENTAL */

#include <thread>

// LoggerFactory base constructor
LoggerFactory::LoggerFactory(uint32_t app_id)
    : app_id_(app_id),
      dir_path_(""),
      app_name_(GetAppName()),
      rank_((utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK")),
      data_dir_path_(utils::GetEnv("UNITRACE_DataDir"))
{}

// LoggerFactory directory creation helper
void LoggerFactory::CreateDirectory(const std::string& dir) const {
    if (dir.empty()) return;

    try {
        if (!CXX_STD_FILESYSTEM_NAMESPACE::exists(dir)) {
            CXX_STD_FILESYSTEM_NAMESPACE::create_directories(dir);
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to create directory '" << dir << "': " << e.what() << std::endl;
    }
}

// LoggerFactory singleton implementation
LoggerFactory* LoggerFactory::Create(uint32_t app_id) {
    static std::unique_ptr<LoggerFactory> instance;
    if (!instance) {
        if (app_id == (std::numeric_limits<uint32_t>::max)()) {
            app_id = utils::GetPid();
        }

        const char* env = std::getenv("UNITRACE_UseResultDirectory");
        if (env && std::string(env) == "1") {
            instance = std::make_unique<ResultDirLoggerFactory>(app_id);
        } else {
            std::string value = utils::GetEnv("UNITRACE_LogToFile");
            std::string log_file;
            if (!value.empty() && value == "1") {
                log_file = utils::GetEnv("UNITRACE_LogFilename");
                PTI_ASSERT(!log_file.empty());
            }
            instance = std::make_unique<LegacyLoggerFactory>(app_id, log_file);
        }
    }
    else {
        if (app_id != (std::numeric_limits<uint32_t>::max)()) {
            instance->SetAppId(app_id);
        }
    }
    return instance.get();
}

std::shared_ptr<Logger> LoggerFactory::GetLoggerImpl(LoggerType type, int32_t device_id, bool lazy_flush, bool lock_free) const {
    std::lock_guard<std::mutex> guard(mutex_);

    auto key = std::make_pair(type, device_id);
    auto it = loggers_.find(key);
    if (it != loggers_.end()) {
        return it->second;
    }
    std::string filename = GenerateLogFileName(type, device_id);
    auto logger = std::make_shared<Logger>(filename, lazy_flush, lock_free);
    loggers_[key] = logger;
    return logger;
}

void LegacyLoggerFactory::AdjustLoggerTypeAndDeviceId(LoggerType& type, int32_t& device_id) const {
    
    if (type == LOGGER_TYPE_TRACE_HOST_TIMING ||
        type == LOGGER_TYPE_TRACE_DEVICE_TIMING ||
        type == LOGGER_TYPE_TRACE_DEVICE_SUBMISSION ||
        type == LOGGER_TYPE_TRACE_CALL_LOGGING ||
        type == LOGGER_TYPE_TRACE_DEVICE_TIMELINE ||
        type == LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT) {
        type = LOGGER_TYPE_LEGACY_SHARED_TRACE;
    }

    if (type == LOGGER_TYPE_METRICS_QUERY || type == LOGGER_TYPE_METRICS_SAMPLING) {
        device_id = -1;
    }
}

std::shared_ptr<Logger> LegacyLoggerFactory::GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush, bool lock_free) const{
    AdjustLoggerTypeAndDeviceId(type, device_id);
    return GetLoggerImpl(type, device_id, lazy_flush, lock_free);
}

std::string LoggerFactory::GetAppName(void) {
#ifdef _WIN32
    char str[256];
    if (GetModuleFileNameA(nullptr, str, sizeof(str))) {
        std::string name(str);
        // Remove path
        auto pos = name.find_last_of('\\');
        if (pos != std::string::npos) {
            name = name.substr(pos + 1);
        }
        // Remove .exe extension if present (case-insensitive)
        auto ext_pos = name.find_last_of('.');
        if (ext_pos != std::string::npos) {
            std::string ext = name.substr(ext_pos);
            for (auto& c : ext) c = tolower(c);
            if (ext == ".exe") {
                name = name.substr(0, ext_pos);
            }
        }
        return name;
    }
#else /* _WIN32 */
    std::ifstream comm("/proc/self/comm");
    if (comm) {
        std::string name;
        std::getline(comm, name);
        comm.close();
        if (!name.empty()) {
            return std::move(name);
        }
    }
#endif /* _WIN32 */
    return "unitrace";
}

// LegacyLoggerFactory constructor
LegacyLoggerFactory::LegacyLoggerFactory(uint32_t app_id, const std::string& log_file)
    : LoggerFactory(app_id), log_file_(log_file) {
    std::string dir = utils::GetEnv("UNITRACE_TraceOutputDir");
    if (!dir.empty()) {
        CreateDirectory(dir);
        dir_path_ = dir;
    }
}

// LegacyLoggerFactory helpers
std::string LegacyLoggerFactory::GetLogFileName(const std::string& logfile, uint32_t pid) const {
    if (logfile.empty()) {
        return logfile;
    }

    size_t pos = logfile.find_last_of('.');

    std::string result;
    if (pos == std::string::npos) {
        result = logfile;
    } else {
        result = logfile.substr(0, pos);
    }

    if (pid == (std::numeric_limits<uint32_t>::max)()) {
        pid = utils::GetPid();
    }
    result += "." + std::to_string(pid);

    if (!rank_.empty()) {
        result += "." + rank_;
    }

    if (pos != std::string::npos) {
        result += logfile.substr(pos);
    }

    return result;
}

std::string LegacyLoggerFactory::GetMetricsFileName(const std::string& log_file, uint32_t app_id) const{
    std::string log_file_name = GetLogFileName(log_file, app_id);
    if (log_file_name.empty()) {
        return log_file_name;
    }
    std::string filename;
    size_t pos = log_file_name.find_first_of('.');
    if (pos == std::string::npos) {
        filename = log_file_name;
    } else {
        filename = log_file_name.substr(0, pos);
    }
    filename = filename + ".metrics";
    if (pos != std::string::npos) {
        filename = filename + log_file_name.substr(pos);
    }
    return filename;
}

std::string LegacyLoggerFactory::GetChromeTraceFileName() const {
    std::string filename = app_name_;
    if (!dir_path_.empty()) {
        filename = (dir_path_ + '/' + filename);
    }

    if (!rank_.empty()) {
        return filename + "." + std::to_string(utils::GetPid()) + "." + rank_ + ".json";
    }
    return filename + "." + std::to_string(utils::GetPid()) + ".json";
}

// LegacyLoggerFactory::GenerateLogFileName implementation
std::string LegacyLoggerFactory::GenerateLogFileName(LoggerType type, int32_t device_id) const {
    switch (type) {
        case LOGGER_TYPE_LEGACY_SHARED_TRACE:
        case LOGGER_TYPE_TRACE_HOST_TIMING:
        case LOGGER_TYPE_TRACE_DEVICE_TIMING:
        case LOGGER_TYPE_TRACE_DEVICE_SUBMISSION:
        case LOGGER_TYPE_TRACE_CALL_LOGGING:
        case LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT:
        case LOGGER_TYPE_TRACE_DEVICE_TIMELINE:
            return GetLogFileName(log_file_, app_id_);
        case LOGGER_TYPE_CHROME_TRACE_UNITRACE:
            return GetChromeTraceFileName();
        case LOGGER_TYPE_METRICS_QUERY:
        case LOGGER_TYPE_METRICS_SAMPLING:
            return GetMetricsFileName(log_file_, app_id_);
        case LOGGER_TYPE_KPROPS:
            return data_dir_path_ + "/.kprops."  + std::to_string(device_id) + "." + std::to_string(app_id_) + ".txt";
        case LOGGER_TYPE_KTIME:
            return data_dir_path_ + "/.ktime."  + std::to_string(device_id) + "." + std::to_string(app_id_) + ".txt";
        case LOGGER_TYPE_METRICS_QUERY_TEMP:
            return data_dir_path_ + "/.metrics." + std::to_string(app_id_) + ".q";
        case LOGGER_TYPE_METRICS_SAMPLING_TEMP:
            {
                std::string metric_group = utils::GetEnv("UNITRACE_MetricGroup");
                return data_dir_path_ + "/." + std::to_string(device_id) + "." + metric_group + "." + std::to_string(app_id_) + ".t";
            }
        case LOGGER_TYPE_KMD_TRACE:
            return GetLogFileName("oskmd.json", app_id_);
        default:
            PTI_ASSERT(false);
            return "";
    }
}

#ifdef _WIN32
// For windows - father process need to find .q file that was created by child process
// returns pair of <temporary file path, final file path>
std::pair<std::string, std::string> LegacyLoggerFactory::SearchRawMetricFiles(uint32_t pid) const {
    std::pair<std::string, std::string> result;

    std::string temp_file = data_dir_path_ + "/.metrics." + std::to_string(pid) + ".q";
    std::string final_file = GetMetricsFileName(log_file_, pid);
    result = std::make_pair(temp_file, final_file);
    return result;
}
#endif // _WIN32

// Father process need to find all temp files that were created by child process in order to create final metrics file
std::vector<std::string> LegacyLoggerFactory::SearchFilesByType(LoggerType type, int32_t device_id) const {
    std::vector<std::string> result;
    std::string prefix;
    switch (type)
    {
        case LOGGER_TYPE_KPROPS:
            prefix = ".kprops.";
            break;
        case LOGGER_TYPE_KTIME:
            prefix = ".ktime.";
            break;
        default:
            std::cerr << "[ERROR] LegacyLoggerFactory::SearchFilesByType Unknown invalid type: " << type << std::endl;
            return result;
    }
    
    if (!data_dir_path_.empty() && CXX_STD_FILESYSTEM_NAMESPACE::exists(data_dir_path_) && CXX_STD_FILESYSTEM_NAMESPACE::is_directory(data_dir_path_)) {
        for (const auto& e : CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir_path_))) {
            if (e.path().filename().string().find(prefix + std::to_string(device_id)) == 0) {
                result.push_back(e.path().string());
            }
        }
    }
    return result;
}

std::shared_ptr<Logger> ResultDirLoggerFactory::GetDeviceLogger(LoggerType type, int32_t device_id, bool lazy_flush, bool lock_free) const{
    return GetLoggerImpl(type, device_id, lazy_flush, lock_free);
}

// ResultDirLoggerFactory constructor
ResultDirLoggerFactory::ResultDirLoggerFactory(uint32_t app_id)
    : LoggerFactory(app_id) {
    result_dir_ = utils::GetEnv("UNITRACE_ResultDirectory");
    if (result_dir_.empty()) {
        std::cerr << "[ERROR] Result directory is missing or not specified." << std::endl;
        exit(-1);
    }

    std::string dir = result_dir_;
    dir += "/";
    if (!rank_.empty()) {
        dir += "rank_" + rank_ + ".";
    }

    dir += app_name_ + "." + std::to_string(app_id_);

    dir_path_ = dir;
    metrics_dir_path_ = dir + "/metrics";
    CreateDirectory(dir_path_);
}

std::string ResultDirLoggerFactory::GetTraceLogFileName(LoggerType type) const{
    std::string filename;

    switch (type) {
        case LOGGER_TYPE_TRACE_HOST_TIMING:
            filename = "host_timing.txt";
            break;
        case LOGGER_TYPE_TRACE_DEVICE_TIMING:
            filename = "device_timing.txt";
            break;
        case LOGGER_TYPE_TRACE_DEVICE_SUBMISSION:
            filename = "device_submission.txt";
            break;
        case LOGGER_TYPE_TRACE_CALL_LOGGING:
            filename = "call_logging.txt";
            break;
        case LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT:
            filename = "ccl_summary_report.txt";
            break;
        case LOGGER_TYPE_TRACE_DEVICE_TIMELINE:
            filename = "device_timeline.txt";
            break;
        case LOGGER_TYPE_KMD_TRACE:
            filename = "oskmd.json";
            break;
        default:
            std::cerr << "[ERROR] ResultDirLoggerFactory::GetTraceLogFileName Unknown logger type: " << type << std::endl;
            break;
    }

    return dir_path_ + '/' + filename;
}

// ResultDirLoggerFactory::GenerateLogFileName implementation
std::string ResultDirLoggerFactory::GenerateLogFileName(LoggerType type, int32_t device_id) const {
    switch (type) {
        case LOGGER_TYPE_CONFIG:
            return dir_path_ + '/' + "run_config.json";
        case LOGGER_TYPE_TRACE_HOST_TIMING:
        case LOGGER_TYPE_TRACE_DEVICE_TIMING:
        case LOGGER_TYPE_TRACE_DEVICE_SUBMISSION:
        case LOGGER_TYPE_TRACE_CALL_LOGGING:
        case LOGGER_TYPE_TRACE_CCL_SUMMARY_REPORT:
        case LOGGER_TYPE_TRACE_DEVICE_TIMELINE:
        case LOGGER_TYPE_KMD_TRACE:
            return GetTraceLogFileName(type);
        case LOGGER_TYPE_CHROME_TRACE_UNITRACE:
            return dir_path_ + "/chrome_trace.json";
        case LOGGER_TYPE_METRICS_QUERY:
            CreateDirectory(metrics_dir_path_);
            return metrics_dir_path_ + "/metrics_" + std::to_string(device_id) + ".csv";
        case LOGGER_TYPE_METRICS_SAMPLING:
            CreateDirectory(metrics_dir_path_);
            return metrics_dir_path_ + "/metrics_" + std::to_string(device_id) + ".csv";
        case LOGGER_TYPE_KPROPS:
            return data_dir_path_ + "/kprops_" + std::to_string(device_id) + "." + std::to_string(app_id_) +  ".txt";
        case LOGGER_TYPE_KTIME:
            return data_dir_path_ + "/ktime_" + std::to_string(device_id) + "." + std::to_string(app_id_) + ".txt";
        case LOGGER_TYPE_METRICS_QUERY_TEMP:
            return data_dir_path_ + "/metrics." + std::to_string(app_id_) + ".q";
        case LOGGER_TYPE_METRICS_SAMPLING_TEMP:
            {
                std::string metric_group = utils::GetEnv("UNITRACE_MetricGroup");
                return data_dir_path_ + "/" + std::to_string(device_id) + "." + metric_group + "." + std::to_string(app_id_) + ".t";
            }
        default:
            PTI_ASSERT(false);
            return "";
    }
}

#ifdef _WIN32
// for windows - father process need to find .q file that was created by child process and prepare final metrics files
std::pair<std::string, std::string> ResultDirLoggerFactory::SearchRawMetricFiles(uint32_t pid) const {
    std::pair<std::string, std::string> result;
    std::string temp_file = data_dir_path_ + "/metrics." + std::to_string(pid) + ".q";
    if (result_dir_.empty() || !CXX_STD_FILESYSTEM_NAMESPACE::exists(result_dir_) || !CXX_STD_FILESYSTEM_NAMESPACE::is_directory(result_dir_)) {
        return result;
    }

    for (const auto& entry : CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(result_dir_)) {
        if (entry.is_directory()) {
            std::string dirname = entry.path().filename().string();
            if (dirname.find(std::to_string(pid)) != std::string::npos) {
                CXX_STD_FILESYSTEM_NAMESPACE::path metrics_path = entry.path() / "metrics";
                CreateDirectory(metrics_path.string());
                std::string csv_file = (metrics_path / "metrics.csv").string();
                result = std::make_pair(temp_file, csv_file);
                break;
            }
        }
    }
    return result;
}
#endif // _WIN32

std::vector<std::string> ResultDirLoggerFactory::SearchFilesByType(LoggerType type, int32_t device_id) const {
    std::vector<std::string> result;
    std::string prefix;

    switch (type)
    {
        case LOGGER_TYPE_KPROPS:
            prefix = "kprops_";
            break;
        case LOGGER_TYPE_KTIME:
            prefix = "ktime_";
            break;
        default:
            std::cerr << "[ERROR] ResultDirLoggerFactory::SearchFilesByType Unknown invalid type: " << type << std::endl;
            return result;
    }

    if (!data_dir_path_.empty() && CXX_STD_FILESYSTEM_NAMESPACE::exists(data_dir_path_) && CXX_STD_FILESYSTEM_NAMESPACE::is_directory(data_dir_path_)) {
        for (const auto& e : CXX_STD_FILESYSTEM_NAMESPACE::directory_iterator(CXX_STD_FILESYSTEM_NAMESPACE::path(data_dir_path_))) {
            if (e.path().filename().string().find(prefix + std::to_string(device_id)) == 0) {
                result.push_back(e.path().string());
            }
        }
    }
    return result;
}

bool LoggerFactory::IsLegacy() const {
    return dynamic_cast<const LegacyLoggerFactory*>(this) != nullptr;
}

bool LoggerFactory::IsResultDir() const {
    return dynamic_cast<const ResultDirLoggerFactory*>(this) != nullptr;
}
