//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>
#include <math.h>
#include <string.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sycl/kernel_bundle.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti.h"
#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"
#include "utils/test_helpers.h"
#include "ze_utils.h"

#define ALIGN 64
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

namespace {

constexpr size_t kPtiDeviceId = 0;  // run on first device

uint64_t eid_ = 11;
const uint64_t kCommandListAppendLaunchKernelId = 55;  // apiid of zeCommandListAppendLaunchKernel
size_t requested_buffer_calls = 0;
size_t rejected_buffer_calls = 0;  // Buffer requests that are called and rejected by the API
size_t completed_buffer_calls = 0;
size_t completed_buffer_used_bytes = 0;
bool memory_view_record_created = false;
uint64_t kernel_launch_id = 0;
bool kernel_view_record_created = false;
uint64_t memory_view_record_count = 0;
uint64_t kernel_view_record_count = 0;
bool buffer_size_atleast_largest_record = false;
bool ze_initialization_succeeded = false;
bool capture_records = false;
bool special_record_seen = false;
bool synchronization_record_seen = false;
bool synchronization_record_barrier_exec_seen = false;
bool synchronization_record_barrier_mem_seen = false;
bool synchronization_record_fence_exec_seen = false;
bool synchronization_record_event_seen = false;
bool synchronization_record_clist_seen = false;
bool synchronization_record_cqueue_seen = false;
uint32_t synchronization_barrier_event_number = 0;
uint32_t synch_clist_record_api_group = 0;
void* synch_clist_record_context = nullptr;
void* synch_cqueue_record_queue = nullptr;
void* synch_event_record_context = nullptr;
void* synch_event_record_event = nullptr;
void* synch_fence_record_context = nullptr;
void* synch_fence_record_queue = nullptr;
void* synch_fence_record_event = nullptr;
void* synch_barrier_record_context = nullptr;
void* synch_barrier_record_event = nullptr;
void* synch_barrier_record_queue = nullptr;
void* synch_input_event = nullptr;
void* synch_input_context = nullptr;
void* synch_input_queue = nullptr;
const char* barrier_exec_api_name = nullptr;
const char* barrier_mem_ranges_api_name = nullptr;
const char* fence_api_name = nullptr;
uint32_t synch_input_event_number = 0;
uint32_t synchronization_record_number = 0;
bool sycl_runtime_launch_seen = false;
bool zecall_record_seen = false;
bool external_corrid_special_record_seen = false;
uint32_t num_special_records = 0;
uint32_t num_sycl_runtime_launch_records = 0;
uint64_t corrid_in_special_record = 0;
uint64_t external_corrid_in_ext_rec = 0;
std::vector<pti_view_record_memory_copy> copy_records;
std::vector<pti_view_record_kernel> kernel_records;
ze_device_uuid_t device_uuid = {};
ze_context_handle_t context_test = nullptr;
ze_command_queue_handle_t queue_test_kernel = nullptr;
ze_command_queue_handle_t queue_test_mem_copy = nullptr;

// TODO - make the enable type param more generic (maybe a bitmap of somesort) so that we can enable
// a mishmash of types
void StartTracing(bool include_sycl_runtime = false, bool include_zecalls = false,
                  bool include_gpu_kernels = true, bool include_synch = false) {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
  if (include_gpu_kernels) {
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
  if (include_sycl_runtime) ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  if (include_zecalls) ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  if (include_synch)
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_SYNCHRONIZATION), pti_result::PTI_SUCCESS);
}

void StopTracing(bool include_sycl_runtime = false, bool include_zecalls = false,
                 bool include_gpu_kernels = true, bool include_synch = false) {
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION), pti_result::PTI_SUCCESS);
  if (include_gpu_kernels) {
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  }
  if (include_synch)
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_SYNCHRONIZATION), pti_result::PTI_SUCCESS);
  if (include_sycl_runtime)
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
  if (include_zecalls) ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
}

float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }
  return eps / a.size();
}

