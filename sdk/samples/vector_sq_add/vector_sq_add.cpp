//==============================================================
// Based on Vector Add example from OneAPI samples
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
#include <level_zero/ze_api.h>

#include <iostream>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"

using namespace sycl;

constexpr size_t vector_size = 5000;
typedef std::vector<double> DoubleVector;

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
void vecSq(queue &q, const DoubleVector &a_vector,
           const DoubleVector &b_vector) {
  range<1> num_items{a_vector.size()};
  buffer a_buf(a_vector);
  buffer b_buf(b_vector);

  q.submit([&](handler &h) {
    accessor a(a_buf, h, read_write);
    accessor b(b_buf, h, read_write);
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
void vecAdd(queue &q, const DoubleVector &a_vector,
            const DoubleVector &b_vector, DoubleVector &sq_add) {
  range<1> num_items{a_vector.size()};
  buffer a_buf(a_vector);
  buffer b_buf(b_vector);
  buffer sum_buf(sq_add.data(), num_items);

  q.submit([&](handler &h) {
    accessor a(a_buf, h, read_only);
    accessor b(b_buf, h, read_only);
    accessor sum(sum_buf, h, write_only, no_init);
    h.parallel_for(num_items, [=](auto i) { sum[i] = a[i] + b[i]; });
  });
  q.wait();
}

void print_results(const DoubleVector &sq_add, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += sq_add[i];
  printf("final result: %f\n", sum / n);
}

void StartTracing() {
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS) == pti_result::PTI_SUCCESS);
}

void StopTracing() {
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS);
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_SUCCESS;
  DoubleVector a, b, c, d, sq_add, sq_add2;

  ptiViewSetCallbacks(
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
          auto buf_status =
              ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
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
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_sycl_runtime *>(ptr));
              break;
            }
            case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              std::cout << "Found Memory Record" << '\n';
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_memory_copy *>(ptr));
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
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_memory_fill *>(ptr));
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              break;
            }
            case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
              pti_view_record_kernel* rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              std::cout << "Found Kernel Record" << '\n';
              samples_utils::dump_record(rec);

              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              if (samples_utils::isMonotonic(
                                  {
                                    rec->_sycl_task_begin_timestamp ,
                                    rec->_sycl_enqk_begin_timestamp ,
                                    rec->_append_timestamp ,
                                    rec->_submit_timestamp ,
                                    rec->_start_timestamp ,
                                    rec->_end_timestamp
                                  }
                                )) {
                std::cout << "------------>     All Monotonic" << std::endl;
              } else {
                std::cerr
                    << "------------>     Something wrong: NOT All monotonic"
                    << std::endl;
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
      });

  try {
    auto dev = sycl::device(sycl::gpu_selector_v);

    a.resize(vector_size);
    b.resize(vector_size);
    c.resize(2 * vector_size);
    d.resize(2 * vector_size);
    sq_add.resize(vector_size);
    sq_add2.resize(2 * vector_size);

    for (size_t i = 0; i < vector_size; i++) {
      a[i] = sin(i);
      b[i] = cos(i);
      c[2 * i] = sin(i) * sin(i);
      c[2 * i + 1] = sin(i);
      d[2 * i] = cos(i) * cos(i);
      d[2 * i + 1] = cos(i);
    }

    auto d_selector{gpu_selector_v};
    //sycl::property_list prop{sycl::property::queue::in_order()};
    //queue q(d_selector, prop);
    queue q(d_selector, NULL);

    // Underlying queue handle object changes based on value of
    // SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=?
    auto print_queue_info = [](const sycl::queue &sycl_queue) {
      auto queue_type =
          get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue);
#if __LIBSYCL_MAJOR_VERSION > 6 || (__LIBSYCL_MAJOR_VERSION == 6 && __LIBSYCL_MINOR_VERSION >= 2)
      // 1 (default)
      if (auto *ptr_queue_handle =
              std::get_if<ze_command_list_handle_t>(&queue_type)) {
        printf("Queue  ptr: 0x%p, native queue: 0x%p, native device: 0x%p \n",
               &sycl_queue, ptr_queue_handle,
               get_native<sycl::backend::ext_oneapi_level_zero>(
                   sycl_queue.get_device()));

        // 0
      } else if (auto *ptr_queue_handle =
                     std::get_if<ze_command_queue_handle_t>(&queue_type)) {
        printf("Queue  ptr: 0x%p, native queue: 0x%p, native device: 0x%p \n",
               &sycl_queue, ptr_queue_handle,
               get_native<sycl::backend::ext_oneapi_level_zero>(
                   sycl_queue.get_device()));
      } else {
        std::cerr << "Underlying level zero queue handle could not be obtained."
                  << '\n';
      }
#else
      printf("Queue  ptr: 0x%p, native queue: 0x%p, native device: 0x%p \n",
             &sycl_queue,
             get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue),
             get_native<sycl::backend::ext_oneapi_level_zero>(
                 sycl_queue.get_device()));
#endif
    };

    print_queue_info(q);

    vecSq(q, a, b);
    StartTracing();
    vecAdd(q, a, b, sq_add);
    StopTracing();
    print_results(sq_add, vector_size);

    vecAdd(q, a, b, sq_add);
    print_results(sq_add, vector_size);

    print_queue_info(q);

    StartTracing();
    vecAdd(q, c, d, sq_add2);
    print_results(sq_add2, 2 * vector_size);
  } catch (const sycl::exception &e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value()
              << "\n\tCategory: " << e.category().name()
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
  a.clear();
  b.clear();
  sq_add.clear();
  c.clear();
  d.clear();
  sq_add2.clear();

  ptiFlushAllViews();
  return exit_code;
}
