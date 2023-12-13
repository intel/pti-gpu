//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <iostream>
#include <vector>
#include <CL/sycl.hpp>

#include "queue.h"
#include "device_memory.h"
#include "tiny_tensor.h"
#include "model_mixedprogramming.h"
#include "utils.h"
#include "operation_onednn.h"
#include "pti_view.h"
#include "samples_utils.h"

void PrintUsage()
{
  std::cout << "It is a largely simpilified application to demo mixed programming ";
  std::cout << "on Intel GPU for deep learning (PyTorch&TensorFlow) workloads (ITEX&IPEX) ";
  std::cout << "with direct dpcpp kernel, onednn, onemkl, onedpl, onemkl, eigen, etc." << std::endl;
  std::cout << "IPEX: https://github.com/intel/intel-extension-for-pytorch" << std::endl;
  std::cout << std::endl;
  std::cout << "The purpose of this application is to provide a basic rough requirement for sycl graph capture mode." << std::endl;
  std::cout << std::endl;
  std::cout << "opens:" << std::endl;
  std::cout << "-) out of order queue" << std::endl;
  std::cout << "-) multiple threads" << std::endl;
  std::cout << "-) multiple queues" << std::endl;
  std::cout << "-) distributed traing with multiple instances (oneccl workload captured in the graph)" << std::endl;
  std::cout << "-) share device memory between sycl graphs" << std::endl;
  std::cout << "-) lock device memory used within graph internals" << std::endl;
  std::cout << "-) oneDNN, oneMKL etc might create USM internally" << std::endl;
  std::cout << "-) integrate into deep learning framework (python code)" << std::endl;
  std::cout << "-) ITEX uses eigen instead of oneDPL for fuctions such as cos." << std::endl;
  std::cout << std::endl;
  std::cout << "It is supposed that this application will be updated frequently, so this might be not the latest one." << std::endl;
  std::cout << std::endl;
#if __LIBSYCL_MAJOR_VERSION >= 7
  std::cerr << "Notice: A portion of this sample was not build. To build the whole sample, revert to older oneAPI release (<= 2023.2.0)" << std::endl;
#endif
}

void run(sycl::queue *q)
{
  // shape of model input (NCHW)
  size_t n = 1, c = 3, h = 224, w = 224;

  // prepare host data for model input
  float *inp_h = new float[n*c*h*w];
  for (size_t i = 0; i < n*c*h*w; ++i) {
    inp_h[i] = random_float();
  }

  // prepare model weights
  size_t oc = 5, ks = 3;
  onednn_prepare_weights(oc, c, ks, q);

  // current usage without sycl graph
  for (int i = 0; i < 100; ++i) {
    // Host2Device for model input
    TinyTensor inp(n, c, h, w);
    q->memcpy(inp.data, inp_h, inp.count() * sizeof(float));

    // we may add more special models such as run_model_dlrm for the demo purpose
    TinyTensor outp = run_model_mixedprogramming(inp, q);

    // for model training iterations (one instance), the final step is update
    // the model parameters buffer which is in device memory.
    // for inference, the final step is Device2Host for CPU access.
    // let's mock with wait to reuse outp.
    q->wait();
    GlobalDeviceMemoryManager().free(outp.data);
  }

  // it's not easy to get golden reference to verify the correctness,
  // currently we need to manually check it when add new operations.
  // we can at least compare the results with/without the sycl graph.

  {
    // in sycl graph capture mode, the very rough expectation looks like below.
    // how the input, output device memory are managed are still opens.

    // 1. warmup, there's some one-time workloads that we don't want to capture
    // for (int i = 0; i < 3; ++i) {
    //   run_model_mixedprogramming(inp, q);
    // }

    // 2. capture, no GPU kernel execution at this stage
    // auto graph = sycl::start_capture();
    // outp = run_model_mixedprogramming(inp, q);
    // sycl::end_capture(graph);

    // 3. replay
    // for (int i = 0; i < 100; ++i) {
    //   fill(inp);
    //   outp = graph.replay(inp);
    //   use(outp);
    // }
  }

  delete[] inp_h;
}