int GetGroupOrdinals(ze_device_handle_t device, uint32_t& computeOrdinal, uint32_t& copyOrdinal) {
  // Discover all command queue groups
  uint32_t cmdqueue_group_count = 0;
  ze_result_t status =
      zeDeviceGetCommandQueueGroupProperties(device, &cmdqueue_group_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_command_queue_group_properties_t> cmdqueue_group_props(cmdqueue_group_count);
  for (auto& prop : cmdqueue_group_props) {
    prop.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    prop.pNext = nullptr;
  }
  status = zeDeviceGetCommandQueueGroupProperties(device, &cmdqueue_group_count,
                                                  cmdqueue_group_props.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // Find command queues that support compute and copy
  computeOrdinal = cmdqueue_group_count;
  copyOrdinal = cmdqueue_group_count;
  for (uint32_t i = 0; i < cmdqueue_group_count; ++i) {
    if (cmdqueue_group_props[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
      computeOrdinal = i;
    }
    if (cmdqueue_group_props[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) {
      copyOrdinal = i;
    }
  }
  if (computeOrdinal == cmdqueue_group_count || copyOrdinal == cmdqueue_group_count) {
    std::cout << "No compute or copy command queue group found" << std::endl;
    return 1;
  }
  return 0;
}

float RunWithPollingAndCheck(ze_kernel_handle_t kernel, ze_device_handle_t device,
                             ze_context_handle_t context, const std::vector<float>& a,
                             const std::vector<float>& b, std::vector<float>& c, unsigned size,
                             float expected_result) {
  PTI_ASSERT(kernel != nullptr);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(context != nullptr);

  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t group_size[3] = {0};
  status = zeKernelSuggestGroupSize(kernel, size, size, 1, &(group_size[0]), &(group_size[1]),
                                    &(group_size[2]));
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if ((size % group_size[0]) != 0 || (size % group_size[1]) != 0) {
    std::cout << "Non-uniform workgroups are not supported" << std::endl;
    return 0.0f;
  }

  std::cout << "Sizes and return values: " << size << " : " << group_size[0] << " : "
            << group_size[1] << " : " << group_size[2] << "\n";
  uint32_t compute_queue_ordinal = 0;
  uint32_t copy_queue_ordinal = 0;
  if (0 != GetGroupOrdinals(device, compute_queue_ordinal, copy_queue_ordinal)) {
    // no compute or copy queue group found
    return 0.0f;
  }
  std::cout << "Compute Queue Ordinal: " << compute_queue_ordinal << std::endl;
  std::cout << "Copy Queue Ordinal: " << copy_queue_ordinal << std::endl;

  ze_command_queue_desc_t cmd_queue_desc_copy = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                                 nullptr,
                                                 copy_queue_ordinal,
                                                 0,
                                                 ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                                 ZE_COMMAND_QUEUE_MODE_DEFAULT,
                                                 ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
  ze_command_queue_desc_t cmd_queue_desc_kernel = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                                   nullptr,
                                                   compute_queue_ordinal,
                                                   0,
                                                   ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                                   ZE_COMMAND_QUEUE_MODE_DEFAULT,
                                                   ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

  ze_command_list_handle_t cmd_list_copy = nullptr;
  ze_command_list_handle_t cmd_list_kernel = nullptr;
  status = zeCommandListCreateImmediate(context, device, &cmd_queue_desc_copy, &cmd_list_copy);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  queue_test_mem_copy = reinterpret_cast<ze_command_queue_handle_t>(cmd_list_copy);

  status = zeCommandListCreateImmediate(context, device, &cmd_queue_desc_kernel, &cmd_list_kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  synch_input_context = context;

  queue_test_kernel = reinterpret_cast<ze_command_queue_handle_t>(cmd_list_kernel);

  void* dev_a = nullptr;
  ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_b = nullptr;
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_c = nullptr;
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetGroupSize(kernel, group_size[0], group_size[1], group_size[2]);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetArgumentValue(kernel, 0, sizeof(dev_a), &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 1, sizeof(dev_a), &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 2, sizeof(dev_a), &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 3, sizeof(size), &size);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // No Timestamp information in the Pool
  ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                          ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 3};
  ze_event_pool_handle_t event_pool = nullptr;
  status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_handle_t event_mem_copy1 = nullptr;
  ze_event_handle_t event_mem_copy2 = nullptr;
  ze_event_handle_t event_kernel = nullptr;
  ze_event_desc_t event_desc_copy1 = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, 0, 0};
  ze_event_desc_t event_desc_copy2 = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 1, 0, 0};
  ze_event_desc_t event_desc_kernel = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 2, 0, 0};
  zeEventCreate(event_pool, &event_desc_copy1, &event_mem_copy1);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zeEventCreate(event_pool, &event_desc_copy2, &event_mem_copy2);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  zeEventCreate(event_pool, &event_desc_kernel, &event_kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "Event Mem Copy 1: " << event_mem_copy1 << std::endl;
  std::cout << "Event Mem Copy 2: " << event_mem_copy2 << std::endl;
  std::cout << "Event Kernel: " << event_kernel << std::endl;

  status = zeCommandListAppendMemoryCopy(cmd_list_copy, dev_a, a.data(),
                                         size * size * sizeof(float), event_mem_copy1, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeCommandListAppendMemoryCopy(cmd_list_copy, dev_b, b.data(),
                                         size * size * sizeof(float), event_mem_copy2, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // Trying to simulate oneCCL behavior we saw in traces: event is polled until it is ready
  // No synchronization is used, just polling
  // And destroyed as soon as it is ready
  // This fragment could be changed after we discover more on oneCCL behavior
  ze_result_t status1 = ZE_RESULT_NOT_READY;
  ze_result_t status2 = ZE_RESULT_NOT_READY;
  while (status1 != ZE_RESULT_SUCCESS && status2 != ZE_RESULT_SUCCESS) {
    status1 = zeEventQueryStatus(event_mem_copy1);
    status2 = zeEventQueryStatus(event_mem_copy2);
  }

  ze_group_count_t dim = {size / group_size[0], size / group_size[1], 1};
  status = zeCommandListAppendLaunchKernel(cmd_list_kernel, kernel, &dim, event_kernel, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status1 = zeEventDestroy(event_mem_copy1);
  PTI_ASSERT(status1 == ZE_RESULT_SUCCESS);
  status2 = zeEventDestroy(event_mem_copy2);
  PTI_ASSERT(status2 == ZE_RESULT_SUCCESS);

  status = zeEventHostSynchronize(event_kernel, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status2 = zeEventDestroy(event_kernel);
  PTI_ASSERT(status2 == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendMemoryCopy(cmd_list_copy, c.data(), dev_c,
                                         size * size * sizeof(float), nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListHostSynchronize(cmd_list_copy, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListDestroy(cmd_list_copy);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeCommandListDestroy(cmd_list_kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeMemFree(context, dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeMemFree(context, dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeMemFree(context, dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeEventPoolDestroy(event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return Check(c, expected_result);
}

float RunAndCheck(ze_kernel_handle_t kernel, ze_device_handle_t device, ze_context_handle_t context,
                  const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c,
                  unsigned size, float expected_result) {
  PTI_ASSERT(kernel != nullptr);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(context != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  // TODO -- uncomment this test call after confirming support in L0 driver.
  //      -- current status (loader 1.20.0) returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
  // status = zeContextSystemBarrier(context, device);
  // PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  uint32_t group_size[3] = {0};
  status = zeKernelSuggestGroupSize(kernel, size, size, 1, &(group_size[0]), &(group_size[1]),
                                    &(group_size[2]));
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if ((size % group_size[0]) != 0 || (size % group_size[1]) != 0) {
    std::cout << "Non-uniform workgroups are not supported" << std::endl;
    return 0.0f;
  }

  void* dev_a = nullptr;
  ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_b = nullptr;
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_c = nullptr;
  status =
      zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float), ALIGN, device, &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetGroupSize(kernel, group_size[0], group_size[1], group_size[2]);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetArgumentValue(kernel, 0, sizeof(dev_a), &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 1, sizeof(dev_a), &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 2, sizeof(dev_a), &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 3, sizeof(size), &size);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_command_list_desc_t cmd_list_desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};
  ze_command_list_handle_t cmd_list = nullptr;
  status = zeCommandListCreate(context, device, &cmd_list_desc, &cmd_list);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendMemoryCopy(cmd_list, dev_a, a.data(), size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // std::vector<const void *> ranges{dev_a, a};
  std::vector<const void*> ranges{dev_a};
  std::vector<size_t> range_sizes{a.size()};
  // memory_ranges_barrier for memory coherency after copy to device memory
  status = zeCommandListAppendMemoryRangesBarrier(cmd_list, ranges.size(), range_sizes.data(),
                                                  ranges.data(), nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendMemoryCopy(cmd_list, dev_b, b.data(), size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
      ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
  ze_event_pool_handle_t event_pool = nullptr;
  status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, ZE_EVENT_SCOPE_FLAG_HOST,
                                ZE_EVENT_SCOPE_FLAG_HOST};
  ze_event_handle_t event = nullptr;
  status = zeEventCreate(event_pool, &event_desc, &event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_group_count_t dim = {size / group_size[0], size / group_size[1], 1};
  status = zeCommandListAppendLaunchKernel(cmd_list, kernel, &dim, event, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 1, &event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  synch_input_event_number = 1;

  status = zeCommandListAppendMemoryCopy(cmd_list, c.data(), dev_c, size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListClose(cmd_list);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
  ze_command_queue_handle_t cmd_queue = nullptr;
  status = zeCommandQueueCreate(context, device, &cmd_queue_desc, &cmd_queue);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && cmd_queue != nullptr);
  synch_input_queue = cmd_queue;

  ze_fence_desc_t fence_desc = {ZE_STRUCTURE_TYPE_FENCE_DESC, nullptr, 0};
  ze_fence_handle_t fence_handle;
  status = zeFenceCreate(cmd_queue, &fence_desc, &fence_handle);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  queue_test_mem_copy = static_cast<ze_command_queue_handle_t>(cmd_queue);
  queue_test_kernel = static_cast<ze_command_queue_handle_t>(cmd_queue);

  status = zeCommandQueueExecuteCommandLists(cmd_queue, 1, &cmd_list, fence_handle);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeFenceHostSynchronize(fence_handle, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeEventHostSynchronize(event, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  synch_input_event = event;
  synch_input_context = context;
  status = zeCommandQueueSynchronize(cmd_queue, UINT32_MAX);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandQueueDestroy(cmd_queue);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListDestroy(cmd_list);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeMemFree(context, dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeMemFree(context, dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeMemFree(context, dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_device_properties_t props{};
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_kernel_timestamp_result_t timestamp{};
  status = zeEventQueryKernelTimestamp(event, &timestamp);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeEventDestroy(event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeEventPoolDestroy(event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  double time = static_cast<double>(timestamp.global.kernelEnd - timestamp.global.kernelStart) /
                props.timerResolution;
  std::cout << "Matrix multiplication time: " << time << " sec" << std::endl;

  return Check(c, expected_result);
}

void ComputeUsingSycl(std::vector<float>& a, unsigned repeat_count) {
  sycl::queue q;
  for (auto platform : sycl::platform::get_platforms()) {
    std::vector<sycl::device> gpu_devices = platform.get_devices();
    if (platform.get_backend() == sycl::backend::ext_oneapi_level_zero) {
      q = sycl::queue(gpu_devices[0]);
    }
  }

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    for (unsigned i = 0; i < repeat_count; ++i) {
      sycl::event event = q.submit([&](sycl::handler& cgh) {
        auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
        cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
      });
      q.wait_and_throw();
    }
  } catch (const sycl::exception& e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

void Compute(ze_device_handle_t device, ze_driver_handle_t driver, const std::vector<float>& a,
             const std::vector<float>& b, std::vector<float>& c, unsigned size,
             unsigned repeat_count, float expected_result, bool with_polling = false) {
  PTI_ASSERT(device != nullptr && driver != nullptr);
  PTI_ASSERT(size > 0 && repeat_count > 0);

  std::string module_name = "gemm.spv";
  std::cout << utils::GetExecutablePath() + module_name << std::endl;
  std::vector<uint8_t> binary = utils::LoadBinaryFile(utils::GetExecutablePath() + module_name);
  if (binary.size() == 0) {
    std::cout << "Unable to find module " << module_name << std::endl;
    return;
  }

  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  context_test = context;
  ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                  nullptr,
                                  ZE_MODULE_FORMAT_IL_SPIRV,
                                  static_cast<uint32_t>(binary.size()),
                                  binary.data(),
                                  nullptr,
                                  nullptr};
  ze_module_handle_t module = nullptr;
  status = zeModuleCreate(context, device, &module_desc, &module, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && module != nullptr);

  ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "GEMM"};
  ze_kernel_handle_t kernel = nullptr;
  status = zeKernelCreate(module, &kernel_desc, &kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && kernel != nullptr);

  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps =
        (with_polling)
            ? RunWithPollingAndCheck(kernel, device, context, a, b, c, size, expected_result)
            : RunAndCheck(kernel, device, context, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") << "CORRECT with accuracy: " << eps
              << std::endl;
  }

  status = zeKernelDestroy(kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeModuleDestroy(module);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeContextDestroy(context);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

}  // namespace

class MainZeFixtureTest : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {  // Called right after constructor before each test
    buffer_cb_registered = true;
    requested_buffer_calls = 0;
    rejected_buffer_calls = 0;
    completed_buffer_calls = 0;
    completed_buffer_used_bytes = 0;
    memory_view_record_created = false;
    kernel_launch_id = 0;
    kernel_view_record_created = false;
    memory_view_record_count = 0;
    kernel_view_record_count = 0;
    capture_records = false;
    special_record_seen = false;
    synchronization_record_seen = false;
    synchronization_record_barrier_exec_seen = false;
    synchronization_record_barrier_mem_seen = false;
    synchronization_record_fence_exec_seen = false;
    synchronization_record_event_seen = false;
    synchronization_record_clist_seen = false;
    synchronization_record_cqueue_seen = false;
    synchronization_barrier_event_number = 0;
    synchronization_record_number = 0;
    synch_clist_record_api_group = 0;
    synch_clist_record_context = nullptr;
    synch_cqueue_record_queue = nullptr;
    synch_event_record_context = nullptr;
    synch_event_record_event = nullptr;
    synch_fence_record_context = nullptr;
    synch_fence_record_queue = nullptr;
    synch_fence_record_event = nullptr;
    synch_barrier_record_context = nullptr;
    synch_barrier_record_event = nullptr;
    synch_barrier_record_queue = nullptr;
    synch_input_context = nullptr;
    synch_input_event = nullptr;
    synch_input_event_number = 0;
    sycl_runtime_launch_seen = false;
    barrier_exec_api_name = nullptr;
    barrier_mem_ranges_api_name = nullptr;
    fence_api_name = nullptr;
    zecall_record_seen = false;
    external_corrid_special_record_seen = false;
    num_special_records = 0;
    num_sycl_runtime_launch_records = 0;
    corrid_in_special_record = 0;
    external_corrid_in_ext_rec = 0;
    copy_records.clear();
    kernel_records.clear();
    device_uuid = {};
    context_test = nullptr;
    queue_test_kernel = nullptr;
    queue_test_mem_copy = nullptr;
  }

  void TearDown() override {
    // Called right before destructor after each test
  }

  // Class members commonly used by all tests in the test suite for MainFixture
  unsigned size = 1024;

  unsigned repeat_count = 1;

  bool buffer_cb_registered = false;

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      ::operator delete(buf);
      return;
    }

    completed_buffer_calls += 1;
    completed_buffer_used_bytes = used_bytes;
    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        std::cerr << "Found Error Parsing Records from PTI" << '\n';
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          std::cout << "Found Invalid Record" << '\n';
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          if (capture_records) {
            std::cout << "--- Record Memory Copy" << '\n';
            pti_view_record_memory_copy* rec = reinterpret_cast<pti_view_record_memory_copy*>(ptr);
            // std::cout << "---------------------------------------------------" << '\n';
            // samples_utils::DumpRecord(rec);
            uint64_t duration = rec->_end_timestamp - rec->_start_timestamp;
            std::cout << "  Start: " << rec->_start_timestamp << '\n';
            std::cout << "  End: " << rec->_end_timestamp << '\n';
            std::cout << "  Duration: " << duration << '\n';
            std::cout << "  Memcpy Type: " << rec->_memcpy_type << '\n';
            ASSERT_EQ(std::memcmp(&rec->_device_uuid, &device_uuid, sizeof(ze_device_uuid_t)), 0);
            ASSERT_EQ(context_test, reinterpret_cast<ze_context_handle_t>(rec->_context_handle));
            ASSERT_EQ(queue_test_mem_copy,
                      reinterpret_cast<ze_command_queue_handle_t>(rec->_queue_handle));
            copy_records.push_back(*rec);
          }
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          memory_view_record_created = true;
          memory_view_record_count += 1;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_SYNCHRONIZATION: {
          pti_view_record_synchronization* rec =
              reinterpret_cast<pti_view_record_synchronization*>(ptr);
          // std::cout << "---------------------------------------------------" << '\n';
          // samples_utils::DumpRecord(rec);
          synchronization_record_seen = true;
          switch (rec->_synch_type) {
            case pti_view_synchronization_type::
                PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_EXECUTION: {
              synchronization_record_barrier_exec_seen = true;
              synch_barrier_record_context = rec->_context_handle;
              synch_barrier_record_queue = rec->_queue_handle;
              synch_barrier_record_event = rec->_event_handle;
              synchronization_barrier_event_number = rec->_number_wait_events;
              PTI_THROW(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, rec->_api_id,
                                            &barrier_exec_api_name));
            }; break;
            case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_GPU_BARRIER_MEMORY: {
              synch_barrier_record_context = rec->_context_handle;
              synch_barrier_record_event = rec->_event_handle;
              synchronization_record_barrier_mem_seen = true;
              PTI_THROW(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, rec->_api_id,
                                            &barrier_mem_ranges_api_name));
            }; break;
            case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_FENCE: {
              synchronization_record_fence_exec_seen = true;
              synch_fence_record_context = rec->_context_handle;
              synch_fence_record_event = rec->_event_handle;
              synch_fence_record_queue = rec->_queue_handle;
              PTI_THROW(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, rec->_api_id,
                                            &fence_api_name));
            }; break;
            case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_EVENT: {
              synchronization_record_event_seen = true;
              synch_event_record_context = rec->_context_handle;
              synch_event_record_event = rec->_event_handle;
            }; break;
            case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_LIST: {
              synchronization_record_clist_seen = true;
              synch_clist_record_api_group = rec->_api_group;
              if (!synch_clist_record_context)
                synch_clist_record_context = rec->_context_handle;  // capture the 1st one for test
            }; break;
            case pti_view_synchronization_type::PTI_VIEW_SYNCHRONIZATION_TYPE_HOST_COMMAND_QUEUE: {
              synchronization_record_cqueue_seen = true;
              synch_cqueue_record_queue = rec->_queue_handle;
              // synch_cqueue_record_context = rec->_context_handle;
            }; break;
            default:
              break;
          }
          synchronization_record_number++;
          break;
        }
        case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
          pti_view_record_external_correlation* aExtRec =
              reinterpret_cast<pti_view_record_external_correlation*>(ptr);
          external_corrid_special_record_seen = true;
          if (!special_record_seen) external_corrid_in_ext_rec = aExtRec->_correlation_id;
          break;
        }
        case pti_view_kind::PTI_VIEW_RUNTIME_API: {
          pti_view_record_api* rec = reinterpret_cast<pti_view_record_api*>(ptr);
          const char* api_name = nullptr;
          if (rec->_api_group == pti_api_group_id::PTI_API_GROUP_HYBRID_SYCL_LEVELZERO) {
            pti_result status = ptiViewGetApiIdName(
                pti_api_group_id::PTI_API_GROUP_HYBRID_SYCL_LEVELZERO, rec->_api_id, &api_name);
            PTI_ASSERT(status == PTI_SUCCESS);
            std::string function_name(api_name);
            std::cout << "--- Record Special Sycl: " << rec->_correlation_id << ": "
                      << function_name << '\n';
            if (strcmp(api_name, "zeCommandListAppendLaunchKernel") == 0) {
              special_record_seen = true;
              kernel_launch_id = rec->_api_id;
              num_special_records++;
              corrid_in_special_record = rec->_correlation_id;
            }
          } else {
            pti_result status =
                ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, rec->_api_id, &api_name);
            PTI_ASSERT(status == PTI_SUCCESS);
            std::string function_name(api_name);
            if ((function_name.find("EnqueueKernelLaunch") != std::string::npos)) {
              sycl_runtime_launch_seen = true;
              num_sycl_runtime_launch_records++;
            }
          }
          break;
        }
        case pti_view_kind::PTI_VIEW_DRIVER_API: {
          [[maybe_unused]] pti_view_record_api* rec = reinterpret_cast<pti_view_record_api*>(ptr);
          zecall_record_seen = true;
          // std::cout << "---------------------------------------------------" << '\n';
          // samples_utils::DumpRecord(rec);
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          kernel_view_record_created = true;
          kernel_view_record_count += 1;
          pti_view_record_kernel* rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
          // std::cout << "---------------------------------------------------" << '\n';
          // samples_utils::DumpRecord(rec);
          if (capture_records) {
            std::cout << "--- Record Kernel: " << rec->_name << '\n';
            std::cout << "  Cid: " << rec->_correlation_id << '\n';
            uint64_t duration = rec->_end_timestamp - rec->_start_timestamp;
            std::cout << "  Start: " << rec->_start_timestamp << '\n';
            std::cout << "  End: " << rec->_end_timestamp << '\n';
            std::cout << "  Duration: " << duration << '\n';
            ASSERT_EQ(std::memcmp(&rec->_device_uuid, &device_uuid, sizeof(ze_device_uuid_t)), 0);
            ASSERT_EQ(context_test, reinterpret_cast<ze_context_handle_t>(rec->_context_handle));
            ASSERT_EQ(queue_test_kernel,
                      reinterpret_cast<ze_command_queue_handle_t>(rec->_queue_handle));
            kernel_records.push_back(*rec);
          }
          break;
        }
        default: {
          std::cerr << "This shouldn't happen" << '\n';
          break;
        }
      }
    }
    ::operator delete(buf);
  }
  static void NullBufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_memory_copy) - sizeof(pti_view_record_memory_copy);
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    rejected_buffer_calls += 1;
    *buf = static_cast<unsigned char*>(ptr);
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_memory_copy);
  }

  static void InadequateBufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel) - 1;
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    rejected_buffer_calls += 1;
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_memory_copy);
  }
  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel);
    void* ptr = ::operator new(*buf_size);
    requested_buffer_calls += 1;
    ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
    *buf = static_cast<unsigned char*>(ptr);
    if (!*buf) {
      std::abort();
    }
    buffer_size_atleast_largest_record = (*buf_size) >= sizeof(pti_view_record_memory_copy);
  }

  int RunGemm(bool with_polling = false, bool include_sycl_runtime = false,
              bool include_zecalls = false, bool include_gpu_kernels = true, bool add_sycl = false,
              bool include_synch = false) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    ze_initialization_succeeded = (status == ZE_RESULT_SUCCESS);

    ze_device_handle_t device = utils::ze::GetGpuDevice(kPtiDeviceId);
    ze_driver_handle_t driver = utils::ze::GetGpuDriver(kPtiDeviceId);
    if (device == nullptr || driver == nullptr) {
      std::cout << "Unable to find GPU device" << std::endl;
      return 0;
    }

    if (!utils::ze::GetDeviceUUID(device, device_uuid.id)) {
      std::cout << "Unable to get device UUID" << std::endl;
      return 1;
    }

    StartTracing(include_sycl_runtime, include_zecalls, include_gpu_kernels, include_synch);

    std::cout << "Level Zero Matrix Multiplication (matrix size: " << size << " x " << size
              << ", repeats " << repeat_count << " times)" << std::endl;
    std::cout << "Target device: " << utils::ze::GetDeviceName(device) << std::endl;

    std::vector<float> a(size * size, A_VALUE);
    std::vector<float> b(size * size, B_VALUE);
    std::vector<float> c(size * size, 0.0f);
    StopTracing(include_sycl_runtime, include_zecalls, include_gpu_kernels, include_synch);

    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * static_cast<float>(size);

    StartTracing(include_sycl_runtime, include_zecalls, include_gpu_kernels, include_synch);
    PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid_));

    Compute(device, driver, a, b, c, size, repeat_count, expected_result, with_polling);
    if (add_sycl) {
      ComputeUsingSycl(a, repeat_count);
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid_));
    StopTracing(include_sycl_runtime, include_zecalls, include_gpu_kernels, include_synch);

    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
    auto flush_results = ptiFlushAllViews();
    return flush_results;
  }
};

/**
 * This test body uses directly L0 API to simulate one of oneCCL behaviors
 * It creates two immediate command lists, do not synchronize on events but rather poll them and
 * destroy them after they found signaled
 * so it verified that such case profiled correctly
 *
 * This test is to be skipped if Local Profiling is not available,
 * Because this test specifics are
 * 1) not having "usual" synchronization but rather realy on events polling,
 * 2) destroying events as soon as they are signaled
 * and such case is not handled by Full API Profiling mode implementation so far -
 * as Full API Profiling mode doesn't create any special events but rather relies on
 * intercepting EventPool creation and so makeing all events with Timestamp property
 *
 */
TEST_F(MainZeFixtureTest, ProfilingSuccededWhenEventPolling) {
  if (pti_result::PTI_SUCCESS != ptiViewGPULocalAvailable()) {
    GTEST_SKIP();
  }
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  capture_records = true;
  repeat_count = 1;
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(copy_records.size(), static_cast<std::size_t>(3));
  auto m2d_1 = static_cast<std::size_t>(-1);
  auto m2d_2 = static_cast<std::size_t>(-1);
  for (std::size_t i = 0; i < copy_records.size(); i++) {
    if (copy_records[i]._memcpy_type == pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2D) {
      if (m2d_1 == static_cast<std::size_t>(-1)) {
        m2d_1 = i;
      } else {
        m2d_2 = i;
        break;
      }
    }
  }
  EXPECT_EQ(m2d_1 != static_cast<std::size_t>(-1), true);
  EXPECT_EQ(m2d_2 != static_cast<std::size_t>(-1), true);
  EXPECT_NE(m2d_1, m2d_2);
  // Check if the duration diff between the two similar H2D transfers is less than several percents
  // E.g. 20% or 70% is just some common sense number to check if the durations are close enough
#ifdef _WIN32
  // On Windows (on integrated GPU) the difference is expected to be higher
  // as the first transer seems warming up the hardware and the second one is faster
  float expected_diff = 0.70f;
#else
  float expected_diff = 0.20f;
#endif
  std::cout << "Expected max difference between two similar M2D transfers: " << expected_diff
            << std::endl;
  uint64_t dur1 = copy_records[m2d_1]._end_timestamp - copy_records[m2d_1]._start_timestamp;
  uint64_t dur2 = copy_records[m2d_2]._end_timestamp - copy_records[m2d_2]._start_timestamp;
  std::cout << "Duration 1: " << dur1 << ", Duration 2: " << dur2 << std::endl;
  float rel_diff = fabs(2.f * ((float)dur1 - (float)dur2) / ((float)dur1 + (float)dur2));
  std::cout << "Relative difference between two similar M2D transfers: " << rel_diff << std::endl;
  EXPECT_LT(rel_diff, expected_diff);
  // Check if the kernel duration is greater than 0
  EXPECT_EQ(kernel_records.size(), static_cast<std::size_t>(1));
  EXPECT_GT(kernel_records[0]._end_timestamp, kernel_records[0]._start_timestamp);
}

TEST_F(MainZeFixtureTest, ZeInitializationSucceeded) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(ze_initialization_succeeded, true);
}

TEST_F(MainZeFixtureTest, NegTestBufferSizeAtleastLargestRecord) {
  // Checks if ptiViewSetCallbacks rejects callback and using default
  // or existing callbacks.
  EXPECT_EQ(ptiViewSetCallbacks(InadequateBufferRequested, BufferCompleted),
            pti_result::PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(RunGemm(), 0);
  ASSERT_EQ(rejected_buffer_calls, 1 * repeat_count);
}

TEST_F(MainZeFixtureTest, BufferSizeAtleastLargestRecord) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  ASSERT_EQ(buffer_size_atleast_largest_record, true);
}

TEST_F(MainZeFixtureTest, BufferCallBacksRegistered) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(buffer_cb_registered, true);
}

