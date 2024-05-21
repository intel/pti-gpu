// ==============================================================
// Based on Vector Add example from OneAPI samples
// ==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
#include <level_zero/ze_api.h>

#include <iostream>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <variant>
#include <vector>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"

#define PTI_THROW(X)                                    \
  do {                                                  \
    if (X != pti_result::PTI_SUCCESS) {                 \
      throw std::runtime_error("PTI CALL FAILED: " #X); \
    }                                                   \
  } while (0)

void StartTracing() {
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS));
}

void StopTracing() {
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_THROW(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_THROW(ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS));
}

constexpr size_t kVectorSize = 5000;

void PrintQueueInfo(const sycl::queue &sycl_queue) {
  auto queue_type = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue);
#if __LIBSYCL_MAJOR_VERSION > 6 || (__LIBSYCL_MAJOR_VERSION == 6 && __LIBSYCL_MINOR_VERSION >= 2)
  // 1 (default)
  if (auto *ptr_queue_handle = std::get_if<ze_command_list_handle_t>(&queue_type)) {
    std::cout << "Queue ptr: " << static_cast<const void *>(&sycl_queue)
              << ", native queue: " << static_cast<const void *>(ptr_queue_handle)
              << ", native device: "
              << static_cast<const void *>(sycl::get_native<sycl::backend::ext_oneapi_level_zero>(
                     sycl_queue.get_device()))
              << '\n';
    // 0
  } else if (auto *ptr_queue_handle = std::get_if<ze_command_queue_handle_t>(&queue_type)) {
    std::cout << "Queue ptr: " << static_cast<const void *>(&sycl_queue)
              << ", native queue: " << static_cast<const void *>(ptr_queue_handle)
              << ", native device: "
              << static_cast<const void *>(sycl::get_native<sycl::backend::ext_oneapi_level_zero>(
                     sycl_queue.get_device()))
              << '\n';

  } else {
    std::cerr << "Underlying level zero queue handle could not be obtained." << '\n';
  }
#else
  std::cout << "Queue ptr: " << static_cast<const void *>(&sycl_queue) << ", native queue: "
            << static_cast<const void *>(
                   sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue))
            << ", native device: "
            << static_cast<const void *>(
                   sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue.get_device()))
            << '\n';
#endif
}

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
template <typename T>
void VecSq(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_write);
    sycl::accessor b(b_buf, h, sycl::read_write);
    h.parallel_for(num_items, [=](auto i) {
      a[i] = a[i] * a[i];
      b[i] = b[i] * b[i];
    });
  });
  q.wait();
}

//************************************
// Vector add in SYCL on device: returns sum in 4th parameter "sq_add".
//************************************
template <typename T>
void VecAdd(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector,
            std::vector<T> &sq_add) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);
  sycl::buffer sum_buf(sq_add.data(), num_items);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_only);
    sycl::accessor b(b_buf, h, sycl::read_only);
    sycl::accessor sum(sum_buf, h, sycl::write_only, sycl::no_init);
    h.parallel_for(num_items, [=](auto i) { sum[i] = a[i] + b[i]; });
  });
  q.wait();
}

template <typename T>
void PrintResults(const std::vector<T> &sq_add, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) {
    sum += sq_add[i];
  }
  std::cout << "find result: " << sum / static_cast<double>(n) << '\n';
}

template <typename T>
void RunProfiledVecSqAdd(sycl::queue &sycl_queue) {
  std::vector<T> a(kVectorSize);
  std::vector<T> b(kVectorSize);
  std::vector<T> c(2 * kVectorSize);
  std::vector<T> d(2 * kVectorSize);
  std::vector<T> sq_add(kVectorSize);
  std::vector<T> sq_add2(2 * kVectorSize);

  for (size_t i = 0; i < kVectorSize; i++) {
    a[i] = sin(i);
    b[i] = cos(i);
    c[2 * i] = sin(i) * sin(i);
    c[2 * i + 1] = sin(i);
    d[2 * i] = cos(i) * cos(i);
    d[2 * i + 1] = cos(i);
  }

  VecSq(sycl_queue, a, b);
  StartTracing();
  VecAdd(sycl_queue, a, b, sq_add);
  StopTracing();
  PrintResults(sq_add, kVectorSize);

  VecAdd(sycl_queue, a, b, sq_add);
  PrintResults(sq_add, kVectorSize);

  StartTracing();
  VecAdd(sycl_queue, c, d, sq_add2);
  PrintResults(sq_add2, 2 * kVectorSize);
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_SUCCESS;

  try {
    PTI_THROW(ptiViewSetCallbacks(
        [](auto **buf, auto *buf_size) {
          *buf_size = sizeof(pti_view_record_kernel) * 100;
          void *ptr = ::operator new(*buf_size);
          ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
          *buf = reinterpret_cast<unsigned char *>(ptr);
          if (!*buf) {
            std::abort();
          }
          return;
        },
        [](auto *buf, auto buf_size, auto valid_buf_size) {
          if (!buf_size || !valid_buf_size || !buf_size) {
            std::cerr << "Received empty buffer" << '\n';
            if (valid_buf_size) {
              ::operator delete(buf);
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
              case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
                std::cout << "---------------------------------------------------"
                             "-----------------------------"
                          << '\n';
                std::cout << "Found Sycl Runtime Record" << '\n';
                samples_utils::dump_record(reinterpret_cast<pti_view_record_sycl_runtime *>(ptr));
                break;
              }
              case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
                std::cout << "---------------------------------------------------"
                             "-----------------------------"
                          << '\n';
                std::cout << "Found Memory Record" << '\n';
                samples_utils::dump_record(reinterpret_cast<pti_view_record_memory_copy *>(ptr));
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
                samples_utils::dump_record(reinterpret_cast<pti_view_record_memory_fill *>(ptr));
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
                samples_utils::dump_record(rec);

                std::cout << "---------------------------------------------------"
                             "-----------------------------"
                          << '\n';
                if (samples_utils::isMonotonic({rec->_sycl_task_begin_timestamp,
                                                rec->_sycl_enqk_begin_timestamp,
                                                rec->_append_timestamp, rec->_submit_timestamp,
                                                rec->_start_timestamp, rec->_end_timestamp})) {
                  std::cout << "------------>     All Monotonic" << std::endl;
                } else {
                  std::cerr << "------------>     Something wrong: NOT All monotonic" << std::endl;
                };
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
          ::operator delete(buf);
        }));

#if __INTEL_LLVM_COMPILER >= 20240000
    sycl::property_list prop{sycl::property::queue::in_order()};
#else
    sycl::property_list prop{};
#endif
    sycl::queue q(sycl::gpu_selector_v, prop);
    PrintQueueInfo(q);

    if (q.get_device().has(sycl::aspect::fp64)) {
      RunProfiledVecSqAdd<double>(q);
    } else {
      RunProfiledVecSqAdd<float>(q);
    }

    StopTracing();
    PTI_THROW(ptiFlushAllViews());
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
  return exit_code;
}
