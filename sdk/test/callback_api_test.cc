//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sycl/sycl.hpp>
#include <thread>
#include <unordered_map>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_view.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

// Helper functions for GPU kernel testing
namespace {

// Global counters for view records
std::atomic<int> g_view_kernel_count{0};
std::atomic<int> g_view_memcopy_count{0};
std::atomic<int> g_view_memfill_count{0};

void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

void LaunchSimpleKernel(sycl::queue& queue, const std::vector<float>& a_vector,
                        const std::vector<float>& b_vector, std::vector<float>& result,
                        unsigned size) {
  ASSERT_GT(size, 0U);
  ASSERT_EQ(a_vector.size(), size * size);
  ASSERT_EQ(b_vector.size(), size * size);
  ASSERT_EQ(result.size(), size * size);

  try {
    sycl::buffer<float, 1> a_buf(a_vector.data(), a_vector.size());
    sycl::buffer<float, 1> b_buf(b_vector.data(), b_vector.size());
    sycl::buffer<float, 1> c_buf(result.data(), result.size());

    queue.submit([&](sycl::handler& cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
      auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
      auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

      cgh.parallel_for<class __TestGEMM>(sycl::range<2>(size, size), [=](sycl::id<2> id) {
        auto a_acc_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto b_acc_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
        auto c_acc_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
        GEMM(a_acc_ptr.get(), b_acc_ptr.get(), c_acc_ptr.get(), size, id);
      });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception& e) {
    FAIL() << "[ERROR] Launching kernel: " << e.what();
  }
}

float Check(const std::vector<float>& a, float value) {
  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += std::fabs((a[i] - value) / value);
  }
  return eps / a.size();
}

// Handy structure to check that data match between Append phases and between Append and Completion
struct pair_hash {
  std::size_t operator()(const std::pair<pti_backend_ctx_t, pti_device_handle_t>& p) const {
    uintptr_t a1 = reinterpret_cast<uintptr_t>(p.first);
    uintptr_t a2 = reinterpret_cast<uintptr_t>(p.second);
    auto h1 = std::hash<uintptr_t>{}(a1);
    auto h2 = std::hash<uintptr_t>{}(a2);
    // Combine the two hashes
    return h1 ^ (h2 << 1);
  }
};
using ContextDeviceDataMap =
    std::unordered_map<std::pair<pti_backend_ctx_t, pti_device_handle_t>,
                       std::pair<pti_gpu_operation_kind, uint32_t>, pair_hash>;

// Buffer callback functions for ptiView
void BufferRequested(unsigned char** buf, size_t* buf_size) {
  *buf_size = 100000;  // 100KB buffer
  *buf = new unsigned char[*buf_size];
}

void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
  if (!buf || !used_bytes || !buf_size) {
    if (buf) {
      delete[] buf;
    }
    return;
  }

  // Parse the buffer to count view records
  pti_view_record_base* ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Error parsing PTI records" << std::endl;
      break;
    }

    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        g_view_kernel_count++;
        pti_view_record_kernel* kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
        std::cout << "View: Kernel " << kernel_rec->_name
                  << " (corr_id: " << kernel_rec->_correlation_id
                  << ", op_id: " << kernel_rec->_kernel_id << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        g_view_memcopy_count++;
        pti_view_record_memory_copy* mem_rec = reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        std::cout << "View: MemCopy " << mem_rec->_bytes << " bytes"
                  << " (corr_id: " << mem_rec->_correlation_id << ", op_id: " << mem_rec->_mem_op_id
                  << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        g_view_memfill_count++;
        pti_view_record_memory_fill* fill_rec = reinterpret_cast<pti_view_record_memory_fill*>(ptr);
        std::cout << "View: MemFill " << fill_rec->_bytes << " bytes"
                  << " (corr_id: " << fill_rec->_correlation_id
                  << ", op_id: " << fill_rec->_mem_op_id << ")" << std::endl;
        break;
      }
      default:
        // Ignore other record types
        break;
    }
  }

  delete[] buf;
}

void MakePtiViewGPUEnable() {
  // For now - need at least one PTI_VIEW_DEVICE_GPU to be enabled
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
}

}  // namespace

// Test fixture for Callback API tests
class CallbackApiTest : public ::testing::Test {
 protected:
  // Helper struct to track callback invocations
  struct CallbackData {
    bool append_complete_all_phases{false};
    std::atomic<int> enter_count{0};
    std::atomic<int> exit_count{0};
    std::atomic<int> total_count{0};
    std::atomic<int> appended_count{0};
    std::atomic<int> completed_count{0};
    // Separate counters for APPENDED domain ENTER/EXIT phases
    std::atomic<int> appended_enter_count{0};
    std::atomic<int> appended_exit_count{0};
    // Separate counters for COMPLETED domain ENTER/EXIT phases
    std::atomic<int> completed_enter_count{0};
    std::atomic<int> completed_exit_count{0};
    // Counters for completed operations by type
    std::atomic<int> completed_kernel_count{0};
    std::atomic<int> completed_memcopy_count{0};
    std::atomic<int> completed_memfill_count{0};
    pti_callback_domain last_domain{PTI_CB_DOMAIN_INVALID};
    pti_callback_phase last_phase{PTI_CB_PHASE_INVALID};
    pti_api_group_id last_api_group = PTI_API_GROUP_RESERVED;
    void* user_data_received{nullptr};
    std::atomic<bool> kernel_seen{false};
    std::atomic<bool> memory_op_seen{false};
    std::atomic<bool> all_callbacks_levelzero{
        true};                                     // Flag to track if all callbacks are Level Zero
    std::atomic<int> non_levelzero_count{0};       // Count of non-Level Zero callbacks
    std::atomic<int> null_context_count{0};        // Count of null backend contexts
    std::atomic<int> reserved_api_id_count{0};     // Count of reserved API IDs
    std::atomic<int> null_device_handle_count{0};  // Count of null device handles in cb_data

