//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_ZE_GRAPH_WORKLOADS_H_
#define TEST_GRAPH_ZE_GRAPH_WORKLOADS_H_

#include <level_zero/ze_api.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

#include "graph/graph_dotproduct_workload_info.h"
#include "utils/ze_memory_helper.h"

struct ZeKernelDestroy {
  void operator()(ze_kernel_handle_t kernel) const {
    if (kernel) {
      zeKernelDestroy(kernel);
    }
  }
};

using ZeKernel = std::unique_ptr<std::remove_pointer_t<ze_kernel_handle_t>, ZeKernelDestroy>;

struct ZeCommandListDestroy {
  void operator()(ze_command_list_handle_t clist) const {
    if (clist) {
      zeCommandListDestroy(clist);
    }
  }
};

using ZeCommandList =
    std::unique_ptr<std::remove_pointer_t<ze_command_list_handle_t>, ZeCommandListDestroy>;

template <std::size_t ArgN>
struct KernelBundle {
  ZeKernel kernel = nullptr;
  ze_group_size_t group_size{};
  ze_group_count_t group_count{};
  std::array<void*, ArgN> args{};

  template <typename... Args>
  void SetArgs(Args*... kernel_args) {
    static_assert(sizeof...(kernel_args) == ArgN, "Number of kernel arguments must match ArgN");
    static_assert(((std::is_void_v<std::remove_pointer_t<Args>> ||
                    (std::is_trivial_v<std::remove_pointer_t<Args>> &&
                     std::is_standard_layout_v<std::remove_pointer_t<Args>>)) &&
                   ...),
                  "Kernel arguments must be POD types");
    args = {static_cast<void*>(kernel_args)...};
  }
};

template <typename T>
[[nodiscard]] inline auto CreateDotProductVectors(ze_context_handle_t ctx, ze_device_handle_t dev,
                                                  size_t vector_size) {
  return std::make_tuple(pti::test::utils::CreateSharedMemVector<T>(ctx, dev, 1),
                         pti::test::utils::CreateDeviceMemVector<T>(ctx, dev, vector_size),
                         pti::test::utils::CreateDeviceMemVector<T>(ctx, dev, vector_size),
                         pti::test::utils::CreateDeviceMemVector<T>(ctx, dev, vector_size));
}

[[nodiscard]] inline auto CreateInorderImmediateCommandList(ze_context_handle_t ctx,
                                                            ze_device_handle_t dev) {
  ze_command_queue_desc_t cmd_queue_desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                            nullptr,
                                            0,
                                            0,
                                            ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                            ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                            ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

  ze_command_list_handle_t clist = nullptr;
  if (zeCommandListCreateImmediate(ctx, dev, &cmd_queue_desc, &clist) != ZE_RESULT_SUCCESS) {
    throw std::runtime_error("Failed to create immediate command list");
  }

  return ZeCommandList{clist};
}

[[nodiscard]] inline auto CreateDotProductLists(ze_context_handle_t ctx, ze_device_handle_t dev) {
  return std::make_tuple(CreateInorderImmediateCommandList(ctx, dev),
                         CreateInorderImmediateCommandList(ctx, dev),
                         CreateInorderImmediateCommandList(ctx, dev));
}

[[nodiscard]] inline auto CreateKernel(ze_module_handle_t module, const char* name) {
  const ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, name};
  ze_kernel_handle_t kernel = nullptr;
  if (zeKernelCreate(module, &kernel_desc, &kernel) != ZE_RESULT_SUCCESS) {
    return ZeKernel{nullptr};
  }

  return ZeKernel{kernel};
}

template <std::size_t ArgN>
[[nodiscard]] inline auto MakeKernelBundle(ze_module_handle_t module, const char* kernel_name,
                                           ze_group_size_t group_size,
                                           ze_group_count_t group_count) {
  ZeKernel kernel = CreateKernel(module, kernel_name);
  if (!kernel) {
    throw std::runtime_error("Failed to create kernel: " + std::string(kernel_name));
  }

  KernelBundle<ArgN> bundle;
  bundle.kernel = std::move(kernel);
  bundle.group_size = group_size;
  bundle.group_count = group_count;
  return bundle;
}

template <std::size_t ArgN>
[[nodiscard]] inline auto MakeVectorKernelBundle(ze_module_handle_t module, const char* kernel_name,
                                                 std::uint32_t vector_size) {
  constexpr static auto kVectorYDimension = 1U;
  constexpr static auto kVectorZDimension = 1U;

  auto bundle = MakeKernelBundle<ArgN>(module, kernel_name, {}, {});

  ze_group_size_t group_size{};
  ze_group_count_t group_count{};
  if (zeKernelSuggestGroupSize(bundle.kernel.get(), vector_size, kVectorYDimension,
                               kVectorZDimension, &group_size.groupSizeX, &group_size.groupSizeY,
                               &group_size.groupSizeZ) != ZE_RESULT_SUCCESS) {
    throw std::runtime_error("Failed to suggest group size for kernel");
  }

  if ((vector_size % group_size.groupSizeX) != 0) {
    throw std::runtime_error("Non-uniform group size suggested by driver");
  }

  group_count.groupCountX = vector_size / group_size.groupSizeX;
  group_count.groupCountY = kVectorYDimension;
  group_count.groupCountZ = kVectorZDimension;

  bundle.group_size = group_size;
  bundle.group_count = group_count;

  return bundle;
}