TEST_F(MainZeFixtureTest, SecondCallbackCalled) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_GT(completed_buffer_used_bytes, static_cast<size_t>(0));
}

TEST_F(MainZeFixtureTest, MemoryViewRecordCreated) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(memory_view_record_created, true);
}

TEST_F(MainZeFixtureTest, KernelViewRecordCreated) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(kernel_view_record_created, true);
}

TEST_F(MainZeFixtureTest, NumberOfExpectedMemoryRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(memory_view_record_count, 3 * repeat_count);
}

TEST_F(MainZeFixtureTest, NumberOfExpectedKernelRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(kernel_view_record_count, 1 * repeat_count);
}

TEST_F(MainZeFixtureTest, RequestedAndCompletedBuffers) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(requested_buffer_calls, completed_buffer_calls);
}

TEST_F(MainZeFixtureTest, NegTestNullBufferSize) {
  ASSERT_EQ(ptiViewSetCallbacks(NullBufferRequested, BufferCompleted),
            pti_result::PTI_ERROR_BAD_ARGUMENT);
  EXPECT_EQ(RunGemm(), 0);
  ASSERT_EQ(rejected_buffer_calls, 1 * repeat_count);
}

TEST_F(MainZeFixtureTest, TestCLImmediateSynch) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(RunGemm(true, true, true, true, false, true),
            0);  // with_polling, sycl, zecalls, kernel and sych enabled

  // CommandListSynchronization
  EXPECT_EQ(synchronization_record_clist_seen, true);
  EXPECT_EQ(synch_clist_record_context, synch_input_context);
  EXPECT_EQ(synch_clist_record_api_group, 1);
}

