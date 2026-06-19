//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file main.cc
 * @brief PC Sampling sample demonstrating GPU EU Stall Sampling workflow.
 *
 * This sample demonstrates the complete PC sampling workflow:
 *   1. Initialize environment and enable PTI kernel view
 *   2. Enable PC sampling
 *   3. Configure PC sampling with devices and sampling period
 *   4. Start PC sampling collection
 *   5. Run compute workloads (two GEMM kernels)
 *   6. Stop PC sampling collection
 *   7. Print collected stall data
 *   8. Disable PC sampling
 *   9. Cleanup environment
 *
 * The PC sampling API calls are separated into pc_sampling_client.h/pc_sampling_client.cc
 * to demonstrate how users can integrate metric collection into their applications.
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sycl/sycl.hpp>
#include <vector>

#include "pc_sampling_client.h"

//-----------------------------------------------------------------------------
// Configuration Constants
//-----------------------------------------------------------------------------

constexpr float kAValue = 0.128f;
constexpr float kBValue = 0.256f;
constexpr float kMaxEps = 1.0e-4f;
constexpr unsigned kDefaultMatrixSize = 1024;
constexpr unsigned kMaxMatrixSize = 8192;
constexpr unsigned kMinMatrixSize = 32;
constexpr unsigned kDefaultRepeatCount = 1;
constexpr unsigned kMaxRepeatCount = 100;
constexpr uint32_t kDefaultSamplingPeriodNs = 50'000;  // 50 µs sampling period
constexpr uint32_t kMinSamplingPeriodNs = 1'000;       // 1 µs minimum
constexpr uint32_t kMaxSamplingPeriodNs = 10'000'000;  // 10 ms maximum

class PrimaryGEMMKernel;
class SecondaryGEMMKernel;

//-----------------------------------------------------------------------------
// GEMM Kernel Implementation
//-----------------------------------------------------------------------------

/**
 * @brief Matrix multiplication kernel function.
 */
void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

/**
 * @brief Verify matrix multiplication results.
 */
static float CheckResults(const std::vector<float>& c, float expected_value) {
  float eps = 0.0f;
  for (size_t i = 0; i < c.size(); ++i) {
    eps += std::fabs((c[i] - expected_value) / expected_value);
  }
  return eps / c.size();
}

/**
 * @brief Run GEMM kernel and verify results.
 */
template <typename KernelName>
static bool RunGEMM(sycl::queue& queue, const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& c, unsigned size, float expected_result) {
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    queue.submit([&](sycl::handler& cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
      auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
      auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

      cgh.parallel_for<KernelName>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
        auto a_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto b_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto c_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
        GEMM(a_ptr.get(), b_ptr.get(), c_ptr.get(), size, id);
      });
    });
    queue.wait_and_throw();

  } catch (const sycl::exception& e) {
    std::cerr << "SYCL Exception: " << e.what() << std::endl;
    return false;
  }

  float eps = CheckResults(c, expected_result);
  bool passed = (eps < kMaxEps);
  std::cout << "GEMM results are " << (passed ? "CORRECT" : "INCORRECT") << " (accuracy: " << eps
            << ")" << std::endl;
  return passed;
}

/**
 * @brief Run GEMM kernel multiple times.
 */
template <typename KernelName>
static bool RunGEMMMultiple(sycl::queue& queue, const std::vector<float>& a,
                            const std::vector<float>& b, std::vector<float>& c, unsigned size,
                            unsigned repeat_count, float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    std::cout << "  Iteration " << (i + 1) << "/" << repeat_count << ": ";
    if (!RunGEMM<KernelName>(queue, a, b, c, size, expected_result)) {
      std::cerr << "GEMM execution failed at iteration " << (i + 1) << std::endl;
      return false;
    }
  }

  return true;
}

template <typename KernelName>
static bool RunGEMMWorkload(sycl::queue& queue, unsigned size, unsigned repeat_count,
                            const char* workload_name) {
  std::cout << workload_name << " kernel " << repeat_count << " time(s) (matrix size: " << size
            << "x" << size << ")..." << std::endl;

  std::vector<float> a(size * size, kAValue);
  std::vector<float> b(size * size, kBValue);
  std::vector<float> c(size * size, 0.0f);
  float expected_result = kAValue * kBValue * size;

  return RunGEMMMultiple<KernelName>(queue, a, b, c, size, repeat_count, expected_result);
}

