//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <atomic>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ze_utils.h"
#include "utils.h"

#define MAX_KERNEL_COUNT 1024

struct InstanceData {
  uint32_t kernel_id;
  zet_metric_query_handle_t metric_query;
};

struct QueryData {
  std::string kernel_name;
  zet_metric_query_handle_t query;
  ze_event_handle_t event;
};

struct KernelData {
  uint32_t call_count;
  float eu_active;
  float eu_stall;
  uint64_t gpu_time;
};

struct GlobalToolState {
  ze_device_handle_t device = nullptr;
  ze_context_handle_t context = nullptr;
  zet_tracer_exp_handle_t tracer = nullptr;
  zet_metric_group_handle_t metric_group = nullptr;
  zet_metric_query_pool_handle_t metric_query_pool = nullptr;
  ze_event_pool_handle_t event_pool = nullptr;
  std::atomic<uint32_t> kernel_id{0};
  std::mutex lock;
  std::map<ze_kernel_handle_t, std::string> kernel_name_map;
  std::vector<QueryData> query_list;
  std::map<std::string, KernelData> kernel_data_map;
};

const char* kLine =
  "+----------------------------------------------------------"
  "----------------------------------------------------------+";

const char* kHeader =
  "| Kernel                                     | Call Count |"
  " Total Time, ms | EU Active, % | EU Stall, % | EU Idle, % |";

static GlobalToolState* state = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_metric_query[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
  utils::SetEnv("ZET_ENABLE_METRICS=1");
}

