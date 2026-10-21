//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sycl/sycl.hpp>
#include <vector>

constexpr float kAValue = 0.128f;
constexpr float kBValue = 0.256f;
constexpr float kMaxEps = 1.0e-4f;
constexpr unsigned kDefaultMatrixSize = 1024;
constexpr unsigned kMaxMatrixSize = 8192;
constexpr unsigned kMinMatrixSize = 32;
constexpr unsigned kDefaultRepeatCount = 1;
constexpr unsigned kMaxRepeatCount = 100;

class GEMMKernel;

void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  const int row = id.get(0);
  const int column = id.get(1);
  float sum = 0.0f;
  for (unsigned index = 0; index < size; ++index) {
    sum += a[row * size + index] * b[index * size + column];
  }
  c[row * size + column] = sum;
}

float CheckResults(const std::vector<float>& c, float expected_value) {
  float epsilon = 0.0f;
  for (size_t index = 0; index < c.size(); ++index) {
    epsilon += std::fabs((c[index] - expected_value) / expected_value);
  }
  return epsilon / c.size();
}

bool RunGEMM(sycl::queue& queue, const std::vector<float>& a, const std::vector<float>& b,
             std::vector<float>& c, unsigned size, float expected_result) {
  try {
    sycl::buffer<float, 1> a_buffer(a.data(), a.size());
    sycl::buffer<float, 1> b_buffer(b.data(), b.size());
    sycl::buffer<float, 1> c_buffer(c.data(), c.size());
    queue.submit([&](sycl::handler& command_group) {
      auto a_accessor = a_buffer.get_access<sycl::access::mode::read>(command_group);
      auto b_accessor = b_buffer.get_access<sycl::access::mode::read>(command_group);
      auto c_accessor = c_buffer.get_access<sycl::access::mode::write>(command_group);
      command_group.parallel_for<GEMMKernel>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
        GEMM(a_accessor.get_multi_ptr<sycl::access::decorated::no>().get(),
             b_accessor.get_multi_ptr<sycl::access::decorated::no>().get(),
             c_accessor.get_multi_ptr<sycl::access::decorated::no>().get(), size, id);
      });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception& exception) {
    std::cerr << "SYCL exception: " << exception.what() << std::endl;
    return false;
  }

  const float epsilon = CheckResults(c, expected_result);
  const bool passed = epsilon < kMaxEps;
  std::cout << "GEMM results are " << (passed ? "CORRECT" : "INCORRECT")
            << " (accuracy: " << epsilon << ")" << std::endl;
  return passed;
}

bool RunGEMMWorkload(sycl::queue& queue, unsigned size, unsigned repeat_count) {
  std::vector<float> a(size * size, kAValue);
  std::vector<float> b(size * size, kBValue);
  std::vector<float> c(size * size, 0.0f);
  const float expected_result = kAValue * kBValue * size;

  std::cout << "GEMM kernel " << repeat_count << " time(s) (matrix size: " << size << "x" << size
            << ")..." << std::endl;
  for (unsigned iteration = 0; iteration < repeat_count; ++iteration) {
    std::cout << "  Iteration " << (iteration + 1) << "/" << repeat_count << ": ";
    if (!RunGEMM(queue, a, b, c, size, expected_result)) {
      return false;
    }
  }
  return true;
}

struct GemmConfig {
  unsigned matrix_size = kDefaultMatrixSize;
  unsigned repeat_count = kDefaultRepeatCount;
};

void Usage(const char* name) {
  std::cout << "Usage: " << name << " [options]" << std::endl;
  std::cout << "  --size [-s] <size>     Matrix size (default: " << kDefaultMatrixSize
            << ", range: " << kMinMatrixSize << "-" << kMaxMatrixSize << ")" << std::endl;
  std::cout << "  --repeat [-r] <count>  Number of kernel executions (default: "
            << kDefaultRepeatCount << ", max: " << kMaxRepeatCount << ")" << std::endl;
}

enum class ParseResult { kRun, kHelp, kError };

ParseResult ParseCommandLine(int argc, char* argv[], GemmConfig& config) {
  try {
    for (int index = 1; index < argc; ++index) {
      if (std::strcmp(argv[index], "-h") == 0 || std::strcmp(argv[index], "--help") == 0) {
        Usage(argv[0]);
        return ParseResult::kHelp;
      }
      if (std::strcmp(argv[index], "-s") == 0 || std::strcmp(argv[index], "--size") == 0) {
        if (++index >= argc || argv[index][0] == '-') {
          throw std::invalid_argument("--size requires a positive argument");
        }
        config.matrix_size = std::clamp(static_cast<unsigned>(std::stoul(argv[index])),
                                        kMinMatrixSize, kMaxMatrixSize);
        continue;
      }
      if (std::strcmp(argv[index], "-r") == 0 || std::strcmp(argv[index], "--repeat") == 0) {
        if (++index >= argc || argv[index][0] == '-') {
          throw std::invalid_argument("--repeat requires a positive argument");
        }
        config.repeat_count =
            std::min(static_cast<unsigned>(std::stoul(argv[index])), kMaxRepeatCount);
        continue;
      }
      throw std::invalid_argument(std::string("unknown option '") + argv[index] + "'");
    }
  } catch (const std::exception& exception) {
    std::cerr << "Error: " << exception.what() << std::endl;
    Usage(argv[0]);
    return ParseResult::kError;
  }
  return ParseResult::kRun;
}

int main(int argc, char* argv[]) {
  GemmConfig config;
  switch (ParseCommandLine(argc, argv, config)) {
    case ParseResult::kHelp:
      return EXIT_SUCCESS;
    case ParseResult::kError:
      return EXIT_FAILURE;
    case ParseResult::kRun:
      break;
  }

  try {
    sycl::queue queue(sycl::gpu_selector_v, sycl::async_handler{},
                      sycl::property_list{sycl::property::queue::in_order()});
    std::cout << "Target device: " << queue.get_device().get_info<sycl::info::device::name>()
              << std::endl;
    return RunGEMMWorkload(queue, config.matrix_size, config.repeat_count) ? EXIT_SUCCESS
                                                                           : EXIT_FAILURE;
  } catch (const sycl::exception& exception) {
    std::cerr << "Failed to select GPU device: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }
}
