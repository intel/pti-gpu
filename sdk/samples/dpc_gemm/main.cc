//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <string.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <sycl/sycl.hpp>

#include "pti/pti_view.h"
#include "samples_utils.h"

#define NSEC_IN_SEC 1'000'000'000
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

constexpr auto kRequestedRecordCount = 5'000'000ULL;
constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

void EnableIndividualDriverApis() {
  PTI_CHECK_SUCCESS(
      ptiViewEnableDriverApi(1, pti_api_group_id::PTI_API_GROUP_LEVELZERO,
                             pti_api_id_driver_levelzero::zeCommandListAppendLaunchKernel_id));
}

void StartTracing() {
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_RUNTIME_API));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DRIVER_API));
  EnableIndividualDriverApis();
}

void StopTracing() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_RUNTIME_API));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DRIVER_API));
}

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = kRequestedBufferSize;
}

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (valid_buf_size) {
      samples_utils::AlignedDealloc(buf);
    }
    return;
  }
  pti_view_record_base *ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      std::cout << "Reached End of buffer" << '\n';
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_overhead *>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_external_correlation *>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Sycl Runtime Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_api *>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_DRIVER_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Driver Api Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_api *>(ptr));
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_memory_copy *>(ptr));
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_memory_fill *>(ptr));
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        pti_view_record_kernel *rec = reinterpret_cast<pti_view_record_kernel *>(ptr);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Kernel Record" << '\n';
        samples_utils::DumpRecord(rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        if (samples_utils::isMonotonic({rec->_sycl_task_begin_timestamp,
                                        rec->_sycl_enqk_begin_timestamp, rec->_append_timestamp,
                                        rec->_submit_timestamp, rec->_start_timestamp,
                                        rec->_end_timestamp})) {
          std::cout << "------------>     All Monotonic" << std::endl;
        } else {
          std::cerr << "------------>     Something wrong: NOT All monotonic" << std::endl;
        }
        if (rec->_sycl_task_begin_timestamp == 0) {
          std::cerr << "------------>     Something wrong: Sycl Task "
                       "Begin Time is 0"
                    << std::endl;
        }
        if (rec->_sycl_enqk_begin_timestamp == 0) {
          std::cerr << "------------>     Something wrong: Sycl Enq "
                       "Launch Kernel Time is 0"
                    << std::endl;
        }
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  samples_utils::AlignedDealloc(buf);
}

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

static float RunAndCheck(sycl::queue queue, const std::vector<float> &a,
                         const std::vector<float> &b, std::vector<float> &c, unsigned size,
                         float expected_result) {
  assert(size > 0);
  assert(a.size() == size * size);
  assert(b.size() == size * size);
  assert(c.size() == size * size);

  double time = 0.0;

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> c_buf(c.data(), c.size());

    sycl::event event = queue.submit([&](sycl::handler &cgh) {
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

    auto start = event.get_profiling_info<sycl::info::event_profiling::command_start>();
    auto end = event.get_profiling_info<sycl::info::event_profiling::command_end>();
    time = static_cast<double>(end - start) / NSEC_IN_SEC;
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }

  std::cout << "Matrix multiplication time: " << time << " sec" << std::endl;

  return Check(c, expected_result);
}

static void Compute(sycl::queue queue, const std::vector<float> &a, const std::vector<float> &b,
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
  std::cout << " Calculating floating point matrix multiply on gpu\n";
  std::cout << name
            << " [ [gpu|cpu|host, default=gpu],  [matrix size, default=1024, max=" << max_size
            << "], [repetition count, default=4]] \n";
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_SUCCESS;
  uint64_t eid = 11;  // external correlation id base.
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
  StartTracing();
  // start tracing early enables to capture nodes creation at piProgramCreate
  //  and Kernel Task sycl file/line info is captured, as exampple shows at a Node Creation
  // Emit external correlation id records by marking section of code by
  // ptiViewPushExternalCorrelationId / ptiViewPopExternalCorrelationId
  //   Each of the enabled activity view records (sycl runtime, kernel launches) will be *preceeded*
  //   by 1 external correlation id record per kind.
  PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid));
  PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid + 50));
  PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, eid + 30));
  PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_2, eid + 40));
  unsigned repeat_count = 1;
  unsigned size = 1024;
  sycl::device dev;
  try {
    dev = sycl::device(sycl::gpu_selector_v);
    if (argc > 1 && strcmp(argv[1], "cpu") == 0) {
      dev = sycl::device(sycl::cpu_selector_v);
      std::cerr << "PTI doesn't support cpu profiling yet" << '\n';
      std::exit(EXIT_FAILURE);
    } else if (argc > 1 && strcmp(argv[1], "host") == 0) {
      dev = sycl::device(sycl::default_selector_v);
      std::cerr << "PTI doesn't support host profiling yet" << '\n';
      std::exit(EXIT_FAILURE);
    }

    unsigned temp = size;
    if (argc > 2) {
      temp = std::stoul(argv[2]);
      size = (temp < min_size) ? min_size : (temp > max_size) ? max_size : temp;
    }

    if (argc > 3) {
      temp = std::stoul(argv[3]);
      repeat_count = (temp < 1) ? 1 : temp;
    }
  } catch (const sycl::exception &e) {
    Usage(argv[0]);
    std::cerr << "Error: Exception caught while executing SYCL " << e.what() << '\n';
    std::cerr << "Unable to select valid sycl device" << '\n';
    return EXIT_FAILURE;
  } catch (...) {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  sycl::property_list prop_list{sycl::property::queue::enable_profiling(),
                                sycl::property::queue::in_order()};
  sycl::queue queue(dev, sycl::async_handler{}, prop_list);   // Main runandcheck kernel
  sycl::queue queue1(dev, sycl::async_handler{}, prop_list);  // Main runandcheck kernel

  PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid));
  PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid));
  PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &eid));
  PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_2, &eid));

  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size << " x " << size << ", repeats "
            << repeat_count << " times)" << std::endl;
  std::cout << "Target device: "
            << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
            << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  PTI_CHECK_SUCCESS(ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_1, eid + 50));

  try {
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    Compute(queue1, a, b, c, size, repeat_count, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    std::cout << "Total execution time with tracing: " << time.count() << " sec" << std::endl;

    start = std::chrono::steady_clock::now();
    expected_result = A_VALUE * B_VALUE * size;

    PTI_CHECK_SUCCESS(ptiViewPopExternalCorrelationId(
        pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_1, &eid));

    Compute(std::move(queue), a, b, c, size, repeat_count, expected_result);
    end = std::chrono::steady_clock::now();
    time = end - start;

    std::cout << "Total execution time without tracing: " << time.count() << " sec" << std::endl;
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
  StopTracing();
  PTI_CHECK_SUCCESS(ptiFlushAllViews());

  return exit_code;
}
