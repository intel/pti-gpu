//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "utils.h"
#include "ze_utils.h"

#include "metric_device.h"

enum CollectorState {
  COLLECTOR_STATE_IDLE = 0,
  COLLECTOR_STATE_ENABLED = 1,
  COLLECTOR_STATE_DISABLED = 2
};

enum EventType {
  EVENT_TYPE_USER = 0,
  EVENT_TYPE_TOOL = 1
};

struct ActivityEventInfo {
  std::string name;
  ze_event_pool_handle_t event_pool;
  ze_event_handle_t event;
  EventType event_type;
};

struct GlobalToolState {
  ze_driver_handle_t driver;
  ze_device_handle_t device;
  ze_context_handle_t context;
  zet_tracer_exp_handle_t tracer;
  std::mutex lock;
  std::map<ze_kernel_handle_t, std::string> kernel_name_map;
  std::vector<ActivityEventInfo> activity_event_list;
  std::mutex event_lock;
  std::map< std::string,
            std::vector< std::pair<uint64_t, uint64_t> > > kernel_time_map;
  zet_metric_group_handle_t group;
  uint32_t group_metric_count = 0;
  float timer_resolution = 0.0f;
  std::atomic<uint32_t> collector_state;
  std::thread* collector = nullptr;
  std::vector<zet_typed_value_t> metric_report_list;
  int queue_count = 0;
};

const char* kLine =
  "+---------------------------------------------------"
  "---------------------------------------------------+";
const char* kHeader =
  "| Kernel                       | Call Count | "
  "Total Time, ms | EU Active, % | EU Stall, % | EU Idle, % |";

static GlobalToolState* state = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_metric_streamer[.exe] <application> <args>" <<
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

