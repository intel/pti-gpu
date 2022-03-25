//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_METRIC_QUERY_COLLECTOR_H_
#define PTI_TOOLS_ONEPROF_METRIC_QUERY_COLLECTOR_H_

#include <list>
#include <vector>
#include <map>
#include <mutex>

#include <level_zero/layers/zel_tracing_api.h>

#include "metric_query_cache.h"
#include "ze_event_cache.h"
#include "ze_utils.h"

struct ZeQueryGroupSize {
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

struct ZeQueryProps {
  std::string name;
  size_t simd_width;
  uint32_t group_count[3];
  uint32_t group_size[3];
};

struct ZeQueryInfo {
  std::string name;
  zet_metric_query_handle_t query;
  ze_event_handle_t event;
};

using ZeQueryGroupSizeMap = std::map<ze_kernel_handle_t, ZeQueryGroupSize>;
using ZeQueryInfoMap =
  std::map<ze_command_list_handle_t, std::vector<ZeQueryInfo> >;
using ZeQueryContextInfo =
  std::map<ze_command_list_handle_t, ze_context_handle_t>;

class MetricQueryCollector {
 public:
  static MetricQueryCollector* Create(
      ze_driver_handle_t driver,
      ze_device_handle_t device,
      const char* group_name,
      const std::string& raw_data_path) {
    PTI_ASSERT(driver != nullptr);
    PTI_ASSERT(device != nullptr);
    PTI_ASSERT(group_name != nullptr);

    zet_metric_group_handle_t group = utils::ze::FindMetricGroup(
        device, group_name,
        ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    if (group == nullptr) {
      std::cerr << "[WARNING] Unable to find target metric group: " <<
        group_name << std::endl;
      return nullptr;
    }

    ze_context_handle_t context = utils::ze::GetContext(driver);
    PTI_ASSERT(context != nullptr);

    MetricQueryCollector* collector = new MetricQueryCollector(
        device, context, group, raw_data_path);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {
        ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;
    status = zelTracerCreate(&tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to create Level Zero tracer" << std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~MetricQueryCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;

    PTI_ASSERT(metric_storage_ != nullptr);
    delete metric_storage_;

    if (tracer_ != nullptr) {
      status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(context_ != nullptr);
    status = zetContextActivateMetricGroups(context_, device_, 0, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
#if !defined(_WIN32)
    ze_result_t status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
#endif
  }

  const std::vector<std::string>& GetKernels() const {
    return kernel_list_;
  }

  MetricQueryCollector(const MetricQueryCollector& copy) = delete;
  MetricQueryCollector& operator=(const MetricQueryCollector& copy) = delete;

 private:
  MetricQueryCollector(
      ze_device_handle_t device,
      ze_context_handle_t context,
      zet_metric_group_handle_t group,
      const std::string& raw_data_path)
      : device_(device),
        context_(context),
        group_(group),
        raw_data_path_(raw_data_path),
        query_cache_(device, group),
        event_cache_(0) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(context_ != nullptr);
    PTI_ASSERT(group_ != nullptr);

    metric_storage_ = MetricStorage::Create(
        0, utils::GetPid(), "query", raw_data_path_);
    PTI_ASSERT(metric_storage_ != nullptr);

    ze_result_t status = zetContextActivateMetricGroups(
        context_, device_, 1, &group_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void EnableTracing(zel_tracer_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t prologue_callbacks{};
    zet_core_callbacks_t epilogue_callbacks{};

    prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnEnterCommandListAppendLaunchKernel;
    epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
      OnExitCommandListAppendLaunchKernel;

    epilogue_callbacks.CommandList.pfnCreateCb =
      OnExitCommandListCreate;
    epilogue_callbacks.CommandList.pfnCreateImmediateCb =
      OnExitCommandListCreateImmediate;
    epilogue_callbacks.CommandList.pfnDestroyCb =
      OnExitCommandListDestroy;
    epilogue_callbacks.CommandList.pfnResetCb =
      OnExitCommandListReset;

    epilogue_callbacks.CommandQueue.pfnSynchronizeCb =
      OnExitCommandQueueSynchronize;
    epilogue_callbacks.CommandQueue.pfnDestroyCb =
      OnExitCommandQueueDestroy;

    epilogue_callbacks.Kernel.pfnSetGroupSizeCb =
      OnExitKernelSetGroupSize;
    epilogue_callbacks.Kernel.pfnDestroyCb =
      OnExitKernelDestroy;

    epilogue_callbacks.Event.pfnHostSynchronizeCb =
      OnExitEventHostSynchronize;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetPrologues(tracer_, &prologue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddQueryGroupSize(
      ze_kernel_handle_t kernel, const ZeQueryGroupSize& group_size) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    query_group_size_map_[kernel] = group_size;
  }

  void RemoveQueryGroupSize(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    query_group_size_map_.erase(kernel);
  }

  ZeQueryGroupSize GetQueryGroupSize(ze_kernel_handle_t kernel) {
    PTI_ASSERT(kernel != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    if (query_group_size_map_.count(kernel) == 0) {
      return {0, 0, 0};
    }
    return query_group_size_map_[kernel];
  }

  void AddQuery(
      ze_command_list_handle_t command_list,
      const ZeQueryInfo& info) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(query_map_.count(command_list) == 1);
    query_map_[command_list].push_back(info);
  }

  void AddQueryMetrics(
      const std::string& name,
      const std::vector<uint8_t>& data) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(!data.empty());

    size_t kernel_id = kernel_list_.size();
    for (size_t i = 0; i < kernel_list_.size(); ++i) {
      if (kernel_list_[i] == name) {
        kernel_id = i;
        break;
      }
    }

    if (kernel_id == kernel_list_.size()) {
      kernel_list_.push_back(name);
    }

    PTI_ASSERT(metric_storage_ != nullptr);
    uint32_t size = static_cast<uint32_t>(data.size());

    metric_storage_->Dump(
        reinterpret_cast<const uint8_t*>(&kernel_id), sizeof(uint32_t), 0);
    metric_storage_->Dump(
        reinterpret_cast<const uint8_t*>(&size), sizeof(uint32_t), 0);
    metric_storage_->Dump(data.data(), size, 0);
  }

  void AddCommandList(
      ze_command_list_handle_t command_list, ze_context_handle_t context) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(query_map_.count(command_list) == 0);
    query_map_.emplace(command_list, std::vector<ZeQueryInfo>());

    PTI_ASSERT(context_map_.count(command_list) == 0);
    context_map_.emplace(command_list, context);
  }

  void RemoveCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    PTI_ASSERT(query_map_.count(command_list) == 1);
    for (auto& info : query_map_[command_list]) {
      event_cache_.ReleaseEvent(info.event);
      query_cache_.ReleaseQuery(info.query);
    }
    query_map_.erase(command_list);

    PTI_ASSERT(context_map_.count(command_list) == 1);
    context_map_.erase(command_list);
  }

  void ResetCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(query_map_.count(command_list) == 1);
    for (auto& info : query_map_[command_list]) {
      event_cache_.ReleaseEvent(info.event);
      query_cache_.ReleaseQuery(info.query);
    }
    query_map_[command_list].clear();
  }

  ze_context_handle_t GetContext(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(context_map_.count(command_list) == 1);
    return context_map_[command_list];
  }

  void ProcessQuery(const ZeQueryInfo& info) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_metric_query_handle_t query = info.query;
    PTI_ASSERT(query != nullptr);

    size_t size = 0;
    status = zetMetricQueryGetData(query, &size, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    PTI_ASSERT(size > 0);

    std::vector<uint8_t> data(size);
    status = zetMetricQueryGetData(query, &size, data.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    AddQueryMetrics(info.name, data);
  }

  void ProcessCommandList(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    PTI_ASSERT(query_map_.count(command_list) == 1);

    for (auto& info : query_map_[command_list]) {
      PTI_ASSERT(info.event != nullptr);
      ze_result_t status = zeEventQueryStatus(info.event);
      if (status == ZE_RESULT_SUCCESS) {
        ProcessQuery(info);
        event_cache_.ResetEvent(info.event);
        query_cache_.ResetQuery(info.query);
      }
    }
  }

  void ProcessQueries() {
    const std::lock_guard<std::mutex> lock(lock_);
    for (auto& item : query_map_) {
      ProcessCommandList(item.first);
    }
  }

  void ProcessQueries(ze_command_list_handle_t command_list) {
    PTI_ASSERT(command_list != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);
    ProcessCommandList(command_list);
  }

  static std::string GetVerboseName(const ZeQueryProps* props) {
    PTI_ASSERT(props != nullptr);
    PTI_ASSERT(!props->name.empty());

    std::stringstream sstream;
    sstream << props->name;
    sstream << "[SIMD";
    if (props->simd_width == 1) {
      sstream << "_ANY";
    } else {
      sstream << props->simd_width;
    }
    sstream << " {" <<
      props->group_count[0] << "; " <<
      props->group_count[1] << "; " <<
      props->group_count[2] << "} {" <<
      props->group_size[0] << "; " <<
      props->group_size[1] << "; " <<
      props->group_size[2] << "}]";

    return sstream.str();
  }

 private:
  static void OnEnterCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    MetricQueryCollector* collector =
      reinterpret_cast<MetricQueryCollector*>(global_data);
    PTI_ASSERT(collector != nullptr);

    ze_command_list_handle_t command_list = *(params->phCommandList);
    if (command_list == nullptr) {
      *instance_data = nullptr;
      return;
    }

    zet_metric_query_handle_t query =
      collector->query_cache_.GetQuery(collector->GetContext(command_list));
    PTI_ASSERT(query != nullptr);

    ze_result_t status =
      zetCommandListAppendMetricQueryBegin(command_list, query);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    *instance_data = reinterpret_cast<void*>(query);
  }

  static void OnExitCommandListAppendLaunchKernel(
      ze_command_list_append_launch_kernel_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    PTI_ASSERT(instance_data != nullptr);
    zet_metric_query_handle_t query =
      reinterpret_cast<zet_metric_query_handle_t>(*instance_data);

    if (query != nullptr) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      if (result != ZE_RESULT_SUCCESS) {
        collector->query_cache_.ReleaseQuery(query);
        return;
      }

      ze_command_list_handle_t command_list = *(params->phCommandList);
      PTI_ASSERT(command_list != nullptr);

      ze_event_handle_t event =
        collector->event_cache_.GetEvent(collector->GetContext(command_list));
      PTI_ASSERT(event != nullptr);

      ze_result_t status = zetCommandListAppendMetricQueryEnd(
          command_list, query, event, 0, nullptr);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      ze_kernel_handle_t kernel = *(params->phKernel);
      PTI_ASSERT(kernel != nullptr);

      const ze_group_count_t* group_count = *(params->ppLaunchFuncArgs);

      ZeQueryProps props{};
      props.name = utils::ze::GetKernelName(kernel, false);
      props.simd_width = utils::ze::GetKernelMaxSubgroupSize(kernel);

      if (group_count != nullptr) {
        props.group_count[0] = group_count->groupCountX;
        props.group_count[1] = group_count->groupCountY;
        props.group_count[2] = group_count->groupCountZ;
      }

      ZeQueryGroupSize group_size = collector->GetQueryGroupSize(kernel);
      props.group_size[0] = group_size.x;
      props.group_size[1] = group_size.y;
      props.group_size[2] = group_size.z;

      std::string name = GetVerboseName(&props);
      collector->AddQuery(command_list, {name, query, event});
    }
  }

  static void OnExitCommandListCreate(
      ze_command_list_create_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ze_command_list_handle_t command_list = **(params->pphCommandList);
      PTI_ASSERT(command_list != nullptr);

      ze_context_handle_t context = *(params->phContext);
      PTI_ASSERT(context != nullptr);

      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      collector->AddCommandList(command_list, context);
    }
  }

  static void OnExitCommandListCreateImmediate(
      ze_command_list_create_immediate_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ze_command_list_handle_t command_list = **(params->pphCommandList);
      PTI_ASSERT(command_list != nullptr);

      ze_context_handle_t context = *(params->phContext);
      PTI_ASSERT(context != nullptr);

      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      collector->AddCommandList(command_list, context);
    }
  }