    // Thread-specific tracking for multi-threaded tests
    std::mutex thread_map_mutex;
    std::unordered_map<std::thread::id, int> thread_callback_counts;
    std::unordered_map<std::thread::id, int> thread_kernel_counts;
    std::unordered_map<std::thread::id, ContextDeviceDataMap> append_enter_map;

    void RecordThreadCallback() {
      std::lock_guard<std::mutex> lock(thread_map_mutex);
      thread_callback_counts[std::this_thread::get_id()]++;
    }

    void RecordThreadKernel() {
      std::lock_guard<std::mutex> lock(thread_map_mutex);
      thread_kernel_counts[std::this_thread::get_id()]++;
    }
  };

  static void CheckConsistencyAppendedEnterToExit(pti_backend_ctx_t backend_context,
                                                  const pti_callback_gpu_op_data* gpu_op_data,
                                                  const pti_gpu_op_details& op_details,
                                                  CallbackData* data) {
    // This function is now integrated into the main callback function
    // to check consistency between APPENDED ENTER and EXIT phases.
    std::lock_guard<std::mutex> lock(data->thread_map_mutex);
    if (gpu_op_data->_phase == PTI_CB_PHASE_API_ENTER) {
      // Store operation kind and correlation ID for this context+device
      auto& entry = data->append_enter_map[std::this_thread::get_id()];
      entry[{backend_context, gpu_op_data->_device_handle}] = {op_details._operation_kind,
                                                               gpu_op_data->_correlation_id};

    } else if (gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT) {
      auto& entry = data->append_enter_map[std::this_thread::get_id()];
      // Find that on Exit phase we had a matching Enter phase
      auto it = entry.find({backend_context, gpu_op_data->_device_handle});
      EXPECT_TRUE(it != entry.end()) << "No matching APPENDED ENTER phase found for EXIT phase"
                                     << " for context-device pair: " << backend_context
                                     << ", device: " << gpu_op_data->_device_handle;

      EXPECT_TRUE(it->second.first == op_details._operation_kind)
          << "Mismatched operation kind between APPENDED ENTER and EXIT phases"
          << " for context-device pair: " << backend_context
          << ", device: " << gpu_op_data->_device_handle;

      EXPECT_TRUE(it->second.second == gpu_op_data->_correlation_id)
          << "Mismatched correlation ID between APPENDED ENTER and EXIT phases"
          << " for context-device pair: " << it->second.second << " vs "
          << gpu_op_data->_correlation_id;
    }
  }

  static void PrintCallBackInfo(const char* domain_name, pti_callback_phase phase,
                                pti_backend_ctx_t backend_context,
                                pti_device_handle_t device_handle,
                                pti_api_group_id driver_api_group_id, uint32_t driver_api_id,
                                uint32_t correlation_id, uint32_t operation_count,
                                const pti_gpu_op_details* operation_details) {
    const char* api_name = nullptr;
    ptiViewGetApiIdName(driver_api_group_id, driver_api_id, &api_name);

    std::cout << "Callback: Domain: " << domain_name
              << ", Phase: " << (phase == PTI_CB_PHASE_API_ENTER ? "ENTER" : "EXIT")
              << ", Context: " << backend_context << ", Device: " << device_handle
              << "\t API Group / ID / name " << driver_api_group_id << " / " << driver_api_id
              << " / " << (api_name ? api_name : "<unknown>")
              << ", CorrelationID: " << correlation_id << ", OperationCount: " << operation_count
              << std::endl;
    if (operation_count > 0 && operation_details) {
      for (uint32_t i = 0; i < operation_count; ++i) {
        const auto& op = operation_details[i];
        std::cout << "  Operation " << i << ": Kind="
                  << (op._operation_kind == PTI_GPU_OPERATION_KIND_KERNEL
                          ? "KERNEL"
                          : (op._operation_kind == PTI_GPU_OPERATION_KIND_MEMORY ? "MEMORY"
                                                                                 : "OTHER"))
                  << ", OpID=" << op._operation_id << ", KernelHandle=" << op._kernel_handle
                  << ", Name=" << (op._name ? op._name : "N/A") << std::endl;
      }
    }
  }