void StartTracing() {
  ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS);
  ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION);
  ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD);
}

void StopTracing() {
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS);
  ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION);
  ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD);
}


int main()
{
  int exit_code = EXIT_SUCCESS;
  uint64_t eid = 21;
  PrintUsage();
  // Xunsong: Enable PTI at here

  ptiViewSetCallbacks(
      [](auto **buf, auto *buf_size) {
        *buf_size = sizeof(pti_view_record_kernel);
        void *ptr = ::operator new(*buf_size);
        ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
        *buf = reinterpret_cast<unsigned char *>(ptr);
        if (!*buf) {
          std::abort();
        }
        return;
      },
      [](auto *buf, auto buf_size, auto valid_buf_size) {
        if (!buf || !valid_buf_size || !buf_size) {
          // std::cerr << "Received empty buffer" << '\n';
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
            // std::cout << "Reached End of buffer" << '\n';
            break;
          }
          if (buf_status != pti_result::PTI_SUCCESS) {
            // std::cerr << "Found Error Parsing Records from PTI" << '\n';
            break;
          }
          switch (ptr->_view_kind) {
            case pti_view_kind::PTI_VIEW_INVALID: {
              // std::cout << "Found Invalid Record" << '\n';
              break;
            }
            case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_overhead *>(ptr));
              break;
            }
            case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_external_correlation *>(
                      ptr));
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
            case pti_view_kind:: PTI_VIEW_DEVICE_GPU_MEM_COPY: {
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
            case pti_view_kind:: PTI_VIEW_DEVICE_GPU_MEM_FILL: {
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
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              std::cout << "Found Kernel Record" << '\n';
              samples_utils::dump_record(
                  reinterpret_cast<pti_view_record_kernel *>(ptr));
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              if (((reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_sycl_task_begin_timestamp) <=
                   (reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_sycl_enqk_begin_timestamp)) &&

                  ((reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_sycl_enqk_begin_timestamp) <=
                   (reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_append_timestamp)) &&

                  ((reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_append_timestamp) <=
                   (reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_submit_timestamp)) &&

                  ((reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_submit_timestamp) <=
                   (reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_start_timestamp)) &&

                  ((reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_start_timestamp) <=
                   (reinterpret_cast<pti_view_record_kernel *>(ptr)
                        ->_end_timestamp))) {
                std::cout << "------------>     All Monotonic" << std::endl;
              } else {
                std::cerr
                    << "------------>     Something wrong: NOT All monotonic"
                    << std::endl;
              };
              if (reinterpret_cast<pti_view_record_kernel *>(ptr)
                      ->_sycl_task_begin_timestamp == 0)
                std::cerr << "------------>     Something wrong: Sycl Task "
                             "Begin Time is 0"
                          << std::endl;
              if (reinterpret_cast<pti_view_record_kernel *>(ptr)
                      ->_sycl_enqk_begin_timestamp == 0)
                std::cerr << "------------>     Something wrong: Sycl Enq "
                             "Launch Kernel Time is 0"
                          << std::endl;

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
  StartTracing();
  ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, eid);

  auto q = CreateQueue();
  if (q == NULL) {
    std::cout << "failed to create sycl queue." << std::endl;
    return 1;
  }

  GlobalDeviceMemoryManager().init(q.get());

  // Xunsong: Start PTI trace at here
  // execute the model (we are now focus on training)
  try {
    run(q.get());
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

  // make sure all the GPU tasks are done when cleanup
  q->wait();
  // Xunsong: Stop PTI trace at here
  StopTracing();

  GlobalDeviceMemoryManager().deinit();
  // Xunsong: Disable PTI at here

  std::cout  << std::endl << "program finished." << std::endl;
  auto flush_results = ptiFlushAllViews();
  assert(flush_results == pti_result::PTI_SUCCESS);
  ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &eid);
  return exit_code;
}
