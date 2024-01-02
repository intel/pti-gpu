//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
#define PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_

/*
 * Level zero collection methods:  hook into the level zero api to capture
 * kernel/memory movement, collect relevent timing and handles key structures
 * and issue callbacks to buffer,etc interfaces to capture this data in view
 * records.
 */

#include <dlfcn.h>
#include <level_zero/layers/zel_tracing_api.h>
#include <level_zero/ze_api.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "collector_options.h"
#include "common_header.gen"
#include "overhead_kinds.h"
#include "unicontrol.h"
#include "unikernel.h"
#include "unitimer.h"
#include "utils.h"
#include "ze_event_cache.h"
#include "ze_utils.h"

struct CallbacksEnabled {
  std::atomic<bool> acallback = false;
};

struct ZeMetricQueryPools {
  ZeMetricQueryPools() {}
  ZeMetricQueryPools(const ZeMetricQueryPools&) = delete;
  ZeMetricQueryPools& operator=(const ZeMetricQueryPools&) = delete;
  ZeMetricQueryPools(ZeMetricQueryPools&&) = delete;
  ZeMetricQueryPools& operator=(ZeMetricQueryPools&&) = delete;

  ~ZeMetricQueryPools() {
    ze_result_t status;

    for (auto it = query_pool_map_.begin(); it != query_pool_map_.end(); it++) {
      overhead::Init();
      status = zetMetricQueryDestroy(it->first);
      {
        std::string o_api_string = "zetMetricQueryDestroy";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      }
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
    query_pool_map_.clear();

    for (auto it = pools_.begin(); it != pools_.end(); it++) {
      overhead::Init();
      status = zetMetricQueryPoolDestroy(*it);
      {
        std::string o_api_string = "zetMetricQueryPoolDestroy";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      }
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    pools_.clear();

    free_pool_.clear();
  }

  constexpr static uint32_t pool_size_ = 128;
  std::map<zet_metric_query_handle_t,
           std::tuple<ze_context_handle_t, ze_device_handle_t, zet_metric_group_handle_t>>
      query_pool_map_;
  std::map<std::tuple<ze_context_handle_t, ze_device_handle_t, zet_metric_group_handle_t>,
           std::vector<zet_metric_query_handle_t>>
      free_pool_;
  std::vector<zet_metric_query_pool_handle_t> pools_;

  zet_metric_query_handle_t GetQuery(ze_context_handle_t context, ze_device_handle_t device,
                                     zet_metric_group_handle_t group) {
    ze_result_t status;
    zet_metric_query_handle_t query;

    auto it = free_pool_.find({context, device, group});
    if (it == free_pool_.end()) {
      // no pools created

      zet_metric_query_pool_desc_t desc = {ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC, nullptr,
                                           ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE, pool_size_};
      zet_metric_query_pool_handle_t pool;

      overhead::Init();
      status = zetMetricQueryPoolCreate(context, device, group, &desc, &pool);
      {
        std::string o_api_string = "zetMetricQueryPoolCreate";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      pools_.push_back(pool);

      std::vector<zet_metric_query_handle_t> queries;
      for (uint32_t i = 0; i < pool_size_ - 1; i++) {
        overhead::Init();
        status = zetMetricQueryCreate(pool, i, &query);
        {
          std::string o_api_string = "zetMetricQueryCreate";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        queries.push_back(query);
        query_pool_map_.insert({query, {context, device, group}});
      }
      overhead::Init();
      status = zetMetricQueryCreate(pool, pool_size_ - 1, &query);
      {
        std::string o_api_string = "zetMetricQueryCreate";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      query_pool_map_.insert({query, {context, device, group}});

      free_pool_.insert({{context, device, group}, queries});
    } else {
      if (it->second.size() == 0) {
        // no free queries, create a new pool

        zet_metric_query_pool_desc_t desc = {ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC, nullptr,
                                             ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE, pool_size_};
        zet_metric_query_pool_handle_t pool;

        overhead::Init();
        status = zetMetricQueryPoolCreate(context, device, group, &desc, &pool);
        {
          std::string o_api_string = "zetMetricQueryPoolCreate";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        pools_.push_back(pool);

        for (uint32_t i = 0; i < pool_size_ - 1; i++) {
          overhead::Init();
          status = zetMetricQueryCreate(pool, i, &query);
          {
            std::string o_api_string = "zetMetricQueryCreate";
            overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                 o_api_string.c_str());
          };
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          it->second.push_back(query);
          query_pool_map_.insert({query, {context, device, group}});
        }
        overhead::Init();
        status = zetMetricQueryCreate(pool, pool_size_ - 1, &query);
        {
          std::string o_api_string = "zetMetricQueryCreate";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        query_pool_map_.insert({query, {context, device, group}});
      } else {
        query = it->second.back();
        it->second.pop_back();
      }
    }

    return query;
  }

  void PutQuery(zet_metric_query_handle_t query) {
    auto it = query_pool_map_.find(query);
    if (it == query_pool_map_.end()) {
      return;
    }
    auto it2 = free_pool_.find(it->second);
    PTI_ASSERT(it2 != free_pool_.end());
    it2->second.push_back(query);
  }

  void ResetQuery(zet_metric_query_handle_t query) {
    if (query_pool_map_.find(query) == query_pool_map_.end()) {
      return;
    }
    overhead::Init();
    ze_result_t status = zetMetricQueryReset(query);
    {
      std::string o_api_string = "zetMetricQueryReset";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }
};

struct ZeInstanceData {
  uint64_t start_time_host;
  uint64_t end_time_host;
  uint64_t kid;  // passing kid from enter callback to exit callback
};

thread_local ZeInstanceData ze_instance_data;

struct ZeFunction {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ZeFunction& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ZeFunction& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

using ZeFunctionInfoMap = std::map<std::string, ZeFunction>;

typedef void (*OnZeFunctionFinishCallback)(void* data, std::vector<uint64_t>* kids,
                                           FLOW_DIR flow_dir, API_TRACING_ID api_id,
                                           uint64_t started, uint64_t ended);

ze_result_t (*zexKernelGetBaseAddress)(ze_kernel_handle_t hKernel, uint64_t* baseAddress) = nullptr;

struct ZeSyncPoint {
  uint64_t host_sync;
  uint64_t device_sync;
};

struct ZeKernelGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

enum KERNEL_COMMAND_TYPE {
  KERNEL_COMMAND_TYPE_INVALID = 0,
  KERNEL_COMMAND_TYPE_KERNEL = 1,
  KERNEL_COMMAND_TYPE_MEMORY = 2,
  KERNEL_COMMAND_TYPE_COMMAND = 3
};

struct ZeKernelCommandProps {
  std::string name;
  KERNEL_COMMAND_TYPE type;
  size_t simd_width;
  size_t bytes_transferred;
  uint32_t group_count[3];
  uint32_t group_size[3];
  uint64_t base_addr;
  uint32_t size;
  size_t value_size;
  std::byte* value_array;
};

struct ZeKernelCommand {
  ZeKernelCommandProps props;
  ze_event_handle_t event = nullptr;
  zet_metric_query_handle_t metric_query = nullptr;
  ze_event_handle_t metric_query_event = nullptr;
  ze_device_handle_t device =
      nullptr;  // Device where the operation is submitted, associated with command list
  ze_device_handle_t src_device = nullptr;  // Device for p2p memcpy, source of copy data
  ze_device_handle_t dst_device = nullptr;  // Device for p2p memcpy, destination of copy data
  uint64_t kernel_id = 0;
  uint64_t append_time = 0;
  ze_command_list_handle_t command_list = nullptr;
  ze_command_queue_handle_t queue = nullptr;
  ze_fence_handle_t fence;
  uint64_t submit_time = 0;
  uint64_t tid = 0;
  uint64_t sycl_node_id_ = 0;
  uint32_t sycl_invocation_id_ = 0;
  uint64_t sycl_task_begin_time_ = 0;
  uint64_t sycl_enqk_begin_time_ = 0;
  std::string source_file_name_ = "";
  uint32_t source_line_number_ = 0;
  uint32_t corr_id_ = 0;
};

struct ZeKernelProfileTimestamps {
  uint32_t device_id;
  uint64_t metric_start;
  uint64_t metric_end;
  uint64_t device_start;
  uint64_t device_end;
};

struct ZeKernelProfileRecord {
  ze_device_handle_t device = nullptr;
  std::vector<ZeKernelProfileTimestamps> timestamps;
  ZeKernelCommandProps props;
  std::unique_ptr<std::vector<uint8_t>> metrics = nullptr;
};

struct ZeKernelInfo {
  uint64_t append_time;
  uint64_t submit_time;
  uint64_t execute_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ZeKernelInfo& r) const {
    if (execute_time != r.execute_time) {
      return execute_time > r.execute_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ZeKernelInfo& r) const {
    if (execute_time == r.execute_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

struct ZeCommandListInfo {
  std::vector<ZeKernelCommand*> kernel_commands;
  ze_context_handle_t context;
  ze_device_handle_t device;
  ze_device_handle_t src_device = nullptr;  // source used for memcpy commands
  ze_device_handle_t dst_device = nullptr;  // destination for memcpy commands
  bool immediate;
  std::pair<uint32_t, uint32_t> oi_pair;
};

struct ZeKernelMemInfo {
  std::string name;
  uint64_t base_addr;
  uint32_t size;
};

struct ZeDeviceDescriptor {
  uint64_t host_time_origin;
  uint64_t device_time_origin;
  uint64_t device_timer_frequency;
  uint64_t device_timer_mask;
  uint64_t metric_time_origin;
  uint64_t metric_timer_frequency;
  uint64_t metric_timer_mask;
  ze_driver_handle_t driver = nullptr;
  ze_context_handle_t context = nullptr;
  zet_metric_group_handle_t metric_group = nullptr;
  ze_pci_ext_properties_t pci_properties;
};

using ZeKernelMemInfoMap = std::map<uint64_t, ZeKernelMemInfo>;

using ZeKernelGroupSizeMap = std::map<ze_kernel_handle_t, ZeKernelGroupSize>;
using ZeKernelInfoMap = std::map<std::string, ZeKernelInfo>;
using ZeCommandListMap = std::map<ze_command_list_handle_t, ZeCommandListInfo>;
using ZeImageSizeMap = std::map<ze_image_handle_t, size_t>;
using ZeDeviceMap = std::map<ze_device_handle_t, std::vector<ze_device_handle_t>>;
using ZeKernelProfiles = std::map<uint64_t, ZeKernelProfileRecord>;

typedef void (*OnZeKernelFinishCallback)(void* data,
                                         std::vector<ZeKernelCommandExecutionRecord>& kcexec);

// Work-around for ensuring ZE_ENABLE_TRACING_LAYER=1 is set before zeInit() is
// called. Not guarenteed to work if user calls zeInit() before main() in their
// program.
// Warning: Do not add a dependency on another static variable or there is a
// risk of undefined behavior.
// TODO: Fix when there's a better solution.
class GlobalZeInitializer {
 public:
  inline static ze_result_t Initialize() {
    utils::SetEnv("ZE_ENABLE_TRACING_LAYER", "1");
    overhead::Init();
    ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    {
      std::string o_api_string = "zeInit";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    return status;
  }

  inline static ze_result_t result_ = Initialize();
};

class ZeCollector {
 public:  // Interface
  ZeCollector(const ZeCollector&) = delete;
  ZeCollector& operator=(const ZeCollector&) = delete;
  ZeCollector(ZeCollector&&) = delete;
  ZeCollector& operator=(ZeCollector&&) = delete;

  static ZeCollector* Create(CollectorOptions options, OnZeKernelFinishCallback acallback = nullptr,
                             OnZeKernelFinishCallback kcallback = nullptr,
                             OnZeFunctionFinishCallback fcallback = nullptr,
                             void* callback_data = nullptr) {
    if (GlobalZeInitializer::result_ != ZE_RESULT_SUCCESS) {
      SPDLOG_WARN("Unable to initialize ZeCollector, error code {0:x}",
                  static_cast<std::size_t>(GlobalZeInitializer::result_));
      return nullptr;
    }

    ze_api_version_t version = utils::ze::GetVersion();
    PTI_ASSERT(ZE_MAJOR_VERSION(version) >= 1 && ZE_MINOR_VERSION(version) >= 2);

    ZeCollector* collector =
        new ZeCollector(options, acallback, kcallback, fcallback, callback_data);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;
    overhead::Init();
    status = zelTracerCreate(&tracer_desc, &tracer);
    {
      std::string o_api_string = "zelTracerCreate";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to create Level Zero tracer" << std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);

    collector->tracer_ = tracer;

    ze_driver_handle_t driver;
    uint32_t count = 1;
    overhead::Init();
    status = zeDriverGet(&count, &driver);
    {
      std::string o_api_string = "zeDriverGet";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    if (status == ZE_RESULT_SUCCESS) {
      if (zeDriverGetExtensionFunctionAddress(driver, "zexKernelGetBaseAddress",
                                              (void**)&zexKernelGetBaseAddress) !=
          ZE_RESULT_SUCCESS) {
        zexKernelGetBaseAddress = nullptr;
      }
    }

    return collector;
  }

  ~ZeCollector() {
    ProcessCalls(nullptr, nullptr);
    if (options_.metric_query) {
      DumpKernelProfiles();
      for (auto it = metric_activations_.begin(); it != metric_activations_.end(); it++) {
        overhead::Init();
        zetContextActivateMetricGroups(it->first, it->second, 0, nullptr);
        {
          std::string o_api_string = "zetContextActivateMetricGroups";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
      }
      metric_activations_.clear();
      for (auto& context : metric_contexts_) {
        overhead::Init();
        zeContextDestroy(context);
        {
          std::string o_api_string = "zeContextDestroy";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
      }
      metric_contexts_.clear();
    }
    if (tracer_ != nullptr) {
#if !defined(_WIN32)
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
    }
  }

  bool IsCollectionEnabled() const {
    // std::string enabled = utils::GetEnv("PTI_ENABLE_COLLECTION");
    // if (enabled.empty() || enabled == "0") {
    // return false;
    //}
    return true;
  }

  void EnableTracer() { cb_enabled_.acallback = true; }

  void DisableTracer() { cb_enabled_.acallback = false; }

  const ZeKernelMemInfoMap& GetKernelMemInfo() const { return kernel_mem_info_map_; }

  void PrintKernelsTable() const {
    std::set<std::pair<std::string, ZeKernelInfo>, utils::Comparator> sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.execute_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Kernel"
           << "," << std::setw(kCallsLength) << "Calls"
           << "," << std::setw(kTimeLength) << "Time (ns)"
           << "," << std::setw(kPercentLength) << "Time (%)"
           << "," << std::setw(kTimeLength) << "Average (ns)"
           << "," << std::setw(kTimeLength) << "Min (ns)"
           << "," << std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.execute_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      stream << std::setw(max_name_length) << function << "," << std::setw(kCallsLength)
             << call_count << "," << std::setw(kTimeLength) << duration << ","
             << std::setw(kPercentLength) << std::setprecision(2) << std::fixed << percent_duration
             << "," << std::setw(kTimeLength) << avg_duration << "," << std::setw(kTimeLength)
             << min_duration << "," << std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

  void PrintSubmissionTable() const {
    std::set<std::pair<std::string, ZeKernelInfo>, utils::Comparator> sorted_list(
        kernel_info_map_.begin(), kernel_info_map_.end());

    uint64_t total_append_duration = 0;
    uint64_t total_submit_duration = 0;
    uint64_t total_execute_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_append_duration += value.second.append_time;
      total_submit_duration += value.second.submit_time;
      total_execute_duration += value.second.execute_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_execute_duration == 0) {
      return;
    }

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Kernel"
           << "," << std::setw(kCallsLength) << "Calls"
           << "," << std::setw(kTimeLength) << "Append (ns)"
           << "," << std::setw(kPercentLength) << "Append (%)"
           << "," << std::setw(kTimeLength) << "Submit (ns)"
           << "," << std::setw(kPercentLength) << "Submit (%)"
           << "," << std::setw(kTimeLength) << "Execute (ns)"
           << "," << std::setw(kPercentLength) << "Execute (%)"
           << "," << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t append_duration = value.second.append_time;
      float append_percent = 100.0f * append_duration / total_append_duration;
      uint64_t submit_duration = value.second.submit_time;
      float submit_percent = 100.0f * submit_duration / total_submit_duration;
      uint64_t execute_duration = value.second.execute_time;
      float execute_percent = 100.0f * execute_duration / total_execute_duration;
      stream << std::setw(max_name_length) << function << "," << std::setw(kCallsLength)
             << call_count << "," << std::setw(kTimeLength) << append_duration << ","
             << std::setw(kPercentLength) << std::setprecision(2) << std::fixed << append_percent
             << "," << std::setw(kTimeLength) << submit_duration << "," << std::setw(kPercentLength)
             << std::setprecision(2) << std::fixed << submit_percent << ","
             << std::setw(kTimeLength) << execute_duration << "," << std::setw(kPercentLength)
             << std::setprecision(2) << std::fixed << execute_percent << "," << std::endl;
    }
  }

  void DisableTracing() {
    // PTI_ASSERT(tracer_ != nullptr);
#if !defined(_WIN32)
    overhead::Init();
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    {
      std::string o_api_string = "zelTracerSetEnabled";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
  }

  const ZeKernelInfoMap& GetKernelInfoMap() const { return kernel_info_map_; }

  const ZeFunctionInfoMap& GetFunctionInfoMap() const { return function_info_map_; }

  void PrintFunctionsTable() const {
    std::set<std::pair<std::string, ZeFunction>, utils::Comparator> sorted_list(
        function_info_map_.begin(), function_info_map_.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kFunctionLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::stringstream stream;
    stream << std::setw(max_name_length) << "Function"
           << "," << std::setw(kCallsLength) << "Calls"
           << "," << std::setw(kTimeLength) << "Time (ns)"
           << "," << std::setw(kPercentLength) << "Time (%)"
           << "," << std::setw(kTimeLength) << "Average (ns)"
           << "," << std::setw(kTimeLength) << "Min (ns)"
           << "," << std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      stream << std::setw(max_name_length) << function << "," << std::setw(kCallsLength)
             << call_count << "," << std::setw(kTimeLength) << duration << ","
             << std::setw(kPercentLength) << std::setprecision(2) << std::fixed << percent_duration
             << "," << std::setw(kTimeLength) << avg_duration << "," << std::setw(kTimeLength)
             << min_duration << "," << std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

 private:  // Implementation
  ZeCollector(CollectorOptions options, OnZeKernelFinishCallback acallback,
              OnZeKernelFinishCallback kcallback, OnZeFunctionFinishCallback fcallback,
              void* callback_data)
      : options_(options),
        acallback_(acallback),
        kcallback_(kcallback),
        fcallback_(fcallback),
        callback_data_(callback_data),
        event_cache_(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
    CreateDeviceMap();
  }

  void CreateDeviceMap() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t num_drivers = 0;
    overhead::Init();
    status = zeDriverGet(&num_drivers, nullptr);
    {
      std::string o_api_string = "zeDriverGet";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (num_drivers > 0) {
      std::vector<ze_driver_handle_t> drivers(num_drivers);
      std::vector<ze_context_handle_t> contexts;
      overhead::Init();
      status = zeDriverGet(&num_drivers, drivers.data());
      {
        std::string o_api_string = "zeDriverGet";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      for (auto driver : drivers) {
        ze_context_handle_t context = nullptr;
        if (options_.metric_query) {
          ze_context_desc_t cdesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

          overhead::Init();
          status = zeContextCreate(driver, &cdesc, &context);
          {
            std::string o_api_string = "zeContextCreate";
            overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                 o_api_string.c_str());
          };
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          metric_contexts_.push_back(context);
        }

        uint32_t num_devices = 0;
        overhead::Init();
        status = zeDeviceGet(driver, &num_devices, nullptr);
        {
          std::string o_api_string = "zeDeviceGet";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        if (num_devices) {
          std::vector<ze_device_handle_t> devices(num_devices);
          overhead::Init();
          status = zeDeviceGet(driver, &num_devices, devices.data());
          {
            std::string o_api_string = "zeDeviceGet";
            overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                 o_api_string.c_str());
          };
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          for (auto device : devices) {
            ZeDeviceDescriptor desc;

            desc.device_timer_frequency = utils::ze::GetDeviceTimerFrequency(device);
            desc.device_timer_mask = utils::ze::GetDeviceTimestampMask(device);
            desc.metric_timer_frequency = utils::ze::GetMetricTimerFrequency(device);
            desc.metric_timer_mask = utils::ze::GetMetricTimestampMask(device);

            ze_pci_ext_properties_t pci_device_properties;
            overhead::Init();
            ze_result_t status = zeDevicePciGetPropertiesExt(device, &pci_device_properties);
            {
              std::string o_api_string = "zeDevicePciGetPropertiesExt";
              overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                   o_api_string.c_str());
            };
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            desc.pci_properties = pci_device_properties;

            zet_metric_group_handle_t group = nullptr;
            if (options_.metric_query) {
              uint32_t num_groups = 0;
              overhead::Init();
              status = zetMetricGroupGet(device, &num_groups, nullptr);
              {
                std::string o_api_string = "zetMetricGroupGet";
                overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                     o_api_string.c_str());
              };
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);
              if (num_groups > 0) {
                std::vector<zet_metric_group_handle_t> groups(num_groups, nullptr);
                overhead::Init();
                status = zetMetricGroupGet(device, &num_groups, groups.data());
                {
                  std::string o_api_string = "zetMetricGroupGet";
                  overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                       o_api_string.c_str());
                };
                PTI_ASSERT(status == ZE_RESULT_SUCCESS);

                for (uint32_t k = 0; k < num_groups; ++k) {
                  zet_metric_group_properties_t group_props{};
                  group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                  group_props.pNext = nullptr;
                  overhead::Init();
                  status = zetMetricGroupGetProperties(groups[k], &group_props);
                  {
                    std::string o_api_string = "zetMetricGroupGetProperties";
                    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                         o_api_string.c_str());
                  };
                  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

                  if ((strcmp(group_props.name, utils::GetEnv("UNITRACE_MetricGroup").c_str()) ==
                       0) &&
                      (group_props.samplingType &
                       ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED)) {
                    group = groups[k];
                    break;
                  }
                }
              }

              overhead::Init();
              status = zetContextActivateMetricGroups(context, device, 1, &group);
              {
                std::string o_api_string = "zetContextActivateMetricGroups";
                overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                     o_api_string.c_str());
              };
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);
              metric_activations_.insert({context, device});

              desc.driver = driver;
              desc.context = context;
              desc.metric_group = group;
            }

            uint64_t host_time;
            uint64_t ticks;
            uint64_t device_time;
            uint64_t metric_time;
            overhead::Init();
            zeDeviceGetGlobalTimestamps(device, &host_time, &ticks);
            {
              std::string o_api_string = "zeDeviceGetGlobalTimestamps";
              overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                   o_api_string.c_str());
            };

            device_time = ticks & desc.device_timer_mask;
            if (desc.device_timer_frequency)
              device_time = device_time * NSEC_IN_SEC / desc.device_timer_frequency;

            overhead::Init();
            // zetMetricGroupGetGlobalTimestampsExp() is broken.
            {
              std::string o_api_string = "zetMetricGroupGetGlobalTimestampsExp";
              overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                   o_api_string.c_str());
            };
            metric_time = ticks & desc.metric_timer_mask;
            if (desc.metric_timer_frequency)
              metric_time = metric_time * NSEC_IN_SEC / desc.metric_timer_frequency;

            desc.host_time_origin = host_time;
            desc.device_time_origin = device_time;
            desc.metric_time_origin = metric_time;

            device_descriptors_[device] = desc;

            uint32_t num_sub_devices = 0;
            overhead::Init();
            status = zeDeviceGetSubDevices(device, &num_sub_devices, nullptr);
            {
              std::string o_api_string = "zeDeviceGetSubDevices";
              overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                   o_api_string.c_str());
            };
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);

            if (num_sub_devices) {
              std::vector<ze_device_handle_t> sub_devices(num_sub_devices);

              overhead::Init();
              status = zeDeviceGetSubDevices(device, &num_sub_devices, sub_devices.data());
              {
                std::string o_api_string = "zeDeviceGetSubDevices";
                overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                     o_api_string.c_str());
              };
              PTI_ASSERT(status == ZE_RESULT_SUCCESS);

              device_map_[device] = sub_devices;

              for (uint32_t j = 0; j < num_sub_devices; j++) {
                ZeDeviceDescriptor sub_desc = {};

                sub_desc.device_timer_frequency =
                    utils::ze::GetDeviceTimerFrequency(sub_devices[j]);
                sub_desc.device_timer_mask = utils::ze::GetDeviceTimestampMask(sub_devices[j]);
                sub_desc.metric_timer_frequency =
                    utils::ze::GetMetricTimerFrequency(sub_devices[j]);
                sub_desc.metric_timer_mask = utils::ze::GetMetricTimestampMask(sub_devices[j]);

                ze_pci_ext_properties_t pci_device_properties;
                overhead::Init();
                ze_result_t status =
                    zeDevicePciGetPropertiesExt(sub_devices[j], &pci_device_properties);
                {
                  std::string o_api_string = "zeDevicePciGetPropertiesExt";
                  overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                       o_api_string.c_str());
                };
                PTI_ASSERT(status == ZE_RESULT_SUCCESS);
                sub_desc.pci_properties = pci_device_properties;

                uint64_t ticks;
                uint64_t host_time;
                uint64_t device_time;
                uint64_t metric_time;
                overhead::Init();
                zeDeviceGetGlobalTimestamps(sub_devices[j], &host_time, &ticks);
                {
                  std::string o_api_string = "zeDeviceGetGlobalTimestamps";
                  overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                       o_api_string.c_str());
                };
                device_time = ticks & sub_desc.device_timer_mask;
                if (sub_desc.device_timer_frequency) {
                  device_time = device_time * NSEC_IN_SEC / sub_desc.device_timer_frequency;
                }

                metric_time = ticks & sub_desc.metric_timer_mask;
                if (sub_desc.metric_timer_frequency) {
                  metric_time = metric_time * NSEC_IN_SEC / sub_desc.metric_timer_frequency;
                }

                sub_desc.host_time_origin = host_time;
                sub_desc.device_time_origin = device_time;
                sub_desc.metric_time_origin = metric_time;

                if (options_.metric_query) {
                  uint32_t num_groups = 0;
                  overhead::Init();
                  status = zetMetricGroupGet(device, &num_groups, nullptr);
                  {
                    std::string o_api_string = "zetMetricGroupGet";
                    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                         o_api_string.c_str());
                  };
                  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
                  if (num_groups > 0) {
                    std::vector<zet_metric_group_handle_t> groups(num_groups, nullptr);
                    overhead::Init();
                    status = zetMetricGroupGet(device, &num_groups, groups.data());
                    {
                      std::string o_api_string = "zetMetricGroupGet";
                      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                           o_api_string.c_str());
                    };
                    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

                    for (uint32_t k = 0; k < num_groups; ++k) {
                      zet_metric_group_properties_t group_props{};
                      group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                      group_props.pNext = nullptr;
                      overhead::Init();
                      status = zetMetricGroupGetProperties(groups[k], &group_props);
                      {
                        std::string o_api_string = "zetMetricGroupGetProperties";
                        overhead::FiniLevel0(
                            overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                            o_api_string.c_str());
                      };
                      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

                      if ((strcmp(group_props.name,
                                  utils::GetEnv("UNITRACE_MetricGroup").c_str()) == 0) &&
                          (group_props.samplingType &
                           ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED)) {
                        group = groups[k];
                        break;
                      }
                    }
                  }
                  overhead::Init();
                  status = zetContextActivateMetricGroups(context, sub_devices[j], 1, &group);
                  {
                    std::string o_api_string = "zetContextActivateMetricGroups";
                    overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                                         o_api_string.c_str());
                  };
                  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
                  metric_activations_.insert({context, sub_devices[j]});

                  sub_desc.driver = driver;
                  sub_desc.context = context;
                  sub_desc.metric_group = group;
                }

                device_descriptors_[sub_devices[j]] = sub_desc;
              }
            }
          }
        }
      }
    }
  }

  int GetSubDeviceId(ze_device_handle_t sub_device) const {
    for (auto& it : device_map_) {
      const std::vector<ze_device_handle_t>& sub_device_list = it.second;
      for (size_t i = 0; i < sub_device_list.size(); ++i) {
        if (sub_device_list[i] == sub_device) {
          return static_cast<int>(i);
        }
      }
    }
    return -1;
  }

  ze_device_handle_t GetDeviceForSubDevice(ze_device_handle_t sub_device) const {
    for (auto it : device_map_) {
      std::vector<ze_device_handle_t>& sub_device_list = it.second;
      for (size_t i = 0; i < sub_device_list.size(); ++i) {
        if (sub_device_list[i] == sub_device) {
          return it.first;
        }
      }
    }
    return nullptr;
  }

#if 0
  void GetDeviceSyncTimestamps(ze_device_handle_t device, uint64_t& host_timestamp, uint64_t& device_timestamp) const {
overhead::Init();
    zeDeviceGetGlobalTimestamps(device, &host_timestamp, &device_timestamp);
{std::string o_api_string = "zeDeviceGetGlobalTimestamps";overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,o_api_string.c_str());};
    uint64_t mask = device_descriptors_.at(device).device_timer_mask;
    uint64_t freq = device_descriptors_.at(device).device_timer_frequency;
    device_timestamp &= mask;
    device_timestamp = device_timestamp * NSEC_IN_SEC / freq;
  }

