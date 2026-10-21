//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <level_zero/ze_api.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

void Check(ze_result_t result, const char* operation) {
  if (result != ZE_RESULT_SUCCESS) {
    throw std::runtime_error(std::string(operation) +
                             " failed: " + std::to_string(static_cast<uint32_t>(result)));
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <gemm.spv>\n";
    return 1;
  }

  try {
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
      throw std::runtime_error("cannot open SPIR-V kernel");
    }
    std::vector<char> binary(std::istreambuf_iterator<char>{input}, {});
    if (binary.empty()) {
      throw std::runtime_error("SPIR-V kernel is empty");
    }

    Check(zeInit(ZE_INIT_FLAG_GPU_ONLY), "zeInit");
    uint32_t driver_count = 0;
    Check(zeDriverGet(&driver_count, nullptr), "zeDriverGet(count)");
    std::vector<ze_driver_handle_t> drivers(driver_count);
    Check(zeDriverGet(&driver_count, drivers.data()), "zeDriverGet");
    ze_driver_handle_t driver = nullptr;
    ze_device_handle_t device = nullptr;
    for (auto candidate : drivers) {
      uint32_t device_count = 0;
      Check(zeDeviceGet(candidate, &device_count, nullptr), "zeDeviceGet(count)");
      std::vector<ze_device_handle_t> devices(device_count);
      Check(zeDeviceGet(candidate, &device_count, devices.data()), "zeDeviceGet");
      for (auto candidate_device : devices) {
        ze_device_properties_t properties{};
        properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        Check(zeDeviceGetProperties(candidate_device, &properties), "zeDeviceGetProperties");
        if (properties.type == ZE_DEVICE_TYPE_GPU) {
          driver = candidate;
          device = candidate_device;
          break;
        }
      }
      if (device != nullptr) break;
    }
    if (device == nullptr) throw std::runtime_error("no Level Zero GPU found");

    ze_context_desc_t context_desc{};
    context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    ze_context_handle_t context = nullptr;
    Check(zeContextCreate(driver, &context_desc, &context), "zeContextCreate");
    ze_module_desc_t module_desc{};
    module_desc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    module_desc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    module_desc.inputSize = binary.size();
    module_desc.pInputModule = reinterpret_cast<const uint8_t*>(binary.data());
    ze_module_handle_t module = nullptr;
    Check(zeModuleCreate(context, device, &module_desc, &module, nullptr), "zeModuleCreate");
    ze_kernel_desc_t kernel_desc{};
    kernel_desc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    kernel_desc.pKernelName = "GEMM";
    ze_kernel_handle_t kernel = nullptr;
    Check(zeKernelCreate(module, &kernel_desc, &kernel), "zeKernelCreate");

    constexpr uint32_t size = 1024;
    constexpr uint32_t repeats = 8;
    constexpr size_t bytes = static_cast<size_t>(size) * size * sizeof(float);
    std::vector<float> a(size * size, 0.128f);
    std::vector<float> b(size * size, 0.256f);
    std::vector<float> c(size * size);
    ze_device_mem_alloc_desc_t alloc_desc{};
    alloc_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    void* device_a = nullptr;
    void* device_b = nullptr;
    void* device_c = nullptr;
    Check(zeMemAllocDevice(context, &alloc_desc, bytes, 64, device, &device_a),
          "zeMemAllocDevice(a)");
    Check(zeMemAllocDevice(context, &alloc_desc, bytes, 64, device, &device_b),
          "zeMemAllocDevice(b)");
    Check(zeMemAllocDevice(context, &alloc_desc, bytes, 64, device, &device_c),
          "zeMemAllocDevice(c)");
    Check(zeKernelSetArgumentValue(kernel, 0, sizeof(device_a), &device_a),
          "zeKernelSetArgumentValue(a)");
    Check(zeKernelSetArgumentValue(kernel, 1, sizeof(device_b), &device_b),
          "zeKernelSetArgumentValue(b)");
    Check(zeKernelSetArgumentValue(kernel, 2, sizeof(device_c), &device_c),
          "zeKernelSetArgumentValue(c)");
    Check(zeKernelSetArgumentValue(kernel, 3, sizeof(size), &size),
          "zeKernelSetArgumentValue(size)");

    uint32_t group_x = 0, group_y = 0, group_z = 0;
    Check(zeKernelSuggestGroupSize(kernel, size, size, 1, &group_x, &group_y, &group_z),
          "zeKernelSuggestGroupSize");
    if (size % group_x != 0 || size % group_y != 0) {
      throw std::runtime_error("unsupported group size");
    }
    Check(zeKernelSetGroupSize(kernel, group_x, group_y, group_z), "zeKernelSetGroupSize");

    uint32_t group_count = 0;
    Check(zeDeviceGetCommandQueueGroupProperties(device, &group_count, nullptr),
          "zeDeviceGetCommandQueueGroupProperties(count)");
    std::vector<ze_command_queue_group_properties_t> groups(group_count);
    for (auto& group : groups) group.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    Check(zeDeviceGetCommandQueueGroupProperties(device, &group_count, groups.data()),
          "zeDeviceGetCommandQueueGroupProperties");
    uint32_t ordinal = group_count;
    for (uint32_t index = 0; index < group_count; ++index) {
      if (groups[index].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
        ordinal = index;
        break;
      }
    }
    if (ordinal == group_count) throw std::runtime_error("no compute queue group");

    ze_command_list_desc_t list_desc{};
    list_desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    list_desc.commandQueueGroupOrdinal = ordinal;
    ze_command_list_handle_t list = nullptr;
    Check(zeCommandListCreate(context, device, &list_desc, &list), "zeCommandListCreate");
    Check(zeCommandListAppendMemoryCopy(list, device_a, a.data(), bytes, nullptr, 0, nullptr),
          "zeCommandListAppendMemoryCopy(a)");
    Check(zeCommandListAppendMemoryCopy(list, device_b, b.data(), bytes, nullptr, 0, nullptr),
          "zeCommandListAppendMemoryCopy(b)");
    ze_group_count_t dispatch{size / group_x, size / group_y, 1};
    for (uint32_t index = 0; index < repeats; ++index) {
      Check(zeCommandListAppendLaunchKernel(list, kernel, &dispatch, nullptr, 0, nullptr),
            "zeCommandListAppendLaunchKernel");
    }
    Check(zeCommandListAppendMemoryCopy(list, c.data(), device_c, bytes, nullptr, 0, nullptr),
          "zeCommandListAppendMemoryCopy(c)");
    Check(zeCommandListClose(list), "zeCommandListClose");

    ze_command_queue_desc_t queue_desc{};
    queue_desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queue_desc.ordinal = ordinal;
    ze_command_queue_handle_t queue = nullptr;
    Check(zeCommandQueueCreate(context, device, &queue_desc, &queue), "zeCommandQueueCreate");
    Check(zeCommandQueueExecuteCommandLists(queue, 1, &list, nullptr),
          "zeCommandQueueExecuteCommandLists");
    Check(zeCommandQueueSynchronize(queue, UINT64_MAX), "zeCommandQueueSynchronize");

    const float expected = 0.128f * 0.256f * size;
    if (std::fabs(c.front() - expected) > 0.01f || std::fabs(c.back() - expected) > 0.01f) {
      throw std::runtime_error("GEMM output mismatch");
    }
    std::cout << "Level Zero GEMM: " << repeats << " launches, valid output " << c.front() << '\n';

    Check(zeCommandQueueDestroy(queue), "zeCommandQueueDestroy");
    Check(zeCommandListDestroy(list), "zeCommandListDestroy");
    Check(zeMemFree(context, device_a), "zeMemFree(a)");
    Check(zeMemFree(context, device_b), "zeMemFree(b)");
    Check(zeMemFree(context, device_c), "zeMemFree(c)");
    Check(zeKernelDestroy(kernel), "zeKernelDestroy");
    Check(zeModuleDestroy(module), "zeModuleDestroy");
    Check(zeContextDestroy(context), "zeContextDestroy");
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