//-----------------------------------------------------------------------------
// Environment Setup
//-----------------------------------------------------------------------------

/**
 * @brief Set up environment variables required for PC sampling.
 *
 * This function sets ZET_ENABLE_METRICS=1 which is required for
 * Level Zero metrics to be available.
 */
void setPcSamplingEnvironment() {
#ifdef _WIN32
  _putenv_s("ZET_ENABLE_METRICS", "1");
#else
  setenv("ZET_ENABLE_METRICS", "1", 0);  // 0 = don't overwrite if already set
#endif
  std::cout << "Set ZET_ENABLE_METRICS=1" << std::endl;
}

//-----------------------------------------------------------------------------
// Command Line Parsing
//-----------------------------------------------------------------------------

struct SampleConfig {
  unsigned matrix_size;
  unsigned repeat_count;
  uint32_t sampling_period_ns;
};

void Usage(const char* name) {
  std::cout << "Usage: " << name << " [options]" << std::endl;
  std::cout << "\nOptions:" << std::endl;
  std::cout << "  --size [-s]     <size>     Matrix size (default: " << kDefaultMatrixSize
            << ", range: " << kMinMatrixSize << "-" << kMaxMatrixSize << ")" << std::endl;
  std::cout << "  --repeat [-r]   <count>    Number of times to run each GEMM kernel (default: "
            << kDefaultRepeatCount << ", max: " << kMaxRepeatCount << ")" << std::endl;
  std::cout << "  --period [-p]   <ns>       Sampling period in nanoseconds (default: "
            << kDefaultSamplingPeriodNs << ", range: " << kMinSamplingPeriodNs << "-"
            << kMaxSamplingPeriodNs << ")" << std::endl;
  std::cout << "  --help [-h]                Display this help message" << std::endl;
  std::cout << "\nExample:" << std::endl;
  std::cout << "  " << name << " --size 2048 --repeat 5 --period 100000" << std::endl;
  std::cout << "  " << name << " -s 512 -r 3 -p 25000" << std::endl;
}

/**
 * @brief Parse command line arguments and populate configuration.
 * @return true on success, false on error or help request
 */