  void GetDeviceSyncTimestamps(ze_command_list_handle_t command_list, uint64_t& host_timestamp, uint64_t& device_timestamp) {
    ze_device_handle_t device = GetCommandListDevice(command_list);
    GetDeviceSyncTimestamps(device, host_timestamp, device_timestamp);
  }
#endif /* 0 */

  static void PrintTypedValue(std::stringstream& stream, const zet_typed_value_t& typed_value) {
    switch (typed_value.type) {
      case ZET_VALUE_TYPE_UINT32:
        stream << typed_value.value.ui32;
        break;
      case ZET_VALUE_TYPE_UINT64:
        stream << typed_value.value.ui64;
        break;
      case ZET_VALUE_TYPE_FLOAT32:
        stream << typed_value.value.fp32;
        break;
      case ZET_VALUE_TYPE_FLOAT64:
        stream << typed_value.value.fp64;
        break;
      case ZET_VALUE_TYPE_BOOL8:
        stream << static_cast<uint32_t>(typed_value.value.b8);
        break;
      default:
        PTI_ASSERT(0);
        break;
    }
  }

  inline static std::string GetMetricUnits(const char* units) {
    PTI_ASSERT(units != nullptr);

    std::string result = units;
    if (result.find("null") != std::string::npos) {
      result = "";
    } else if (result.find("percent") != std::string::npos) {
      result = "%";
    }

    return result;
  }