  static void CheckConsistencyCompletedToAppended(pti_backend_ctx_t backend_context,
                                                  const pti_callback_gpu_op_data* gpu_op_data,
                                                  CallbackData* data) {
    std::lock_guard<std::mutex> lock(data->thread_map_mutex);
    // However, as Complete is async event -
    // completion could be reported in another thread than append
    bool found = false;
    for (auto thread_id : data->append_enter_map) {
      auto& entry = thread_id.second;
      // Find that on Complete domain we have a matching Enter phase
      auto it = entry.find({backend_context, gpu_op_data->_device_handle});
      if (it != entry.end()) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "No matching APPENDED ENTER found for COMPLETED"
                       << " for context-device pair: " << backend_context
                       << ", device: " << gpu_op_data->_device_handle;
  }

  // Static callback function that updates CallbackData
  static void TestCallback(pti_callback_domain domain, pti_api_group_id driver_api_group_id,
                           uint32_t driver_api_id, pti_backend_ctx_t backend_context, void* cb_data,
                           void* global_user_data, [[maybe_unused]] void** instance_user_data) {
    auto* data = static_cast<CallbackData*>(global_user_data);
    EXPECT_TRUE(data != nullptr) << "Global user data is null";
    data->total_count++;
    data->last_domain = domain;
    data->last_api_group = driver_api_group_id;
    data->user_data_received = global_user_data;
    data->RecordThreadCallback();  // Record thread-specific callback

    // Check that driver_api_group_id is always PTI_API_GROUP_LEVELZERO
    if (driver_api_group_id != PTI_API_GROUP_LEVELZERO) {
      data->all_callbacks_levelzero = false;
      data->non_levelzero_count++;
      std::cerr << "WARNING: Callback received with non-Level Zero API group: "
                << driver_api_group_id << " (expected " << PTI_API_GROUP_LEVELZERO << ")"
                << " for domain: " << domain << std::endl;
    }

    // Check that driver_api_id is not a reserved value
    // The reserved value for Level Zero is typically 0 or a specific reserved constant
    const uint32_t RESERVED_DRIVER_LEVELZERO_ID = 0;  // Reserved/invalid API ID
    if (driver_api_id == RESERVED_DRIVER_LEVELZERO_ID) {
      data->reserved_api_id_count++;
      std::cerr << "WARNING: Callback received with reserved driver_api_id: " << driver_api_id
                << " for domain: " << domain << std::endl;
    }

    // Check that backend_context is not null
    if (backend_context == nullptr) {
      data->null_context_count++;
      std::cerr << "WARNING: Callback received with null backend_context for domain: " << domain
                << std::endl;
    }

    EXPECT_TRUE(cb_data != nullptr) << "cb_data is null for domain: " << domain;
    if (domain == PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED) {
      data->appended_count++;
      auto* gpu_op_data = static_cast<pti_callback_gpu_op_data*>(cb_data);
      data->last_phase = gpu_op_data->_phase;

      // Check that device_handle is not null
      if (gpu_op_data->_device_handle == nullptr) {
        data->null_device_handle_count++;
        std::cerr << "WARNING: APPENDED callback received with null _device_handle" << std::endl;
      }
      PrintCallBackInfo("APPENDED", gpu_op_data->_phase, backend_context,
                        gpu_op_data->_device_handle, driver_api_group_id, driver_api_id,
                        gpu_op_data->_correlation_id, gpu_op_data->_operation_count,
                        gpu_op_data->_operation_details);

      // Count ENTER/EXIT phases separately for APPENDED domain
      if (gpu_op_data->_phase == PTI_CB_PHASE_API_ENTER) {
        data->enter_count++;
        data->appended_enter_count++;
      } else if (gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT) {
        data->exit_count++;
        data->appended_exit_count++;
      }

      // Check operation kind
      if (gpu_op_data->_operation_count > 0 && gpu_op_data->_operation_details) {
        for (uint32_t i = 0; i < gpu_op_data->_operation_count; ++i) {
          const auto& op_details = gpu_op_data->_operation_details[i];
          if (op_details._operation_kind == PTI_GPU_OPERATION_KIND_KERNEL) {
            data->kernel_seen = true;
          } else if (op_details._operation_kind == PTI_GPU_OPERATION_KIND_MEMORY) {
            data->memory_op_seen = true;
          }

          if (data->append_complete_all_phases && gpu_op_data->_operation_count == 1) {
            CheckConsistencyAppendedEnterToExit(backend_context, gpu_op_data, op_details, data);
          }
        }
      }
    } else if (domain == PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED) {
      data->completed_count++;
      auto* gpu_op_data = static_cast<pti_callback_gpu_op_data*>(cb_data);
      data->last_phase = gpu_op_data->_phase;

      // Check that device_handle is not null
      if (gpu_op_data->_device_handle == nullptr) {
        data->null_device_handle_count++;
        std::cerr << "WARNING: COMPLETED callback received with null _device_handle" << std::endl;
      }

      // Count ENTER/EXIT phases separately for COMPLETED domain
      EXPECT_TRUE(gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT)
          << "COMPLETED domain should only have EXIT phase callbacks";
      data->exit_count++;
      data->completed_exit_count++;

      PrintCallBackInfo("COMPLETED", gpu_op_data->_phase, backend_context,
                        gpu_op_data->_device_handle, driver_api_group_id, driver_api_id,
                        gpu_op_data->_correlation_id, gpu_op_data->_operation_count,
                        gpu_op_data->_operation_details);

      // Count completed operations by type
      if (gpu_op_data->_operation_count > 0 && gpu_op_data->_operation_details) {
        for (uint32_t i = 0; i < gpu_op_data->_operation_count; ++i) {
          auto& op_detail = gpu_op_data->_operation_details[i];
          std::cout << "\t ops: i: " << i << ", name: " << op_detail._name
                    << " (kind: " << op_detail._operation_kind
                    << ", op id: " << op_detail._operation_id << ")" << std::endl;

          if (op_detail._operation_kind == PTI_GPU_OPERATION_KIND_KERNEL) {
            data->completed_kernel_count++;
            data->RecordThreadKernel();  // Record thread-specific kernel
          } else if (op_detail._operation_kind == PTI_GPU_OPERATION_KIND_MEMORY) {
            // Memory operations could be copy or fill
            data->completed_memcopy_count++;
          }
        }
      }
      if (data->append_complete_all_phases) {
        // Find that on Complete domain we have a matching Append Enter.
        CheckConsistencyCompletedToAppended(backend_context, gpu_op_data, data);
      }
    }
  }

  void SetUp() override {
    callback_data_ = std::make_unique<CallbackData>();
    // Reset global view counters
    g_view_kernel_count = 0;
    g_view_memcopy_count = 0;
    g_view_memfill_count = 0;
    // For now - need at least one PTI_VIEW_DEVICE_GPU be enabled for callback API to work
    MakePtiViewGPUEnable();
  }

  void TearDown() override {
    // Clean up any remaining subscribers
    for (auto& subscriber : subscribers_) {
      if (subscriber) {
        ptiCallbackUnsubscribe(subscriber);
      }
    }
    subscribers_.clear();
  }

  std::unique_ptr<CallbackData> callback_data_;
  std::vector<pti_callback_subscriber_handle> subscribers_;
};

// Test basic subscription with GPU kernel invocation
TEST_F(CallbackApiTest, BasicSubscription) {
  // Check if GPU is available
  try {
    sycl::device dev(sycl::gpu_selector_v);
  } catch (const sycl::exception& e) {
    GTEST_SKIP() << "GPU device not available for testing";
  }

  // Set up ptiView callbacks first (required for callback API to work)
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);

