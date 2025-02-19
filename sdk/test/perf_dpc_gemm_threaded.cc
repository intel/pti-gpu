//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

//
// Based on dpc_gemm sample. Added multithreading
//

#include <stdarg.h>
#include <string.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sycl/sycl.hpp>
#include <thread>

#if !defined(NO_PTI)
#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"
#endif

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

static bool verbose = false;

static float Check(const std::vector<float>& a, float value) {
  assert(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

static float RunAndCheck(sycl::queue queue, const std::vector<float>& a,
                         const std::vector<float>& b, std::vector<float>& c, unsigned size,
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

    sycl::event event = queue.submit([&](sycl::handler& cgh) {
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
  } catch (const sycl::exception& e) {
    std::cerr << "[ERROR] " << e.what() << std::endl;
    throw;
  }

  if (verbose) {
    std::cout << "\tMatrix multiplication time: " << time << " sec" << std::endl;
  }

  return Check(c, expected_result);
}

static void Compute(sycl::queue queue, const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& c, unsigned size, unsigned repeat_count,
                    float expected_result) {
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(queue, a, b, c, size, expected_result);
    if (eps > MAX_EPS) {
      std::cerr << "[ERROR] Results are "
                << "INCORRECT with accuracy: " << eps << "while expected less than " << MAX_EPS
                << std::endl;
    }
    if (verbose) {
      std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN")
                << "CORRECT with accuracy: " << eps << std::endl;
    }
  }
}

#if !defined(NO_PTI)
constexpr auto kRequestedRecordCount = 1'000ULL;
constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);

std::atomic<uint64_t> g_record_count{0};
#if defined(CAPTURE_OVERHEAD)
std::atomic<uint64_t> overhead_time_ns{0};
#endif

void StartTracing() {
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
#if !defined(CAPTURE_OVERHEAD)
  PTI_THROW(ptiViewEnable(PTI_VIEW_RUNTIME_API));
#endif /* ! CAPTURE_OVERHEAD */
  PTI_THROW(ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD));
}

void StopTracing() {
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
#if !defined(CAPTURE_OVERHEAD)
  PTI_THROW(ptiViewDisable(PTI_VIEW_RUNTIME_API));
#endif /* ! CAPTURE_OVERHEAD */
  PTI_THROW(ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD));
}

void ProvideBuffer(unsigned char** buf, std::size_t* buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = kRequestedBufferSize;
}
void ParseBuffer(unsigned char* buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cout << "Received empty buffer" << '\n';
    samples_utils::AlignedDealloc(buf);
    return;
  }
  pti_view_record_base* ptr = nullptr;

  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
#if defined(RECORD_PARSE_AND_PRINT)
      std::cout << "Reached End of buffer" << '\n';
#endif
      break;
    }
    g_record_count++;
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
#if defined(CAPTURE_OVERHEAD)
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        pti_view_record_overhead* record = reinterpret_cast<pti_view_record_overhead*>(ptr);
        overhead_time_ns += record->_overhead_duration_ns;
        // Keep commented lines around for the purpose of reviewing latency of L0 calls
        // std::cout << " ======== Overhead Time: " << std::setw(7) << std::right
        //           << record->_overhead_duration_ns << " ns"
        //           << "\t API Id: " << record->_api_id << '\n';
        break;
      }
#if defined(RECORD_PARSE_AND_PRINT)
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Sycl Runtime Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_api*>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_external_correlation*>(ptr));
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';

        pti_view_record_memory_copy* p_memory_rec =
            reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        samples_utils::DumpRecord(p_memory_rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        auto issues = samples_utils::ValidateTimestamps(
            p_memory_rec->_append_timestamp, p_memory_rec->_submit_timestamp,
            p_memory_rec->_start_timestamp, p_memory_rec->_end_timestamp);
        if (issues) {
          std::cerr << "Memcopy Timestamp error on line: " << __LINE__ << '\n';
          std::exit(EXIT_FAILURE);
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Memory Record" << '\n';

        pti_view_record_memory_fill* p_memory_rec =
            reinterpret_cast<pti_view_record_memory_fill*>(ptr);
        samples_utils::DumpRecord(p_memory_rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        auto issues = samples_utils::ValidateTimestamps(
            p_memory_rec->_append_timestamp, p_memory_rec->_submit_timestamp,
            p_memory_rec->_start_timestamp, p_memory_rec->_end_timestamp);
        if (issues) {
          std::cerr << "Memfill Timestamp error on line: " << __LINE__ << '\n';
          std::exit(EXIT_FAILURE);
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found Kernel Record" << '\n';

        pti_view_record_kernel* p_kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
        samples_utils::DumpRecord(p_kernel_rec);
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        auto issues = samples_utils::ValidateTimestamps(
            p_kernel_rec->_sycl_task_begin_timestamp, p_kernel_rec->_sycl_enqk_begin_timestamp,
            p_kernel_rec->_append_timestamp, p_kernel_rec->_submit_timestamp,
            p_kernel_rec->_start_timestamp, p_kernel_rec->_end_timestamp);
        if (issues) {
          std::cerr << "Kernel Timestamp error on line: " << __LINE__ << '\n';
          std::exit(EXIT_FAILURE);
        }
        break;
      }
#endif  // RECORD_PARSE_AND_PRINT
      default: {
        break;
      }
    }
#endif  // CAPTURE_OVERHEAD
  }
  samples_utils::AlignedDealloc(buf);
}
#endif

