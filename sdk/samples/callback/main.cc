//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <level_zero/ze_api.h>
#include <string.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <sycl/sycl.hpp>

#include "pti/pti_sync_callback.h"
#include "samples_utils.h"

#define NSEC_IN_SEC 1'000'000'000
#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

constexpr auto kRequestedRecordCount = 5'000'000ULL;
constexpr auto kRequestedBufferSize = kRequestedRecordCount * sizeof(pti_view_record_kernel);
char kEnterString[] = "I have seen ENTER";

ze_event_handle_t global_time_stamp_event = nullptr;
ze_event_pool_handle_t event_pool = nullptr;
void *buff = nullptr;

bool PrepareDataForGlobalEventAppend(ze_command_list_handle_t command_list) {
  static bool ready = false;
  if (!ready) {
    ze_context_handle_t hContext = nullptr;
    ze_result_t status = zeCommandListGetContextHandle(command_list, &hContext);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeCommandListGetContextHandle failed with error code: " << status << '\n';
      return false;
    }
    ze_device_handle_t hDevice = nullptr;
    status = zeCommandListGetDeviceHandle(command_list, &hDevice);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeCommandListGetDeviceHandle failed with error code: " << status << '\n';
      return false;
    }

    ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                            ZE_EVENT_POOL_FLAG_IPC |
                                                ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP |
                                                ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                            10};

    status = zeEventPoolCreate(hContext, &event_pool_desc, 1, &hDevice, &event_pool);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeEventPoolCreate failed with error code: " << status << '\n';
      return false;
    }
    ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0,
                                             0};
    status = zeMemAllocDevice(hContext, &alloc_desc, 64, 64, hDevice, &buff);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeMemAllocDevice failed with error code: " << status << '\n';
      return false;
    }
    ze_event_desc_t event_desc = {};
    event_desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    event_desc.index = 0;
    event_desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;  // Event is signaled on host
    event_desc.wait = ZE_EVENT_SCOPE_FLAG_HOST;    // Event is waited on host
    status = zeEventCreate(event_pool, &event_desc, &global_time_stamp_event);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "zeEventCreate failed with error code: " << status << '\n';
      return false;
    }
    ready = true;
  }
  return true;
}

void StartTracing() {
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
}

void StopTracing() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
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

void CallbackGPUOperationAppend([[maybe_unused]] pti_callback_domain domain,
                                pti_api_group_id driver_group_id, uint32_t driver_api_id,
                                [[maybe_unused]] pti_backend_ctx_t backend_context, void *cb_data,
                                void *user_data, void **instance_user_data) {
  std::cout << "In " << __func__
            << " Subscriber: " << *(reinterpret_cast<pti_callback_subscriber_handle *>(user_data))
            << std::endl;

  samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                  user_data, instance_user_data);

  pti_callback_gpu_op_data *callback_data = static_cast<pti_callback_gpu_op_data *>(cb_data);
  if (callback_data == nullptr) {
    std::cerr << "CallbackGPUOperationAppend: callback_data is null" << std::endl;
    return;
  }

  ze_bool_t is_immediate = 0;
  if (callback_data->_cmd_list_handle != nullptr) {
    ze_result_t res = zeCommandListIsImmediate(
        static_cast<ze_command_list_handle_t>(callback_data->_cmd_list_handle), &is_immediate);
    if (ZE_RESULT_SUCCESS == res) {
      std::cout << "Command List is " << (is_immediate ? "Immediate" : "Regular") << std::endl;
    } else {
      std::cout << "zeCommandListIsImmediate failed with error code: " << res << std::endl;
    }
  }

  pti_callback_gpu_op_data *gpu_op_data = static_cast<pti_callback_gpu_op_data *>(cb_data);

  pti_gpu_op_details *op_details =
      (gpu_op_data->_operation_details != nullptr)
          ? static_cast<pti_gpu_op_details *>(gpu_op_data->_operation_details)
          : nullptr;
  bool is_op_kernel = (op_details != nullptr)
                          ? (op_details->_operation_kind == PTI_GPU_OPERATION_KIND_KERNEL)
                          : false;
  uint32_t operation_count = (gpu_op_data != nullptr) ? gpu_op_data->_operation_count : 0;

  if (operation_count != 1) {
    std::cout << "WARNING: Operation count is not 1, it is: " << operation_count
              << " .It is unexpected for now!" << std::endl;
  }

  if (callback_data->_phase == PTI_CB_PHASE_API_ENTER) {
    *instance_user_data = static_cast<void *>(kEnterString);
    std::cout << "Append started...";
    if (is_op_kernel) {
      std::cout << "Operation is Kernel\n" << std::endl;
      std::cout << " Preparing data to append smth from here" << std::endl;
      auto res = PrepareDataForGlobalEventAppend(
          static_cast<ze_command_list_handle_t>(callback_data->_queue_handle));
      if (res) {
        std::cout << "Prepared data for Append" << std::endl;
        res = zeCommandListAppendWriteGlobalTimestamp(
            static_cast<ze_command_list_handle_t>(callback_data->_queue_handle),
            static_cast<uint64_t *>(buff), global_time_stamp_event, 0, nullptr);

        if (res == ZE_RESULT_SUCCESS) {
          std::cout << "Appended Write Global Timestamp to Command List" << std::endl;
        } else {
          std::cout << "zeCommandListAppendWriteGlobalTimestamp failed with error code: " << res
                    << std::endl;
        }
      } else {
        std::cout << "Failed to prepare data for Append" << std::endl;
      }
    } else {
      std::cout << "Operation is not Kernel" << std::endl;
    }
  } else if (callback_data->_phase == PTI_CB_PHASE_API_EXIT) {
    std::cout << "Append ended. Data from ENTER: " << static_cast<char *>(*instance_user_data)
              << std::endl;
  } else {
    std::cout << "Unexpected phase: " << callback_data->_phase << std::endl;
  }
}

