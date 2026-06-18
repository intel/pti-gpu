//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file main_metrics_scope.cc
 * @brief Example usage of PTI Metrics Scopes library
 *
 * Demonstrates two modes of metrics scope collection:
 *   - Auto-detect (default, or `--devices=auto`): a profiler is lazily created
 *     for each level_zero GPU the workload actually uses.
 *   - Explicit (`--devices=0,2,3`): only the listed device indices are profiled.
 *
 * By default the GEMM workload runs on the same devices being profiled.
 * `--workload`/`-w` decouples the two so the workload can target a different
 * GPU subset (e.g. profile auto, run only on GPU 1) -- used by the
 * multi-process test driver to exercise auto-detect lazy construction.
 *
 * Wall-clock markers and a GPU count line are emitted on stdout (prefixed
 * with [PTI_MP]) so external test drivers can verify collection windows
 * overlap across processes.
 */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "client.h"

#define NSEC_IN_SEC 1'000'000'000
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

static uint64_t WallClockNs() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

static float Check(const std::vector<float> &a, float value) {
  assert(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

// GEMM kernel function
void GEMM(const float *a, const float *b, float *c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static float RunAndCheck(sycl::queue &queue, const std::vector<float> &a,
                         const std::vector<float> &b, std::vector<float> &c, unsigned size,
                         float expected_result) {
  assert(size > 0);
  assert(a.size() == size * size);
  assert(b.size() == size * size);
  assert(c.size() == size * size);

  try {
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
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }

  std::cout << "Matrix multiplication done. Checking result.." << std::endl;

  return Check(c, expected_result);
}

static void Compute(sycl::queue &queue, const std::vector<float> &a, const std::vector<float> &b,
                    std::vector<float> &c, unsigned size, unsigned repeat_count,
                    float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") << "CORRECT with accuracy: " << eps
              << std::endl;
  }
}

const unsigned max_size = 8192;
const unsigned min_size = 32;

void Usage(const char *name) {
  std::cout << " Calculating floating point matrix multiply on selected GPU(s).\n"
            << "  Usage " << name << "  [ options ]\n";
  std::cout
      << "--devices  [-d]  string         "
      << "Comma-separated indices (e.g. 0,2,3) or \"auto\" to profile GPU(s) the workload uses. "
         "Default: auto\n";
  std::cout << "--workload [-w]  string         "
            << "Comma-separated indices or \"auto\". Selects which GPU(s) the GEMM runs on. "
               "Default: matches --devices\n";
  std::cout << "--size     [-s]  integer        "
            << "Matrix size. Default: 1024 (max " << max_size << ")\n";
  std::cout << "--repeat   [-r]  integer        "
            << "Repetition count per device. Default: 1\n";
  std::cout << "--help     [-h]                 Print this help message.\n";
}

static std::vector<sycl::device> EnumerateLevelZeroGpuDevices() {
  std::vector<sycl::device> all_devices;
  for (auto &platform : sycl::platform::get_platforms()) {
    if (platform.get_backend() != sycl::backend::ext_oneapi_level_zero) {
      continue;
    }
    for (auto &dev : platform.get_devices(sycl::info::device_type::gpu)) {
      all_devices.push_back(dev);
    }
  }
  return all_devices;
}

// Map a sycl::device on the level_zero backend to its native L0 handle, which
// equals the pti_device_handle_t the metrics scope API expects.
static pti_device_handle_t GetPtiHandleFromSyclDevice(const sycl::device &dev) {
  auto native = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(dev);
  return static_cast<pti_device_handle_t>(native);
}

namespace {

// Result of parsing a comma-separated index list. Success when `error` is empty.
struct ParsedDeviceIndices {
  std::vector<unsigned> indices;
  std::string error;
};

// Parse a comma-separated list of indices into a sorted, deduped vector.
static ParsedDeviceIndices ParseDeviceIndices(const std::string &csv, size_t num_available) {
  ParsedDeviceIndices result;
  std::set<unsigned> unique_indices;
  size_t pos = 0;
  while (pos <= csv.size()) {
    size_t comma = csv.find(',', pos);
    std::string token =
        csv.substr(pos, (comma == std::string::npos) ? std::string::npos : comma - pos);
    if (token.empty()) {
      result.error = "empty device index in --devices=" + csv;
      return result;
    }
    try {
      size_t consumed = 0;
      unsigned long parsed = std::stoul(token, &consumed);
      if (consumed != token.size()) {
        result.error = "non-numeric device index '" + token + "' in --devices=" + csv;
        return result;
      }
      if (parsed >= num_available) {
        result.error = "device index " + token + " out of range (have " +
                       std::to_string(num_available) + " device(s), valid: 0.." +
                       std::to_string(num_available - 1) + ")";
        return result;
      }
      unique_indices.insert(static_cast<unsigned>(parsed));
    } catch (const std::exception &) {
      result.error = "could not parse device index '" + token + "' in --devices=" + csv;
      return result;
    }
    if (comma == std::string::npos) {
      break;
    }
    pos = comma + 1;
  }
  result.indices.assign(unique_indices.begin(), unique_indices.end());
  return result;
}

}  // namespace

int main(int argc, char *argv[]) {
  std::cout << "PTI Metrics Scope Example\n";

#ifndef _WIN32
  setenv("ZET_ENABLE_METRICS", "1", 0);  // Don't overwrite if already set
#else
  _putenv_s("ZET_ENABLE_METRICS", "1");
#endif

  unsigned size = 1024;
  unsigned repeat_count = 1;
  std::string devices_arg = "auto";
  std::string workload_arg;  // empty = inherit from devices_arg

  try {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
        Usage(argv[0]);
        return EXIT_SUCCESS;
      } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        size = (temp < min_size) ? min_size : (temp > max_size) ? max_size : temp;
      } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repeat") == 0) {
        i++;
        repeat_count = std::stoul(argv[i]);
      } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--devices") == 0) {
        i++;
        devices_arg = argv[i];
      } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--workload") == 0) {
        i++;
        workload_arg = argv[i];
      } else {
        std::cerr << "Error: Unknown option: " << argv[i] << '\n';
        Usage(argv[0]);
        return EXIT_FAILURE;
      }
    }
  } catch (...) {
    std::cerr << "Error: Failed to parse command line arguments\n";
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (workload_arg.empty()) {
    workload_arg = devices_arg;
  }

  // Enumerate available L0 GPUs first so we can validate --devices indices.
  std::vector<sycl::device> all_sycl_devices = EnumerateLevelZeroGpuDevices();
  if (all_sycl_devices.empty()) {
    std::cerr << "Error: No level_zero GPU devices found.\n";
    return EXIT_FAILURE;
  }

  // Resolve --devices into pti_handles passed to StartProfiling.
  std::vector<pti_device_handle_t> pti_handles;
  const bool profile_auto = (devices_arg == "auto");
  if (!profile_auto) {
    ParsedDeviceIndices parsed = ParseDeviceIndices(devices_arg, all_sycl_devices.size());
    if (!parsed.error.empty()) {
      std::cerr << "Error: " << parsed.error << '\n';
      Usage(argv[0]);
      return EXIT_FAILURE;
    }
    pti_handles.reserve(parsed.indices.size());
    for (unsigned idx : parsed.indices) {
      pti_handles.push_back(GetPtiHandleFromSyclDevice(all_sycl_devices[idx]));
    }
  }

  // Resolve --workload into the SYCL device set the GEMM loop runs on.
  std::vector<sycl::device> workload_sycl_devices;
  const bool workload_auto = (workload_arg == "auto");
  if (workload_auto) {
    workload_sycl_devices = all_sycl_devices;
  } else {
    ParsedDeviceIndices parsed = ParseDeviceIndices(workload_arg, all_sycl_devices.size());
    if (!parsed.error.empty()) {
      std::cerr << "Error: " << parsed.error << '\n';
      Usage(argv[0]);
      return EXIT_FAILURE;
    }
    workload_sycl_devices.reserve(parsed.indices.size());
    for (unsigned idx : parsed.indices) {
      workload_sycl_devices.push_back(all_sycl_devices[idx]);
    }
  }

  std::cout << "[PTI_MP] gpu_count=" << all_sycl_devices.size() << '\n';
  for (size_t i = 0; i < workload_sycl_devices.size(); ++i) {
    std::cout << "[PTI_MP] workload_device_handle[" << i
              << "]=" << GetPtiHandleFromSyclDevice(workload_sycl_devices[i]) << '\n';
  }
  std::cout << "Discovered " << all_sycl_devices.size() << " GPU device(s):\n";
  for (size_t i = 0; i < all_sycl_devices.size(); ++i) {
    std::cout << "  [" << i << "] " << all_sycl_devices[i].get_info<sycl::info::device::name>()
              << "\n";
  }
  std::cout << (profile_auto ? "Auto-detect mode: profiling all available devices.\n"
                             : "Explicit mode: profiling " + std::to_string(pti_handles.size()) +
                                   " selected device(s).\n");
  std::cout << (workload_auto ? "Workload: GEMM on every available GPU.\n"
                              : "Workload: GEMM on " +
                                    std::to_string(workload_sycl_devices.size()) + " device(s).\n");

  int exit_code = EXIT_SUCCESS;
  StartProfiling(pti_handles);

  uint64_t collection_start_wall_ns = WallClockNs();
  std::cout << "[PTI_MP] collection_window_start_ns=" << collection_start_wall_ns << '\n';
  std::cout.flush();

  std::cout << "Running GEMM workload on selected GPU(s)...\n";
  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size << " x " << size << ", repeats "
            << repeat_count << " times per device)\n";

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  sycl::property_list prop_list{sycl::property::queue::in_order()};

  try {
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    for (size_t i = 0; i < workload_sycl_devices.size(); ++i) {
      std::cout << "\n--- Running GEMM on device [" << i
                << "]: " << workload_sycl_devices[i].get_info<sycl::info::device::name>()
                << " ---\n";
      sycl::queue queue(workload_sycl_devices[i], sycl::async_handler{}, prop_list);
      std::fill(c.begin(), c.end(), 0.0f);
      Compute(queue, a, b, c, size, repeat_count, expected_result);
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    std::cout << "Total execution time across all devices: " << time.count() << " sec" << std::endl;

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

  std::cout << "GPU workload completed\n";

  uint64_t collection_stop_wall_ns = WallClockNs();
  std::cout << "[PTI_MP] collection_window_stop_ns=" << collection_stop_wall_ns << '\n';
  std::cout.flush();

  StopProfiling();

  return exit_code;
}
