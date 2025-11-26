//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sycl/sycl.hpp>

#include "client.h"

#define NSEC_IN_SEC 1'000'000'000
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

constexpr unsigned kMaxSize = 8192;
constexpr unsigned kMinSize = 32;

static float Check(const std::vector<float> &a, float value) {
  assert(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

void GEMM(const float *a, const float *b, float *c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static void Run(sycl::queue queue, const std::vector<float> &a, const std::vector<float> &b,
                std::vector<float> &c, unsigned size) {
  assert(size > 0);
  assert(a.size() == size * size);
  assert(b.size() == size * size);
  assert(c.size() == size * size);

  sycl::buffer<float, 1> a_buf(a.data(), a.size());
  sycl::buffer<float, 1> b_buf(b.data(), b.size());
  sycl::buffer<float, 1> c_buf(c.data(), c.size());

  [[maybe_unused]] sycl::event event = queue.submit([&](sycl::handler &cgh) {
    auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
    auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
    auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

    cgh.parallel_for<class __GEMM>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
      auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
      auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
      auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
      GEMM(a_acc_ptr.get(), b_acc_ptr.get(), c_acc_ptr.get(), size, id);
    });
  });
}

static void Compute(sycl::queue queue, const std::vector<float> &a, const std::vector<float> &b,
                    std::vector<float> &c, unsigned size, unsigned repeat_count,
                    float expected_result) {
  try {
    for (unsigned i = 0; i < repeat_count; ++i) {
      Run(queue, a, b, c, size);
    }
    queue.wait_and_throw();
    float eps = Check(c, expected_result);
    std::cout << "Matrix multiplication done. Checking result.." << std::endl;
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") << "CORRECT with accuracy: " << eps
              << std::endl;
  } catch (const sycl::exception &e) {
    std::cerr << "[ERROR] Exception during Compute: " << e.what() << std::endl;
    throw;
  }
}

void Usage(const char *name) {
  std::cout << " Calculating floating point matrix multiply on gpu. Usage:\n";
  std::cout << name << " [matrix size] [repetition count]\n"
            << "\t - matrix size, default=1024, max=" << kMaxSize << "\n"
            << "\t - repetition count, default=1 \n";
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_SUCCESS;
  StartProfiling();
  unsigned repeat_count = 1;
  unsigned size = 1024;
  try {
    if (argc > 1) {
      unsigned temp = std::stoul(argv[1]);
      size = (temp < kMinSize) ? kMinSize : (temp > kMaxSize) ? kMaxSize : temp;
    }
    if (argc > 2) {
      repeat_count = std::stoul(argv[2]);
    }
  } catch (std::invalid_argument const &e) {
    std::cerr << "Error: Invalid argument when processing command line " << e.what() << '\n';
    Usage(argv[0]);
    return EXIT_FAILURE;
  } catch (std::out_of_range const &e) {
    std::cerr << "Error: Out-of-range when processing command line " << e.what() << '\n';
    Usage(argv[0]);
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  sycl::device dev(sycl::gpu_selector_v);
  sycl::queue queue(dev, sycl::async_handler{}, {sycl::property::queue::in_order()});

  // Main run and check kernel

  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size << " x " << size << ", repeats "
            << repeat_count << " times)" << std::endl;
  std::cout << "Target device: "
            << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
            << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  try {
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    Compute(queue, a, b, c, size, repeat_count, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;

  } catch (const sycl::exception &e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (const std::exception &e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    exit_code = EXIT_FAILURE;
  }

  StopProfiling();

  return exit_code;
}