  static uint32_t GetMetricCount(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    group_props.pNext = nullptr;
    overhead::Init();
    ze_result_t status = zetMetricGroupGetProperties(group, &group_props);
    {
      std::string o_api_string = "zetMetricGroupGetProperties";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    return group_props.metricCount;
  }

  static std::vector<std::string> GetMetricNames(zet_metric_group_handle_t group) {
    PTI_ASSERT(group != nullptr);

    uint32_t metric_count = GetMetricCount(group);
    PTI_ASSERT(metric_count > 0);

    std::vector<zet_metric_handle_t> metrics(metric_count);
    overhead::Init();
    ze_result_t status = zetMetricGet(group, &metric_count, metrics.data());
    {
      std::string o_api_string = "zetMetricGet";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(metric_count == metrics.size());

    std::vector<std::string> names;
    for (auto metric : metrics) {
      zet_metric_properties_t metric_props;
      metric_props.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
      metric_props.pNext = nullptr;
      overhead::Init();
      status = zetMetricGetProperties(metric, &metric_props);
      {
        std::string o_api_string = "zetMetricGetProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::string units = GetMetricUnits(metric_props.resultUnits);
      std::string name = metric_props.name;
      if (!units.empty()) {
        name += "[" + units + "]";
      }
      names.push_back(name);
    }

    return names;
  }

  void QueryKernelCommandMetrics(ZeKernelCommand* command) {
    ze_result_t status;
    overhead::Init();
    status = zeEventQueryStatus(command->metric_query_event);
    {
      std::string o_api_string = "zeEventQueryStatus";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    if (status == ZE_RESULT_SUCCESS) {
      size_t size = 0;
      overhead::Init();
      status = zetMetricQueryGetData(command->metric_query, &size, nullptr);
      {
        std::string o_api_string = "zetMetricQueryGetData";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      if ((status == ZE_RESULT_SUCCESS) && (size > 0)) {
        auto kmetrics = std::make_unique<std::vector<uint8_t>>(size);

        size_t size2 = size;
        overhead::Init();
        status = zetMetricQueryGetData(command->metric_query, &size2, kmetrics->data());
        {
          std::string o_api_string = "zetMetricQueryGetData";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        if (size2 == size) {
          auto it = kernel_profiles_.find(command->kernel_id);
          if (it != kernel_profiles_.end()) {
            it->second.metrics = std::move(kmetrics);
          }
        }
      }

      query_pools_.ResetQuery(command->metric_query);
      event_cache_.ReleaseEvent(command->metric_query_event);
      command->metric_query_event = nullptr;
      command->metric_query = nullptr;
    }
  }

  void DumpKernelProfiles(void) {
    ze_device_handle_t device = nullptr;
    zet_metric_group_handle_t group = nullptr;
    std::vector<std::string> metric_names;

    if (kernel_profiles_.size() == 0) {
      return;
    }

    for (auto it = kernel_profiles_.begin(); it != kernel_profiles_.end(); it++) {
      if ((it->second.metrics == nullptr) || it->second.metrics->empty()) {
        continue;
      }

      std::string kname = GetVerboseName(&(it->second.props));

      if (device != it->second.device) {
        device = it->second.device;
        auto it2 = device_descriptors_.find(it->second.device);
        if (it2 != device_descriptors_.end()) {
          group = it2->second.metric_group;
          metric_names = GetMetricNames(it2->second.metric_group);
          PTI_ASSERT(!metric_names.empty());
        }
      }

      uint32_t num_samples = 0;
      uint32_t num_metrics = 0;
      overhead::Init();
      ze_result_t status = zetMetricGroupCalculateMultipleMetricValuesExp(
          group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, it->second.metrics->size(),
          it->second.metrics->data(), &num_samples, &num_metrics, nullptr, nullptr);
      {
        std::string o_api_string = "zetMetricGroupCalculateMultipleMetricValuesExp";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      PTI_ASSERT(num_samples > 0);
      std::vector<uint32_t> samples(num_samples);
      PTI_ASSERT(num_metrics > 0);
      std::vector<zet_typed_value_t> metrics(num_metrics);

      overhead::Init();
      status = zetMetricGroupCalculateMultipleMetricValuesExp(
          group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, it->second.metrics->size(),
          it->second.metrics->data(), &num_samples, &num_metrics, samples.data(), metrics.data());
      {
        std::string o_api_string = "zetMetricGroupCalculateMultipleMetricValuesExp";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::stringstream header;
      header << std::endl;
      header << "Kernel,";
      header << "SubDeviceId,";
      for (auto& metric : metric_names) {
        header << metric << ",";
      }
      header << std::endl;

      std::stringstream stream;
      for (uint32_t i = 0; i < num_samples; ++i) {
        stream << kname << ",";
        stream << i << ",";

        uint32_t size = samples[i];
        PTI_ASSERT(size == metric_names.size());

        const zet_typed_value_t* value = metrics.data() + i * size;
        for (uint32_t j = 0; j < size; ++j) {
          PrintTypedValue(stream, value[j]);
          stream << ",";
        }
        stream << std::endl;
      }
    }

    kernel_profiles_.clear();
  }

  void ProcessCall(ze_event_handle_t event, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeEventQueryStatus(event);
    {
      std::string o_api_string = "zeEventQueryStatus";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = *it;
      PTI_ASSERT(command != nullptr);
      if (command->event != nullptr) {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        if (status == ZE_RESULT_SUCCESS) {
          if (command->event == event) {
            ProcessCall(command, kids, kcexecrec);
            done = true;
          } else {
            ProcessCall(command, nullptr, kcexecrec);
          }
        }
      }
      if ((command->metric_query != nullptr) && (command->metric_query_event != nullptr)) {
        QueryKernelCommandMetrics(command);
      }
      if ((command->event == nullptr) && (command->metric_query_event == nullptr)) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        it++;
      }
      if (done) {
        break;
      }
    }
  }

  void ProcessCall(ze_fence_handle_t fence, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    overhead::Init();
    status = zeFenceQueryStatus(fence);
    {
      std::string o_api_string = "zeFenceQueryStatus";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    if (status != ZE_RESULT_SUCCESS) {
      return;
    }

    bool done = false;
    for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
      ZeKernelCommand* command = *it;
      PTI_ASSERT(command != nullptr);
      if ((command->fence != nullptr) && (command->fence == fence)) {
        ProcessCall(command, kids, kcexecrec);
        done = true;
      } else {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        if ((command->event != nullptr) && (status == ZE_RESULT_SUCCESS)) {
          ProcessCall(command, nullptr, kcexecrec);
        }
      }
      if ((command->metric_query != nullptr) && (command->metric_query_event != nullptr)) {
        QueryKernelCommandMetrics(command);
      }
      if ((command->event == nullptr) && (command->metric_query_event == nullptr)) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        it++;
      }
      if (done) {
        break;
      }
    }
  }

  uint64_t ComputeDuration(uint64_t start, uint64_t end, uint64_t freq, uint64_t mask) {
    uint64_t duration = 0;
    if (start <= end) {
      duration = (end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    } else {  // Timer Overflow
      duration = ((mask + 1ull) + end - start) * static_cast<uint64_t>(NSEC_IN_SEC) / freq;
    }
    return duration;
  }

  uint64_t GetDeviceTimeNs(uint64_t cycles, uint64_t freq) { return cycles * NSEC_IN_SEC / freq; }

  void GetHostTime(const ZeKernelCommand* command, const ze_kernel_timestamp_result_t& timestamp,
                   uint64_t& host_start, uint64_t& host_end) {
    uint64_t device_freq =
        device_descriptors_[command->device].device_timer_frequency;  // command->timer_frequency;
    uint64_t device_mask =
        device_descriptors_[command->device].device_timer_mask;  // command->timer_mask;

    uint64_t metric_freq =
        device_descriptors_[command->device].metric_timer_frequency;  // command->timer_frequency;
    uint64_t metric_mask =
        device_descriptors_[command->device].metric_timer_mask;  // command->timer_mask;

    uint64_t start = timestamp.global.kernelStart & device_mask;
    uint64_t end = timestamp.global.kernelEnd & device_mask;

    uint64_t device_submit_time =
        (device_descriptors_.at(command->device).device_time_origin + command->submit_time -
         device_descriptors_.at(command->device).host_time_origin);

    uint64_t start_ns = (start * NSEC_IN_SEC / device_freq);
    int64_t time_shift;
    while ((start_ns < device_submit_time) &&
           ((int64_t)(device_submit_time - start_ns) >=
            (int64_t)((device_mask + 1) * NSEC_IN_SEC / device_freq))) {
      start += device_mask + 1;
      end += device_mask + 1;
      start_ns = (start * NSEC_IN_SEC / device_freq);
    }

    if (start_ns < device_submit_time) {
      // device and host clocks out of sync

      uint64_t host_time;
      uint64_t device_time;
      uint64_t metric_time;
      uint64_t ticks;
      overhead::Init();
      zeDeviceGetGlobalTimestamps(command->device, &host_time, &ticks);
      {
        std::string o_api_string = "zeDeviceGetGlobalTimestamps";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      device_time = ticks & device_mask;
      device_time = device_time * NSEC_IN_SEC / device_freq;
      metric_time = ticks & metric_mask;
      metric_time = metric_time * NSEC_IN_SEC / metric_freq;

      int64_t host_time_shift = static_cast<int64_t>(host_time - command->submit_time);
      int64_t device_time_shift = static_cast<int64_t>(device_time - device_submit_time);
      device_submit_time = static_cast<uint64_t>(static_cast<int64_t>(device_submit_time) -
                                                 (host_time_shift - device_time_shift));

      device_descriptors_[command->device].host_time_origin = host_time;
      device_descriptors_[command->device].device_time_origin = device_time;
      device_descriptors_[command->device].metric_time_origin = metric_time;
    }
    if (start_ns < device_submit_time) {
      time_shift = static_cast<int64_t>(start_ns - device_submit_time);
    } else {
      time_shift = 0;
    }

    uint64_t duration = ComputeDuration(start, end, device_freq, device_mask);

    // use the high order bit to ensure we won't lose a bit when casting submit_time value to int64.
    const uint64_t min_high_order_set_integer = 1ULL << 63;

    // Ensure high order bit not set.
    PTI_ASSERT(command->submit_time < min_high_order_set_integer);

    // cast to int64 for submit_time needed since time_shift is implicitly uintized otherwise.
    host_start = static_cast<uint64_t>(static_cast<int64_t>(command->submit_time) + time_shift);
    if (time_shift < 0) PTI_ASSERT(host_start > command->submit_time);
    PTI_ASSERT(UINT64_MAX - duration >= host_start);
    host_end = host_start + duration;
  }

  void ProcessCall(const ZeKernelCommand* command, const ze_kernel_timestamp_result_t& timestamp,
                   int tile, bool in_summary,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    uint64_t host_start = 0, host_end = 0;
    GetHostTime(command, timestamp, host_start, host_end);
    PTI_ASSERT(host_start <= host_end);

    std::string name;

    if (options_.verbose) {
      name = GetVerboseName(&command->props);
    } else {
      name = command->props.name;
    }
    PTI_ASSERT(!name.empty());

    if (in_summary) {
      // PTI_ASSERT(command->append_time > 0);
      // PTI_ASSERT(command->append_time <= command->submit_time);
      uint64_t append_time = command->submit_time - command->append_time;
      // PTI_ASSERT(command->submit_time <= host_start);
      uint64_t submit_time = host_start - command->submit_time;
      // PTI_ASSERT(host_start <= host_end);
      uint64_t execute_time = host_end - host_start;
      AddKernelInfo(append_time, submit_time, execute_time, name);
    }

    if (kcexecrec && (acallback_ || kcallback_)) {
      ZeKernelCommandExecutionRecord rec = {};

      rec.kid_ = command->kernel_id;
      rec.tid_ = command->tid;
      rec.cid_ = command->corr_id_;
      rec.append_time_ = command->append_time;
      rec.submit_time_ = command->submit_time;
      rec.start_time_ = host_start;
      rec.end_time_ = host_end;
      PTI_ASSERT(queue_ordinal_index_map_.count(command->queue) != 0);
      std::pair<uint32_t, uint32_t> oi;
      oi = queue_ordinal_index_map_[command->queue];
      rec.engine_ordinal_ = oi.first;
      rec.engine_index_ = oi.second;
      rec.tile_ = tile;
      auto it = device_descriptors_.find(command->device);
      PTI_ASSERT(it != device_descriptors_.end());
      rec.pci_prop_ = it->second.pci_properties;
      rec.name_ = std::move(name);
      rec.queue_ = command->queue;
      rec.device_ = command->device;
      std::copy_n(dev_uuid_map[command->src_device], ZE_MAX_DEVICE_UUID_SIZE, rec.src_device_uuid);
      std::copy_n(dev_uuid_map[command->dst_device], ZE_MAX_DEVICE_UUID_SIZE, rec.dst_device_uuid);

      if ((tile >= 0) && (device_map_.count(command->device) == 1) &&
          !device_map_[command->device].empty()) {  // Implicit Scaling
        rec.implicit_scaling_ = true;
      } else {
        rec.implicit_scaling_ = false;
      }

      if (command->props.type == KERNEL_COMMAND_TYPE_MEMORY) {
        rec.device_ = command->src_device;
        rec.dst_device_ = command->dst_device;
        if (command->src_device != nullptr) {
          auto it = device_descriptors_.find(command->src_device);
          PTI_ASSERT(it != device_descriptors_.end());
          rec.pci_prop_ = it->second.pci_properties;
        }
        if (command->dst_device != nullptr) {
          auto it = device_descriptors_.find(command->dst_device);
          PTI_ASSERT(it != device_descriptors_.end());
          rec.dst_pci_prop_ = it->second.pci_properties;
        }
        if (command->props.bytes_transferred > 0) {
          rec.bytes_xfered_ = command->props.bytes_transferred;
        } else if (command->props.value_size > 0) {
          rec.value_set_ = command->props.value_size;
        }
      }

      rec.context_ = command_list_map_[command->command_list].context;

      if (command->props.type == KERNEL_COMMAND_TYPE_KERNEL) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.sycl_enqk_begin_time_ = command->sycl_enqk_begin_time_;

        // rec.sycl_enqk_end_time_ = sycl_data_kview.sycl_enqk_end_time_;

        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
      }
      if (command->props.type == KERNEL_COMMAND_TYPE_MEMORY) {
        rec.sycl_node_id_ = command->sycl_node_id_;
        rec.sycl_invocation_id_ = command->sycl_invocation_id_;
        rec.sycl_task_begin_time_ = command->sycl_task_begin_time_;
        // rec.sycl_task_end_time_ = command->sycl_task_end_time_;
        rec.source_file_name_ = command->source_file_name_;
        rec.source_line_number_ = command->source_line_number_;
      }

      kcexecrec->push_back(rec);
    }
  }

  void ProcessCall(ZeKernelCommand* command, std::vector<uint64_t>* kids,
                   std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    if (kids) {
      kids->push_back(command->kernel_id);
    }

    ze_kernel_timestamp_result_t timestamp{};
    overhead::Init();
    ze_result_t status = zeEventQueryKernelTimestamp(command->event, &timestamp);
    {
      std::string o_api_string = "zeEventQueryKernelTimestamp";
      overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                           o_api_string.c_str());
    };
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    uint64_t device_freq =
        device_descriptors_[command->device].device_timer_frequency;  // command->timer_frequency;
    uint64_t device_mask =
        device_descriptors_[command->device].device_timer_mask;  // command->timer_mask;

    uint64_t metric_freq =
        device_descriptors_[command->device].metric_timer_frequency;  // command->timer_frequency;
    uint64_t metric_mask =
        device_descriptors_[command->device].metric_timer_mask;  // command->timer_mask;

    ZeKernelProfileRecord r;

    r.device = command->device;
    r.props = command->props;

    ZeKernelProfileTimestamps ts;

    if (options_.kernels_per_tile && (command->props.type == KERNEL_COMMAND_TYPE_KERNEL) &&
        (command->props.simd_width > 0)) {
      if (device_map_.count(command->device) == 1 &&
          !device_map_[command->device].empty()) {  // Implicit Scaling
        uint32_t count = 0;
        overhead::Init();
        ze_result_t status =
            zeEventQueryTimestampsExp(command->event, command->device, &count, nullptr);
        {
          std::string o_api_string = "zeEventQueryTimestampsExp";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        PTI_ASSERT(count > 0);

        std::vector<ze_kernel_timestamp_result_t> timestamps(count);
        overhead::Init();
        status =
            zeEventQueryTimestampsExp(command->event, command->device, &count, timestamps.data());
        {
          std::string o_api_string = "zeEventQueryTimestampsExp";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        for (uint32_t i = 0; i < count; i++) {
          ts.device_id = i;

          ts.device_start = timestamps[i].global.kernelStart & device_mask;
          ts.device_start = (ts.device_start * NSEC_IN_SEC / device_freq);

          ts.metric_start = timestamps[i].global.kernelStart & metric_mask;
          ts.metric_start = (ts.metric_start * NSEC_IN_SEC / metric_freq);

          ts.device_end = timestamps[i].global.kernelEnd & device_mask;
          ts.device_end = (ts.device_end * NSEC_IN_SEC / device_freq);

          ts.metric_end = timestamps[i].global.kernelEnd & metric_mask;
          ts.metric_end = (ts.metric_end * NSEC_IN_SEC / metric_freq);

          r.timestamps.push_back(ts);
        }

        kernel_profiles_.insert({command->kernel_id, std::move(r)});

        if (count == 1) {  // First tile is used only
          ProcessCall(command, timestamps[0], 0, true, kcexecrec);
        } else {
          ProcessCall(command, timestamp, -1, false, kcexecrec);
          for (uint32_t i = 0; i < count; ++i) {
            ProcessCall(command, timestamps[i], static_cast<int>(i), true, kcexecrec);
          }
        }
      } else {  // Explicit Scaling

        ts.device_start = timestamp.global.kernelStart & device_mask;
        ts.device_start = (ts.device_start * NSEC_IN_SEC / device_freq);

        ts.metric_start = timestamp.global.kernelStart & metric_mask;
        ts.metric_start = (ts.metric_start * NSEC_IN_SEC / metric_freq);

        ts.device_end = timestamp.global.kernelEnd & device_mask;
        ts.device_end = (ts.device_end * NSEC_IN_SEC / device_freq);

        ts.metric_end = timestamp.global.kernelEnd & metric_mask;
        ts.metric_end = (ts.metric_end * NSEC_IN_SEC / metric_freq);

        if (device_map_.count(command->device) == 0) {  // Subdevice
          int sub_device_id = GetSubDeviceId(command->device);
          PTI_ASSERT(sub_device_id >= 0);
          ts.device_id = sub_device_id;
          ProcessCall(command, timestamp, sub_device_id, true, kcexecrec);
        } else {  // Device with no subdevices
          ts.device_id = 0;
          ProcessCall(command, timestamp, 0, true, kcexecrec);
        }

        r.timestamps.push_back(ts);

        kernel_profiles_.insert({command->kernel_id, std::move(r)});
      }
    } else {
      ts.device_start = timestamp.global.kernelStart & device_mask;
      ts.device_start = (ts.device_start * NSEC_IN_SEC / device_freq);

      ts.metric_start = timestamp.global.kernelStart & metric_mask;
      ts.metric_start = (ts.metric_start * NSEC_IN_SEC / metric_freq);

      ts.device_end = timestamp.global.kernelEnd & device_mask;
      ts.device_end = (ts.device_end * NSEC_IN_SEC / device_freq);

      ts.metric_end = timestamp.global.kernelEnd & metric_mask;
      ts.metric_end = (ts.metric_end * NSEC_IN_SEC / metric_freq);

      ts.device_id = 0;
      r.timestamps.push_back(ts);

      kernel_profiles_.insert({command->kernel_id, std::move(r)});

      ProcessCall(command, timestamp, -1, true, kcexecrec);
    }

    // if (command->command_list == nullptr) {
    event_cache_.ReleaseEvent(command->event);
    command->event = nullptr;
    //}
    // DO NOT RESET EVENT
    // event_cache_.ResetEvent(command->event);
  }

  void ProcessCalls(std::vector<uint64_t>* kids,
                    std::vector<ZeKernelCommandExecutionRecord>* kcexecrec) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    auto it = kernel_command_list_.begin();
    while (it != kernel_command_list_.end()) {
      ZeKernelCommand* command = *it;

      if (command->event != nullptr) {
        overhead::Init();
        status = zeEventQueryStatus(command->event);
        {
          std::string o_api_string = "zeEventQueryStatus";
          overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                               o_api_string.c_str());
        };
        if (status == ZE_RESULT_SUCCESS) {
          ProcessCall(command, kids, kcexecrec);
        }
      }

      if ((command->metric_query != nullptr) && (command->metric_query_event != nullptr)) {
        QueryKernelCommandMetrics(command);
      }

      if ((command->event == nullptr) && (command->metric_query_event == nullptr)) {
        delete command;
        it = kernel_command_list_.erase(it);
      } else {
        ++it;
      }
    }
  }

  static std::string GetVerboseName(const ZeKernelCommandProps* props) {
    // PTI_ASSERT(props != nullptr);
    // PTI_ASSERT(!props->name.empty());

    std::stringstream sstream;
    sstream << props->name;
    if (props->type == KERNEL_COMMAND_TYPE_KERNEL) {
      if (props->simd_width > 0) {
        sstream << "[SIMD";
        if (props->simd_width == 1) {
          sstream << "_ANY";
        } else {
          sstream << props->simd_width;
        }
        sstream << " {" << props->group_count[0] << "; " << props->group_count[1] << "; "
                << props->group_count[2] << "} {" << props->group_size[0] << "; "
                << props->group_size[1] << "; " << props->group_size[2] << "}]";
      }
    } else if ((props->type == KERNEL_COMMAND_TYPE_MEMORY) && (props->bytes_transferred > 0)) {
      sstream << "[" << props->bytes_transferred << " bytes]";
    }

    return sstream.str();
  }

  void AddKernelInfo(uint64_t append_time, uint64_t submit_time, uint64_t execute_time,
                     const std::string& name) {
    // PTI_ASSERT(!name.empty());

    if (kernel_info_map_.count(name) == 0) {
      ZeKernelInfo info;
      info.append_time = append_time;
      info.submit_time = submit_time;
      info.execute_time = execute_time;
      info.min_time = execute_time;
      info.max_time = execute_time;
      info.call_count = 1;
      kernel_info_map_[name] = info;
    } else {
      ZeKernelInfo& kernel = kernel_info_map_[name];
      kernel.append_time += append_time;
      kernel.submit_time += submit_time;
      kernel.execute_time += execute_time;
      if (execute_time > kernel.max_time) {
        kernel.max_time = execute_time;
      }
      if (execute_time < kernel.min_time) {
        kernel.min_time = execute_time;
      }
      kernel.call_count += 1;
    }
  }

  void CreateCommandList(ze_command_list_handle_t command_list, ze_context_handle_t context,
                         ze_device_handle_t device, std::pair<uint32_t, uint32_t>& oi_pair,
                         bool immediate) {
    const std::lock_guard<std::mutex> lock(lock_);

#if 0
    auto it = device_descriptors_.find(device);
    if (it != device_descriptors_.end()) {
      //if (metric_activations_.find({it->second.context, device}) == metric_activations_.end()) {
      if (metric_activations_.find({context, device}) == metric_activations_.end()) {
overhead::Init();
        //ze_result_t status = zetContextActivateMetricGroups(it->second.context, device, 1, &(it->second.metric_group));
{std::string o_api_string = "zetContextActivateMetricGroups";overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,o_api_string.c_str());};
        std::cout << "CreateCommandList it->second.metric_group = " << it->second.metric_group << std::endl;
overhead::Init();
        ze_result_t status = zetContextActivateMetricGroups(context, device, 1, &(it->second.metric_group));
{std::string o_api_string = "zetContextActivateMetricGroups";overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,o_api_string.c_str());};
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        //metric_activations_.insert({it->second.context, device});
        metric_activations_.insert({context, device});
      }
    }
#endif /* 0 */

    if (command_list_map_.count(command_list)) {
      ZeCommandListInfo& command_list_info = command_list_map_[command_list];
      if (command_list_info.immediate) {
        queue_ordinal_index_map_.erase((ze_command_queue_handle_t)command_list);
      }
      command_list_map_.erase(command_list);
    }

    PTI_ASSERT(device_descriptors_.count(device) != 0);

    command_list_map_[command_list] = {
        std::vector<ZeKernelCommand*>(), context, device, nullptr, nullptr, immediate, oi_pair};

    if (immediate) {
      if (queue_ordinal_index_map_.count((ze_command_queue_handle_t)command_list) == 0) {
        queue_ordinal_index_map_[(ze_command_queue_handle_t)command_list] = oi_pair;
      }
    }
  }

#if 0
  void RemoveKernelCommands(ZeCommandListInfo& info) {
    for (ZeKernelCommand* command : info.kernel_commands) {
      bool signaled = true;
      for (auto it = kernel_command_list_.begin(); it != kernel_command_list_.end();) {
        if (command == *it) {
          (*it)->command_list = nullptr;
          signaled = false;
        }
        it++;
      }
      if (signaled) {
        event_cache_.ReleaseEvent(command->event);
        delete command;
      }
    }
    info.kernel_commands.clear();
  }

  void RemoveCommandList(ze_command_list_handle_t command_list) {

    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    RemoveKernelCommands(command_list_info);
    if (command_list_info.immediate) {
      queue_ordinal_index_map_.erase((ze_command_queue_handle_t)command_list);
    }
    command_list_map_.erase(command_list);
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    // lock is acquired in the caller
    // const std::lock_guard<std::mutex> lock(lock_);

    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    RemoveKernelCommands(command_list_info);
  }
#endif /* 0 */

  void SubmitKernelCommands(ze_command_list_handle_t* command_lists, uint32_t command_list_count,
                            ze_command_queue_handle_t queue, ze_fence_handle_t fence,
                            const uint64_t host_sync, std::vector<uint64_t>* kids) {
    const std::lock_guard<std::mutex> lock(lock_);

    for (uint32_t i = 0; i < command_list_count; ++i) {
      ze_command_list_handle_t clist = command_lists[i];
      PTI_ASSERT(clist != nullptr);
      PTI_ASSERT(command_list_map_.count(clist) == 1);
      ZeCommandListInfo& info = command_list_map_[clist];
      if (!info.immediate) {
        for (ZeKernelCommand* command : info.kernel_commands) {
          if (!command->tid) command->tid = utils::GetTid();
          command->queue = queue;
          command->submit_time = host_sync;

          PTI_ASSERT(command->append_time <= command->submit_time);
          command->fence = fence;

          // if (queue_ordinal_index_map_.count(queue) != 0) {
          //   std::pair<uint32_t, uint32_t> oi = queue_ordinal_index_map_[queue];
          // }

          if (kids) {
            kids->push_back(command->kernel_id);
          }
          kernel_command_list_.push_back(command);
        }
        info.kernel_commands.clear();
      }
    }
  }

  ze_context_handle_t GetCommandListContext(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    // const std::lock_guard<std::mutex> lock(lock_);

    if (command_list_map_.count(command_list) != 1) {
      std::cout << "ERROR: command list " << command_list << " not created" << std::endl;
      PTI_ASSERT(0);
    }

    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.context;
  }

  ze_device_handle_t GetCommandListDevice(ze_command_list_handle_t command_list) {
    // PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.device;
  }

  bool IsCommandListImmediate(ze_command_list_handle_t command_list) {
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];
    return command_list_info.immediate;
  }

  void AddImage(ze_image_handle_t image, size_t size) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 0);
    image_size_map_[image] = size;
  }

  void RemoveImage(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(image_size_map_.count(image) == 1);
    image_size_map_.erase(image);
  }

  size_t GetImageSize(ze_image_handle_t image) {
    // PTI_ASSERT(image != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (image_size_map_.count(image) == 1) {
      return image_size_map_[image];
    }
    return 0;
  }

