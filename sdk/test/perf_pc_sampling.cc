//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#if defined(__linux__)
#include <sys/resource.h>
#endif

#if defined(PC_SAMPLING)
#include "pc_sampling_client.h"
#endif

namespace {

constexpr float kAValue = 0.128f;
constexpr float kBValue = 0.256f;
constexpr float kMaxEps = 1.0e-4f;
constexpr unsigned kDefaultSize = 4096;
constexpr unsigned kDefaultRepeatCount = 4;

#if defined(PC_SAMPLING)
constexpr uint32_t kPcSamplingPeriodNs = 50'000;
constexpr int kSkipReturnCode = 77;
#endif

class PcSamplingPerfGemmKernel;

using Clock = std::chrono::steady_clock;

void PrintDuration(const char* name, Clock::time_point start, Clock::time_point end) {
  const std::chrono::duration<float> elapsed = end - start;
  std::cout << "-- " << name << ": " << elapsed.count() << " sec" << '\n';
}

#if defined(__linux__)
struct CpuTimes {
  double user;
  double system;
};

double ToSeconds(const timeval& time) {
  return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_usec) / 1'000'000;
}

CpuTimes GetCpuTimes() {
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    throw std::runtime_error("Failed to read process CPU time");
  }
  return {ToSeconds(usage.ru_utime), ToSeconds(usage.ru_stime)};
}

void PrintCpuTimes(CpuTimes start, CpuTimes end, Clock::time_point wallStart,
                   Clock::time_point wallEnd) {
  const double user = end.user - start.user;
  const double system = end.system - start.system;
  const double total = user + system;
  const std::chrono::duration<double> wallTime = wallEnd - wallStart;
  const double utilization = wallTime.count() > 0.0 ? total / wallTime.count() * 100.0 : 0.0;

  std::cout << "-- Workload user CPU time: " << user << " sec" << '\n';
  std::cout << "-- Workload system CPU time: " << system << " sec" << '\n';
  std::cout << "-- Workload total CPU time: " << total << " sec" << '\n';
  std::cout << "-- Workload CPU utilization: " << utilization << " %" << '\n';
}
#endif

void Gemm(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  const int row = id.get(0);
  const int column = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[row * size + k] * b[k * size + column];
  }
  c[row * size + column] = sum;
}

bool Check(const std::vector<float>& values, float expectedValue) {
  float error = 0.0f;
  for (float value : values) {
    error += std::fabs((value - expectedValue) / expectedValue);
  }
  return error / values.size() < kMaxEps;
}

bool RunWorkload(sycl::queue& queue, unsigned size, unsigned repeatCount) {
  std::vector<float> a(size * size, kAValue);
  std::vector<float> b(size * size, kBValue);
  std::vector<float> c(size * size, 0.0f);

  try {
    {
      sycl::buffer<float, 1> aBuffer(a.data(), a.size());
      sycl::buffer<float, 1> bBuffer(b.data(), b.size());
      sycl::buffer<float, 1> cBuffer(c.data(), c.size());

      for (unsigned repeat = 0; repeat < repeatCount; ++repeat) {
        queue.submit([&](sycl::handler& commandGroupHandler) {
          auto aAccessor = aBuffer.get_access<sycl::access::mode::read>(commandGroupHandler);
          auto bAccessor = bBuffer.get_access<sycl::access::mode::read>(commandGroupHandler);
          auto cAccessor = cBuffer.get_access<sycl::access::mode::write>(commandGroupHandler);

          commandGroupHandler.parallel_for<PcSamplingPerfGemmKernel>(
              sycl::range<2>(size, size), [=](sycl::id<2> id) {
                Gemm(aAccessor.get_multi_ptr<sycl::access::decorated::no>().get(),
                     bAccessor.get_multi_ptr<sycl::access::decorated::no>().get(),
                     cAccessor.get_multi_ptr<sycl::access::decorated::no>().get(), size, id);
              });
        });
      }
      queue.wait_and_throw();
    }
  } catch (const sycl::exception& exception) {
    std::cerr << "SYCL workload failed: " << exception.what() << '\n';
    return false;
  }

  return Check(c, kAValue * kBValue * size);
}