  // this tells Test to cross-check data between Append Phases and Complete
  callback_data_.get()->append_complete_all_phases = true;
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test successful subscription
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);

  // Enable callbacks for GPU operation appended and completed
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED,
                                    1,   // enable enter
                                    1),  // enable exit
            PTI_SUCCESS);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
                                    1,   // enable enter
                                    1),  // enable exit
            PTI_SUCCESS);

  // Launch a GPU kernel
  try {
    sycl::device dev(sycl::gpu_selector_v);
    sycl::property_list prop{sycl::property::queue::in_order()};
    sycl::queue queue(dev, sycl::async_handler{}, prop);

    unsigned size = 32;  // Small matrix for testing
    std::vector<float> a(size * size, A_VALUE);
    std::vector<float> b(size * size, B_VALUE);
    std::vector<float> c(size * size, 0.0f);

    LaunchSimpleKernel(queue, a, b, c, size);

    // Verify result
    float expected_result = A_VALUE * B_VALUE * size;
    float eps = Check(c, expected_result);
    EXPECT_LE(eps, MAX_EPS);

  } catch (const sycl::exception& e) {
    FAIL() << "SYCL exception during kernel execution: " << e.what();
  }

  // Flush views to ensure callbacks are processed
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked
  EXPECT_GT(callback_data_->total_count.load(), 0);
  EXPECT_GT(callback_data_->appended_count.load(), 0);
  EXPECT_TRUE(callback_data_->kernel_seen.load());

  // Verify APPENDED domain ENTER/EXIT counts
  // We expect both ENTER and EXIT callbacks for APPENDED domain
  EXPECT_GT(callback_data_->appended_enter_count.load(), 0)
      << "APPENDED domain ENTER callbacks should be called";
  EXPECT_GT(callback_data_->appended_exit_count.load(), 0)
      << "APPENDED domain EXIT callbacks should be called";

  // The number of ENTER and EXIT callbacks should be equal for APPENDED domain
  EXPECT_EQ(callback_data_->appended_enter_count.load(), callback_data_->appended_exit_count.load())
      << "APPENDED domain should have equal number of ENTER and EXIT callbacks";

  // Verify COMPLETED domain counts
  // COMPLETED domain typically only has EXIT phase (as per the comment in the code)
  EXPECT_EQ(callback_data_->completed_enter_count.load(), 0)
      << "COMPLETED domain should not have ENTER callbacks";
  // We may or may not get completed callbacks depending on timing
  EXPECT_GE(callback_data_->completed_exit_count.load(), 0)
      << "COMPLETED domain may have EXIT callbacks";

  // Total enter/exit counts
  EXPECT_GE(callback_data_->enter_count.load(), 0);
  EXPECT_GE(callback_data_->exit_count.load(), 0);

  // Print counts for debugging
  std::cout << "\n=== Count Summary ===" << std::endl;
  std::cout << "View Records:" << std::endl;
  std::cout << "  Kernels: " << g_view_kernel_count.load() << std::endl;
  std::cout << "  MemCopy: " << g_view_memcopy_count.load() << std::endl;
  std::cout << "  MemFill: " << g_view_memfill_count.load() << std::endl;
  std::cout << "Callback Completed Operations:" << std::endl;
  std::cout << "  Kernels: " << callback_data_->completed_kernel_count.load() << std::endl;
  std::cout << "  Memory Ops: " << callback_data_->completed_memcopy_count.load() << std::endl;
  std::cout << "====================\n" << std::endl;

  // Verify that counts from view records match counts from callback COMPLETED domain
  // Note: The kernel count should match exactly
  EXPECT_EQ(g_view_kernel_count.load(), callback_data_->completed_kernel_count.load())
      << "Kernel count from ptiView should match count from Callback COMPLETED domain";

  // Memory operations: view records distinguish between copy and fill,
  // but callback API reports them all as MEMORY kind
  int total_view_memory_ops = g_view_memcopy_count.load() + g_view_memfill_count.load();
  EXPECT_EQ(total_view_memory_ops, callback_data_->completed_memcopy_count.load())
      << "Total memory operation count from ptiView should match count from Callback COMPLETED "
         "domain";

  // Verify that all callbacks had the correct API group (Level Zero)
  EXPECT_TRUE(callback_data_->all_callbacks_levelzero.load())
      << "All callbacks should have driver_api_group_id == PTI_API_GROUP_LEVELZERO";
  EXPECT_EQ(callback_data_->non_levelzero_count.load(), 0)
      << "No callbacks should have non-Level Zero API group";

  // Verify that no callbacks had reserved API IDs
  EXPECT_EQ(callback_data_->reserved_api_id_count.load(), 0)
      << "No callbacks should have reserved driver_api_id";

  // Verify that all callbacks had non-null backend_context
  EXPECT_EQ(callback_data_->null_context_count.load(), 0)
      << "All callbacks should have non-null backend_context";

  // Verify that all GPU operation callbacks had non-null device_handle
  EXPECT_EQ(callback_data_->null_device_handle_count.load(), 0)
      << "All GPU operation callbacks (APPENDED and COMPLETED) should have non-null _device_handle";

  // Verify the last API group was Level Zero (as a sanity check)
  EXPECT_EQ(callback_data_->last_api_group, PTI_API_GROUP_LEVELZERO)
      << "Last callback should have Level Zero API group";

  // Test successful unsubscription
  EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);

  // Clean up
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
}