void CallbackGPUOperationCompletion([[maybe_unused]] pti_callback_domain domain,
                                    pti_api_group_id driver_group_id, uint32_t driver_api_id,
                                    [[maybe_unused]] pti_backend_ctx_t backend_context,
                                    void *cb_data, void *user_data,
                                    [[maybe_unused]] void **instance_user_data) {
  std::cout << "In " << __func__
            << " Subscriber: " << *(reinterpret_cast<pti_callback_subscriber_handle *>(user_data))
            << std::endl;

  samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                  user_data, instance_user_data);

  pti_callback_gpu_op_data *callback_data = static_cast<pti_callback_gpu_op_data *>(cb_data);
  if (callback_data == nullptr) {
    std::cerr << "CallbackGPUOperationCompletion: callback_data is null" << std::endl;
    return;
  }

  if (global_time_stamp_event != nullptr) {
    auto result = zeEventQueryStatus(global_time_stamp_event);
    if (result == ZE_RESULT_SUCCESS) {
      std::cout << "Appended Global Time Stamp Signaled. Resetting the event" << std::endl;
      auto result2 = zeEventHostReset(global_time_stamp_event);
      if (result2 != ZE_RESULT_SUCCESS) {
        std::cout << "zeEventHostReset failed with error code: " << result2 << std::endl;
      }
    } else if (result == ZE_RESULT_NOT_READY) {
      std::cout << "Appended Global Time Stamp NOT Ready " << std::endl;
    } else {
      std::cout << "zeEventQueryStatus failed with error code: " << result << std::endl;
    }
  }
}

void CallbackCommon(pti_callback_domain domain, pti_api_group_id driver_group_id,
                    uint32_t driver_api_id, pti_backend_ctx_t backend_context, void *cb_data,
                    void *user_data, void **instance_user_data) {
  switch (domain) {
    case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED:
      CallbackGPUOperationAppend(domain, driver_group_id, driver_api_id, backend_context, cb_data,
                                 user_data, instance_user_data);
      break;
    case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED:
      CallbackGPUOperationCompletion(domain, driver_group_id, driver_api_id, backend_context,
                                     cb_data, user_data, instance_user_data);
      break;
    default: {
      std::cout << "In " << __func__ << " (default case)" << std::endl;
      samples_utils::DumpCallbackData(domain, driver_group_id, driver_api_id, backend_context,
                                      cb_data, user_data, instance_user_data);
      break;
    }
  }
  std::cout << std::endl;
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
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
  StartTracing();
  pti_callback_subscriber_handle subscriber1 = PTI_CALLBACK_SUBSCRIBER_HANDLE_INVALID;
  pti_callback_subscriber_handle subscriber2 = PTI_CALLBACK_SUBSCRIBER_HANDLE_INVALID;
  PTI_CHECK_SUCCESS(ptiCallbackSubscribe(&subscriber1, CallbackCommon, &subscriber1));
  std::cout << "Initialized Subscriber: " << subscriber1 << std::endl << std::flush;
  PTI_CHECK_SUCCESS(ptiCallbackSubscribe(&subscriber2, CallbackCommon, &subscriber2));
  std::cout << "Initialized Subscriber: " << subscriber2 << std::endl << std::flush;
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber1, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1));
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber1, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1));
  PTI_CHECK_SUCCESS(
      ptiCallbackEnableDomain(subscriber2, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1));

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

  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue queue(dev, sycl::async_handler{}, prop_list);  // Main runandcheck kernel

  std::cout << "DPC++ Matrix Multiplication (matrix size: " << size << " x " << size << ", repeats "
            << repeat_count << " times)" << std::endl;
  std::cout << "Target device: "
            << queue.get_info<sycl::info::queue::device>().get_info<sycl::info::device::name>()
            << std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  try {
    auto start = std::chrono::steady_clock::now();
    float expected_result = A_VALUE * B_VALUE * size;
    Compute(queue, a, b, c, size, repeat_count, expected_result);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time = end - start;
    std::cout << "Total execution time: " << time.count() << " sec" << std::endl;

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
  PTI_CHECK_SUCCESS(ptiCallbackUnsubscribe(subscriber1));
  PTI_CHECK_SUCCESS(ptiCallbackUnsubscribe(subscriber2));
  StopTracing();
  PTI_CHECK_SUCCESS(ptiFlushAllViews());

  return exit_code;
}
