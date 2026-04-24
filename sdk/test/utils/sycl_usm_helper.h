//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_UTILS_SYCL_USM_HELPER_H_
#define TEST_UTILS_SYCL_USM_HELPER_H_

#include <cstddef>
#include <memory>
#include <sycl/sycl.hpp>

namespace pti::test::utils {

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

template <typename T>
SyclUsmVector<T> CreateHostUsmVector(sycl::queue& queue, std::size_t vector_size) {
  return SyclUsmVector<T>(sycl::malloc_host<T>(vector_size, queue), SyclUsmFree{queue});
}

}  // namespace pti::test::utils
#endif  // TEST_UTILS_SYCL_USM_HELPER_H_