const unsigned max_thread_count = 64;
const unsigned max_size = 8192;
const unsigned min_size = 32;

const unsigned default_size = 1024;
const unsigned default_thread_count = 2;
const unsigned default_repetition_per_thread = 4;

void Usage(const char* name) {
  std::cout << " Calculating floating point matrix multiply on gpu, submitting the work from many "
               "CPU threads\n"
            << "  Usage " << name << "  [ options ]" << std::endl;
  std::cout << "--threads [-t]  integer         "
            << "Threads number, default: " << default_thread_count << std::endl;
  std::cout << "--size [-s]     integer        "
            << "Matrix size, default: " << default_size << std::endl;
  std::cout << "--repeat [-r]   integer         "
            << "Repetition number per thread, default: " << default_repetition_per_thread
            << std::endl;
  std::cout << "--verbose [-v]                 "
            << "Enable verbose mode to report the app progress, default: off" << std::endl;
}

int main(int argc, char* argv[]) {
  int exit_code = EXIT_SUCCESS;
  unsigned thread_count = default_thread_count;
  unsigned repeat_count = default_repetition_per_thread;
  unsigned size = default_size;

  try {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        size = (temp < min_size) ? min_size : (temp > max_size) ? max_size : temp;
      } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        thread_count = (temp < 1) ? 1 : (temp > max_thread_count) ? max_thread_count : temp;
      } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repeat") == 0) {
        i++;
        auto temp = std::stoul(argv[i]);
        repeat_count = (temp < 1) ? 1 : temp;
      } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
        // verbosity off makes minimal the sample self output -
        // so profiling output won't be intermixed with the sample output
        // and could be analyzed by tests
        verbose = true;
      } else {
        Usage(argv[0]);
        return EXIT_SUCCESS;
      }
    }
  } catch (...) {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  try {
#if !defined(NO_PTI)
    PTI_THROW(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));

    StartTracing();
#endif
    sycl::device dev;
    dev = sycl::device(sycl::gpu_selector_v);
    sycl::property_list prop_list{sycl::property::queue::in_order()};
    sycl::queue queue(dev, sycl::async_handler{}, prop_list);
    if (argc > 1 && strcmp(argv[1], "cpu") == 0) {
      dev = sycl::device(sycl::cpu_selector_v);
      std::cerr << "PTI doesn't support cpu profiling yet" << '\n';
      std::exit(EXIT_FAILURE);
    } else if (argc > 1 && strcmp(argv[1], "host") == 0) {
      dev = sycl::device(sycl::default_selector_v);
      std::cerr << "PTI doesn't support host profiling yet" << '\n';
      std::exit(EXIT_FAILURE);
    }

    float expected_result = A_VALUE * B_VALUE * size;

    auto threadFunction = [&queue](unsigned _size, unsigned _repeat_count, float _expected_result) {
      std::vector<float> a(_size * _size, A_VALUE);
      std::vector<float> b(_size * _size, B_VALUE);
      std::vector<float> c(_size * _size, 0.0f);

      auto start = std::chrono::steady_clock::now();
      Compute(queue, a, b, c, _size, _repeat_count, _expected_result);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<float> time = end - start;

      if (verbose) {
        std::cout << "\t-- Total execution time: " << time.count() << " sec" << std::endl;
      }
    };

    std::cout << "DPC++ Matrix Multiplication (CPU threads: " << thread_count
              << ", matrix size: " << size << " x " << size << ", repeats: " << repeat_count
              << " times)" << std::endl;
    std::cout << "Target device: "
              << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
              << std::endl
              << std::flush;

    auto start = std::chrono::steady_clock::now();

    if (thread_count > 1) {
      std::vector<std::thread> the_threads;
      for (unsigned i = 0; i < thread_count; i++) {
        std::thread t = std::thread(threadFunction, size, repeat_count, expected_result);
        the_threads.push_back(std::move(t));
      }

      for (auto& th : the_threads) {
        if (th.joinable()) {
          th.join();
        }
      }
    } else {
      threadFunction(size, repeat_count, expected_result);
    }

#if defined(NO_PTI)
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    auto gemm_count = thread_count * repeat_count;
#endif /* NO_PTI */

#if !defined(NO_PTI)
    StopTracing();
    PTI_THROW(ptiFlushAllViews());
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    auto gemm_count = thread_count * repeat_count;
    std::cout << "-- PTI tracing was enabled, Record count: " << g_record_count << '\n';
#if defined(CAPTURE_OVERHEAD)
    std::cout << "-- For Overhead View test - only GPU ops and Overhead View are ON (not Sycl) "
              << '\n';
    std::cout << "-- Summed from Overhead View records Overhead time: "
              << static_cast<float>(overhead_time_ns) / static_cast<float>(NSEC_IN_SEC) << " sec"
              << '\n';
#endif /* CAPTURE_OVERHEAD */
#endif /* ! NO_PTI */

    std::cout << "-- Total execution time: " << time.count() << " sec" << std::endl;
    std::cout << "-- Throughput: "
              << static_cast<int>(static_cast<float>(gemm_count) / time.count())
              << " gemms of size " << size << "x" << size << " in sec" << std::endl;

  } catch (const sycl::exception& e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    exit_code = EXIT_FAILURE;
  }
  return exit_code;
}
