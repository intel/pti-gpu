//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <iostream>
#include <vector>
#include <atomic>
#include <sycl/sycl.hpp>

#include "queue.h"
#include "device_memory.h"
#include "tiny_tensor.h"
#include "model_mixedprogramming.h"
#include "utils.h"
#include "operation_onednn.h"
#include "pti/pti_view.h"
#include "samples_utils.h"

// Global counter for GPU kernel records
std::atomic<uint64_t> g_gpu_kernel_count{0};

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
  std::cout << "-) ITEX uses eigen instead of oneDPL for functions such as cos." << std::endl;
  std::cout << std::endl;
  std::cout << "It is supposed that this application will be updated frequently, so this might be not the latest one." << std::endl;
  std::cout << std::endl;
}

void run(sycl::queue *q, size_t iterations)
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
  for (size_t i = 0; i < iterations; ++i) {
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
  ptiViewEnable(PTI_VIEW_RUNTIME_API);
  ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION);
  ptiViewEnable(PTI_VIEW_COLLECTION_OVERHEAD);
}

void StopTracing() {
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewDisable(PTI_VIEW_RUNTIME_API);
  ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION);
  ptiViewDisable(PTI_VIEW_COLLECTION_OVERHEAD);
}


int main()
{
  int exit_code = EXIT_SUCCESS;
  uint64_t eid = 21;

  PrintUsage();

  pti_result result = ptiViewSetCallbacks(
      [](auto **buf, auto *buf_size) {
        static constexpr size_t kBufferSize = 1000;
        *buf_size = kBufferSize;
        *buf = reinterpret_cast<unsigned char *>(::operator new(*buf_size));
        if (!*buf) {
          std::abort();
        }
        return;
      },
      [](auto *buf, auto buf_size, auto valid_buf_size) {
        if (!buf || !valid_buf_size || !buf_size) {
          std::cerr << "Received empty buffer" << '\n';
          if (buf) {
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
              samples_utils::DumpRecord(
                  reinterpret_cast<pti_view_record_overhead *>(ptr));
              break;
            }
            case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              samples_utils::DumpRecord(
                  reinterpret_cast<pti_view_record_external_correlation *>(
                      ptr));
              break;
            }
            case pti_view_kind::PTI_VIEW_RUNTIME_API: {
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              std::cout << "Found Sycl Runtime Record" << '\n';
              samples_utils::DumpRecord(
                  reinterpret_cast<pti_view_record_api *>(ptr));
              break;
            }
            case pti_view_kind:: PTI_VIEW_DEVICE_GPU_MEM_COPY: {
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
            case pti_view_kind:: PTI_VIEW_DEVICE_GPU_MEM_FILL: {
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
              pti_view_record_kernel* rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
              g_gpu_kernel_count++;
              std::cout << "---------------------------------------------------"
                           "-----------------------------"
                        << '\n';
              std::cout << "Found Kernel Record" << '\n';
              samples_utils::DumpRecord(rec);
              if (samples_utils::IsMonotonic(
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
              if (rec->_sycl_task_begin_timestamp == 0)
                std::cerr << "------------>     Something wrong: Sycl Task "
                             "Begin Time is 0"
                          << std::endl;
              if (rec->_sycl_enqk_begin_timestamp == 0)
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
  if (result != pti_result::PTI_SUCCESS) {
    std::cerr << "Failed to set PTI View callbacks" << '\n';
    return EXIT_FAILURE;
  }
  StartTracing();
  ptiViewPushExternalCorrelationId(
      pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, eid);

  auto q = CreateQueue();
  if (q == NULL) {
    std::cout << "failed to create sycl queue." << std::endl;
    return EXIT_FAILURE;
  }

  GlobalDeviceMemoryManager().init(q.get());

  // execute the model (we are now focus on training)
  const size_t iterations = 100;
  try {
    run(q.get(), iterations);
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

  StopTracing();

  GlobalDeviceMemoryManager().deinit();
  // Xunsong: Disable PTI at here

  std::cout  << std::endl << "program finished." << std::endl;

  if (ptiFlushAllViews() != pti_result::PTI_SUCCESS) {
    std::cerr << "Failed to flush views" << '\n';
    exit_code = EXIT_FAILURE;
  }

  ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &eid);

  // Validate that "many" GPU_KERNEL records were captured,
  // the exact count depends on the number of iterations in run() and
  // the number of kernels in run_model_mixedprogramming(), which depends on runtime and platform,
  // so just check if the actual count is greater than number of iterations
  uint64_t min_kernel_count = iterations;
  uint64_t actual_kernel_count = g_gpu_kernel_count.load();

  std::cout << std::endl;
  std::cout << "================================================" << std::endl;
  std::cout << "GPU Kernel Record Validation:" << std::endl;
  std::cout << "  Expected > " << min_kernel_count << std::endl;
  std::cout << "  Actual:     " << actual_kernel_count << std::endl;

  if (actual_kernel_count > min_kernel_count) {
    std::cout << "  Status:     PASS" << std::endl;
    std::cout << "================================================" << std::endl;
  } else {
    std::cout << "  Status:     FAIL" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cerr << "ERROR: Expected more than " << min_kernel_count
              << " GPU kernel records but got " << actual_kernel_count << std::endl;
    exit_code = EXIT_FAILURE;
  }

  return exit_code;
}
