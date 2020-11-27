//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <math.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "pti_assert.h"

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

float ComputeOnCPU(const std::vector<float>& a,
                   const std::vector<float>& b,
                   std::vector<float>& c,
                   unsigned size, float expected_result) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  memset(c.data(), 0, c.size() * sizeof(float));

  #pragma omp parallel for
  for (unsigned i = 0; i < size; ++i) {
    for (unsigned k = 0; k < size; ++k) {
      for (unsigned j = 0; j < size; ++j) {
        c[i * size + j] += a[i * size + k] * b[k * size  + j];
      }
    }
  }

  return Check(c, expected_result);
}

float ComputeOnGPU(const std::vector<float>& a,
                   const std::vector<float>& b,
                   std::vector<float>& c,
                   unsigned size, float expected_result) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  memset(c.data(), 0, c.size() * sizeof(float));

  const float* a_data = a.data();
  const float* b_data = b.data();
  float* c_data = c.data();
  int size2 = size * size;

  #pragma omp target teams distribute parallel for collapse(2)\
    map(to: a_data[0:size2], b_data[0:size2]) map(from: c_data[0:size2])
  for (unsigned i = 0; i < size; ++i) {
    for (unsigned j = 0; j < size; ++j) {
      float sum = 0.0f;
      for (unsigned k = 0; k < size; ++k) {
        sum += a_data[i * size + k] * b_data[k * size  + j];
      }
      c_data[i * size + j] = sum;
    }
  }

  return Check(c, expected_result);
}

static void Compute(const std::vector<float>& a,
                    const std::vector<float>& b,
                    std::vector<float>& c,
                    unsigned size, unsigned repeat_count,
                    float expected_result, bool cpu) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    auto start = std::chrono::steady_clock::now();
    float eps = (cpu ? ComputeOnCPU(a, b, c, size, expected_result) :
      ComputeOnGPU(a, b, c, size, expected_result));
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;

    std::cout << "Matrix multiplication time: " << time.count() <<
      " sec" << std::endl;
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
}

int main(int argc, char* argv[]) {
  bool cpu = false;
  if (argc > 1 && strcmp(argv[1], "cpu") == 0) {
    cpu = true;
  }

  unsigned size = 1024;
  if (argc > 2) {
    size = std::stoul(argv[2]);
  }

  unsigned repeat_count = 4;
  if (argc > 3) {
    repeat_count = std::stoul(argv[3]);
  }

  std::cout << "OpenMP Matrix Multiplication (matrix size: " << size <<
    " x " << size << ", repeats " << repeat_count << " times)" << std::endl;
  std::cout << "Target device: " << (cpu ? "CPU" : "GPU") << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  auto start = std::chrono::steady_clock::now();
  float expected_result = A_VALUE * B_VALUE * size;
  Compute(a, b, c, size, repeat_count, expected_result, cpu);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;

  std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
  return 0;
}