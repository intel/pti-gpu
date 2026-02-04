//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SYCL_GRAPH_TEST_KERNELS_H_
#define SYCL_GRAPH_TEST_KERNELS_H_

#include <cstddef>
#include <sycl/sycl.hpp>

inline static constexpr auto kDefaultAlpha = 1.0F;
inline static constexpr auto kDefaultBeta = 2.0F;
inline static constexpr auto kDefaultGamma = 3.0F;

template <typename T>
struct InitDotProductVectors {
  T* x_vec;
  T* y_vec;
  T* z_vec;
  T alpha = static_cast<T>(kDefaultAlpha);
  T beta = static_cast<T>(kDefaultBeta);
  T gamma = static_cast<T>(kDefaultGamma);

  InitDotProductVectors(T* x_vec, T* y_vec, T* z_vec) : x_vec(x_vec), y_vec(y_vec), z_vec(z_vec) {}

  void operator()(sycl::id<1> it) const {  // NOLINT
    const size_t index = it[0];
    x_vec[index] = alpha;  // NOLINT
    y_vec[index] = beta;   // NOLINT
    z_vec[index] = gamma;  // NOLINT
  }
};

template <typename T>
struct CombineTwoVectors {
  T* a_vec;
  T* b_vec;
  T alpha;
  T beta;

  CombineTwoVectors(T* a_vec, T* b_vec, T alpha, T beta)
      : a_vec(a_vec), b_vec(b_vec), alpha(alpha), beta(beta) {}

  void operator()(sycl::id<1> it) const {                           // NOLINT
    const size_t index = it[0];                                     // NOLINT
    a_vec[index] = (alpha * a_vec[index]) + (beta * b_vec[index]);  // NOLINT
  }
};

template <typename T>
struct CalculateDotProduct {
  T* a_vec;
  T* b_vec;
  T* result;
  size_t vector_size;

  CalculateDotProduct(T* a_vec, T* b_vec, T* result, size_t vector_size)
      : a_vec(a_vec), b_vec(b_vec), result(result), vector_size(vector_size) {}

  void operator()() const {
    for (size_t index = 0; index < vector_size; ++index) {
      *result += a_vec[index] * b_vec[index];  // NOLINT
    }
  }
};

template <typename T>
struct IncrementBufferKernel {
  T data_ptr;

  explicit IncrementBufferKernel(T data_ptr) : data_ptr(data_ptr) {}

  void operator()(sycl::item<1> id) const {  // NOLINT
    data_ptr[id]++;                          // NOLINT
  }
};

template <typename T, typename U>
struct AddBuffersKernel {
  T read_buffer;
  U read_write_buffer;

  AddBuffersKernel(T read_buffer, U read_write_buffer)
      : read_buffer(read_buffer), read_write_buffer(read_write_buffer) {}

  void operator()(sycl::item<1> id) const {    // NOLINT
    read_write_buffer[id] += read_buffer[id];  // NOLINT
  }
};

template <typename T, typename U>
struct SubtractBuffersKernel {
  T read_buffer;
  U read_write_buffer;

  SubtractBuffersKernel(T read_buffer, U read_write_buffer)
      : read_buffer(read_buffer), read_write_buffer(read_write_buffer) {}

  void operator()(sycl::item<1> id) const {    // NOLINT
    read_write_buffer[id] -= read_buffer[id];  // NOLINT
  }
};

template <typename T>
struct DecrementBuffersKernel {
  T first_buffer;
  T second_buffer;

  DecrementBuffersKernel(T first_buffer, T second_buffer)
      : first_buffer(first_buffer), second_buffer(second_buffer) {}

  void operator()(sycl::item<1> id) const {  // NOLINT
    first_buffer[id]--;
    second_buffer[id]--;
  }
};

#endif  // SYCL_GRAPH_TEST_KERNELS_H_