bool ParseArguments(int argc, char* argv[], unsigned& size, unsigned& repeatCount) {
  for (int index = 1; index < argc; ++index) {
    if ((std::string(argv[index]) == "-s" || std::string(argv[index]) == "--size") &&
        ++index < argc) {
      size = std::stoul(argv[index]);
    } else if ((std::string(argv[index]) == "-r" || std::string(argv[index]) == "--repeat") &&
               ++index < argc) {
      repeatCount = std::stoul(argv[index]);
    } else {
      std::cerr << "Usage: " << argv[0] << " [-s size] [-r repeat]" << '\n';
      return false;
    }
  }
  return size > 0 && repeatCount > 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  unsigned size = kDefaultSize;
  unsigned repeatCount = kDefaultRepeatCount;
  if (!ParseArguments(argc, argv, size, repeatCount)) {
    return EXIT_FAILURE;
  }

  try {
    std::cout << std::fixed << std::setprecision(6);
    const auto benchmarkStart = Clock::now();
    sycl::queue queue(sycl::gpu_selector_v, sycl::property::queue::in_order{});
    const auto queueReady = Clock::now();
    PrintDuration("Queue creation time", benchmarkStart, queueReady);

#if defined(PC_SAMPLING)
    const auto profilingSetupStart = Clock::now();
    if (!InitializeProfilingEnvironment()) {
      std::cerr << "Failed to initialize PC Sampling" << '\n';
      return EXIT_FAILURE;
    }
    const pti_result enableStatus = EnablePcSampling();
    if (enableStatus == PTI_ERROR_PC_SAMPLING_UNSUPPORTED) {
      std::cout << "PC Sampling is unsupported on this system" << '\n';
      CleanupProfilingEnvironment();
      return kSkipReturnCode;
    }
    if (enableStatus != PTI_SUCCESS) {
      std::cerr << "Failed to enable PC Sampling" << '\n';
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    const auto device = static_cast<pti_device_handle_t>(
        sycl::get_native<sycl::backend::ext_oneapi_level_zero>(queue.get_device()));
    const pti_result configureStatus = ConfigurePcSampling(&device, 1, kPcSamplingPeriodNs);
    if (configureStatus == PTI_ERROR_PC_SAMPLING_CONFIGURATION_FAIL) {
      std::cout << "Selected GPU does not support PC Sampling" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return kSkipReturnCode;
    }
    if (configureStatus != PTI_SUCCESS) {
      std::cerr << "Failed to configure PC Sampling" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    const auto profilingConfigured = Clock::now();
    PrintDuration("PC Sampling enable and configure time", profilingSetupStart,
                  profilingConfigured);
#endif

    const auto warmUpStart = Clock::now();
    if (!RunWorkload(queue, size, 1)) {
      return EXIT_FAILURE;
    }
    const auto warmUpEnd = Clock::now();
    PrintDuration("Warm-up time", warmUpStart, warmUpEnd);

#if defined(PC_SAMPLING)
    const auto collectionStartBegin = Clock::now();
    if (!StartPcSamplingCollection()) {
      std::cerr << "Failed to start PC Sampling collection" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    const auto collectionStartEnd = Clock::now();
    PrintDuration("PC Sampling start collection time", collectionStartBegin, collectionStartEnd);
#endif

    const auto workloadStart = Clock::now();
#if defined(__linux__)
    const CpuTimes workloadCpuStart = GetCpuTimes();
#endif
    if (!RunWorkload(queue, size, repeatCount)) {
#if defined(PC_SAMPLING)
      StopPcSamplingCollection();
      DisablePcSampling();
      CleanupProfilingEnvironment();
#endif
      return EXIT_FAILURE;
    }
    const auto workloadEnd = Clock::now();
#if defined(__linux__)
    const CpuTimes workloadCpuEnd = GetCpuTimes();
#endif
    PrintDuration("Workload execution time", workloadStart, workloadEnd);
#if defined(__linux__)
    PrintCpuTimes(workloadCpuStart, workloadCpuEnd, workloadStart, workloadEnd);
#endif

#if defined(PC_SAMPLING)
    const auto collectionStopBegin = Clock::now();
    if (!StopPcSamplingCollection()) {
      std::cerr << "Failed to stop PC Sampling collection" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    const auto collectionStopEnd = Clock::now();
    PrintDuration("PC Sampling stop collection time", collectionStopBegin, collectionStopEnd);

    const auto postProcessingStart = Clock::now();
    std::ostream nullOutput(nullptr);
    const bool postProcessingSucceeded = PrintPcSamplingData(nullOutput);
    if (!postProcessingSucceeded) {
      std::cerr << "Failed to post-process PC Sampling data" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    if (!HasObservedPcSamplingKernels()) {
      std::cerr << "No kernels were profiled during PC Sampling collection" << '\n';
      DisablePcSampling();
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    const auto postProcessingEnd = Clock::now();
    PrintDuration("PC Sampling post-processing time", postProcessingStart, postProcessingEnd);

    const auto teardownStart = Clock::now();
    if (!DisablePcSampling()) {
      std::cerr << "Failed to disable PC Sampling" << '\n';
      CleanupProfilingEnvironment();
      return EXIT_FAILURE;
    }
    CleanupProfilingEnvironment();
    const auto teardownEnd = Clock::now();
    PrintDuration("PC Sampling teardown time", teardownStart, teardownEnd);
#endif

    const auto timedEnd = Clock::now();
    PrintDuration("Total execution time", benchmarkStart, timedEnd);
  } catch (const sycl::exception& exception) {
    std::cerr << "Failed to create GPU queue: " << exception.what() << '\n';
    return EXIT_FAILURE;
  } catch (const std::exception& exception) {
    std::cerr << "PC Sampling benchmark failed: " << exception.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