  void AddKernelGroupSize(ze_kernel_handle_t kernel, const ZeKernelGroupSize& group_size) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_[kernel] = group_size;
  }

  void RemoveKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    kernel_group_size_map_.erase(kernel);
  }

  ZeKernelGroupSize GetKernelGroupSize(ze_kernel_handle_t kernel) {
    // PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (kernel_group_size_map_.count(kernel) == 0) {
      return {0, 0, 0};
    }
    return kernel_group_size_map_[kernel];
  }

 private:  // Callbacks
  static void OnEnterEventPoolCreate(ze_event_pool_create_params_t* params, ze_result_t, void*,
                                     void** instance_data) {
    const ze_event_pool_desc_t* desc = *(params->pdesc);
    if (desc == nullptr) {
      return;
    }
    if (desc->flags & ZE_EVENT_POOL_FLAG_IPC) {
      return;
    }

    ze_event_pool_desc_t* profiling_desc = new ze_event_pool_desc_t;
    PTI_ASSERT(profiling_desc != nullptr);
    profiling_desc->stype = desc->stype;
    // PTI_ASSERT(profiling_desc->stype == ZE_STRUCTURE_TYPE_EVENT_POOL_DESC);
    profiling_desc->pNext = desc->pNext;
    profiling_desc->flags = desc->flags;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    profiling_desc->flags |= ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    profiling_desc->count = desc->count;

    *(params->pdesc) = profiling_desc;
    *instance_data = profiling_desc;
  }

  static void OnExitEventPoolCreate(ze_event_pool_create_params_t* /*params*/,
                                    ze_result_t /*result*/, void* /*global_data*/,
                                    void** instance_data) {
    ze_event_pool_desc_t* desc = static_cast<ze_event_pool_desc_t*>(*instance_data);
    if (desc != nullptr) {
      delete desc;
    }
  }

  static void OnEnterEventDestroy(ze_event_destroy_params_t* params, ze_result_t /*result*/,
                                  void* global_data, void** /*instance_data*/,
                                  std::vector<uint64_t>* kids) {
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnEnterEventHostReset(ze_event_host_reset_params_t* params, ze_result_t /*result*/,
                                    void* global_data, void** /*instance_data*/,
                                    std::vector<uint64_t>* kids) {
    if (*(params->phEvent) != nullptr) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitEventHostSynchronize(ze_event_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitEventQueryStatus(ze_event_query_status_params_t* params, ze_result_t result,
                                     void* global_data, void** /*instance_data*/,
                                     std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phEvent) != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phEvent), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitFenceHostSynchronize(ze_fence_host_synchronize_params_t* params,
                                         ze_result_t result, void* global_data,
                                         void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*(params->phFence) != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCall(*(params->phFence), kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitImageCreate(ze_image_create_params_t* params, ze_result_t result,
                                void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);

      ze_image_desc_t image_desc = **(params->pdesc);
      size_t image_size = image_desc.width;
      switch (image_desc.type) {
        case ZE_IMAGE_TYPE_2D:
        case ZE_IMAGE_TYPE_2DARRAY:
          image_size *= image_desc.height;
          break;
        case ZE_IMAGE_TYPE_3D:
          image_size *= image_desc.height * image_desc.depth;
          break;
        default:
          break;
      }

      switch (image_desc.format.type) {
        case ZE_IMAGE_FORMAT_TYPE_UINT:
        case ZE_IMAGE_FORMAT_TYPE_UNORM:
        case ZE_IMAGE_FORMAT_TYPE_FORCE_UINT32:
          image_size *= sizeof(unsigned int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_SINT:
        case ZE_IMAGE_FORMAT_TYPE_SNORM:
          image_size *= sizeof(int);
          break;
        case ZE_IMAGE_FORMAT_TYPE_FLOAT:
          image_size *= sizeof(float);
          break;
        default:
          break;
      }

      collector->AddImage(**(params->pphImage), image_size);
    }
  }

  static void OnExitImageDestroy(ze_image_destroy_params_t* params, ze_result_t result,
                                 void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      collector->RemoveImage(*(params->phImage));
    }
  }

  static zet_metric_query_handle_t PrepareToAppendKernelCommand(
      ZeCollector* collector, ze_event_handle_t& signal_event,
      ze_command_list_handle_t command_list, bool iskernel) {
    PTI_ASSERT(command_list != nullptr);

    const std::lock_guard<std::mutex> lock(collector->lock_);
    ze_context_handle_t context = collector->GetCommandListContext(command_list);
    if (signal_event == nullptr) {
      signal_event = collector->event_cache_.GetEvent(context);
      PTI_ASSERT(signal_event != nullptr);
    }

    zet_metric_query_handle_t query = nullptr;
    if (collector->options_.metric_query && iskernel) {
      auto it = collector->command_list_map_.find(command_list);
      PTI_ASSERT(it != collector->command_list_map_.end());
      auto it2 = collector->device_descriptors_.find(it->second.device);
      PTI_ASSERT(it2 != collector->device_descriptors_.end());
      query =
          collector->query_pools_.GetQuery(context, it->second.device, it2->second.metric_group);
      PTI_ASSERT(query != nullptr);

      overhead::Init();
      ze_result_t status = zetCommandListAppendMetricQueryBegin(command_list, query);
      {
        std::string o_api_string = "zetCommandListAppendMetricQueryBegin";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
    return query;
  }

  void AppendKernelCommandCommon(ZeCollector* collector, ZeKernelCommandProps& props,
                                 ze_event_handle_t& signal_event, zet_metric_query_handle_t& query,
                                 ze_command_list_handle_t command_list,
                                 ZeCommandListInfo& command_list_info, void** /*instance_data*/,
                                 std::vector<uint64_t>* kids) {
    ZeKernelCommand* command = new ZeKernelCommand;
    PTI_ASSERT(command != nullptr);
    command->props = props;
    command->command_list = command_list;

    ze_device_handle_t device = command_list_info.device;
    PTI_ASSERT(device != nullptr);
    command->device = device;

    PTI_ASSERT(signal_event != nullptr);
    command->event = signal_event;
    command->metric_query = query;
    ze_event_handle_t metric_query_event = nullptr;
    if (query != nullptr) {
      ze_context_handle_t context = collector->GetCommandListContext(command_list);
      metric_query_event = collector->event_cache_.GetEvent(context);
      overhead::Init();
      ze_result_t status =
          zetCommandListAppendMetricQueryEnd(command_list, query, metric_query_event, 0, nullptr);
      {
        std::string o_api_string = "zetCommandListAppendMetricQueryEnd";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
    command->metric_query_event = metric_query_event;
#if 0
    if (signal_event == nullptr) {
      ze_context_handle_t context = command_list_info.context;
      command->event = event_cache_.GetEvent(context);
      PTI_ASSERT(command->event != nullptr);
      signal_event = command->event;
    } else {
      command->event = signal_event;
    }
#endif /* 0 */

    command->tid = utils::GetTid();
    uint64_t host_timestamp = ze_instance_data.start_time_host;
    command->append_time = host_timestamp;
    command->kernel_id = UniKernelId::GetKernelId();
    if (command->props.type == KERNEL_COMMAND_TYPE_KERNEL) {
      ze_device_properties_t dev_props;
      dev_props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
      dev_props.pNext = nullptr;
      ze_result_t status = zeDeviceGetProperties(device, &dev_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      std::copy_n(dev_props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE, dev_uuid_map[command->src_device]);

      command->sycl_node_id_ = sycl_data_kview.sycl_node_id_;
      command->sycl_invocation_id_ = sycl_data_kview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_kview.sycl_task_begin_time_;
      command->sycl_enqk_begin_time_ = sycl_data_kview.sycl_enqk_begin_time_;
      sycl_data_kview.kid_ = command->kernel_id;
      sycl_data_kview.tid_ = command->tid;
      command->source_file_name_ = sycl_data_kview.source_file_name_;
      command->source_line_number_ = sycl_data_kview.source_line_number_;
      if (sycl_data_kview.cid_)
        command->corr_id_ = sycl_data_kview.cid_;
      else
        command->corr_id_ = UniCorrId::GetUniCorrId();
    } else if (command->props.type == KERNEL_COMMAND_TYPE_MEMORY) {
      command->src_device = command_list_info.src_device;
      command->dst_device = command_list_info.dst_device;

      sycl_data_mview.kid_ = command->kernel_id;
      sycl_data_mview.tid_ = command->tid;
      if (sycl_data_mview.cid_)
        command->corr_id_ = sycl_data_mview.cid_;
      else
        command->corr_id_ = UniCorrId::GetUniCorrId();

      command->sycl_node_id_ = sycl_data_mview.sycl_node_id_;
      command->sycl_invocation_id_ = sycl_data_mview.sycl_invocation_id_;
      command->sycl_task_begin_time_ = sycl_data_mview.sycl_task_begin_time_;
      command->source_file_name_ = sycl_data_mview.source_file_name_;
      command->source_line_number_ = sycl_data_mview.source_line_number_;
    } else {
      command->corr_id_ = UniCorrId::GetUniCorrId();
    }
    if (command_list_info.immediate) {
      // command->tid = utils::GetTid();
      command->submit_time = command->append_time;
      command->queue = reinterpret_cast<ze_command_queue_handle_t>(command_list);
      kernel_command_list_.push_back(command);
      kids->push_back(command->kernel_id);
    } else {
      command_list_info.kernel_commands.push_back(command);
    }
  }

  void AppendKernel(ZeCollector* collector, ze_kernel_handle_t kernel,
                    const ze_group_count_t* group_count, ze_event_handle_t& signal_event,
                    zet_metric_query_handle_t& query, ze_command_list_handle_t command_list,
                    void** instance_data, std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    ZeKernelCommandProps props{};

    props.name = utils::ze::GetKernelName(kernel, options_.demangle);
    props.type = KERNEL_COMMAND_TYPE_KERNEL;
    props.simd_width = utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelGroupSize group_size;
    if (kernel_group_size_map_.count(kernel) == 0) {
      group_size = {0, 0, 0};
    } else {
      group_size = kernel_group_size_map_[kernel];
    }

    props.group_size[0] = group_size.x;
    props.group_size[1] = group_size.y;
    props.group_size[2] = group_size.z;

    if (group_count != nullptr) {
      props.group_count[0] = group_count->groupCountX;
      props.group_count[1] = group_count->groupCountY;
      props.group_count[2] = group_count->groupCountZ;
    }

#if 0
    uint64_t base_addr = 0;
    if ((zexKernelGetBaseAddress != nullptr) && (zexKernelGetBaseAddress(kernel, &base_addr) == ZE_RESULT_SUCCESS)) {
      base_addr &= 0xFFFFFFFF;
    }
    props.base_addr = base_addr;
    props.size = 0;

    if (props.type == KERNEL_COMMAND_TYPE_KERNEL) {
      if ((props.base_addr != 0) && (kernel_mem_info_map_.find(props.base_addr) == kernel_mem_info_map_.end())) {
        ZeKernelMemInfo m;

        m.name = props.name;
        m.base_addr = props.base_addr;
        m.size = props.size;
        kernel_mem_info_map_.insert(std::pair<uint64_t, ZeKernelMemInfo>(props.base_addr, m));
      }
    }
#endif /* 0 */

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];

    AppendKernelCommandCommon(collector, props, signal_event, query, command_list,
                              command_list_info, instance_data, kids);
  }

  void AppendMemoryCommand(ZeCollector* collector, std::string command, size_t bytes_transferred,
                           const void* src, const void* dst, ze_event_handle_t& signal_event,
                           zet_metric_query_handle_t& query, ze_command_list_handle_t command_list,
                           void** instance_data, std::vector<uint64_t>* kids,
                           const void* pattern = nullptr, size_t pattern_size = 0) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    ze_memory_allocation_properties_t mem_props;
    mem_props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    mem_props.pNext = nullptr;

    ze_device_properties_t dev_props;
    dev_props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    dev_props.pNext = nullptr;

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    command_list_info.src_device = nullptr;
    command_list_info.dst_device = nullptr;

    if (dst != nullptr) {
      ze_result_t status =
          zeMemGetAllocProperties(context, dst, &mem_props, &command_list_info.dst_device);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (command_list_info.dst_device) {
        ze_result_t status = zeDeviceGetProperties(command_list_info.dst_device, &dev_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::copy_n(dev_props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE,
                    dev_uuid_map[command_list_info.dst_device]);
      }
    }
    if (src != nullptr) {
      ze_result_t status =
          zeMemGetAllocProperties(context, src, &mem_props, &command_list_info.src_device);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      if (command_list_info.src_device) {
        ze_result_t status = zeDeviceGetProperties(command_list_info.src_device, &dev_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::copy_n(dev_props.uuid.id, ZE_MAX_DEVICE_UUID_SIZE,
                    dev_uuid_map[command_list_info.src_device]);
      }
    }

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, (src ? context : nullptr), src,
                         (dst ? context : nullptr), dst, pattern, pattern_size);

    AppendKernelCommandCommon(collector, props, signal_event, query, command_list,
                              command_list_info, instance_data, kids);
  }

  void AppendMemoryCommandContext(ZeCollector* collector, std::string command,
                                  size_t bytes_transferred, ze_context_handle_t src_context,
                                  const void* src, ze_context_handle_t dst_context, const void* dst,
                                  ze_event_handle_t& signal_event, zet_metric_query_handle_t& query,
                                  ze_command_list_handle_t command_list, void** instance_data,
                                  std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, src_context, src,
                         (dst_context ? dst_context : context), dst);

    AppendKernelCommandCommon(collector, props, signal_event, query, command_list,
                              command_list_info, instance_data, kids);
  }

  void AppendImageMemoryCopyCommand(ZeCollector* collector, std::string command,
                                    ze_image_handle_t image, const void* src, const void* dst,
                                    ze_event_handle_t& signal_event,
                                    zet_metric_query_handle_t& query,
                                    ze_command_list_handle_t command_list, void** instance_data,
                                    std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    size_t bytes_transferred = 0;
    if (image_size_map_.count(image) == 1) {
      bytes_transferred = image_size_map_[image];
    }

    ZeKernelCommandProps props =
        GetTransferProps(std::move(command), bytes_transferred, context, src, context, dst);

    AppendKernelCommandCommon(collector, props, signal_event, query, command_list,
                              command_list_info, instance_data, kids);
  }

  void AppendCommand(ZeCollector* collector, std::string command, ze_event_handle_t& signal_event,
                     zet_metric_query_handle_t& query, ze_command_list_handle_t command_list,
                     void** instance_data, std::vector<uint64_t>* kids) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(command_list_map_.count(command_list) == 1);
    ZeCommandListInfo& command_list_info = command_list_map_[command_list];

    ze_context_handle_t context = command_list_info.context;
    PTI_ASSERT(context != nullptr);

    ZeKernelCommandProps props{};
    props.name = std::move(command);
    props.type = KERNEL_COMMAND_TYPE_COMMAND;

    AppendKernelCommandCommon(collector, props, signal_event, query, command_list,
                              command_list_info, instance_data, kids);
  }

  static ZeKernelCommandProps GetKernelProps(ZeCollector* collector, ze_kernel_handle_t kernel,
                                             const ze_group_count_t* group_count) {
    PTI_ASSERT(kernel != nullptr);

    ZeKernelCommandProps props{};

    props.name = utils::ze::GetKernelName(kernel, collector->options_.demangle);
    props.type = KERNEL_COMMAND_TYPE_KERNEL;
    props.simd_width = utils::ze::GetKernelMaxSubgroupSize(kernel);
    props.bytes_transferred = 0;

    ZeKernelGroupSize group_size = collector->GetKernelGroupSize(kernel);
    props.group_size[0] = group_size.x;
    props.group_size[1] = group_size.y;
    props.group_size[2] = group_size.z;

    if (group_count != nullptr) {
      props.group_count[0] = group_count->groupCountX;
      props.group_count[1] = group_count->groupCountY;
      props.group_count[2] = group_count->groupCountZ;
    }

    uint64_t base_addr = 0;
    if ((zexKernelGetBaseAddress != nullptr) &&
        (zexKernelGetBaseAddress(kernel, &base_addr) == ZE_RESULT_SUCCESS)) {
      base_addr &= 0xFFFFFFFF;
    }
    props.base_addr = base_addr;
    props.size = 0;

    return props;
  }

  static ZeKernelCommandProps GetTransferProps(std::string name, size_t bytes_transferred,
                                               ze_context_handle_t src_context, const void* src,
                                               ze_context_handle_t dst_context, const void* dst,
                                               [[maybe_unused]] const void* pattern = nullptr,
                                               size_t pattern_size = 0) {
    PTI_ASSERT(!name.empty());

    std::string direction;
    ze_device_handle_t hSrcDevice = nullptr;
    ze_device_handle_t hDstDevice = nullptr;
    bool p2p = false;

    if (src_context != nullptr && src != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(src_context, src, &props, &hSrcDevice);
      {
        std::string o_api_string = "zeMemGetAllocProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          p2p = true;
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          p2p = true;
          break;
        default:
          p2p = false;
          break;
      }
    }

    if (dst_context != nullptr && dst != nullptr) {
      ze_memory_allocation_properties_t props;
      props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
      props.pNext = nullptr;
      props.pNext = nullptr;
      overhead::Init();
      ze_result_t status = zeMemGetAllocProperties(dst_context, dst, &props, &hDstDevice);
      {
        std::string o_api_string = "zeMemGetAllocProperties";
        overhead::FiniLevel0(overhead::OverheadRuntimeType::OVERHEAD_RUNTIME_TYPE_L0,
                             o_api_string.c_str());
      };
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      direction.push_back('2');
      switch (props.type) {
        case ZE_MEMORY_TYPE_UNKNOWN:
          direction.push_back('M');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_HOST:
          direction.push_back('H');
          p2p = false;
          break;
        case ZE_MEMORY_TYPE_DEVICE:
          direction.push_back('D');
          break;
        case ZE_MEMORY_TYPE_SHARED:
          direction.push_back('S');
          break;
        default:
          p2p = false;
          break;
      }
    }

    //
    // TODO:  Redo the stringified -P2P propagation.
    //
    if (!direction.empty()) {
      ze_bool_t p2p_access = 0;
      ze_result_t status;
      if (p2p && hSrcDevice && hDstDevice && (hSrcDevice != hDstDevice)) {
        status = zeDeviceCanAccessPeer(hSrcDevice, hDstDevice, &p2p_access);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        if (p2p_access) direction.append(" - P2P");
      }
      name += "(" + direction + ")";
    }

    ZeKernelCommandProps props{};
    props.name = std::move(name);
    props.bytes_transferred = bytes_transferred;
    props.value_size = pattern_size;
    props.type = KERNEL_COMMAND_TYPE_MEMORY;
    return props;
  }

  static ZeKernelCommandProps GetCommandProps(std::string name) {
    PTI_ASSERT(!name.empty());

    ZeKernelCommandProps props{};
    props.name = name;
    props.type = KERNEL_COMMAND_TYPE_COMMAND;
    return props;
  }

  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, ze_result_t result, void* global_data,
      void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), true);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params, ze_result_t result, void* global_data,
      void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                              *(params->phSignalEvent), query, *(params->phCommandList),
                              instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), true);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendLaunchCooperativeKernel(
      ze_command_list_append_launch_cooperative_kernel_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchFuncArgs),
                              *(params->phSignalEvent), query, *(params->phCommandList),
                              instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), true);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendLaunchKernelIndirect(
      ze_command_list_append_launch_kernel_indirect_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendKernel(collector, *(params->phKernel), *(params->ppLaunchArgumentsBuffer),
                              *(params->phSignalEvent), query, *(params->phCommandList),
                              instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopy(
      ze_command_list_append_memory_copy_params_t* params, ze_result_t result, void* global_data,
      void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendMemoryCopy(ze_command_list_append_memory_copy_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryCopy", *(params->psize),
                                     *(params->psrcptr), *(params->pdstptr),
                                     *(params->phSignalEvent), query, *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryFill(
      ze_command_list_append_memory_fill_params_t* params, ze_result_t result, void* global_data,
      void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendMemoryFill(ze_command_list_append_memory_fill_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryFill", *(params->psize),
                                     *(params->pptr), nullptr, *(params->phSignalEvent), query,
                                     *(params->phCommandList), instance_data, kids,
                                     *(params->ppattern), *(params->ppattern_size));
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                              ze_result_t result, void* global_data,
                                              void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendBarrier(ze_command_list_append_barrier_params_t* params,
                                             ze_result_t result, void* global_data,
                                             void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendCommand(collector, "zeCommandListAppendBarrier", *(params->phSignalEvent),
                               query, *(params->phCommandList), instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendMemoryRangesBarrier(
      ze_command_list_append_memory_ranges_barrier_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendCommand(collector, "zeCommandListAppendMemoryRangesBarrier",
                               *(params->phSignalEvent), query, *(params->phCommandList),
                               instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendMemoryCopyRegion(
      ze_command_list_append_memory_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      size_t bytes_transferred = 0;
      const ze_copy_region_t* region = *(params->psrcRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height * (*(params->psrcPitch));
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      collector->AppendMemoryCommand(collector, "zeCommandListAppendMemoryCopyRegion",
                                     bytes_transferred, *(params->psrcptr), *(params->pdstptr),
                                     *(params->phSignalEvent), query, *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendMemoryCopyFromContext(
      ze_command_list_append_memory_copy_from_context_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ze_context_handle_t src_context = *(params->phContextSrc);
      // ze_context_handle_t dst_context = nullptr;
      collector->AppendMemoryCommandContext(collector, "zeCommandListAppendMemoryCopyFromContext",
                                            *(params->psize), src_context, *(params->psrcptr),
                                            nullptr, *(params->pdstptr), *(params->phSignalEvent),
                                            query, *(params->phCommandList), instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                                ze_result_t result, void* global_data,
                                                void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendImageCopy(ze_command_list_append_image_copy_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopy", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), query, *(params->phCommandList), instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendImageCopyRegion(
      ze_command_list_append_image_copy_region_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendImageMemoryCopyCommand(
          collector, "zeCommandListAppendImageCopyRegion", *(params->phSrcImage), nullptr, nullptr,
          *(params->phSignalEvent), query, *(params->phCommandList), instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendImageCopyToMemory(
      ze_command_list_append_image_copy_to_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      collector->AppendImageMemoryCopyCommand(collector, "zeCommandListAppendImageCopyRegion",
                                              *(params->phSrcImage), nullptr, *(params->pdstptr),
                                              *(params->phSignalEvent), query,
                                              *(params->phCommandList), instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnEnterCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data) {
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      zet_metric_query_handle_t query = PrepareToAppendKernelCommand(
          collector, *(params->phSignalEvent), *(params->phCommandList), false);
      *instance_data = reinterpret_cast<void*>(query);
    } else {
      *instance_data = nullptr;
    }
  }

  static void OnExitCommandListAppendImageCopyFromMemory(
      ze_command_list_append_image_copy_from_memory_params_t* params, ze_result_t result,
      void* global_data, void** instance_data, std::vector<uint64_t>* kids) {
    zet_metric_query_handle_t query = reinterpret_cast<zet_metric_query_handle_t>(*instance_data);
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    if ((result == ZE_RESULT_SUCCESS) && (UniController::IsCollectionEnabled())) {
      size_t bytes_transferred = 0;
      const ze_image_region_t* region = *(params->ppDstRegion);

      if (region != nullptr) {
        bytes_transferred = region->width * region->height;
        if (region->depth != 0) {
          bytes_transferred *= region->depth;
        }
      }

      collector->AppendMemoryCommand(collector, "zeCommandListAppendImageCopyFromMemory",
                                     bytes_transferred, *(params->psrcptr), nullptr,
                                     *(params->phSignalEvent), query, *(params->phCommandList),
                                     instance_data, kids);
    } else {
      collector->query_pools_.PutQuery(query);
      collector->event_cache_.ReleaseEvent(*(params->phSignalEvent));
    }
  }

  static void OnExitCommandListCreate(ze_command_list_create_params_t* params, ze_result_t result,
                                      void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);

      // dummy pair
      std::pair<uint32_t, uint32_t> oi(-1, -1);
      collector->CreateCommandList(**(params->pphCommandList), *(params->phContext),
                                   *(params->phDevice), oi, false);
    }
  }

  static void OnExitCommandListCreateImmediate(ze_command_list_create_immediate_params_t* params,
                                               ze_result_t result, void* global_data,
                                               void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(**params->pphCommandList != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      ze_device_handle_t* hDevice = params->phDevice;
      if (hDevice == nullptr) {
        return;
      }

      const ze_command_queue_desc_t* clq_desc = *params->paltdesc;
      if (clq_desc == nullptr) {
        return;
      }

      ze_command_list_handle_t* command_list = *params->pphCommandList;
      if (command_list == nullptr) {
        return;
      }

      std::pair<uint32_t, uint32_t> oi(clq_desc->ordinal, clq_desc->index);

      collector->CreateCommandList(**(params->pphCommandList), *(params->phContext),
                                   *(params->phDevice), oi, true);
    }
  }

  static void OnExitCommandListDestroy(ze_command_list_destroy_params_t* params, ze_result_t result,
                                       void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;

      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      // collector->RemoveCommandList(*params->phCommandList);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandListReset(ze_command_list_reset_params_t* params, ze_result_t result,
                                     void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      PTI_ASSERT(*params->phCommandList != nullptr);
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      // collector->ResetCommandList(*params->phCommandList);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandQueueExecuteCommandLists(
      ze_command_queue_execute_command_lists_params_t* params, ze_result_t result,
      void* global_data, void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);

      if (UniController::IsCollectionEnabled()) {
        uint32_t command_list_count = *params->pnumCommandLists;
        if (command_list_count == 0) {
          return;
        }

        ze_command_list_handle_t* command_lists = *params->pphCommandLists;
        if (command_lists == nullptr) {
          return;
        }

        uint64_t host_timestamp = ze_instance_data.start_time_host;
        collector->SubmitKernelCommands(command_lists, command_list_count,
                                        *(params->phCommandQueue), *(params->phFence),
                                        host_timestamp, kids);
      }
    }
  }

  static void OnExitCommandQueueSynchronize(ze_command_queue_synchronize_params_t* /*params*/,
                                            ze_result_t result, void* global_data,
                                            void** /*instance_data*/, std::vector<uint64_t>* kids) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(kids, &kcexec);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitCommandQueueCreate(ze_command_queue_create_params_t* params,
                                       ze_result_t /*result*/, void* global_data,
                                       void** /*instance_data*/) {
    ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
    ze_device_handle_t* hDevice = params->phDevice;
    if (hDevice == nullptr) {
      return;
    }
    const ze_command_queue_desc_t* queue_desc = *params->pdesc;
    if (queue_desc == nullptr) {
      return;
    }
    ze_command_queue_handle_t* command_queue = *params->pphCommandQueue;
    if (command_queue == nullptr) {
      return;
    }

    const std::lock_guard<std::mutex> lock(collector->lock_);
    if (collector->queue_ordinal_index_map_.count(*command_queue) == 0) {
      collector->queue_ordinal_index_map_[*command_queue] =
          std::make_pair(queue_desc->ordinal, queue_desc->index);
    }
  }

  static void OnExitCommandQueueDestroy(ze_command_queue_destroy_params_t* params,
                                        ze_result_t result, void* global_data,
                                        void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      std::vector<ZeKernelCommandExecutionRecord> kcexec;
      collector->lock_.lock();
      collector->ProcessCalls(nullptr, &kcexec);
      collector->queue_ordinal_index_map_.erase(*params->phCommandQueue);
      collector->lock_.unlock();

      if (collector->kcallback_ != nullptr) {
        collector->kcallback_(collector->callback_data_, kcexec);
      }
      if (collector->cb_enabled_.acallback && collector->acallback_ != nullptr) {
        collector->acallback_(collector->callback_data_, kcexec);
      }
    }
  }

  static void OnExitKernelSetGroupSize(ze_kernel_set_group_size_params_t* params,
                                       ze_result_t result, void* global_data,
                                       void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      if (UniController::IsCollectionEnabled()) {
        ZeKernelGroupSize group_size{*(params->pgroupSizeX), *(params->pgroupSizeY),
                                     *(params->pgroupSizeZ)};
        collector->AddKernelGroupSize(*(params->phKernel), group_size);
      }
    }
  }

  static void OnExitKernelDestroy(ze_kernel_destroy_params_t* params, ze_result_t result,
                                  void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      collector->RemoveKernelGroupSize(*(params->phKernel));
    }
  }

  static void OnExitContextDestroy(ze_context_destroy_params_t* params, ze_result_t result,
                                   void* global_data, void** /*instance_data*/) {
    if (result == ZE_RESULT_SUCCESS) {
      ZeCollector* collector = reinterpret_cast<ZeCollector*>(global_data);
      collector->ProcessCalls(nullptr, nullptr);
      collector->event_cache_.ReleaseContext(*(params->phContext));
    }
  }

#include <tracing.gen>  // Auto-generated callbacks

  void AddFunctionTime(const std::string& name, uint64_t time) {
    const std::lock_guard<std::mutex> lock(lock_);
    if (function_info_map_.count(name) == 0) {
      function_info_map_[name] = {time, time, time, 1};
    } else {
      ZeFunction& function = function_info_map_[name];
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

 private:  // Data
  zel_tracer_handle_t tracer_ = nullptr;
  CollectorOptions options_;
  CallbacksEnabled cb_enabled_ = {};
  OnZeKernelFinishCallback acallback_ = nullptr;
  OnZeKernelFinishCallback kcallback_ = nullptr;
  OnZeFunctionFinishCallback fcallback_ = nullptr;
  void* callback_data_ = nullptr;
  std::mutex lock_;

  ZeKernelInfoMap kernel_info_map_;
  std::list<ZeKernelCommand*> kernel_command_list_;
  ZeCommandListMap command_list_map_;
  ZeImageSizeMap image_size_map_;
  ZeKernelGroupSizeMap kernel_group_size_map_;
  ZeDeviceMap device_map_;
  std::map<ze_device_handle_t, ZeDeviceDescriptor> device_descriptors_;

  ZeEventCache event_cache_;

  ZeKernelMemInfoMap kernel_mem_info_map_;

  ZeFunctionInfoMap function_info_map_;
  std::map<ze_command_queue_handle_t, std::pair<uint32_t, uint32_t>> queue_ordinal_index_map_;
  std::map<ze_device_handle_t, uint8_t[ZE_MAX_DEVICE_UUID_SIZE]> dev_uuid_map;

  std::set<std::pair<ze_context_handle_t, ze_device_handle_t>> metric_activations_;
  static const uint32_t kFunctionLength = 10;
  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 12;

  ZeMetricQueryPools query_pools_;
  ZeKernelProfiles kernel_profiles_;

  std::vector<ze_context_handle_t> metric_contexts_;
};

#endif  // PTI_TOOLS_PTI_LEVEL_ZERO_COLLECTOR_H_