TEST_F(MainZeFixtureTest, AllSynchronizationRelated) {
  // capture_records = true;
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  EXPECT_EQ(RunGemm(), 0);
  EXPECT_EQ(RunGemm(false, true, true, true, false, true),
            0);  // sycl, zecalls, kernel and sych enabled

  // FenceSynchronization
  EXPECT_EQ(synchronization_record_fence_exec_seen, true);
  EXPECT_EQ(synch_fence_record_event, nullptr);
  EXPECT_EQ(synch_fence_record_context, synch_input_context);
  EXPECT_EQ(synch_fence_record_queue, synch_input_queue);

  // CommandQueueSynchronization
  EXPECT_EQ(synch_cqueue_record_queue, synch_input_queue);

  // EventHostSynchronization
  EXPECT_EQ(synchronization_record_event_seen, true);
  EXPECT_EQ(synch_event_record_event, synch_input_event);
  EXPECT_EQ(synch_event_record_context, synch_input_context);

  // BarrierExecSynchronization
  EXPECT_EQ(synch_barrier_record_context, synch_input_context);
  EXPECT_EQ(synch_barrier_record_queue, synch_input_queue);
  EXPECT_EQ(synch_barrier_record_event, nullptr);
  EXPECT_EQ(synchronization_barrier_event_number, synch_input_event_number);

  // SampleSynchApiNamesSeen
  EXPECT_EQ((std::strcmp(fence_api_name, "zeFenceHostSynchronize") == 0), true);
  EXPECT_EQ(
      (std::strcmp(barrier_mem_ranges_api_name, "zeCommandListAppendMemoryRangesBarrier") == 0),
      true);
  EXPECT_EQ((std::strcmp(barrier_exec_api_name, "zeCommandListAppendBarrier") == 0), true);

  // AllSynchronizationRecordTypesSeen
  EXPECT_EQ(synchronization_record_barrier_exec_seen, true);
  EXPECT_EQ(synchronization_record_barrier_mem_seen, true);
  EXPECT_EQ(synchronization_record_fence_exec_seen, true);
  EXPECT_EQ(synchronization_record_event_seen, true);
  EXPECT_EQ(synchronization_record_cqueue_seen, true);
}