using DotProductLists = std::tuple<ZeCommandList, ZeCommandList, ZeCommandList>;

using DotProductKernels =
    std::tuple<KernelBundle<DotProductWorkload<float>::InitDotProductCl::kArgN>,
               KernelBundle<DotProductWorkload<float>::VecAddAlphaCl::kArgN>,
               KernelBundle<DotProductWorkload<float>::VecAddGammaCl::kArgN>,
               KernelBundle<DotProductWorkload<float>::CalculateDotProductCl::kArgN>>;

using DotProductVectors =
    std::tuple<pti::test::utils::ZeMemVector<float>, pti::test::utils::ZeMemVector<float>,
               pti::test::utils::ZeMemVector<float>, pti::test::utils::ZeMemVector<float>>;

[[nodiscard]] inline DotProductKernels CreateDotProductKernels(ze_module_handle_t module) {
  using Workload = DotProductWorkload<float>;

  auto init = MakeVectorKernelBundle<Workload::InitDotProductCl::kArgN>(
      module, Workload::InitDotProductCl::kKernelName, Workload::kDefaultVectorSize);
  auto vec_add_alpha = MakeVectorKernelBundle<Workload::VecAddAlphaCl::kArgN>(
      module, Workload::VecAddAlphaCl::kKernelName, Workload::kDefaultVectorSize);
  auto vec_add_gamma = MakeVectorKernelBundle<Workload::VecAddGammaCl::kArgN>(
      module, Workload::VecAddGammaCl::kKernelName, Workload::kDefaultVectorSize);
  auto calculate_dot_product = MakeVectorKernelBundle<Workload::CalculateDotProductCl::kArgN>(
      module, Workload::CalculateDotProductCl::kKernelName, 1U);

  return std::make_tuple(std::move(init), std::move(vec_add_alpha), std::move(vec_add_gamma),
                         std::move(calculate_dot_product));
}

template <typename T, std::size_t NumberOfWaitEvents = 0>
inline void AppendKernelBundle(ze_command_list_handle_t clist, T& bundle,
                               ze_event_handle_t signal_event,
                               std::array<ze_event_handle_t, NumberOfWaitEvents> wait_events = {}) {
  if (zeCommandListAppendLaunchKernelWithArguments(clist, bundle.kernel.get(), bundle.group_count,
                                                   bundle.group_size, bundle.args.data(), nullptr,
                                                   signal_event, std::size(wait_events),
                                                   std::data(wait_events)) != ZE_RESULT_SUCCESS) {
    throw std::runtime_error("Failed to append kernel to command list");
  }
}

inline void RecordDotProductGraph(ze_command_list_handle_t primary,
                                  ze_command_list_handle_t fork_one,
                                  ze_command_list_handle_t fork_two,
                                  ze_event_handle_t vector_init_event,
                                  ze_event_handle_t vec_add_event, ze_event_handle_t vec_add2_event,
                                  DotProductKernels& kernels, DotProductVectors& vectors,
                                  std::uint32_t* vector_size) {
  auto& [init_kernel, vec_add_alpha_kernel, vec_add_gamma_kernel, calculate_dot_product_kernel] =
      kernels;
  auto& [dot_product_result, a_vec, b_vec, c_vec] = vectors;
  auto* dot_product_result_ptr = dot_product_result.get();
  auto* a_vec_ptr = a_vec.get();
  auto* b_vec_ptr = b_vec.get();
  auto* c_vec_ptr = c_vec.get();

  init_kernel.SetArgs(&a_vec_ptr, &b_vec_ptr, &c_vec_ptr);
  vec_add_alpha_kernel.SetArgs(&a_vec_ptr, &b_vec_ptr);
  vec_add_gamma_kernel.SetArgs(&c_vec_ptr, &b_vec_ptr);
  calculate_dot_product_kernel.SetArgs(&a_vec_ptr, &c_vec_ptr, &dot_product_result_ptr,
                                       vector_size);
  AppendKernelBundle(primary, init_kernel, vector_init_event);
  AppendKernelBundle(fork_one, vec_add_alpha_kernel, vec_add_event, std::array{vector_init_event});
  AppendKernelBundle(fork_two, vec_add_gamma_kernel, vec_add2_event, std::array{vector_init_event});
  AppendKernelBundle(primary, calculate_dot_product_kernel, nullptr,
                     std::array{vec_add_event, vec_add2_event});
}

#endif  // TEST_GRAPH_ZE_GRAPH_WORKLOADS_H_
