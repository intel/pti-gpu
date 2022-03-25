//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <math.h>
#include <string.h>

#include <chrono>
#include <iostream>

#include "ze_utils.h"
#include "utils.h"

#define ALIGN 64

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

static float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

static float RunAndCheck(ze_kernel_handle_t kernel,
                         ze_device_handle_t device,
                         ze_context_handle_t context,
                         const std::vector<float>& a,
                         const std::vector<float>& b,
                         std::vector<float>& c,
                         unsigned size,
                         float expected_result) {
  PTI_ASSERT(kernel != nullptr);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(context != nullptr);

  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t group_size[3] = { 0 };
  status = zeKernelSuggestGroupSize(kernel, size, size, 1,
    &(group_size[0]), &(group_size[1]), &(group_size[2]));
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if ((size % group_size[0]) != 0 || (size % group_size[1]) != 0) {
    std::cout << "Non-uniform workgroups are not supported" << std::endl;
    return 0.0f;
  }

  void* dev_a = nullptr;
  ze_device_mem_alloc_desc_t alloc_desc = {
      ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
  status = zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float),
                            ALIGN, device, &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_b = nullptr;
  status = zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float),
                            ALIGN, device, &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  void* dev_c = nullptr;
  status = zeMemAllocDevice(context, &alloc_desc, size * size * sizeof(float),
                            ALIGN, device, &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetGroupSize(
      kernel, group_size[0], group_size[1], group_size[2]);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeKernelSetArgumentValue(kernel, 0, sizeof(dev_a), &dev_a);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 1, sizeof(dev_a), &dev_b);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 2, sizeof(dev_a), &dev_c);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeKernelSetArgumentValue(kernel, 3, sizeof(size), &size);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_command_list_desc_t cmd_list_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};
  ze_command_list_handle_t cmd_list = nullptr;
  status = zeCommandListCreate(context, device, &cmd_list_desc, &cmd_list);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendMemoryCopy(cmd_list, dev_a, a.data(),
                                         size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zeCommandListAppendMemoryCopy(cmd_list, dev_b, b.data(),
                                         size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
      ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
      1};
  ze_event_pool_handle_t event_pool = nullptr;
  status = zeEventPoolCreate(context, &event_pool_desc,
                             0, nullptr, &event_pool);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_event_desc_t event_desc = {
      ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
      ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
  ze_event_handle_t event = nullptr;
  zeEventCreate(event_pool, &event_desc, &event);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_group_count_t dim = { size / group_size[0],
                           size / group_size[1],
                           1 };
  status = zeCommandListAppendLaunchKernel(cmd_list, kernel, &dim,
                                           event, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListAppendMemoryCopy(cmd_list, c.data(), dev_c,
                                         size * size * sizeof(float),
                                         nullptr, 0, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeCommandListClose(cmd_list);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0,
      ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
  ze_command_queue_handle_t cmd_queue = nullptr;
  status = zeCommandQueueCreate(context, device, &cmd_queue_desc, &cmd_queue);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && cmd_queue != nullptr);

  status = zeCommandQueueExecuteCommandLists(
      cmd_queue, 1, &cmd_list, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

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

  double time = static_cast<double>(
      timestamp.global.kernelEnd - timestamp.global.kernelStart) /
      props.timerResolution;
  std::cout << "Matrix multiplication time: " << time <<
    " sec" << std::endl;

  return Check(c, expected_result);
}

static void Compute(ze_device_handle_t device,
                    ze_driver_handle_t driver,
                    const std::vector<float>& a,
                    const std::vector<float>& b,
                    std::vector<float>& c,
                    unsigned size, unsigned repeat_count,
                    float expected_result) {
  PTI_ASSERT(device != nullptr && driver != nullptr);
  PTI_ASSERT(size > 0 && repeat_count > 0);

  std::string module_name = "gemm.spv";
  std::vector<uint8_t> binary = utils::LoadBinaryFile(
    utils::GetExecutablePath() + module_name);
  if (binary.size() == 0) {
    std::cout << "Unable to find module " << module_name << std::endl;
    return;
  }

  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  ze_module_desc_t module_desc = {
      ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr,
      ZE_MODULE_FORMAT_IL_SPIRV, static_cast<uint32_t>(binary.size()),
      binary.data(), nullptr, nullptr};
  ze_module_handle_t module = nullptr;
  status = zeModuleCreate(context, device, &module_desc, &module, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && module != nullptr);

  ze_kernel_desc_t kernel_desc = {
      ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "GEMM"};
  ze_kernel_handle_t kernel = nullptr;
  status = zeKernelCreate(module, &kernel_desc, &kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS && kernel != nullptr);

  for (unsigned i = 0; i < repeat_count; ++i) {
    if (i == 0) { // Enable data collection for the first iteration
      utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
    }

    float eps = RunAndCheck(kernel, device, context, a, b, c,
                            size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;

    if (i == 0) { // Disable data collection for the rest iterations
      utils::SetEnv("PTI_ENABLE_COLLECTION", "");
    }
  }

  status = zeKernelDestroy(kernel);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeModuleDestroy(module);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  status = zeContextDestroy(context);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

int main(int argc, char* argv[]) {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_device_handle_t device = utils::ze::GetGpuDevice();
  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  if (device == nullptr || driver == nullptr) {
      std::cout << "Unable to find GPU device" << std::endl;
      return 0;
  }

  unsigned size = 1024;
  if (argc > 1) {
    size = std::stoul(argv[1]);
  }

  unsigned repeat_count = 4;
  if (argc > 2) {
    repeat_count = std::stoul(argv[2]);
  }

  std::cout << "Level Zero Matrix Multiplication (matrix size: " << size <<
    " x " << size << ", repeats " << repeat_count << " times)" << std::endl;
  std::cout << "Target device: " << utils::ze::GetDeviceName(device) <<
    std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  auto start = std::chrono::steady_clock::now();
  float expected_result = A_VALUE * B_VALUE * size;
  Compute(device, driver, a, b, c, size, repeat_count, expected_result);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;

  std::cout << "Total execution time: " << time.count() <<
    " sec" << std::endl;
  return 0;
}