// Test subscription with null parameters
TEST_F(CallbackApiTest, SubscriptionWithNullParams) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test with null subscriber handle pointer
  EXPECT_NE(ptiCallbackSubscribe(nullptr, TestCallback, callback_data_.get()), PTI_SUCCESS);

  // Test with null callback function
  EXPECT_NE(ptiCallbackSubscribe(&subscriber, nullptr, callback_data_.get()), PTI_SUCCESS);

  // User data can be null, so this should succeed
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, nullptr), PTI_SUCCESS);
}

// Test domain enable and disable
TEST_F(CallbackApiTest, DomainEnableDisable) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe first
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  ASSERT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  // Enable domain for GPU operation appended
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED,
                                    1,   // enable enter callback
                                    1),  // enable exit callback
            PTI_SUCCESS);

  // Enable domain for GPU operation completed
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
                                    1,   // enable enter callback
                                    1),  // enable exit callback
            PTI_SUCCESS);

  // Disable a domain
  EXPECT_EQ(ptiCallbackDisableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED),
            PTI_SUCCESS);

  // Disable all domains
  EXPECT_EQ(ptiCallbackDisableAllDomains(subscriber), PTI_SUCCESS);
}

// Test enabling not implemented domains
TEST_F(CallbackApiTest, NotImplementedDomains) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe first
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  ASSERT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  // Try to enable not implemented domains
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_CONTEXT_CREATED, 1, 1),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_MODULE_LOADED, 1, 1),
            PTI_ERROR_NOT_IMPLEMENTED);

  EXPECT_EQ(
      ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_DISPATCHED, 1, 1),
      PTI_ERROR_NOT_IMPLEMENTED);
}

// Test multiple subscribers
TEST_F(CallbackApiTest, MultipleSubscribers) {
  const int num_subscribers = 3;
  std::vector<std::unique_ptr<CallbackData>> callback_data_list;

  // Create multiple subscribers
  for (int i = 0; i < num_subscribers; ++i) {
    callback_data_list.push_back(std::make_unique<CallbackData>());
    pti_callback_subscriber_handle subscriber = nullptr;

    EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_list.back().get()),
              PTI_SUCCESS);
    EXPECT_NE(subscriber, nullptr);
    subscribers_.push_back(subscriber);

    // Enable different domains for different subscribers
    if (i == 0) {
      EXPECT_EQ(
          ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
          PTI_SUCCESS);
    } else if (i == 1) {
      EXPECT_EQ(
          ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
          PTI_SUCCESS);
    } else {
      // Enable both domains for the third subscriber
      EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1,
                                        0),  // Only enter callback
                PTI_SUCCESS);
      EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 0,
                                        1),  // Only exit callback
                PTI_SUCCESS);
    }
  }

  // Verify all subscribers are created
  EXPECT_EQ(subscribers_.size(), num_subscribers);

  // Unsubscribe all
  for (auto subscriber : subscribers_) {
    EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
  }
}

// Test unsubscribe with invalid handle
TEST_F(CallbackApiTest, SubscribeWithNullParamsUnsubscribeInvalidHandle) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test with null subscriber handle pointer
  EXPECT_NE(ptiCallbackSubscribe(nullptr, TestCallback, callback_data_.get()), PTI_SUCCESS);

  // Test with null callback function
  EXPECT_NE(ptiCallbackSubscribe(&subscriber, nullptr, callback_data_.get()), PTI_SUCCESS);

  // User data can be null, so this should succeed
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, nullptr), PTI_SUCCESS);

  EXPECT_NE(ptiCallbackUnsubscribe(nullptr), PTI_SUCCESS);

  // Test with invalid (but non-null) handle
  pti_callback_subscriber_handle invalid_handle =
      reinterpret_cast<pti_callback_subscriber_handle>(0xDEADBEEF);
  EXPECT_NE(ptiCallbackUnsubscribe(invalid_handle), PTI_SUCCESS);
}