TEST_F(MainZeFixtureTest, SyclBasedAndZeBasedKernelLaunchesPresent) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  // Enable sycl and kernel view kinds only. Additionally run Sycl based launch kernel.
  EXPECT_EQ(RunGemm(false, true, false, true, true), 0);
  EXPECT_EQ(special_record_seen, true);
  EXPECT_EQ(kernel_launch_id, kCommandListAppendLaunchKernelId);  // zeCommandListAppendLaunchKernel
  EXPECT_EQ(num_special_records, repeat_count);
  EXPECT_EQ(sycl_runtime_launch_seen, true);
  EXPECT_EQ(num_sycl_runtime_launch_records, repeat_count);
  EXPECT_EQ(num_sycl_runtime_launch_records + num_special_records, kernel_view_record_count);
}

// Zecalls Disabled, Sycl Enabled, no sycl api fires, gpu kernel enabled and fires
TEST_P(MainZeFixtureTest, SpecialRecordPresent) {
  // GetParam returns 3 valued tuple: values correspond to (from left to right)
  //     whether we enable the viewkinds for --- sycl, zecalls, kernel.
  auto [sycl, zecall, kernel] = GetParam();
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  // Polling, sycl, zecalls --- enabled/disabled
  EXPECT_EQ(RunGemm(false, sycl, zecall, kernel), 0);
  if (sycl == true && zecall == false && kernel == true) {
    EXPECT_EQ(special_record_seen, true);
    EXPECT_EQ(kernel_launch_id,
              kCommandListAppendLaunchKernelId);  // zeCommandListAppendLaunchKernel
    EXPECT_EQ(zecall_record_seen, false);
    EXPECT_EQ(external_corrid_special_record_seen, true);
    EXPECT_EQ(corrid_in_special_record, external_corrid_in_ext_rec);
    EXPECT_GT(corrid_in_special_record, 0UL);
    EXPECT_EQ(num_special_records, repeat_count);
  } else {
    EXPECT_EQ(special_record_seen, false);
    EXPECT_EQ(zecall_record_seen, zecall);
    EXPECT_EQ(external_corrid_special_record_seen, false);
    EXPECT_EQ(corrid_in_special_record, external_corrid_in_ext_rec);
    EXPECT_EQ(corrid_in_special_record, 0UL);
  }
  /*
    std::ostringstream oss;
    oss << "SpecialRecordPresenceTest_enabled_values_" << sycl << ":" << zecall << ":" << kernel;
    SCOPED_TRACE(oss.str());  // Set the test name for better readability on a failing test.
  */
}

