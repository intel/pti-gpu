//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <string.h>
#include <cstdlib>
#include <memory>
#include <sycl/sycl.hpp>
#include <ittnotify.h>


#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

// Collection-control mechanism. By default the test drives unitrace through the
// ITT API (__itt_resume/__itt_pause/__itt_detach). With the "env" command-line
// option it instead writes the PTI_ENABLE_COLLECTION environment variable, the
// env-based form of the same in-application control:
//   resume -> "1"   pause -> "0"   stop/detach -> "-1"
// Both produce the same captured kernel count, so they share the -d gold file.
static bool g_use_env = false;

static void SetCollectionEnv(const char* value) {
#if defined(_WIN32)
  _putenv_s("PTI_ENABLE_COLLECTION", value);
#else
  setenv("PTI_ENABLE_COLLECTION", value, 1);
#endif
}

static void CollectionResume() {
  if (g_use_env) { SetCollectionEnv("1"); } else { __itt_resume(); }
}

static void CollectionPause() {
  if (g_use_env) { SetCollectionEnv("0"); } else { __itt_pause(); }
}

static void CollectionStop() {
  if (g_use_env) { SetCollectionEnv("-1"); } else { __itt_detach(); }
}

static float Check(const std::vector<float>& a, float value) {
  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::abs((a[i] - value) / value);
  }

  return eps / a.size();
}

void GEMM(const float* a, const float* b, float* c,
          unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static float RunAndCheck(sycl::queue queue,
                         const std::vector<float>& a,
                         const std::vector<float>& b,
                         std::vector<float>& c,
                         unsigned size,
                         float expected_result) {
  double time = 0.0;

   {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    sycl::event event = queue.submit([&](sycl::handler& cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
      auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
      auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

      cgh.parallel_for<class __GEMM>(sycl::range<2>(size, size),
                      [=](sycl::id<2> id) {
                        auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
                        auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
                        auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
                        GEMM(a_acc_ptr.get(),
                             b_acc_ptr.get(),
                             c_acc_ptr.get(),
                             size, id);
                      });
    });
    queue.wait_and_throw();

    auto start =
      event.get_profiling_info<sycl::info::event_profiling::command_start>();
    auto end =
      event.get_profiling_info<sycl::info::event_profiling::command_end>();
    time = static_cast<double>(end - start);
  } /*catch (sycl::exception e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
  }*/

  std::cout << "Matrix multiplication time: " << time <<
    " ns" << std::endl;

  return Check(c, expected_result);
}

static void Compute(sycl::queue queue,
                    const std::vector<float>& a,
                    const std::vector<float>& b,
                    std::vector<float>& c,
                    unsigned size,
                    unsigned repeat_count,
                    float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
  CollectionResume();
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
  CollectionPause();
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
  CollectionResume();
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
  CollectionStop();
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
}

int main(int argc, char* argv[]) {
  // Scan for the "env" option anywhere on the command line: it selects the
  // PTI_ENABLE_COLLECTION env-var control path instead of the ITT API. Scanning
  // (rather than consuming a fixed position) keeps the existing positional
  // device/size/repeat arguments intact.
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "env") == 0) {
      g_use_env = true;
    }
  }
  std::cout << "Collection control: " << (g_use_env ? "PTI_ENABLE_COLLECTION env"
                                                    : "ITT API") << std::endl;

  sycl::device dev;
  {
    dev = sycl::device(sycl::gpu_selector_v);
    if (argc > 1 && strcmp(argv[1], "cpu") == 0) {
      dev = sycl::device(sycl::cpu_selector_v);
    } else if (argc > 1 && strcmp(argv[1], "host") == 0) {
      dev = sycl::device(sycl::default_selector_v);
    }
  }

  unsigned size = 1024;
  if (argc > 2) {
    size = std::stoul(argv[2]);
  }

  unsigned repeat_count = 4;
  if (argc > 3) {
    repeat_count = std::stoul(argv[3]);
  }

  sycl::property_list prop_list{sycl::property::queue::enable_profiling()};
  sycl::queue queue(dev, sycl::async_handler{}, prop_list);

  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size <<
    " x " << size << ", repeats " << repeat_count << " times)" << std::endl;
  std::cout << "Target device: " <<
    queue.get_info<sycl::info::queue::device>().get_info<
        sycl::info::device::name>() << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  auto start = std::chrono::steady_clock::now();
  float expected_result = A_VALUE * B_VALUE * size;
  Compute(queue, a, b, c, size, repeat_count, expected_result);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;

  std::cout << "Total execution time: " << time.count() << " sec" << std::endl;
  return 0;
}