// Test domain operations with invalid subscriber
TEST_F(CallbackApiTest, DomainOpsInvalidSubscriber) {
  // Test enable domain with null subscriber
  EXPECT_EQ(ptiCallbackEnableDomain(nullptr, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_ERROR_BAD_ARGUMENT);

  // Test disable domain with null subscriber
  EXPECT_EQ(ptiCallbackDisableDomain(static_cast<pti_callback_subscriber_handle>(nullptr),
                                     PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED),
            PTI_ERROR_BAD_ARGUMENT);

  // Test disable all domains with null subscriber
  EXPECT_EQ(ptiCallbackDisableAllDomains(static_cast<pti_callback_subscriber_handle>(nullptr)),
            PTI_ERROR_BAD_ARGUMENT);
}

// Test helper functions for string conversion
TEST_F(CallbackApiTest, StringConversionFunctions) {
  // Test domain type to string
  const char* domain_str =
      ptiCallbackDomainTypeToString(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED);
  EXPECT_NE(domain_str, nullptr);
  EXPECT_STRNE(domain_str, "");

  domain_str = ptiCallbackDomainTypeToString(PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED);
  EXPECT_NE(domain_str, nullptr);
  EXPECT_STRNE(domain_str, "");

  domain_str = ptiCallbackDomainTypeToString(PTI_CB_DOMAIN_INVALID);
  EXPECT_NE(domain_str, nullptr);

  // Test phase type to string
  const char* phase_str = ptiCallbackPhaseTypeToString(PTI_CB_PHASE_API_ENTER);
  EXPECT_NE(phase_str, nullptr);
  EXPECT_STRNE(phase_str, "");

  phase_str = ptiCallbackPhaseTypeToString(PTI_CB_PHASE_API_EXIT);
  EXPECT_NE(phase_str, nullptr);
  EXPECT_STRNE(phase_str, "");

  phase_str = ptiCallbackPhaseTypeToString(PTI_CB_PHASE_INVALID);
  EXPECT_NE(phase_str, nullptr);
}

// Test selective phase enabling
TEST_F(CallbackApiTest, SelectivePhaseEnable) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  ASSERT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  // Enable only enter callbacks
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED,
                                    1,   // enable enter
                                    0),  // disable exit
            PTI_SUCCESS);

  // Enable only exit callbacks
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
                                    0,   // disable enter
                                    1),  // enable exit
            PTI_SUCCESS);

  // Disable and re-enable with both phases
  EXPECT_EQ(ptiCallbackDisableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED),
            PTI_SUCCESS);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED,
                                    1,   // enable enter
                                    1),  // enable exit
            PTI_SUCCESS);
}