// Tuple values correspond to (from left to right) whether we enable the viewkinds for --- sycl,
// zecalls, kernel.
INSTANTIATE_TEST_SUITE_P(MainZeTests, MainZeFixtureTest,
                         ::testing::Values(std::make_tuple(true, false, true),
                                           std::make_tuple(true, false, false),
                                           std::make_tuple(true, true, true),
                                           std::make_tuple(false, false, true),
                                           std::make_tuple(false, true, true)));

class LocalModeZeGemmTest : public testing::Test {
 protected:
  LocalModeZeGemmTest()
      : spv_binary_(utils::LoadBinaryFile(utils::GetExecutablePath() + kKernelFile)) {
    a_vector_ = std::vector<float>(size_ * size_, A_VALUE);
    b_vector_ = std::vector<float>(size_ * size_, B_VALUE);
    result_vector_ = std::vector<float>(size_ * size_, 0);
  }

  void EnableView(pti_view_kind view) {
    EXPECT_EQ(ptiViewEnable(view), PTI_SUCCESS);
    enabled_views_.push_back(view);
  }

  void DisableAndFlushAllViews() {
    for (auto view : enabled_views_) {
      EXPECT_EQ(ptiViewDisable(view), PTI_SUCCESS);
    }
    enabled_views_.clear();
    EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  }

  void InitializeDrivers() {
    auto status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    drv_ = utils::ze::GetGpuDriver(kPtiDeviceId);
    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
    ASSERT_NE(drv_, nullptr);
    ctx_ = utils::ze::GetContext(drv_);
  }

  void InitializeEvent() {
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,  // all events in pool are visible to Host
        num_events_                       // count
    };

    if (event_timestamps_enabled_) {
      event_pool_desc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }

    ASSERT_NE(ctx_, nullptr);
    ASSERT_NE(dev_, nullptr);
    auto status = zeEventPoolCreate(ctx_, &event_pool_desc, 1, &dev_, &evt_pl_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    const ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                        ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};