static void Calculate(zet_metric_group_handle_t group,
                      const std::vector<uint8_t>& data,
                      uint64_t& gpu_time,
                      float& eu_active,
                      float& eu_stall) {
  PTI_ASSERT(group != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t value_count = 0;
  status = zetMetricGroupCalculateMetricValues(
      group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, data.size(),
      data.data(), &value_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(value_count > 0);

  std::vector<zet_typed_value_t> value_list(value_count);
  status = zetMetricGroupCalculateMetricValues(
      group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES, data.size(),
      data.data(), &value_count, value_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint32_t metric_count = 0;
  status = zetMetricGet(group, &metric_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint32_t report_count = value_count / metric_count;
  PTI_ASSERT(report_count * metric_count == value_count);
  PTI_ASSERT(report_count == 1);

  int gpu_time_id = utils::ze::GetMetricId(group, "GpuTime");
  PTI_ASSERT(gpu_time_id >= 0);
  int eu_active_id = utils::ze::GetMetricId(group, "EuActive");
  PTI_ASSERT(eu_active_id >= 0);
  int eu_stall_id = utils::ze::GetMetricId(group, "EuStall");
  PTI_ASSERT(eu_stall_id >= 0);

  PTI_ASSERT(value_list[gpu_time_id].type == ZET_VALUE_TYPE_UINT64);
  gpu_time = value_list[gpu_time_id].value.ui64;
  PTI_ASSERT(value_list[eu_active_id].type == ZET_VALUE_TYPE_FLOAT32);
  eu_active = value_list[eu_active_id].value.fp32;
  PTI_ASSERT(value_list[eu_stall_id].type == ZET_VALUE_TYPE_FLOAT32);
  eu_stall = value_list[eu_stall_id].value.fp32;
}

static void PrintResults() {
  PTI_ASSERT(state != nullptr);

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  if (state->kernel_data_map.size() == 0) {
    return;
  }

  for (auto kernel : state->kernel_data_map) {
    float eu_active = kernel.second.eu_active;
    float eu_stall = kernel.second.eu_stall;
    float euIdle = eu_active + eu_stall < 100.0f ?
      100.0f - eu_active - eu_stall : 0.0f;

    std::string kernel_name = kernel.first;
    if (kernel_name.size() > 40) {
      kernel_name = kernel_name.substr(0, 32) + "<...>";
    }

    std::cout << "| " << std::left << std::setw(42) << kernel_name << " | ";
    std::cout << std::right << std::setw(10) <<
      kernel.second.call_count << " | ";
    std::cout << std::right << std::setw(14) << std::setprecision(2) <<
      std::fixed << kernel.second.gpu_time / static_cast<float>(NSEC_IN_MSEC) << " | ";
    std::cout << std::right << std::setw(12) << std::setprecision(2) <<
      std::fixed << eu_active << " | ";
    std::cout << std::right << std::setw(11) << std::setprecision(2) <<
      std::fixed << eu_stall << " | ";
    std::cout << std::right << std::setw(10) << std::setprecision(2) <<
      std::fixed << euIdle << " | " << std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

static void ProcessQuery(size_t id) {
  PTI_ASSERT(state != nullptr);
  PTI_ASSERT(id < state->query_list.size());

  QueryData query_data = state->query_list[id];
  state->query_list[id] =  state->query_list[state->query_list.size() - 1];
  state->query_list.pop_back();

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeEventHostSynchronize(query_data.event, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeEventDestroy(query_data.event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  size_t raw_size = 0;
  status = zetMetricQueryGetData(query_data.query, &raw_size, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<uint8_t> raw_data(raw_size);
  status = zetMetricQueryGetData(query_data.query, &raw_size,
                                  raw_data.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zetMetricQueryDestroy(query_data.query);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint64_t gpu_time = 0;
  float eu_active = 0.0f, eu_stall = 0.0f;
  Calculate(state->metric_group, raw_data, gpu_time, eu_active, eu_stall);

  if (state->kernel_data_map.count(query_data.kernel_name) == 0) {
    KernelData kernel_data = { 1, eu_active, eu_stall, gpu_time };
    state->kernel_data_map[query_data.kernel_name] = kernel_data;
  } else {
    KernelData& kernel_data = state->kernel_data_map[query_data.kernel_name];
    kernel_data.eu_active = (kernel_data.call_count * kernel_data.eu_active +
      eu_active) / (kernel_data.call_count + 1);
    kernel_data.eu_stall = (kernel_data.call_count * kernel_data.eu_stall +
      eu_stall) / (kernel_data.call_count + 1);
    kernel_data.gpu_time += gpu_time;
    ++kernel_data.call_count;
  }
}

static void ProcessResults() {
  PTI_ASSERT(state != nullptr);

  state->lock.lock();
  while (state->query_list.size() > 0) {
    ProcessQuery(state->query_list.size() - 1);
  }
  state->lock.unlock();
}

static void OnExitKernelCreate(ze_kernel_create_params_t *params,
                               ze_result_t result,
                               void *global_data,
                               void **instance_data) {
  if (result == ZE_RESULT_SUCCESS) {
    PTI_ASSERT(state != nullptr);
  
    state->lock.lock();
    ze_kernel_handle_t kernel = **(params->pphKernel);
    state->kernel_name_map[kernel] = (*(params->pdesc))->pKernelName;
    state->lock.unlock();
  }
}

static void OnExitKernelDestroy(ze_kernel_destroy_params_t *params,
                                ze_result_t result,
                                void *global_data,
                                void **instance_data) {
  if (result == ZE_RESULT_SUCCESS) {
    PTI_ASSERT(state != nullptr);

    state->lock.lock();
    ze_kernel_handle_t kernel = *(params->phKernel);
    PTI_ASSERT(state->kernel_name_map.count(kernel) == 1);
    state->kernel_name_map.erase(kernel);
    state->lock.unlock();
  }
}

static void OnEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  PTI_ASSERT(state != nullptr);

  uint32_t kernel_id = state->kernel_id.fetch_add(
    1, std::memory_order_acq_rel);
  if (kernel_id < MAX_KERNEL_COUNT) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_metric_query_handle_t metric_query = nullptr;
    PTI_ASSERT(state->metric_query_pool != nullptr);
    status = zetMetricQueryCreate(state->metric_query_pool,
                                  kernel_id, &metric_query);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    
    ze_command_list_handle_t command_list = *(params->phCommandList);
    PTI_ASSERT(command_list != nullptr);

    status = zetCommandListAppendMetricQueryBegin(command_list, metric_query);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    InstanceData* data = new InstanceData;
    PTI_ASSERT(data != nullptr);
    data->kernel_id = kernel_id;
    data->metric_query = metric_query;

    *instance_data = reinterpret_cast<void*>(data);
  } else {
    *instance_data = nullptr;
  }
}

static void OnExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  InstanceData* data = reinterpret_cast<InstanceData*>(*instance_data);
  if (data != nullptr) {
    PTI_ASSERT(state != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    PTI_ASSERT(data->metric_query != nullptr);
    PTI_ASSERT(data->kernel_id < MAX_KERNEL_COUNT);

    ze_command_list_handle_t command_list = *(params->phCommandList);
    PTI_ASSERT(command_list != nullptr);

    ze_event_desc_t event_desc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, data->kernel_id,
        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    ze_event_handle_t event = nullptr;
    status = zeEventCreate(state->event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    status = zetCommandListAppendMetricQueryEnd(
        command_list, data->metric_query, event, 0, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    state->lock.lock();
    ze_kernel_handle_t kernel = *(params->phKernel);
    if (state->kernel_name_map.count(kernel) == 1) {
      QueryData query_data = { state->kernel_name_map[kernel],
                               data->metric_query, event };
      state->query_list.push_back(query_data);
    }
    state->lock.unlock();

    delete[] data;
  }
}

static void OnExitCommandQueueDestroy(
    ze_command_queue_destroy_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  ProcessResults();
}

static zet_tracer_exp_handle_t EnableTracing(
    ze_context_handle_t context) {
  PTI_ASSERT(context != nullptr);
  
  ze_result_t status = ZE_RESULT_SUCCESS;
  zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, nullptr};
  zet_tracer_exp_handle_t tracer = nullptr;
  status = zetTracerExpCreate(context, &tracer_desc, &tracer);
  if (status != ZE_RESULT_SUCCESS) {    
    return nullptr;
  }

  zet_core_callbacks_t prologue_callbacks = {};
  zet_core_callbacks_t epilogue_callbacks = {};
  epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;
  epilogue_callbacks.Kernel.pfnDestroyCb = OnExitKernelDestroy;
  prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnEnterCommandListAppendLaunchKernel;
  epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnExitCommandListAppendLaunchKernel;
  epilogue_callbacks.CommandQueue.pfnDestroyCb = OnExitCommandQueueDestroy;

  status = zetTracerExpSetPrologues(tracer, &prologue_callbacks);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zetTracerExpSetEpilogues(tracer, &epilogue_callbacks);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zetTracerExpSetEnabled(tracer, true);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return tracer;
}

static void DisableTracing(zet_tracer_exp_handle_t tracer) {
  if (tracer != nullptr) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(tracer, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zetTracerExpDestroy(tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }
}

static void EnableMetricCollection(
    ze_device_handle_t device,
    ze_context_handle_t context,
    zet_metric_group_handle_t group,
    zet_metric_query_pool_handle_t& metric_query_pool,
    ze_event_pool_handle_t& event_pool) {
  PTI_ASSERT(context != nullptr);
  PTI_ASSERT(device != nullptr && group != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  status = zetContextActivateMetricGroups(context, device, 1, &group);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zet_metric_query_pool_desc_t metric_query_pool_desc = {
      ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC, nullptr,
      ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE, MAX_KERNEL_COUNT};
  status = zetMetricQueryPoolCreate(context, device, group,
                                    &metric_query_pool_desc,
                                    &metric_query_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, MAX_KERNEL_COUNT};
  status = zeEventPoolCreate(context, &event_pool_desc,
                             0, nullptr, &event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

static void DisableMetricCollection(
    ze_device_handle_t device,
    ze_context_handle_t context,
    zet_metric_query_pool_handle_t metric_query_pool,
    ze_event_pool_handle_t event_pool) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  if (device != nullptr && context != nullptr) {
    status = zetContextActivateMetricGroups(context, device, 0, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  if (event_pool != nullptr) {
    status = zeEventPoolDestroy(event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS); 
  }

  if (metric_query_pool != nullptr) {
    status = zetMetricQueryPoolDestroy(metric_query_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);    
  }
}

void EnableProfiling() {
  PTI_ASSERT(state == nullptr);
  state = new GlobalToolState;
  PTI_ASSERT(state != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_driver_handle_t driver = nullptr;
  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU,
                                     state->device, driver);
  if (state->device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target device" << std::endl;
    return;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);
  state->context = context;
  
  state->tracer = EnableTracing(context);
  if (state->tracer == nullptr) {
    std::cout <<
      "[WARNING] Unable to create Level Zero tracer for target driver" <<
      std::endl;
    return;
  }

  state->metric_group = utils::ze::FindMetricGroup(
      state->device, "ComputeBasic",
      ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
  if (state->metric_group == nullptr) {
    std::cout << "[WARNING] Unable to find target metric group" << std::endl;
    return;
  }

  EnableMetricCollection(state->device, context, state->metric_group,
                         state->metric_query_pool, state->event_pool);
  PTI_ASSERT(state->metric_query_pool != nullptr);
  PTI_ASSERT(state->event_pool != nullptr);
}

void DisableProfiling() {
  if (state != nullptr) {
    if (state->tracer != nullptr) {
      DisableTracing(state->tracer);
      PrintResults();
      DisableMetricCollection(state->device, state->context,
                              state->metric_query_pool,
                              state->event_pool);
    }
    delete state;
  }
}