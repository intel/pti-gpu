//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_SYCL_GRAPH_WORKLOADS_H_
#define TEST_GRAPH_SYCL_GRAPH_WORKLOADS_H_

#include <cstddef>
#include <sycl/sycl.hpp>
#include <tuple>

#include "graph_dotproduct_workload_info.h"
#include "sycl_graph_test_kernels.h"
#include "utils/sycl_usm_helper.h"

// The first USM vector is the result (shared memory with host), the rest are the input vectors
// (device).
template <typename T>
[[nodiscard]] inline auto CreateUsmDotProductVectors(sycl::queue& queue, size_t n) {
  return std::make_tuple(pti::test::utils::CreateSharedUsmVector<T>(queue, 1),
                         pti::test::utils::CreateDeviceUsmVector<T>(queue, n),
                         pti::test::utils::CreateDeviceUsmVector<T>(queue, n),
                         pti::test::utils::CreateDeviceUsmVector<T>(queue, n));
}

template <typename T>
[[nodiscard]] inline auto CreateUsmDotProductGraph(sycl::queue& queue, std::size_t vector_size,
                                                   T* dot_product, T* x_vector, T* y_vector,
                                                   T* z_vector) {
  sycl::ext::oneapi::experimental::command_graph graph{queue};
  const sycl::range<1> exec_range{vector_size};

  const auto node_i = graph.add([x_vector, y_vector, z_vector, exec_range](auto& handler) {
    handler.parallel_for(exec_range, InitDotProductVectors(x_vector, y_vector, z_vector));
  });
  const auto node_a = graph.add(
      [x_vector, y_vector, exec_range](auto& handler) {
        handler.parallel_for(
            exec_range, CombineTwoVectors{x_vector, y_vector, DotProductWorkload<T>::kDefaultAlpha,
                                          DotProductWorkload<T>::kDefaultBeta});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_i)});

  const auto node_b = graph.add(
      [y_vector, z_vector, exec_range](auto& handler) {
        handler.parallel_for(
            exec_range, CombineTwoVectors{z_vector, y_vector, DotProductWorkload<T>::kDefaultGamma,
                                          DotProductWorkload<T>::kDefaultBeta});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_i)});

  graph.add(
      [dot_product, x_vector, z_vector, vector_size](auto& handler) {
        handler.single_task(CalculateDotProduct{x_vector, z_vector, dot_product, vector_size});
      },
      {sycl::ext::oneapi::experimental::property::node::depends_on(node_a, node_b)});

  return graph;
}

#endif  // TEST_GRAPH_SYCL_GRAPH_WORKLOADS_H_