    ASSERT_NE(evt_pl_, nullptr);
    status = zeEventCreate(evt_pl_, &event_desc, &evt_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void InitializeQueue() {
    ze_command_queue_desc_t cmd_queue_desc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    auto status = zeCommandQueueCreate(ctx_, dev_, &cmd_queue_desc, &cmd_q_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    ze_command_list_desc_t cmd_list_desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};

    if (kInorderQueue) {
      cmd_list_desc.flags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;
    }

    status = zeCommandListCreate(ctx_, dev_, &cmd_list_desc, &cmd_list_);

    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetKernelGroupSize() {
    ASSERT_NE(knl_, nullptr);
    auto status = zeKernelSuggestGroupSize(knl_, size_, size_, 1, std::data(group_size_),
                                           std::data(group_size_) + 1, std::data(group_size_) + 2);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    if ((size_ % group_size_[0]) != 0 || (size_ % group_size_[1]) != 0) {
      FAIL() << "Non-uniform group size";
    }
    status = zeKernelSetGroupSize(knl_, group_size_[0], group_size_[1], group_size_[2]);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void* AllocateDeviceBuffer(size_t size, size_t alignment) {
    void* storage = nullptr;
    const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                   0, 0};
    if (!ctx_) {
      return nullptr;
    }
    if (!dev_) {
      return nullptr;
    }

    auto status = zeMemAllocDevice(ctx_, &alloc_desc, size, alignment, dev_, &storage);

    if (status != ZE_RESULT_SUCCESS) {
      return nullptr;
    }

    return storage;
  }

  template <typename T>
  void AppendCopyToDevice(void* dev, const T& host_container) {
    ASSERT_NE(cmd_list_, nullptr);
    ASSERT_NE(dev, nullptr);
    auto status = zeCommandListAppendMemoryCopy(
        cmd_list_, dev, std::data(host_container),
        std::size(host_container) * sizeof(typename T::value_type), nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  template <typename T>
  void AppendCopyFromDevice(T& host_container, const void* dev) {
    ASSERT_NE(cmd_list_, nullptr);
    ASSERT_NE(dev, nullptr);
    auto status = zeCommandListAppendMemoryCopy(
        cmd_list_, std::data(host_container), dev,
        std::size(host_container) * sizeof(typename T::value_type), nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void AppendBarrier() {
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListAppendBarrier(cmd_list_, nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void AppendGemmKernel() {
    ASSERT_NE(group_size_[0], 0U);
    ASSERT_NE(group_size_[1], 0U);
    const ze_group_count_t dim = {size_ / group_size_[0], size_ / group_size_[1], 1};
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListAppendLaunchKernel(cmd_list_, knl_, &dim, evt_, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void ValidateGemmKernel() {
    float expected_result = A_VALUE * B_VALUE * static_cast<float>(size_);
    auto eps = Check(result_vector_, expected_result);
    ASSERT_LE(eps, MAX_EPS);
  }

  void PrepareCommandList() {
    AppendCopyToDevice(a_buf_, a_vector_);
    AppendCopyToDevice(b_buf_, b_vector_);
    AppendBarrier();
    AppendGemmKernel();
    AppendBarrier();
    AppendCopyFromDevice(result_vector_, result_buf_);
    AppendBarrier();
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListClose(cmd_list_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetKernelArguments() {
    a_buf_ = AllocateDeviceBuffer(size_ * size_ * sizeof(typename decltype(a_vector_)::value_type),
                                  ALIGN);
    ASSERT_NE(a_buf_, nullptr);

    b_buf_ = AllocateDeviceBuffer(size_ * size_ * sizeof(typename decltype(b_vector_)::value_type),
                                  ALIGN);
    ASSERT_NE(b_buf_, nullptr);

    result_buf_ = AllocateDeviceBuffer(
        size_ * size_ * sizeof(typename decltype(result_vector_)::value_type), ALIGN);
    ASSERT_NE(result_buf_, nullptr);

    ASSERT_NE(knl_, nullptr);
    auto status = zeKernelSetArgumentValue(knl_, 0, sizeof(a_buf_), &a_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 1, sizeof(b_buf_), &b_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 2, sizeof(result_buf_), &result_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 3, sizeof(size_), &size_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void CreateKernel() {
    const ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                          nullptr,
                                          ZE_MODULE_FORMAT_IL_SPIRV,
                                          std::size(spv_binary_),
                                          std::data(spv_binary_),
                                          nullptr,
                                          nullptr};

    auto status = zeModuleCreate(ctx_, dev_, &module_desc, &mdl_, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    ASSERT_NE(mdl_, nullptr);
    const ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, kKernelName};
    status = zeKernelCreate(mdl_, &kernel_desc, &knl_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetUp() override {
    if (ptiViewGPULocalAvailable() != pti_result::PTI_SUCCESS) {
      GTEST_SKIP() << "GPULocal is not available. Skipping Test Suite";
    }
    ASSERT_NE(std::size(spv_binary_), 0ULL);

    ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);

    LocalModeZeGemmTestData::Instance().Reset();
  }

  void TearDown() override {
    DisableAndFlushAllViews();

    // TODO: RAII-ify this
    if (result_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, result_buf_), ZE_RESULT_SUCCESS);
    }

    if (b_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, b_buf_), ZE_RESULT_SUCCESS);
    }

    if (a_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, a_buf_), ZE_RESULT_SUCCESS);
    }

    if (cmd_list_) {
      EXPECT_EQ(zeCommandListDestroy(cmd_list_), ZE_RESULT_SUCCESS);
    }

    if (cmd_q_) {
      EXPECT_EQ(zeCommandQueueDestroy(cmd_q_), ZE_RESULT_SUCCESS);
    }

    if (evt_) {
      EXPECT_EQ(zeEventDestroy(evt_), ZE_RESULT_SUCCESS);
    }

    if (evt_pl_) {
      EXPECT_EQ(zeEventPoolDestroy(evt_pl_), ZE_RESULT_SUCCESS);
    }

    if (knl_) {
      EXPECT_EQ(zeKernelDestroy(knl_), ZE_RESULT_SUCCESS);
    }

    if (mdl_) {
      EXPECT_EQ(zeModuleDestroy(mdl_), ZE_RESULT_SUCCESS);
    }

    if (ctx_) {
      EXPECT_EQ(zeContextDestroy(ctx_), ZE_RESULT_SUCCESS);
    }
  }

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
    if (!*buf) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf_size = kRequestedBufferSize;
  }

  static void ParseBuffer(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      if (used_bytes) {
        samples_utils::AlignedDealloc(buf);
      }
      return;
    }
    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        FAIL() << "Found Error Parsing Records from PTI";
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          FAIL() << "Found Invalid PTI View Record";
          break;
        }
        case pti_view_kind::PTI_VIEW_DRIVER_API: {
          LocalModeZeGemmTestData::Instance().num_ze_records++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          LocalModeZeGemmTestData::Instance().num_kernels++;
          auto* kernel_record = reinterpret_cast<pti_view_record_kernel*>(ptr);
          EXPECT_THAT(kernel_record->_name, ::testing::StrEq(kKernelName));
          break;
        }
        default: {
          break;
        }
      }
    }
    samples_utils::AlignedDealloc(buf);
  }
  static constexpr auto kRequestedBufferSize = 1'000;
  static constexpr auto kInorderQueue = true;
  static constexpr const char* const kKernelName = "GEMM";
  static constexpr const char* const kKernelFile = "gemm.spv";

  struct LocalModeZeGemmTestData {
    static LocalModeZeGemmTestData& Instance() {
      static LocalModeZeGemmTestData test_data{};
      return test_data;
    }

    void Reset() {
      num_ze_records = 0;
      num_kernels = 0;
    }

    size_t num_ze_records = 0;
    size_t num_kernels = 0;
  };

  std::vector<pti_view_kind> enabled_views_;
  bool event_timestamps_enabled_ = false;
  uint32_t num_events_ = 1;
  std::vector<uint8_t> spv_binary_;
  unsigned int size_ = 1024;
  std::vector<float> a_vector_;
  std::vector<float> b_vector_;
  std::vector<float> result_vector_;
  ze_driver_handle_t drv_ = nullptr;
  ze_device_handle_t dev_ = nullptr;
  ze_context_handle_t ctx_ = nullptr;
  ze_module_handle_t mdl_ = nullptr;
  std::array<uint32_t, 3> group_size_ = {0};
  ze_kernel_handle_t knl_ = nullptr;
  ze_event_pool_handle_t evt_pl_ = nullptr;
  ze_event_handle_t evt_ = nullptr;
  ze_command_queue_handle_t cmd_q_ = nullptr;
  ze_command_list_handle_t cmd_list_ = nullptr;
  void* a_buf_ = nullptr;
  void* b_buf_ = nullptr;
  void* result_buf_ = nullptr;
};

TEST_F(LocalModeZeGemmTest, TestStartTracingExecuteCommandQueue) {
  // Leaving out of Constructor / SetUp for now to allow extending to more test
  // cases.
  InitializeDrivers();
  InitializeEvent();
  InitializeQueue();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  PrepareCommandList();

  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DRIVER_API);

  auto status = zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  status = zeCommandQueueSynchronize(cmd_q_, UINT64_MAX);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_ze_records, static_cast<size_t>(2));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(0));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest, TestStartTracingPrepareCommandList) {
  // Leaving out of Constructor / SetUp for now to allow extending to more test
  // cases.
  InitializeDrivers();
  InitializeEvent();
  InitializeQueue();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  PrepareCommandList();

  auto status = zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  status = zeCommandQueueSynchronize(cmd_q_, UINT64_MAX);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));

  ValidateGemmKernel();
}