bool ParseCommandLine(int argc, char* argv[], SampleConfig& config) {
  // Set defaults
  config.matrix_size = kDefaultMatrixSize;
  config.repeat_count = kDefaultRepeatCount;
  config.sampling_period_ns = kDefaultSamplingPeriodNs;

  try {
    for (int i = 1; i < argc; ++i) {
      if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
        Usage(argv[0]);
        return false;
      } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
        if (++i >= argc) {
          std::cerr << "Error: --size requires an argument" << std::endl;
          Usage(argv[0]);
          return false;
        }
        unsigned temp = std::stoul(argv[i]);
        config.matrix_size = std::clamp(temp, kMinMatrixSize, kMaxMatrixSize);
        if (temp != config.matrix_size) {
          std::cout << "Note: Matrix size clamped to valid range [" << kMinMatrixSize << ", "
                    << kMaxMatrixSize << "]" << std::endl;
        }
      } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repeat") == 0) {
        if (++i >= argc) {
          std::cerr << "Error: --repeat requires an argument" << std::endl;
          Usage(argv[0]);
          return false;
        }
        unsigned temp = std::stoul(argv[i]);
        config.repeat_count = std::min(temp, kMaxRepeatCount);
        if (temp != config.repeat_count) {
          std::cout << "Note: Repeat count clamped to maximum " << kMaxRepeatCount << std::endl;
        }
      } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--period") == 0) {
        if (++i >= argc) {
          std::cerr << "Error: --period requires an argument" << std::endl;
          Usage(argv[0]);
          return false;
        }
        uint32_t temp = std::stoul(argv[i]);
        config.sampling_period_ns = std::clamp(temp, kMinSamplingPeriodNs, kMaxSamplingPeriodNs);
        if (temp != config.sampling_period_ns) {
          std::cout << "Note: Sampling period clamped to valid range [" << kMinSamplingPeriodNs
                    << ", " << kMaxSamplingPeriodNs << "] ns" << std::endl;
        }
      } else {
        std::cerr << "Error: Unknown option '" << argv[i] << "'" << std::endl;
        Usage(argv[0]);
        return false;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: Failed to parse command line arguments: " << e.what() << std::endl;
    Usage(argv[0]);
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Main Entry Point
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  // Set environment variables required for PC sampling
  setPcSamplingEnvironment();

  // Parse command line arguments
  SampleConfig config;
  if (!ParseCommandLine(argc, argv, config)) {
    return EXIT_SUCCESS;  // Help was shown or parsing failed
  }

  std::cout << "\n========================================" << std::endl;
  std::cout << "PC Sampling Sample" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Configuration:" << std::endl;
  std::cout << "  Matrix size: " << config.matrix_size << "x" << config.matrix_size << std::endl;
  std::cout << "  Repeat count per kernel: " << config.repeat_count << std::endl;
  std::cout << "  Sampling period: " << config.sampling_period_ns << " ns ("
            << (config.sampling_period_ns / 1000.0f) << " µs)" << std::endl;

  // Select GPU device
  sycl::device device;
  try {
    device = sycl::device(sycl::gpu_selector_v);
  } catch (const sycl::exception& e) {
    std::cerr << "Failed to select GPU device: " << e.what() << std::endl;
    std::cerr << "PC Sampling requires a GPU device" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Target device: " << device.get_info<sycl::info::device::name>() << std::endl;

  // Step 1: Initialize profiling environment (PTI view setup)
  if (!InitializeProfilingEnvironment()) {
    std::cerr << "Failed to initialize profiling environment" << std::endl;
    return EXIT_FAILURE;
  }

  // Step 2: Enable PC sampling (ptiPcSamplingEnable)
  if (!EnablePcSampling()) {
    std::cerr << "Failed to enable PC sampling" << std::endl;
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 3: Configure PC sampling (ptiPcSamplingConfigure)
  // Pass nullptr and 0 to profile all available devices.
  // Users can pass specific device handles to profile a subset:
  //   pti_device_handle_t devices[] = {device_handle_1, device_handle_2};
  //   ConfigurePcSampling(devices, 2, config.sampling_period_ns);
  if (!ConfigurePcSampling(nullptr, 0, config.sampling_period_ns)) {
    std::cerr << "Failed to configure PC sampling" << std::endl;
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 4: Start PC sampling collection (ptiPcSamplingStartCollection)
  if (!StartPcSamplingCollection()) {
    std::cerr << "Failed to start PC sampling collection" << std::endl;
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 5: Run compute workloads (two GEMM kernels)
  // Run the same kernels but with different names so they will be identified as different kernels
  // in the profiling data.
  std::cout << "\n=== Running Compute Workload ===" << std::endl;
  std::cout << "Executing two GEMM kernels with matrix size " << config.matrix_size << "x"
            << config.matrix_size << std::endl;

  // Create SYCL queue
  sycl::property_list props{sycl::property::queue::in_order()};
  sycl::queue queue(device, sycl::async_handler{}, props);

  if (!RunGEMMWorkload<PrimaryGEMMKernel>(queue, config.matrix_size, config.repeat_count,
                                          "Primary GEMM")) {
    std::cerr << "Primary GEMM workload failed" << std::endl;
    StopPcSamplingCollection();
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  if (!RunGEMMWorkload<SecondaryGEMMKernel>(queue, config.matrix_size, config.repeat_count,
                                            "Secondary GEMM")) {
    std::cerr << "Secondary GEMM workload failed" << std::endl;
    StopPcSamplingCollection();
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  std::cout << "GEMM kernel executions completed" << std::endl;

  // Step 6: Stop PC sampling collection (ptiPcSamplingStopCollection)
  if (!StopPcSamplingCollection()) {
    std::cerr << "Failed to stop PC sampling collection" << std::endl;
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 7: Print collected stall data before releasing the handle
  if (!PrintPcSamplingData()) {
    std::cerr << "Failed to print stall data" << std::endl;
    DisablePcSampling();
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 8: Disable PC sampling (ptiPcSamplingDisable)
  if (!DisablePcSampling()) {
    std::cerr << "Failed to disable PC sampling" << std::endl;
    CleanupProfilingEnvironment();
    return EXIT_FAILURE;
  }

  // Step 9: Cleanup environment (PTI view cleanup)
  CleanupProfilingEnvironment();

  std::cout << "\n========================================" << std::endl;
  std::cout << "PC Sampling sample completed successfully!" << std::endl;
  std::cout << "========================================" << std::endl;

  return EXIT_SUCCESS;
}