static void OnExitKernelCreate(
    ze_kernel_create_params_t *params, ze_result_t result,
    void *global_data, void **instance_data) {
  PTI_ASSERT(state != nullptr);
  if (result == ZE_RESULT_SUCCESS) {
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

static void OnEnterEventPoolCreate(ze_event_pool_create_params_t *params,
                                   ze_result_t result,
                                   void *global_data,
                                   void **instance_data) {
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
  //PTI_ASSERT(profiling_desc->stype == ZE_STRUCTURE_TYPE_EVENT_POOL_DESC);
  profiling_desc->pNext = desc->pNext;
  profiling_desc->flags = (desc->flags | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  profiling_desc->count = desc->count;

  *(params->pdesc) = profiling_desc;
  *instance_data = profiling_desc;
}

static void OnExitEventPoolCreate(ze_event_pool_create_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
  ze_event_pool_desc_t* desc =
    static_cast<ze_event_pool_desc_t*>(*instance_data);
  if (desc != nullptr) {
    delete desc;
  }
}

static void ProcessEvent(size_t id) {
  PTI_ASSERT(state != nullptr);
  PTI_ASSERT(id >= 0 && id < state->activity_event_list.size());

  ActivityEventInfo info = state->activity_event_list[id];
  state->activity_event_list[id] =
    state->activity_event_list[state->activity_event_list.size() - 1];
  state->activity_event_list.pop_back();

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeEventQueryStatus(info.event);
  if (status == ZE_RESULT_SUCCESS) {
    ze_kernel_timestamp_result_t timestamp{};
    status = zeEventQueryKernelTimestamp(info.event, &timestamp);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    uint64_t start = timestamp.global.kernelStart;
    uint64_t end = timestamp.global.kernelEnd;

    auto& item = state->kernel_time_map[info.name];
    item.push_back(std::make_pair(start, end));
  }

  if (info.event_type == EVENT_TYPE_TOOL) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeEventDestroy(info.event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zeEventPoolDestroy(info.event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }
}

static void OnEnterEventDestroy(ze_event_destroy_params_t *params,
                                ze_result_t result,
                                void *global_data,
                                void **instance_data) {
  PTI_ASSERT(state != nullptr);
  state->event_lock.lock();
  for (size_t i = 0; i < state->activity_event_list.size(); ++i) {
    if (state->activity_event_list[i].event == *(params->phEvent)) {
      PTI_ASSERT(state->activity_event_list[i].event_type == EVENT_TYPE_USER);
      ProcessEvent(i);
      break;
    }
  }
  state->event_lock.unlock();
}

static void OnEnterEventHostReset(ze_event_host_reset_params_t *params,
                                  ze_result_t result,
                                  void *global_data,
                                  void **instance_data) {
  PTI_ASSERT(state != nullptr);
  state->event_lock.lock();
  for (size_t i = 0; i < state->activity_event_list.size(); ++i) {
    if (state->activity_event_list[i].event == *(params->phEvent)) {
      PTI_ASSERT(state->activity_event_list[i].event_type == EVENT_TYPE_USER);
      ProcessEvent(i);
      break;
    }
  }
  state->event_lock.unlock();
}

static ze_event_handle_t OnEnterActivitySubmit(
    std::string name, ze_event_handle_t event, void **instance_data) {
  ze_result_t status = ZE_RESULT_SUCCESS;
  ActivityEventInfo* info = new ActivityEventInfo;
  PTI_ASSERT(info != nullptr);
  info->name = name;

  if (event == nullptr) {
    ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
      ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1};
    ze_event_pool_handle_t event_pool = nullptr;
    status = zeEventPoolCreate(state->context, &event_pool_desc,
                               0, nullptr, &event_pool);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    ze_event_desc_t event_desc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
    zeEventCreate(event_pool, &event_desc, &event);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    info->event_pool = event_pool;
    info->event_type = EVENT_TYPE_TOOL;
  } else {
    info->event_pool = nullptr;
    info->event_type = EVENT_TYPE_USER;
  }

  info->event = event;
  *instance_data = info;
  return event;
}

static void OnEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  PTI_ASSERT(state != nullptr);
  
  if (*(params->phKernel) ==  nullptr) {
    return;
  }

  state->lock.lock();
  PTI_ASSERT(state->kernel_name_map.count(*(params->phKernel)) == 1);
  std::string kernel_name = state->kernel_name_map[*(params->phKernel)];
  state->lock.unlock();

  *(params->phSignalEvent) = OnEnterActivitySubmit(
      kernel_name, *(params->phSignalEvent), instance_data);
}

static void OnExitActivitySubmit(void **instance_data, ze_result_t result) {
  PTI_ASSERT(state != nullptr);

  ActivityEventInfo* info = static_cast<ActivityEventInfo*>(*instance_data);
  if (info == nullptr) {
    return;
  }

  if (result != ZE_RESULT_SUCCESS && info != nullptr) {
    if (info->event_type == EVENT_TYPE_TOOL) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zeEventDestroy(info->event);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      status = zeEventPoolDestroy(info->event_pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  } else {
    state->event_lock.lock();
    state->activity_event_list.push_back(*info);
    state->event_lock.unlock();
  }

  delete info;
}

static void OnExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  PTI_ASSERT(*(params->phSignalEvent) != nullptr);
  OnExitActivitySubmit(instance_data, result);
}

static void ProcessResults() {
  PTI_ASSERT(state != nullptr);

  state->event_lock.lock();
  while (state->activity_event_list.size() > 0) {
    ProcessEvent(state->activity_event_list.size() - 1);
  }
  state->event_lock.unlock();
}

static void Collect(
    ze_context_handle_t context, ze_device_handle_t device,
    zet_metric_group_handle_t group) {
  PTI_ASSERT(context != nullptr);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(group != nullptr);
  PTI_ASSERT(state != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zetContextActivateMetricGroups(context, device, 1, &group);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, 0, 1};
  ze_event_pool_handle_t event_pool = nullptr;
  status = zeEventPoolCreate(context, &event_pool_desc,
                             0, nullptr, &event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_desc_t event_desc = {
      ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
      ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
  ze_event_handle_t event = nullptr;
  status = zeEventCreate(event_pool, &event_desc, &event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zet_metric_streamer_desc_t metric_streamer_desc = {
      ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC,
      nullptr,
      32768, /* reports to collect before notify */
      100000 /* sampling period in nanoseconds */};
  zet_metric_streamer_handle_t metric_streamer = nullptr;
  status = zetMetricStreamerOpen(context, device, group, &metric_streamer_desc,
                                 event, &metric_streamer);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<uint8_t> storage;
  state->collector_state.store(COLLECTOR_STATE_ENABLED,
                               std::memory_order_release);
  while (state->collector_state.load(std::memory_order_acquire) !=
         COLLECTOR_STATE_DISABLED) {
    status = zeEventHostSynchronize(
        event, 50000000 /* wait delay in nanoseconds */);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);

    size_t data_size = 0;
    status = zetMetricStreamerReadData(metric_streamer, UINT32_MAX,
                                       &data_size, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (data_size > 0) {
      size_t storage_size = storage.size();
      storage.resize(storage_size + data_size);
      status = zetMetricStreamerReadData(
          metric_streamer, UINT32_MAX, &data_size,
          storage.data() + storage_size);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      storage.resize(storage_size + data_size);
    }
  }

  status = zetMetricStreamerClose(metric_streamer);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeEventDestroy(event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeEventPoolDestroy(event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zetContextActivateMetricGroups(context, device, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (storage.size() > 0) {
    size_t size =  state->metric_report_list.size();
    uint32_t value_count = 0;
    status = zetMetricGroupCalculateMetricValues(
        group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        storage.size(), storage.data(), &value_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    state->metric_report_list.resize(size + value_count);
    status = zetMetricGroupCalculateMetricValues(
        group, ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        storage.size(), storage.data(), &value_count,
        state->metric_report_list.data() + size);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    state->metric_report_list.resize(size + value_count);
  }
}

static void EnableMetricsCollection() {
  PTI_ASSERT(state != nullptr);

  PTI_ASSERT(state->collector == nullptr);
  state->collector_state.store(COLLECTOR_STATE_IDLE,
                               std::memory_order_release);
  state->collector = new std::thread(Collect, state->context,
                                     state->device, state->group);
  PTI_ASSERT(state->collector != nullptr);

  while (state->collector_state.load(std::memory_order_acquire) !=
         COLLECTOR_STATE_ENABLED) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

static void DisableMetricsCollection() {
  PTI_ASSERT(state != nullptr);
  if (state->collector != nullptr) {
    state->collector_state.store(COLLECTOR_STATE_DISABLED,
                                 std::memory_order_release);
    state->collector->join();
    delete state->collector;
    state->collector = nullptr;
  }
}

static void OnExitCommandQueueCreate(
    ze_command_queue_create_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  if (result == ZE_RESULT_SUCCESS) {
    PTI_ASSERT(state != nullptr);
    state->lock.lock();
    if (state->queue_count == 0) {
      EnableMetricsCollection();
    }
    ++(state->queue_count);
    state->lock.unlock();
  }
}

static void OnExitCommandQueueDestroy(
    ze_command_queue_destroy_params_t* params,
    ze_result_t result, void* global_data, void** instance_data) {
  ProcessResults();
  if (result == ZE_RESULT_SUCCESS) {
    PTI_ASSERT(state != nullptr);
    state->lock.lock();
    PTI_ASSERT(state->queue_count > 0);
    --(state->queue_count);
    if (state->queue_count == 0) {
      DisableMetricsCollection();
    }
    state->lock.unlock();
  }
}

static zet_tracer_exp_handle_t EnableTracing(ze_context_handle_t context) {
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
  prologue_callbacks.Event.pfnDestroyCb = OnEnterEventDestroy;
  prologue_callbacks.Event.pfnHostResetCb = OnEnterEventHostReset;
  prologue_callbacks.EventPool.pfnCreateCb = OnEnterEventPoolCreate;
  prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnEnterCommandListAppendLaunchKernel;

  zet_core_callbacks_t epilogue_callbacks = {};
  epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;
  epilogue_callbacks.Kernel.pfnDestroyCb = OnExitKernelDestroy;
  epilogue_callbacks.EventPool.pfnCreateCb = OnExitEventPoolCreate;
  epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnExitCommandListAppendLaunchKernel;
  epilogue_callbacks.CommandQueue.pfnCreateCb = OnExitCommandQueueCreate;
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

static void PrintResults() {
  PTI_ASSERT(state != nullptr);

  if (state->kernel_time_map.size() == 0) {
    return;
  }

  PTI_ASSERT(
      (state->metric_report_list.size() % state->group_metric_count) == 0);

  int eu_active_id = utils::ze::GetMetricId(state->group, "EuActive");
  PTI_ASSERT(eu_active_id != -1);
  int eu_stall_id = utils::ze::GetMetricId(state->group, "EuStall");
  PTI_ASSERT(eu_stall_id != -1);
  int timestamp_id = utils::ze::GetMetricId(state->group, "QueryBeginTime");
  PTI_ASSERT(timestamp_id != -1);

  std::cout << kLine << std::endl;
  std::cout << kHeader << std::endl;
  std::cout << kLine << std::endl;

  for (auto kernel : state->kernel_time_map) {
    float active_total = 0.0f, stall_total = 0.0f;
    int sample_count = 0;

    uint64_t time_total = 0;
    int call_count = 0;

    std::cout << "| " << std::left << std::setw(28) << kernel.first << " | ";

    std::sort(kernel.second.begin(), kernel.second.end(),
              [](const std::pair<uint64_t, uint64_t>& l,
                 const std::pair<uint64_t, uint64_t>& r) {
                return l.first < r.first;
              });

    for (auto time : kernel.second) {
      const zet_typed_value_t* report = state->metric_report_list.data();
      uint64_t start = time.first * state->timer_resolution;
      uint64_t end = time.second * state->timer_resolution;

      while (report < state->metric_report_list.data() +
                      state->metric_report_list.size()) {
        PTI_ASSERT(report[timestamp_id].type == ZET_VALUE_TYPE_UINT64);
        uint64_t report_time = report[timestamp_id].value.ui64;
        if (report_time >= start && report_time <= end) {
          PTI_ASSERT(report[eu_active_id].type == ZET_VALUE_TYPE_FLOAT32);
          active_total += report[eu_active_id].value.fp32;
          PTI_ASSERT(report[eu_stall_id].type == ZET_VALUE_TYPE_FLOAT32);
          stall_total += report[eu_stall_id].value.fp32;
          ++sample_count;
        }

        if (report_time > end) {
          break;
        }

        report += state->group_metric_count;
      }

      time_total += time.second - time.first;
      ++call_count;
    }

    time_total *= state->timer_resolution;

    float active = 0.0f, stall = 0.0f, idle = 100.0f;
    if (sample_count > 0) {
      active = active_total / sample_count;
      stall = stall_total / sample_count;
      idle = idle - active - stall;
      if (idle < 0.0f) idle = 0.0f;
    }

    std::cout << std::right << std::setw(10) << call_count << " | ";
    std::cout << std::right << std::setw(14) << std::setprecision(2) <<
      std::fixed << time_total / static_cast<float>(NSEC_IN_MSEC) << " | ";
    std::cout << std::right << std::setw(12) << std::setprecision(2) <<
      std::fixed << active << " | ";
    std::cout << std::right << std::setw(11) << std::setprecision(2) <<
      std::fixed << stall << " | ";
    std::cout << std::right << std::setw(10) << std::setprecision(2) <<
      std::fixed << idle << " | " << std::endl;
  }

  std::cout << kLine << std::endl;
  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

uint32_t GetGpuTimestampFrequency() {
  MetricDevice* device = MetricDevice::Create();
  if (device == nullptr) {
    std::cout << "[Warning] Unable to find MD library" << std::endl;
    return 0;
  }

  uint32_t timer_freq = 0;
  PTI_ASSERT((*device)->GetParams()->GlobalSymbolsCount > 0);

  for (uint32_t i = 0; i < (*device)->GetParams()->GlobalSymbolsCount; ++i) {
    md::TGlobalSymbol_1_0* symbol = (*device)->GetGlobalSymbol(i);
    if (std::string(symbol->SymbolName) == "GpuTimestampFrequency") {
      PTI_ASSERT(symbol->SymbolTypedValue.ValueType == md::VALUE_TYPE_UINT32);
      timer_freq = symbol->SymbolTypedValue.ValueUInt32;
      break;
    }
  }

  delete device;
  return timer_freq;
}

void EnableProfiling() {
  PTI_ASSERT(state == nullptr);
  state = new GlobalToolState;
  PTI_ASSERT(state != nullptr);

  uint32_t timer_freq = GetGpuTimestampFrequency();
  if (timer_freq == 0) {
    return;
  }
  state->timer_resolution = static_cast<float>(NSEC_IN_SEC) / timer_freq;

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  utils::ze::GetIntelDeviceAndDriver(
      ZE_DEVICE_TYPE_GPU, state->device, state->driver);
  if (state->device == nullptr || state->driver == nullptr) {
    std::cout << "[WARNING] Unable to find target device" << std::endl;
    return;
  }

  state->context = utils::ze::GetContext(state->driver);
  PTI_ASSERT(state->context);

  state->tracer = EnableTracing(state->context);
  if (state->tracer == nullptr) {
    std::cout <<
      "[WARNING] Unable to create Level Zero tracer for target driver" <<
      std::endl;
    return;
  }

  state->group = utils::ze::FindMetricGroup(
      state->device, "ComputeBasic",
      ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
  if (state->group == nullptr) {
    std::cout << "[WARNING] Unable to find target metric group" << std::endl;
    return;
  }

  zet_metric_group_properties_t group_props{};
  group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
  status = zetMetricGroupGetProperties(state->group, &group_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  state->group_metric_count = group_props.metricCount;
}

void DisableProfiling() {
  if (state != nullptr) {
    if (state->tracer != nullptr) {
      DisableTracing(state->tracer);
      PrintResults();
    }
    delete state;
  }
}