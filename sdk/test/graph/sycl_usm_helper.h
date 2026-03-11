//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_SYCL_USM_HELPER_H_
#define TEST_GRAPH_SYCL_USM_HELPER_H_

#include <cstddef>
#include <memory>
#include <sycl/sycl.hpp>

struct SyclUsmFree {
  sycl::queue queue_;
  void operator()(void* ptr) const { sycl::free(ptr, queue_); }
};

template <typename T>
using SyclUsmVector = std::unique_ptr<T, SyclUsmFree>;

template <typename T>
SyclUsmVector<T> CreateDeviceUsmVector(sycl::queue& queue, std::size_t vector_size) {
  return SyclUsmVector<T>(sycl::malloc_device<T>(vector_size, queue), SyclUsmFree{queue});
}

template <typename T>
SyclUsmVector<T> CreateSharedUsmVector(sycl::queue& queue, std::size_t vector_size) {
  return SyclUsmVector<T>(sycl::malloc_shared<T>(vector_size, queue), SyclUsmFree{queue});
}

#endif  // TEST_GRAPH_SYCL_USM_HELPER_H_
