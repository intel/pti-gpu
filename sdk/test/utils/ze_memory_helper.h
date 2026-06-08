//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_UTILS_ZE_MEMORY_HELPER_H_
#define TEST_UTILS_ZE_MEMORY_HELPER_H_

#include <level_zero/ze_api.h>

#include <cstddef>
#include <memory>

namespace pti::test::utils {

struct ZeMemFree {
  ze_context_handle_t context_ = nullptr;
  void operator()(void* ptr) const {
    if (context_ && ptr) {
      zeMemFree(context_, ptr);
    }
  }
};

template <typename T>
using ZeMemVector = std::unique_ptr<T, ZeMemFree>;

template <typename T>
ZeMemVector<T> CreateDeviceMemVector(ze_context_handle_t context, ze_device_handle_t device,
                                     std::size_t vector_size, std::size_t alignment = alignof(T)) {
  void* storage = nullptr;
  const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                 0, 0};
  auto result =
      zeMemAllocDevice(context, &alloc_desc, vector_size * sizeof(T), alignment, device, &storage);

  if (result != ZE_RESULT_SUCCESS) {
    return ZeMemVector<T>(nullptr, ZeMemFree{context});
  }

  return ZeMemVector<T>(static_cast<T*>(storage), ZeMemFree{context});
}

template <typename T>
ZeMemVector<T> CreateSharedMemVector(ze_context_handle_t context, ze_device_handle_t device,
                                     std::size_t vector_size, std::size_t alignment = alignof(T)) {
  void* storage = nullptr;
  const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                 0, 0};
  const ze_host_mem_alloc_desc_t host_alloc_desc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr,
                                                    0};
  auto result = zeMemAllocShared(context, &alloc_desc, &host_alloc_desc, vector_size * sizeof(T),
                                 alignment, device, &storage);

  if (result != ZE_RESULT_SUCCESS) {
    return ZeMemVector<T>(nullptr, ZeMemFree{context});
  }

  return ZeMemVector<T>(static_cast<T*>(storage), ZeMemFree{context});
}

template <typename T>
ZeMemVector<T> CreateHostMemVector(ze_context_handle_t context, std::size_t vector_size,
                                   std::size_t alignment = alignof(T)) {
  void* storage = nullptr;
  const ze_host_mem_alloc_desc_t host_alloc_desc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr,
                                                    0};
  auto result =
      zeMemAllocHost(context, &host_alloc_desc, vector_size * sizeof(T), alignment, &storage);
  if (result != ZE_RESULT_SUCCESS) {
    return ZeMemVector<T>(nullptr, ZeMemFree{context});
  }
  return ZeMemVector<T>(static_cast<T*>(storage), ZeMemFree{context});
}

}  // namespace pti::test::utils
#endif  // TEST_UTILS_ZE_MEMORY_HELPER_H_