  static void OnExitCommandListDestroy(
      ze_command_list_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ze_command_list_handle_t command_list = *(params->phCommandList);
      PTI_ASSERT(command_list != nullptr);

      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      collector->ProcessQueries(command_list);
      collector->RemoveCommandList(command_list);
    }
  }

  static void OnExitCommandListReset(
      ze_command_list_reset_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      ze_command_list_handle_t command_list = *(params->phCommandList);
      PTI_ASSERT(command_list != nullptr);

      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);

      collector->ProcessQueries(command_list);
      collector->ResetCommandList(command_list);
    }
  }

  static void OnExitCommandQueueSynchronize(
      ze_command_queue_synchronize_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessQueries();
    }
  }

  static void OnExitCommandQueueDestroy(
      ze_command_queue_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessQueries();
    }
  }

  static void OnExitEventHostSynchronize(
      ze_event_host_synchronize_params_t *params,
      ze_result_t result, void *global_data, void **instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->ProcessQueries();
    }
  }

  static void OnExitKernelSetGroupSize(
      ze_kernel_set_group_size_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      ZeQueryGroupSize group_size{
          *(params->pgroupSizeX),
          *(params->pgroupSizeY),
          *(params->pgroupSizeZ)};
      collector->AddQueryGroupSize(*(params->phKernel), group_size);
    }
  }

  static void OnExitKernelDestroy(
      ze_kernel_destroy_params_t* params,
      ze_result_t result, void* global_data, void** instance_data) {
    if (result == ZE_RESULT_SUCCESS) {
      MetricQueryCollector* collector =
        reinterpret_cast<MetricQueryCollector*>(global_data);
      PTI_ASSERT(collector != nullptr);
      collector->RemoveQueryGroupSize(*(params->phKernel));
    }
  }

 private:
  ze_context_handle_t context_ = nullptr;
  ze_device_handle_t device_ = nullptr;
  zel_tracer_handle_t tracer_ = nullptr;
  zet_metric_group_handle_t group_ = nullptr;

  ZeEventCache event_cache_;
  MetricQueryCache query_cache_;

  std::mutex lock_;
  ZeQueryInfoMap query_map_;
  ZeQueryGroupSizeMap query_group_size_map_;
  ZeQueryContextInfo context_map_;
  std::vector<std::string> kernel_list_;

  MetricStorage* metric_storage_ = nullptr;

  std::string raw_data_path_;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_QUERY_COLLECTOR_H_