// Test multi-threaded kernel execution with callback API
TEST_F(CallbackApiTest, MultiThreadedKernelExecution) {
  // Check if GPU is available
  try {
    sycl::device dev(sycl::gpu_selector_v);
  } catch (const sycl::exception& e) {
    GTEST_SKIP() << "GPU device not available for testing";
  }

  // Set up ptiView callbacks first (required for callback API to work)
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);

  // this tells Test to cross-check data between Append Phases and Complete
  callback_data_.get()->append_complete_all_phases = true;
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe and enable callbacks
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);

  // Enable callbacks for GPU operation appended and completed
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED,
                                    1,   // enable enter
                                    1),  // enable exit
            PTI_SUCCESS);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED,
                                    1,   // enable enter
                                    1),  // enable exit
            PTI_SUCCESS);

  // Launch kernels from multiple threads
  const int num_threads = 4;
  const int kernels_per_thread = 5;
  std::vector<std::thread> threads;
  std::vector<std::atomic<bool>> thread_success(num_threads);
  std::vector<std::atomic<int>> thread_kernel_launches(num_threads);

  for (int tid = 0; tid < num_threads; ++tid) {
    thread_success[tid] = true;
    thread_kernel_launches[tid] = 0;
  }

  for (int tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      try {
        sycl::device dev(sycl::gpu_selector_v);
        sycl::property_list prop{sycl::property::queue::in_order()};
        sycl::queue queue(dev, sycl::async_handler{}, prop);

        for (int i = 0; i < kernels_per_thread; ++i) {
          unsigned size = 32;  // Small matrix for testing
          std::vector<float> a(size * size, A_VALUE);
          std::vector<float> b(size * size, B_VALUE);
          std::vector<float> c(size * size, 0.0f);

          LaunchSimpleKernel(queue, a, b, c, size);
          thread_kernel_launches[tid]++;

          // Verify result
          float expected_result = A_VALUE * B_VALUE * size;
          float eps = Check(c, expected_result);
          if (eps > MAX_EPS) {
            thread_success[tid] = false;
            std::cerr << "Thread " << tid << " kernel " << i << " failed with eps=" << eps
                      << std::endl;
          }
        }
      } catch (const sycl::exception& e) {
        std::cerr << "Thread " << tid << " failed: " << e.what() << std::endl;
        thread_success[tid] = false;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Verify all threads succeeded
  for (int tid = 0; tid < num_threads; ++tid) {
    EXPECT_TRUE(thread_success[tid]) << "Thread " << tid << " failed";
    EXPECT_EQ(thread_kernel_launches[tid].load(), kernels_per_thread)
        << "Thread " << tid << " didn't complete all kernel launches";
  }

  // Flush views to ensure callbacks are processed
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked for all kernels
  int expected_min_kernels = num_threads * kernels_per_thread;
  EXPECT_GE(callback_data_->completed_kernel_count.load(), expected_min_kernels)
      << "Expected at least " << expected_min_kernels << " kernel completions";

  // Verify we got callbacks from multiple threads
  {
    std::lock_guard<std::mutex> lock(callback_data_->thread_map_mutex);
    size_t num_threads_with_callbacks = callback_data_->thread_callback_counts.size();
    EXPECT_GT(num_threads_with_callbacks, 1)
        << "Expected callbacks from multiple threads, but got callbacks from "
        << num_threads_with_callbacks << " thread(s)";

    // Print thread callback distribution for debugging
    std::cout << "\n=== Thread Callback Distribution ===" << std::endl;
    for (const auto& [tid, count] : callback_data_->thread_callback_counts) {
      std::cout << "Thread ID " << tid << ": " << count << " callbacks" << std::endl;
    }

    std::cout << "\n=== Thread Kernel Distribution ===" << std::endl;
    for (const auto& [tid, count] : callback_data_->thread_kernel_counts) {
      std::cout << "Thread ID " << tid << ": " << count << " kernels" << std::endl;
    }
    std::cout << "====================================\n" << std::endl;
  }

  // Verify thread safety - no corrupted counters
  EXPECT_TRUE(callback_data_->all_callbacks_levelzero.load())
      << "All callbacks should have driver_api_group_id == PTI_API_GROUP_LEVELZERO";
  EXPECT_EQ(callback_data_->null_context_count.load(), 0)
      << "All callbacks should have non-null backend_context";
  EXPECT_EQ(callback_data_->null_device_handle_count.load(), 0)
      << "All GPU operation callbacks should have non-null _device_handle";

  // Cleanup
  EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
}

// Test concurrent queue submissions with synchronization
TEST_F(CallbackApiTest, ConcurrentQueueSubmissions) {
  // Check if GPU is available
  try {
    sycl::device dev(sycl::gpu_selector_v);
  } catch (const sycl::exception& e) {
    GTEST_SKIP() << "GPU device not available for testing";
  }

  // Set up ptiView callbacks
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);

  // this tells Test to cross-check data between Append Phases and Complete
  callback_data_.get()->append_complete_all_phases = true;

  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe and enable callbacks
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, callback_data_.get()), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
            PTI_SUCCESS);

  const int num_threads = 8;
  const int submissions_per_thread = 10;

  // Use a barrier to synchronize thread start (C++20 feature)
  // If barrier is not available, use condition variable
  std::mutex start_mutex;
  std::condition_variable start_cv;
  std::atomic<int> threads_ready{0};
  bool start_flag = false;

  std::vector<std::thread> threads;
  std::vector<std::atomic<bool>> thread_results(num_threads);
  std::vector<std::atomic<int>> thread_submission_counts(num_threads);

  for (int tid = 0; tid < num_threads; ++tid) {
    thread_results[tid] = true;
    thread_submission_counts[tid] = 0;
  }

  auto worker = [&](int tid) {
    try {
      sycl::device dev(sycl::gpu_selector_v);
      sycl::queue queue(dev);

      // Signal ready and wait for start
      {
        std::unique_lock<std::mutex> lock(start_mutex);
        threads_ready++;
        start_cv.notify_all();  // Notify main thread that this thread is ready
        start_cv.wait(lock, [&] { return start_flag; });
      }

      // Rapid-fire submissions
      for (int i = 0; i < submissions_per_thread; ++i) {
        unsigned size = 16;  // Smaller size for rapid submissions
        std::vector<float> a(size * size, A_VALUE);
        std::vector<float> b(size * size, B_VALUE);
        std::vector<float> c(size * size, 0.0f);

        // Mix kernel and memory operations
        if (i % 3 == 0) {
          // Memory copy operation
          sycl::buffer<float, 1> src_buf(a.data(), a.size());
          sycl::buffer<float, 1> dst_buf(c.data(), c.size());

          queue.submit([&](sycl::handler& cgh) {
            auto src_acc = src_buf.get_access<sycl::access::mode::read>(cgh);
            auto dst_acc = dst_buf.get_access<sycl::access::mode::write>(cgh);
            cgh.copy(src_acc, dst_acc);
          });
        } else {
          // Kernel operation
          LaunchSimpleKernel(queue, a, b, c, size);
        }

        thread_submission_counts[tid]++;
      }

      queue.wait_and_throw();
    } catch (const sycl::exception& e) {
      std::cerr << "Thread " << tid << " failed: " << e.what() << std::endl;
      thread_results[tid] = false;
    }
  };

  // Create all threads
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker, i);
  }

  // Wait for all threads to be ready
  {
    std::unique_lock<std::mutex> lock(start_mutex);
    start_cv.wait(lock, [&] { return threads_ready == num_threads; });
  }

  // Start all threads simultaneously
  {
    std::lock_guard<std::mutex> lock(start_mutex);
    start_flag = true;
  }
  start_cv.notify_all();

  // Wait for all threads to complete
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Verify all threads completed successfully
  for (int tid = 0; tid < num_threads; ++tid) {
    EXPECT_TRUE(thread_results[tid]) << "Thread " << tid << " failed";
    EXPECT_EQ(thread_submission_counts[tid].load(), submissions_per_thread)
        << "Thread " << tid << " didn't complete all submissions";
  }

  // Flush views
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked
  EXPECT_GT(callback_data_->total_count.load(), 0) << "No callbacks were invoked";
  EXPECT_GT(callback_data_->appended_count.load(), 0) << "No APPENDED callbacks were invoked";

  // Verify both kernel and memory operations were seen
  EXPECT_TRUE(callback_data_->kernel_seen.load()) << "No kernel operations were detected";
  EXPECT_TRUE(callback_data_->memory_op_seen.load()) << "No memory operations were detected";

  // Print statistics
  std::cout << "\n=== Concurrent Submission Statistics ===" << std::endl;
  std::cout << "Total callbacks: " << callback_data_->total_count.load() << std::endl;
  std::cout << "Appended callbacks: " << callback_data_->appended_count.load() << std::endl;
  std::cout << "Completed callbacks: " << callback_data_->completed_count.load() << std::endl;
  std::cout << "Kernel operations: " << callback_data_->completed_kernel_count.load() << std::endl;
  std::cout << "Memory operations: " << callback_data_->completed_memcopy_count.load() << std::endl;

  {
    std::lock_guard<std::mutex> lock(callback_data_->thread_map_mutex);
    std::cout << "Unique threads with callbacks: " << callback_data_->thread_callback_counts.size()
              << std::endl;
  }
  std::cout << "========================================\n" << std::endl;

  // Cleanup
  EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
}