using pti::test::utils::CreateFullBuffer;
using pti::test::utils::RecordInserts;

class GetNextRecordTestSuite : public ::testing::Test {
 protected:
  static constexpr std::size_t kNumMemRecs = 15;
  static constexpr std::size_t kNumExtRecs = 100;
  static constexpr std::size_t kNumKernelRecs = 3;
  static constexpr std::size_t kNumOhRecs = 1;
  static constexpr std::size_t kTotalRecs =
      2 * kNumOhRecs + 2 * kNumMemRecs + kNumKernelRecs + kNumExtRecs;
  GetNextRecordTestSuite()
      : test_buf_(CreateFullBuffer<RecordInserts<pti_view_record_overhead, kNumOhRecs>,
                                   RecordInserts<pti_view_record_memory_copy, kNumMemRecs>,
                                   RecordInserts<pti_view_record_memory_fill, kNumMemRecs>,
                                   RecordInserts<pti_view_record_external_correlation, kNumExtRecs>,
                                   RecordInserts<pti_view_record_kernel, kNumKernelRecs>,
                                   RecordInserts<pti_view_record_overhead, kNumOhRecs>>()) {}
  std::vector<unsigned char> test_buf_;
};

TEST_F(GetNextRecordTestSuite, NullBufferTest) {
  pti_view_record_base* current_record = nullptr;
  auto result = ptiViewGetNextRecord(nullptr, 0, &current_record);
  ASSERT_EQ(result, pti_result::PTI_STATUS_END_OF_BUFFER);
}

TEST_F(GetNextRecordTestSuite, NullBufferBadSizeTest) {
  pti_view_record_base* current_record = nullptr;
  auto result = ptiViewGetNextRecord(nullptr, static_cast<std::size_t>(-1), &current_record);
  ASSERT_EQ(result, pti_result::PTI_STATUS_END_OF_BUFFER);
}

TEST_F(GetNextRecordTestSuite, NullRecordBufferTest) {
  auto result = ptiViewGetNextRecord(test_buf_.data(), test_buf_.size(), nullptr);
  ASSERT_EQ(result, pti_result::PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(GetNextRecordTestSuite, NullRecordBadSizeBufferTest) {
  auto result = ptiViewGetNextRecord(nullptr, static_cast<std::size_t>(-1), nullptr);
  ASSERT_EQ(result, pti_result::PTI_ERROR_BAD_ARGUMENT);
}

TEST_F(GetNextRecordTestSuite, CheckBufferEndTest) {
  pti_view_record_base* current_record = nullptr;
  std::size_t total_records = 0;
  while (true) {
    auto result = ptiViewGetNextRecord(test_buf_.data(), test_buf_.size(), &current_record);
    if (result == pti_result::PTI_STATUS_END_OF_BUFFER) {
      EXPECT_NE(current_record, nullptr);
      break;
    }
    total_records++;
  }
  EXPECT_EQ(total_records, kTotalRecs);
  auto result = ptiViewGetNextRecord(test_buf_.data(), test_buf_.size(), &current_record);

  EXPECT_NE(current_record, nullptr);
  ASSERT_EQ(result, pti_result::PTI_STATUS_END_OF_BUFFER);
}

TEST_F(GetNextRecordTestSuite, RegularParseRecordsTest) {
  pti_view_record_base* current_record = nullptr;
  std::size_t total_records = 0;
  std::size_t number_of_memory_copies = 0;
  std::size_t number_of_kernel = 0;
  std::size_t number_of_overhead = 0;
  while (true) {
    auto result = ptiViewGetNextRecord(test_buf_.data(), test_buf_.size(), &current_record);
    if (result == pti_result::PTI_STATUS_END_OF_BUFFER) {
      break;
    }
    if (result == pti_result::PTI_ERROR_INTERNAL) {
      FAIL();
    }
    if (result == pti_result::PTI_ERROR_BAD_ARGUMENT) {
      FAIL();
    }
    total_records++;
    if (current_record->_view_kind == PTI_VIEW_DEVICE_GPU_KERNEL) {
      number_of_kernel++;
    }
    if (current_record->_view_kind == PTI_VIEW_DEVICE_GPU_MEM_COPY) {
      number_of_memory_copies++;
    }
    if (current_record->_view_kind == PTI_VIEW_COLLECTION_OVERHEAD) {
      number_of_overhead++;
    }
  }
  EXPECT_EQ(number_of_memory_copies, kNumMemRecs);
  EXPECT_EQ(number_of_overhead, 2 * kNumOhRecs);
  EXPECT_EQ(number_of_kernel, kNumKernelRecs);
  ASSERT_EQ(total_records, kTotalRecs);
}

TEST(PtiVersionTestSuite, TestVersionMacros) {
  // Check against first public PTI version 0.1.0
  EXPECT_GE(PTI_VERSION_MAJOR, 0);
  EXPECT_GE(PTI_VERSION_MINOR, PTI_VERSION_MAJOR == 0 ? 1 : 0);
  EXPECT_GE(PTI_VERSION_PATCH, 0);
}

TEST(PtiVersionTestSuite, TestVersionFunction) {
  // Unit tests should be run against same version of header and lib
  auto pti_ver = ptiVersion();
  EXPECT_EQ(pti_ver._major, static_cast<uint32_t>(PTI_VERSION_MAJOR));
  EXPECT_EQ(pti_ver._minor, static_cast<uint32_t>(PTI_VERSION_MINOR));
  EXPECT_EQ(pti_ver._patch, static_cast<uint32_t>(PTI_VERSION_PATCH));
}

TEST(PtiVersionTestSuite, TestVersionString) {
  using ::testing::ContainsRegex;
  // Unit tests should be run against same version of header and lib
#if !defined(WIN32)
  constexpr const char* const test_version_regex = "^[0-9]+\\.[0-9]+\\.[0-9]+";
#else
  constexpr const char* const test_version_regex = "^\\d+\\.\\d+\\.\\d+";
#endif
  EXPECT_THAT(ptiVersionString(), ContainsRegex(test_version_regex));
  EXPECT_STREQ(PTI_VERSION_STRING, ptiVersionString());
}