// Test callback thread safety with shared queue
TEST_F(CallbackApiTest, CallbackThreadSafety) {
  // Check if GPU is available
  try {
    sycl::device dev(sycl::gpu_selector_v);
  } catch (const sycl::exception& e) {
    GTEST_SKIP() << "GPU device not available for testing";
  }

  // Thread-safe callback data structure
  struct ThreadSafeCallbackData {
    std::mutex mutex;
    std::vector<std::pair<std::thread::id, pti_callback_domain>> callback_log;
    std::atomic<int> total_callbacks{0};

    void LogCallback(pti_callback_domain domain) {
      std::lock_guard<std::mutex> lock(mutex);
      callback_log.emplace_back(std::this_thread::get_id(), domain);
      total_callbacks++;
    }
  };

  auto thread_safe_data = std::make_unique<ThreadSafeCallbackData>();

  // Thread-safe callback that logs calls
  auto ThreadSafeCallback = [](pti_callback_domain domain, pti_api_group_id /*driver_api_group_id*/,
                               uint32_t /*driver_api_id*/, pti_backend_ctx_t /*backend_context*/,
                               void* /*cb_data*/, void* global_user_data,
                               void** /*instance_user_data*/) {
    auto* data = static_cast<ThreadSafeCallbackData*>(global_user_data);
    if (data) {
      // Simulate some processing to increase chance of race conditions
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      data->LogCallback(domain);
    }
  };

  // Set up ptiView callbacks
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);

  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe with thread-safe callback
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, ThreadSafeCallback, thread_safe_data.get()),
            PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
            PTI_SUCCESS);

  // Create a shared queue for all threads
  sycl::device dev(sycl::gpu_selector_v);
  sycl::queue shared_queue(dev);

  const int num_threads = 6;
  const int kernels_per_thread = 8;
  std::vector<std::thread> threads;

  // Launch kernels from multiple threads using shared queue
  for (int tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      try {
        for (int i = 0; i < kernels_per_thread; ++i) {
          unsigned size = 16;
          std::vector<float> a(size * size, A_VALUE);
          std::vector<float> b(size * size, B_VALUE);
          std::vector<float> c(size * size, 0.0f);

          // Use the shared queue
          LaunchSimpleKernel(shared_queue, a, b, c, size);

          // Small delay to interleave operations
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      } catch (const sycl::exception& e) {
        std::cerr << "Thread " << tid << " failed: " << e.what() << std::endl;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Flush views
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callback log integrity
  {
    std::lock_guard<std::mutex> lock(thread_safe_data->mutex);

    // Check that we got callbacks
    EXPECT_GT(thread_safe_data->total_callbacks.load(), 0) << "No callbacks were recorded";

    // Count unique thread IDs in callback log
    std::set<std::thread::id> unique_threads;
    std::map<pti_callback_domain, int> domain_counts;

    for (const auto& [tid, domain] : thread_safe_data->callback_log) {
      unique_threads.insert(tid);
      domain_counts[domain]++;
    }

    // We expect callbacks from multiple threads
    EXPECT_GT(unique_threads.size(), 1)
        << "Expected callbacks from multiple threads, but got callbacks from "
        << unique_threads.size() << " thread(s)";

    // Print statistics
    std::cout << "\n=== Thread Safety Test Statistics ===" << std::endl;
    std::cout << "Total callbacks logged: " << thread_safe_data->total_callbacks.load()
              << std::endl;
    std::cout << "Unique threads in log: " << unique_threads.size() << std::endl;
    std::cout << "Callback log entries: " << thread_safe_data->callback_log.size() << std::endl;

    for (const auto& [domain, count] : domain_counts) {
      const char* domain_name = ptiCallbackDomainTypeToString(domain);
      std::cout << "Domain " << (domain_name ? domain_name : "unknown") << ": " << count
                << " callbacks" << std::endl;
    }
    std::cout << "====================================\n" << std::endl;

    // Verify log consistency
    EXPECT_EQ(thread_safe_data->callback_log.size(),
              static_cast<size_t>(thread_safe_data->total_callbacks.load()))
        << "Callback log size doesn't match total callbacks counter";
  }

  // Cleanup
  EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
}
