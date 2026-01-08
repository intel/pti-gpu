//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <fmt/format.h>
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
#include <utility>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_view.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

// ============================================================================
// TEST CONSTANTS
// ============================================================================
namespace {

// Buffer sizes
constexpr size_t kViewBufferSize = 100'000;  // 100KB buffer for PTI view records

// Matrix and kernel sizes
constexpr unsigned kDefaultMatrixSize = 32;  // Default size for test matrices
constexpr unsigned kSmallMatrixSize = 16;    // Smaller size for rapid tests

// Test iteration counts
constexpr int kDefaultKernelCount = 5;       // Default number of kernels to launch
constexpr int kThreadSafetyKernelCount = 8;  // Kernels per thread in thread safety test
constexpr int kConcurrentSubmissions = 10;   // Submissions per thread in concurrent test

// Thread counts
constexpr int kDefaultThreadCount = 4;       // Default number of threads
constexpr int kConcurrentThreadCount = 8;    // Threads for concurrent queue test
constexpr int kThreadSafetyThreadCount = 6;  // Threads for thread safety test

// External correlation
constexpr uint64_t kExternalIdStart = 1000;  // Starting external correlation ID

// ============================================================================
// TEST STRUCTURES
// ============================================================================

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

// Data structure for external correlation test tracking
struct ExternalCorrTestData {
  // Callback tracking
  std::atomic<uint64_t> next_external_id{kExternalIdStart};
  std::map<uint32_t, uint64_t>
      callback_corr_to_external;  // correlation_id -> external_id we pushed
  std::atomic<int> push_count{0};
  std::atomic<int> pop_count{0};
  std::atomic<int> push_errors{0};
  std::atomic<int> pop_errors{0};

  // View record tracking
  std::map<uint32_t, uint32_t> view_driver_api_records;   // correlation_id -> api_id
  std::map<uint32_t, uint32_t> view_runtime_api_records;  // correlation_id -> api_id
  std::map<uint64_t, uint32_t> view_external_to_corr;     // external_id -> correlation_id

  // Ordering check tracking
  std::set<uint32_t> external_corr_seen_so_far;  // for ordering check
  std::set<uint32_t> callback_pushed_corr_ids;   // correlation_ids we pushed in callbacks

  // Violation tracking
  struct OrderViolation {
    uint32_t correlation_id;
    uint32_t api_id;
  };
  std::vector<OrderViolation> ordering_violations;
};

// Data structure for callback tracking (used by most tests)
struct CallbackData {
  // View record counters
  std::atomic<int> view_kernel_count{0};
  std::atomic<int> view_memcopy_count{0};
  std::atomic<int> view_memfill_count{0};

  // Callback invocation tracking
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

  // Last seen values
  pti_callback_domain last_domain{PTI_CB_DOMAIN_INVALID};
  pti_callback_phase last_phase{PTI_CB_PHASE_INVALID};
  pti_api_group_id last_api_group = PTI_API_GROUP_RESERVED;
  void* user_data_received{nullptr};

  // Operation type flags
  std::atomic<bool> kernel_seen{false};
  std::atomic<bool> memory_op_seen{false};

  // Validation counters
  std::atomic<bool> all_callbacks_levelzero{true};
  std::atomic<int> non_levelzero_count{0};
  std::atomic<int> null_context_count{0};
  std::atomic<int> reserved_api_id_count{0};
  std::atomic<int> null_device_handle_count{0};

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

  bool do_external_correlation_test{false};
  ExternalCorrTestData ext_correlation_data{};

  // ============================================================================
  // Operation ID tracking fields
  // ============================================================================

  // Uniqueness tracking - separate by operation kind
  std::mutex operation_id_mutex;
  std::set<uint64_t> seen_kernel_operation_ids;  // Track uniqueness for kernels
  std::set<uint64_t> seen_memory_operation_ids;  // Track uniqueness for memory ops

  // Track operation_id to correlation_id mapping to detect true duplicates
  // (same operation_id used for different operations/correlation_ids)
  std::map<uint64_t, uint32_t>
      kernel_id_to_first_corr_id;  // kernel_id -> first correlation_id seen
  std::map<uint64_t, uint32_t>
      memory_id_to_first_corr_id;  // mem_op_id -> first correlation_id seen

  // API ID consistency tracking (driver_api_id is stable, name might be empty in ENTER phase)
  std::map<uint64_t, uint32_t> kernel_id_to_api_id;  // kernel_id -> driver_api_id
  std::map<uint64_t, uint32_t> memory_id_to_api_id;  // mem_op_id -> driver_api_id

  // Cross-reference between Callback domains (APPENDED vs COMPLETED)
  std::map<uint64_t, uint32_t> appended_kernel_id_to_corr_id;   // kernel_id -> correlation_id
  std::map<uint64_t, uint32_t> appended_memory_id_to_corr_id;   // mem_op_id -> correlation_id
  std::map<uint64_t, uint32_t> completed_kernel_id_to_corr_id;  // kernel_id -> correlation_id
  std::map<uint64_t, uint32_t> completed_memory_id_to_corr_id;  // mem_op_id -> correlation_id

  // View API tracking (for cross-validation between Callback and View APIs)
  std::map<uint64_t, uint32_t> view_kernel_id_to_corr_id;  // kernel_id -> correlation_id
  std::map<uint64_t, uint32_t> view_memop_id_to_corr_id;   // mem_op_id -> correlation_id

  // Validation error counters
  std::atomic<int> duplicate_kernel_ids{0};
  std::atomic<int> duplicate_memory_ids{0};
  std::atomic<int> zero_operation_ids{0};
  std::atomic<int> kernel_api_id_mismatch{0};
  std::atomic<int> memory_api_id_mismatch{0};
  std::atomic<int> view_callback_id_mismatch{0};

  // Lifecycle validation error counters
  std::atomic<int> completed_without_appended{0};  // Operations completed but never appended
  std::atomic<int> appended_without_completed{0};  // Operations appended but never completed
};

// ============================================================================
// GLOBAL POINTERS
// ============================================================================

// Global pointers to test data (used by BufferCompleted and other callbacks)
CallbackData* g_callback_data = nullptr;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void GEMM(const float* a, const float* b, float* c, unsigned size, sycl::id<2> id) {
  int i = id.get(0);
  int j = id.get(1);
  float sum = 0.0f;
  for (unsigned k = 0; k < size; ++k) {
    sum += a[i * size + k] * b[k * size + j];
  }
  c[i * size + j] = sum;
}

void LaunchMultipleGEMMKernels(sycl::queue& queue, const std::vector<float>& a_vector,
                               const std::vector<float>& b_vector, std::vector<float>& result,
                               unsigned size, int repeat_count) {
  ASSERT_GT(size, 0U);
  ASSERT_EQ(a_vector.size(), size * size);
  ASSERT_EQ(b_vector.size(), size * size);
  ASSERT_EQ(result.size(), size * size);

  try {
    for (int iter = 0; iter < repeat_count; ++iter) {
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
    }
    // Important that wait is outside of the loop to avoid serializing kernel launches
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

// Buffer callback functions for ptiView
void BufferRequested(unsigned char** buf, size_t* buf_size) {
  *buf_size = kViewBufferSize;
  *buf = new unsigned char[*buf_size];
}

// Buffer completion callback function for ptiView
// Statistics are collected here for further verification
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

    // Get pointer to external correlation test data if external correlation testing is enabled
    auto* external_corr_test_data =
        (g_callback_data && g_callback_data->do_external_correlation_test)
            ? &g_callback_data->ext_correlation_data
            : nullptr;

    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        auto* kernel_rec = reinterpret_cast<pti_view_record_kernel*>(ptr);
        if (g_callback_data) {
          g_callback_data->view_kernel_count++;
          // Track kernel operation ID from view records and check for duplicates
          std::lock_guard<std::mutex> lock(g_callback_data->operation_id_mutex);
          auto [it, inserted] = g_callback_data->view_kernel_id_to_corr_id.insert(
              {kernel_rec->_kernel_id, kernel_rec->_correlation_id});
          if (!inserted) {
            // Duplicate kernel_id in view records - this is an error!
            g_callback_data->duplicate_kernel_ids++;
            std::cerr << "ERROR: Duplicate kernel_id " << kernel_rec->_kernel_id
                      << " in View records (correlation_ids: " << it->second << " vs "
                      << kernel_rec->_correlation_id << ")" << std::endl;
          }
        }
        std::cout << "View: Kernel " << kernel_rec->_name
                  << " (corr_id: " << kernel_rec->_correlation_id
                  << ", op_id: " << kernel_rec->_kernel_id << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        auto* mem_rec = reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        if (g_callback_data) {
          g_callback_data->view_memcopy_count++;
          // Track memory operation ID from view records and check for duplicates
          std::lock_guard<std::mutex> lock(g_callback_data->operation_id_mutex);
          auto [it, inserted] = g_callback_data->view_memop_id_to_corr_id.insert(
              {mem_rec->_mem_op_id, mem_rec->_correlation_id});
          if (!inserted) {
            // Duplicate mem_op_id in view records - this is an error!
            g_callback_data->duplicate_memory_ids++;
            std::cerr << "ERROR: Duplicate mem_op_id " << mem_rec->_mem_op_id
                      << " in View records (correlation_ids: " << it->second << " vs "
                      << mem_rec->_correlation_id << ")" << std::endl;
          }
        }
        std::cout << "View: MemCopy " << mem_rec->_bytes << " bytes"
                  << " (corr_id: " << mem_rec->_correlation_id << ", op_id: " << mem_rec->_mem_op_id
                  << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        auto* fill_rec = reinterpret_cast<pti_view_record_memory_fill*>(ptr);
        if (g_callback_data) {
          g_callback_data->view_memfill_count++;
          // Track memory operation ID from view records and check for duplicates
          std::lock_guard<std::mutex> lock(g_callback_data->operation_id_mutex);
          auto [it, inserted] = g_callback_data->view_memop_id_to_corr_id.insert(
              {fill_rec->_mem_op_id, fill_rec->_correlation_id});
          if (!inserted) {
            // Duplicate mem_op_id in view records - this is an error!
            g_callback_data->duplicate_memory_ids++;
            std::cerr << "ERROR: Duplicate mem_op_id " << fill_rec->_mem_op_id
                      << " in View records (correlation_ids: " << it->second << " vs "
                      << fill_rec->_correlation_id << ")" << std::endl;
          }
        }
        std::cout << "View: MemFill " << fill_rec->_bytes << " bytes"
                  << " (corr_id: " << fill_rec->_correlation_id
                  << ", op_id: " << fill_rec->_mem_op_id << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        auto* rec = reinterpret_cast<pti_view_record_external_correlation*>(ptr);
        auto ext_id = rec->_external_id;
        auto corr_id = rec->_correlation_id;

        if (external_corr_test_data) {
          external_corr_test_data->view_external_to_corr[ext_id] = corr_id;
          external_corr_test_data->external_corr_seen_so_far.insert(corr_id);
        }

        std::cout << "View: External Correlation (external_id=" << ext_id
                  << ", correlation_id=" << corr_id << ")" << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_DRIVER_API: {
        auto* rec = reinterpret_cast<pti_view_record_api*>(ptr);
        auto corr_id = rec->_correlation_id;
        auto api_id = rec->_api_id;

        if (external_corr_test_data) {
          external_corr_test_data->view_driver_api_records[corr_id] = api_id;

          // CHECK: Only for Driver API records whose correlation_id we pushed external correlation
          // for should we expect to see a preceding external correlation record
          if (external_corr_test_data->callback_pushed_corr_ids.find(corr_id) !=
              external_corr_test_data->callback_pushed_corr_ids.end()) {
            // This is a Driver API we pushed external correlation for
            if (external_corr_test_data->external_corr_seen_so_far.find(corr_id) ==
                external_corr_test_data->external_corr_seen_so_far.end()) {
              // VIOLATION: Driver API appeared without preceding external correlation
              external_corr_test_data->ordering_violations.push_back({corr_id, api_id});
              std::cerr << "WARNING: Driver API record (correlation_id=" << corr_id
                        << ", api_id=" << api_id
                        << ") has no PRECEDING external correlation record!" << std::endl;
            }
          }
        }

        std::cout << "View: Driver API (api_id=" << api_id << ", correlation_id=" << corr_id << ")"
                  << std::endl;
        break;
      }
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        auto* rec = reinterpret_cast<pti_view_record_api*>(ptr);
        auto corr_id = rec->_correlation_id;
        auto api_id = rec->_api_id;

        // Record this Runtime API record (if test data is available)
        if (external_corr_test_data) {
          external_corr_test_data->view_runtime_api_records[corr_id] = api_id;
        }

        std::cout << "View: Runtime API (api_id=" << api_id << ", correlation_id=" << corr_id << ")"
                  << std::endl;
        break;
      }
      default:
        // Ignore other record types
        break;
    }
  }

  delete[] buf;
}

}  // namespace

// ============================================================================
// Test fixture for Callback API tests
// ============================================================================
// class CallbackApiTest : public ::testing::Test {
class CallbackApiTest : public ::testing::TestWithParam<bool> {
 protected:
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
      // Verify that on Exit phase we had a matching Enter phase
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
    if (PTI_SUCCESS != ptiViewGetApiIdName(driver_api_group_id, driver_api_id, &api_name)) {
      api_name = nullptr;
    }

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
    // Note: As completion is an async event,
    // it could be reported in a different thread than the append
    bool found = false;
    for (auto thread_id : data->append_enter_map) {
      auto& entry = thread_id.second;
      // Verify that for the Complete domain we have a matching Append Enter phase
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

  static void VerifyOperationIdUniqueness(uint64_t operation_id, pti_gpu_operation_kind kind,
                                          uint32_t correlation_id, CallbackData* data) {
    if (operation_id == 0) {
      data->zero_operation_ids++;
      std::cerr << "WARNING: Operation ID is zero" << std::endl;
      return;
    }

    std::lock_guard<std::mutex> lock(data->operation_id_mutex);

    // Track uniqueness within operation kind space
    // NOTE: The same operation_id will appear in multiple callbacks:
    //   - APPENDED ENTER (correlation_id Y)
    //   - APPENDED EXIT (correlation_id Y)
    //   - COMPLETED (correlation_id Z - different!)
    // This is expected. We just track that we've seen this operation_id.
    // The correlation_id changes between APPENDED and COMPLETED domains.
    if (kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      auto [it, inserted] = data->seen_kernel_operation_ids.insert(operation_id);
      if (inserted) {
        // First time seeing this kernel operation_id - record its first correlation_id
        data->kernel_id_to_first_corr_id[operation_id] = correlation_id;
      }
      // If not inserted, we've seen this operation_id before - this is OK (subsequent phases)
    } else if (kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      auto [it, inserted] = data->seen_memory_operation_ids.insert(operation_id);
      if (inserted) {
        // First time seeing this memory operation_id - record its first correlation_id
        data->memory_id_to_first_corr_id[operation_id] = correlation_id;
      }
      // If not inserted, we've seen this operation_id before - this is OK (subsequent phases)
    }
  }

  static void VerifyOperationIdConsistency(uint64_t operation_id, pti_gpu_operation_kind kind,
                                           uint32_t driver_api_id, CallbackData* data) {
    std::lock_guard<std::mutex> lock(data->operation_id_mutex);

    // Check driver_api_id consistency within operation kind space
    // NOTE: We use driver_api_id instead of name because name might be empty in ENTER phase
    if (kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      auto api_it = data->kernel_id_to_api_id.find(operation_id);
      if (api_it != data->kernel_id_to_api_id.end()) {
        if (api_it->second != driver_api_id) {
          data->kernel_api_id_mismatch++;
          std::cerr << "ERROR: Kernel driver_api_id mismatch for kernel_id " << operation_id << ": "
                    << api_it->second << " vs " << driver_api_id << std::endl;
        }
      } else {
        data->kernel_id_to_api_id[operation_id] = driver_api_id;
      }
    } else if (kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      auto api_it = data->memory_id_to_api_id.find(operation_id);
      if (api_it != data->memory_id_to_api_id.end()) {
        if (api_it->second != driver_api_id) {
          data->memory_api_id_mismatch++;
          std::cerr << "ERROR: Memory operation driver_api_id mismatch for mem_op_id "
                    << operation_id << ": " << api_it->second << " vs " << driver_api_id
                    << std::endl;
        }
      } else {
        data->memory_id_to_api_id[operation_id] = driver_api_id;
      }
    }
  }

  static void TrackOperationIdMapping(uint64_t operation_id, pti_gpu_operation_kind kind,
                                      uint32_t correlation_id, bool is_appended,
                                      CallbackData* data) {
    std::lock_guard<std::mutex> lock(data->operation_id_mutex);

    if (kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      if (is_appended) {
        data->appended_kernel_id_to_corr_id[operation_id] = correlation_id;
      } else {
        data->completed_kernel_id_to_corr_id[operation_id] = correlation_id;
      }
    } else if (kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      if (is_appended) {
        data->appended_memory_id_to_corr_id[operation_id] = correlation_id;
      } else {
        data->completed_memory_id_to_corr_id[operation_id] = correlation_id;
      }
    }
  }

  static void VerifyCompletedWasAppended(uint64_t operation_id, pti_gpu_operation_kind kind,
                                         CallbackData* data) {
    std::lock_guard<std::mutex> lock(data->operation_id_mutex);

    if (kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      if (data->appended_kernel_id_to_corr_id.find(operation_id) ==
          data->appended_kernel_id_to_corr_id.end()) {
        data->completed_without_appended++;
        std::cerr << "ERROR: Completed kernel_id " << operation_id
                  << " was never seen in APPENDED domain" << std::endl;
      }
    } else if (kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      if (data->appended_memory_id_to_corr_id.find(operation_id) ==
          data->appended_memory_id_to_corr_id.end()) {
        data->completed_without_appended++;
        std::cerr << "ERROR: Completed mem_op_id " << operation_id
                  << " was never seen in APPENDED domain" << std::endl;
      }
    }
  }

  static void VerifyAllAppendedCompleted(CallbackData* data) {
    // Check kernels
    for (const auto& [kernel_id, corr_id] : data->appended_kernel_id_to_corr_id) {
      if (data->completed_kernel_id_to_corr_id.find(kernel_id) ==
          data->completed_kernel_id_to_corr_id.end()) {
        data->appended_without_completed++;
        std::cerr << "ERROR: Appended kernel_id " << kernel_id << " (correlation_id " << corr_id
                  << ") was never COMPLETED" << std::endl;
      }
    }

    // Check memory operations
    for (const auto& [mem_id, corr_id] : data->appended_memory_id_to_corr_id) {
      if (data->completed_memory_id_to_corr_id.find(mem_id) ==
          data->completed_memory_id_to_corr_id.end()) {
        data->appended_without_completed++;
        std::cerr << "ERROR: Appended mem_op_id " << mem_id << " (correlation_id " << corr_id
                  << ") was never COMPLETED" << std::endl;
      }
    }
  }

  static void PrintOperationIdStats(CallbackData* data, const std::string& context) {
    std::cout << "\n=== Operation ID Stats (" << context << ") ===" << std::endl;
    std::cout << "  Unique kernel IDs (Callback): " << data->seen_kernel_operation_ids.size()
              << std::endl;
    std::cout << "  Unique memory IDs (Callback): " << data->seen_memory_operation_ids.size()
              << std::endl;
    std::cout << "  Unique kernel IDs (View): " << data->view_kernel_id_to_corr_id.size()
              << std::endl;
    std::cout << "  Unique memory IDs (View): " << data->view_memop_id_to_corr_id.size()
              << std::endl;
    std::cout << "  Duplicate kernel IDs in View: " << data->duplicate_kernel_ids.load()
              << std::endl;
    std::cout << "  Duplicate memory IDs in View: " << data->duplicate_memory_ids.load()
              << std::endl;
    std::cout << "  Zero operation IDs: " << data->zero_operation_ids.load() << std::endl;
    std::cout << "  Kernel API ID mismatches (APPENDED): " << data->kernel_api_id_mismatch.load()
              << std::endl;
    std::cout << "  Memory API ID mismatches (APPENDED): " << data->memory_api_id_mismatch.load()
              << std::endl;
    std::cout << "  Completed without appended: " << data->completed_without_appended.load()
              << std::endl;
    std::cout << "  Appended without completed: " << data->appended_without_completed.load()
              << std::endl;
    std::cout << "========================================\n" << std::endl;
  }

  static void PushOrPopExternalCorrelation(bool is_push, CallbackData* data,
                                           uint32_t correlation_id) {
    if (is_push) {
      // Push external correlation ID
      uint64_t my_external_id = data->ext_correlation_data.next_external_id.fetch_add(1);
      auto result = ptiViewPushExternalCorrelationId(
          pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, my_external_id);

      if (result == PTI_SUCCESS) {
        data->ext_correlation_data.callback_corr_to_external[correlation_id] = my_external_id;
        data->ext_correlation_data.callback_pushed_corr_ids.insert(correlation_id);
        data->ext_correlation_data.push_count++;
        std::cout << "Callback ENTER: Pushed external_id=" << my_external_id
                  << " for correlation_id=" << correlation_id << std::endl;
      } else {
        data->ext_correlation_data.push_errors++;
        std::cerr << "ERROR: Push failed with result=" << result << std::endl;
      }
    } else {
      // Pop external correlation ID
      uint64_t popped_external_id = 0;
      auto result = ptiViewPopExternalCorrelationId(
          pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_0, &popped_external_id);

      if (result == PTI_SUCCESS) {
        data->ext_correlation_data.pop_count++;
        std::cout << "Callback EXIT: Popped external_id=" << popped_external_id
                  << " for correlation_id=" << correlation_id << std::endl;

        // Verify popped ID matches what we pushed
        auto it = data->ext_correlation_data.callback_corr_to_external.find(correlation_id);
        if (it != data->ext_correlation_data.callback_corr_to_external.end()) {
          EXPECT_EQ(it->second, popped_external_id)
              << "Popped external_id doesn't match pushed external_id for correlation_id="
              << correlation_id;
        }
      } else {
        data->ext_correlation_data.pop_errors++;
        std::cerr << "ERROR: Pop failed with result=" << result << std::endl;
      }
    }
  }

  // ============================================================================
  // Helper functions for TestCallback
  // ============================================================================

  // Validates basic callback parameters common to all domains
  static void ValidateCallbackParams(CallbackData* data, pti_callback_domain domain,
                                     pti_api_group_id api_group_id, uint32_t driver_api_id,
                                     pti_backend_ctx_t context) {
    if (api_group_id != PTI_API_GROUP_LEVELZERO) {
      data->all_callbacks_levelzero = false;
      data->non_levelzero_count++;
      std::cerr << "WARNING: Non-Level Zero API group: " << api_group_id << " (expected "
                << PTI_API_GROUP_LEVELZERO << ", domain: " << domain << ")" << std::endl;
    }

    constexpr uint32_t RESERVED_DRIVER_LEVELZERO_ID = 0;
    if (driver_api_id == RESERVED_DRIVER_LEVELZERO_ID) {
      data->reserved_api_id_count++;
      std::cerr << "WARNING: Reserved driver_api_id: " << driver_api_id << " (domain: " << domain
                << ")" << std::endl;
    }

    if (context == nullptr) {
      data->null_context_count++;
      std::cerr << "WARNING: Null backend_context (domain: " << domain << ")" << std::endl;
    }
  }

  // Validates GPU operation data
  static void ValidateGpuOpData(CallbackData* data, pti_callback_gpu_op_data* gpu_op_data,
                                const char* domain_name) {
    if (gpu_op_data->_device_handle == nullptr) {
      data->null_device_handle_count++;
      std::cerr << "WARNING: " << domain_name << " callback with null device_handle" << std::endl;
    }
  }

  // Handles phase-specific logic for APPENDED domain
  static void HandlePhaseAppended(CallbackData* data, pti_callback_gpu_op_data* gpu_op_data) {
    // removed from below CheckCommandListProperties(gpu_op_data->_cmd_list_properties)
    // as immediate passed to queue creation per spec is just a hint
    // and runtime might decide to ignore it

    if (gpu_op_data->_phase == PTI_CB_PHASE_API_ENTER) {
      data->enter_count++;
      data->appended_enter_count++;

      if (data->do_external_correlation_test) {
        PushOrPopExternalCorrelation(true, data, gpu_op_data->_correlation_id);
      }
    } else if (gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT) {
      data->exit_count++;
      data->appended_exit_count++;

      if (data->do_external_correlation_test) {
        PushOrPopExternalCorrelation(false, data, gpu_op_data->_correlation_id);
      }
    }
  }

  // Processes a single operation in APPENDED domain
  static void ProcessSingleOperationAppended(CallbackData* data, const pti_gpu_op_details& op,
                                             pti_callback_gpu_op_data* gpu_op_data,
                                             uint32_t driver_api_id, pti_backend_ctx_t context) {
    // Mark operation type as seen
    if (op._operation_kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      data->kernel_seen = true;
    } else if (op._operation_kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      data->memory_op_seen = true;
    }

    // Verify operation ID uniqueness and consistency
    VerifyOperationIdUniqueness(op._operation_id, op._operation_kind, gpu_op_data->_correlation_id,
                                data);
    VerifyOperationIdConsistency(op._operation_id, op._operation_kind, driver_api_id, data);

    // Track operation ID mapping
    TrackOperationIdMapping(op._operation_id, op._operation_kind, gpu_op_data->_correlation_id,
                            true, data);

    // Check consistency if enabled
    if (data->append_complete_all_phases && gpu_op_data->_operation_count == 1) {
      CheckConsistencyAppendedEnterToExit(context, gpu_op_data, op, data);
    }
  }

  // Processes a single operation in COMPLETED domain
  static void ProcessSingleOperationCompleted(CallbackData* data, const pti_gpu_op_details& op,
                                              pti_callback_gpu_op_data* gpu_op_data,
                                              uint32_t index) {
    std::cout << "\t ops: i: " << index << ", name: " << op._name
              << " (kind: " << op._operation_kind << ", op id: " << op._operation_id << ")"
              << std::endl;

    // Verify this operation was previously appended
    VerifyCompletedWasAppended(op._operation_id, op._operation_kind, data);

    // Track operation ID mapping
    TrackOperationIdMapping(op._operation_id, op._operation_kind, gpu_op_data->_correlation_id,
                            false, data);

    // Count by operation type
    if (op._operation_kind == PTI_GPU_OPERATION_KIND_KERNEL) {
      data->completed_kernel_count++;
      data->RecordThreadKernel();
    } else if (op._operation_kind == PTI_GPU_OPERATION_KIND_MEMORY) {
      data->completed_memcopy_count++;
    }
  }

  // Processes all operation details for APPENDED domain
  static void ProcessOperationDetailsAppended(CallbackData* data,
                                              pti_callback_gpu_op_data* gpu_op_data,
                                              uint32_t driver_api_id, pti_backend_ctx_t context) {
    if (gpu_op_data->_operation_count == 0 || !gpu_op_data->_operation_details) {
      return;
    }

    for (uint32_t i = 0; i < gpu_op_data->_operation_count; ++i) {
      ProcessSingleOperationAppended(data, gpu_op_data->_operation_details[i], gpu_op_data,
                                     driver_api_id, context);
    }
  }

  // Processes all operation details for COMPLETED domain
  static void ProcessOperationDetailsCompleted(CallbackData* data,
                                               pti_callback_gpu_op_data* gpu_op_data,
                                               pti_backend_ctx_t context) {
    if (gpu_op_data->_operation_count == 0 || !gpu_op_data->_operation_details) {
      return;
    }

    for (uint32_t i = 0; i < gpu_op_data->_operation_count; ++i) {
      ProcessSingleOperationCompleted(data, gpu_op_data->_operation_details[i], gpu_op_data, i);
    }

    if (data->append_complete_all_phases) {
      CheckConsistencyCompletedToAppended(context, gpu_op_data, data);
    }
  }

  // Handles APPENDED domain callback
  static void HandleAppendedCallback(CallbackData* data, pti_callback_gpu_op_data* gpu_op_data,
                                     uint32_t driver_api_id, pti_api_group_id api_group_id,
                                     pti_backend_ctx_t context) {
    data->appended_count++;
    data->last_phase = gpu_op_data->_phase;

    ValidateGpuOpData(data, gpu_op_data, "APPENDED");

    PrintCallBackInfo("APPENDED", gpu_op_data->_phase, context, gpu_op_data->_device_handle,
                      api_group_id, driver_api_id, gpu_op_data->_correlation_id,
                      gpu_op_data->_operation_count, gpu_op_data->_operation_details);

    HandlePhaseAppended(data, gpu_op_data);
    ProcessOperationDetailsAppended(data, gpu_op_data, driver_api_id, context);
  }

  // Handles COMPLETED domain callback
  static void HandleCompletedCallback(CallbackData* data, pti_callback_gpu_op_data* gpu_op_data,
                                      uint32_t driver_api_id, pti_api_group_id api_group_id,
                                      pti_backend_ctx_t context) {
    data->completed_count++;
    data->last_phase = gpu_op_data->_phase;

    ValidateGpuOpData(data, gpu_op_data, "COMPLETED");

    EXPECT_TRUE(gpu_op_data->_phase == PTI_CB_PHASE_API_EXIT)
        << "COMPLETED domain should only have EXIT phase callbacks";

    data->exit_count++;
    data->completed_exit_count++;

    PrintCallBackInfo("COMPLETED", gpu_op_data->_phase, context, gpu_op_data->_device_handle,
                      api_group_id, driver_api_id, gpu_op_data->_correlation_id,
                      gpu_op_data->_operation_count, gpu_op_data->_operation_details);

    ProcessOperationDetailsCompleted(data, gpu_op_data, context);
  }

  // ============================================================================
  // Main callback function - simplified with helper functions above
  // ============================================================================
  static void TestCallback(pti_callback_domain domain, pti_api_group_id driver_api_group_id,
                           uint32_t driver_api_id, pti_backend_ctx_t backend_context, void* cb_data,
                           void* global_user_data, [[maybe_unused]] void** instance_user_data) {
    // Basic setup
    auto* data = static_cast<CallbackData*>(global_user_data);
    EXPECT_TRUE(data != nullptr) << "Global user data is null";

    // Update global state
    data->total_count++;
    data->last_domain = domain;
    data->last_api_group = driver_api_group_id;
    data->user_data_received = global_user_data;
    data->RecordThreadCallback();

    // Validate common parameters
    ValidateCallbackParams(data, domain, driver_api_group_id, driver_api_id, backend_context);

    // Validate callback data exists
    EXPECT_TRUE(cb_data != nullptr) << "cb_data is null for domain: " << domain;

    // Dispatch to domain-specific handler
    auto* gpu_op_data = static_cast<pti_callback_gpu_op_data*>(cb_data);

    switch (domain) {
      case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED:
        HandleAppendedCallback(data, gpu_op_data, driver_api_id, driver_api_group_id,
                               backend_context);
        break;

      case PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED:
        HandleCompletedCallback(data, gpu_op_data, driver_api_id, driver_api_group_id,
                                backend_context);
        break;

      default:
        FAIL() << "Unexpected callback domain: " << domain;
    }
  }

  void StopCollectionCommon() {
    // Clean up PTI view and callback subscriptions
    // Unsubscribe any remaining subscribers
    for (auto& subscriber : subscribers_) {
      if (subscriber) {
        EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
      }
    }
    subscribers_.clear();

    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  }

  void SetUp() override {
    // Set global pointer for BufferCompleted to access test data
    g_callback_data = &callback_data_;

    // For now, at least one PTI_VIEW_DEVICE_GPU needs to be enabled for callback API to work
    EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  }

  void TearDown() override {
    StopCollectionCommon();
    // Clear global pointers
    g_callback_data = nullptr;
  }

  CallbackData callback_data_;
  inline static bool command_list_immediate_{true};
  std::vector<pti_callback_subscriber_handle> subscribers_;
};

bool SkipNonImmediateTestIfBMG(sycl::device& dev, bool& test_command_list_immediate) {
  // Check Device name to know if it is BMG, and if it is - skip Non-immediate test as by
  // https://intel.github.io/llvm/EnvironmentVariables.html#controlling-dpc-level-zero-adapter
  // only immediate supported in 2025.3 on BMG
  // It is expected to change in 2026.0
  // and by the way: immediate /non-immediate is only a hint - so we can not 100% count on it
  auto platform = dev.get_platform();
  std::string device_name = dev.get_info<sycl::info::device::name>();
  std::cout << "Device name: " << device_name << std::endl;
  return (device_name.find("B580 Graphics") != std::string::npos ||
          device_name.find("B570 Graphics") != std::string::npos) &&
         (!test_command_list_immediate);
}

// ============================================================================
//  TESTS START HERE
// ============================================================================

//
// Test basic subscription with GPU kernel invocation
//
TEST_P(CallbackApiTest, BasicSubscription) {
  std::cout << "\n=== Test: BasicSubscription ===" << std::endl;

  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);

  // Enable cross-checking data consistency between Append phases and Complete phase
  callback_data_.append_complete_all_phases = true;
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test successful subscription
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);

  subscribers_.push_back(subscriber);

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
    command_list_immediate_ = GetParam();

    sycl::device dev(sycl::gpu_selector_v);
    if (SkipNonImmediateTestIfBMG(dev, command_list_immediate_)) {
      GTEST_SKIP() << "Skipping Non-immediate command list test on BMG";
    }
    // Important that queue is in order
    sycl::property_list prop;
    if (command_list_immediate_) {
      std::cout << " ** Immediate command list mode" << std::endl;
      prop = sycl::property_list{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::immediate_command_list()};
    } else {
      std::cout << " ** Non-immediate command list mode" << std::endl;
      prop = sycl::property_list{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::no_immediate_command_list()};
    }
    sycl::queue queue(dev, sycl::async_handler{}, prop);

    unsigned size = kDefaultMatrixSize;
    std::vector<float> a(size * size, A_VALUE);
    std::vector<float> b(size * size, B_VALUE);
    std::vector<float> c(size * size, 0.0f);

    LaunchMultipleGEMMKernels(queue, a, b, c, size, kDefaultKernelCount);

    // Verify last result
    float expected_result = A_VALUE * B_VALUE * size;
    float eps = Check(c, expected_result);
    EXPECT_LE(eps, MAX_EPS);

  } catch (const sycl::exception& e) {
    FAIL() << "SYCL exception during kernel execution: " << e.what();
  }

  // Stop collection
  StopCollectionCommon();
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);

  // Flush views to ensure callbacks are processed
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked
  EXPECT_GT(callback_data_.total_count.load(), 0);
  EXPECT_GT(callback_data_.appended_count.load(), 0);
  EXPECT_TRUE(callback_data_.kernel_seen.load());

  // Verify APPENDED domain ENTER/EXIT counts
  // We expect both ENTER and EXIT callbacks for APPENDED domain
  EXPECT_GT(callback_data_.appended_enter_count.load(), 0)
      << "APPENDED domain ENTER callbacks should be called";
  EXPECT_GT(callback_data_.appended_exit_count.load(), 0)
      << "APPENDED domain EXIT callbacks should be called";

  // The number of ENTER and EXIT callbacks should be equal for APPENDED domain
  EXPECT_EQ(callback_data_.appended_enter_count.load(), callback_data_.appended_exit_count.load())
      << "APPENDED domain should have equal number of ENTER and EXIT callbacks";

  // Verify COMPLETED domain counts
  // COMPLETED domain typically only has EXIT phase (as per the comment in the code)
  EXPECT_EQ(callback_data_.completed_enter_count.load(), 0)
      << "COMPLETED domain should not have ENTER callbacks";
  // We may or may not get completed callbacks depending on timing
  EXPECT_GE(callback_data_.completed_exit_count.load(), 0)
      << "COMPLETED domain may have EXIT callbacks";

  // Total enter/exit counts
  EXPECT_GE(callback_data_.enter_count.load(), 0);
  EXPECT_GE(callback_data_.exit_count.load(), 0);

  // Print counts for debugging
  std::cout << "\n=== Count Summary ===" << std::endl;
  std::cout << "View Records:" << std::endl;
  std::cout << "  Kernels: " << callback_data_.view_kernel_count.load() << std::endl;
  std::cout << "  MemCopy: " << callback_data_.view_memcopy_count.load() << std::endl;
  std::cout << "  MemFill: " << callback_data_.view_memfill_count.load() << std::endl;
  std::cout << "Callback Completed Operations:" << std::endl;
  std::cout << "  Kernels: " << callback_data_.completed_kernel_count.load() << std::endl;
  std::cout << "  Memory Ops: " << callback_data_.completed_memcopy_count.load() << std::endl;
  std::cout << "====================\n" << std::endl;

  // Verify that counts from view records match counts from callback COMPLETED domain
  // Note: The kernel count should match exactly
  EXPECT_EQ(callback_data_.view_kernel_count.load(), callback_data_.completed_kernel_count.load())
      << "Kernel count from ptiView should match count from Callback COMPLETED domain";

  // Memory operations: view records distinguish between copy and fill,
  // but callback API reports them all as MEMORY kind
  int total_view_memory_ops =
      callback_data_.view_memcopy_count.load() + callback_data_.view_memfill_count.load();
  EXPECT_EQ(total_view_memory_ops, callback_data_.completed_memcopy_count.load())
      << "Total memory operation count from ptiView should match count from Callback COMPLETED "
         "domain";

  // Verify that all callbacks had the correct API group (Level Zero)
  EXPECT_TRUE(callback_data_.all_callbacks_levelzero.load())
      << "All callbacks should have driver_api_group_id == PTI_API_GROUP_LEVELZERO";
  EXPECT_EQ(callback_data_.non_levelzero_count.load(), 0)
      << "No callbacks should have non-Level Zero API group";

  // Verify that no callbacks had reserved API IDs
  EXPECT_EQ(callback_data_.reserved_api_id_count.load(), 0)
      << "No callbacks should have reserved driver_api_id";

  // Verify that all callbacks had non-null backend_context
  EXPECT_EQ(callback_data_.null_context_count.load(), 0)
      << "All callbacks should have non-null backend_context";

  // Verify that all GPU operation callbacks had non-null device_handle
  EXPECT_EQ(callback_data_.null_device_handle_count.load(), 0)
      << "All GPU operation callbacks (APPENDED and COMPLETED) should have non-null _device_handle";

  // Verify the last API group was Level Zero (as a sanity check)
  EXPECT_EQ(callback_data_.last_api_group, PTI_API_GROUP_LEVELZERO)
      << "Last callback should have Level Zero API group";

  // ============================================================================
  // NEW: Verify operation ID tracking
  // ============================================================================
  std::cout << "\n=== Operation ID Verification ===" << std::endl;

  EXPECT_EQ(callback_data_.zero_operation_ids.load(), 0) << "No operation IDs should be zero";

  EXPECT_EQ(callback_data_.duplicate_kernel_ids.load(), 0)
      << "All kernel operation IDs should be unique in View records";

  EXPECT_EQ(callback_data_.duplicate_memory_ids.load(), 0)
      << "All memory operation IDs should be unique in View records";

  EXPECT_EQ(callback_data_.kernel_api_id_mismatch.load(), 0)
      << "Kernel driver_api_id should be consistent within APPENDED domain for each kernel "
         "operation ID";

  EXPECT_EQ(callback_data_.memory_api_id_mismatch.load(), 0)
      << "Memory driver_api_id should be consistent within APPENDED domain for each memory "
         "operation ID";

  // Verify operation lifecycle
  EXPECT_EQ(callback_data_.completed_without_appended.load(), 0)
      << "All completed operations should have been previously appended";

  // Verify all appended operations were eventually completed
  VerifyAllAppendedCompleted(&callback_data_);
  EXPECT_EQ(callback_data_.appended_without_completed.load(), 0)
      << "All appended operations should eventually be completed";

  // Cross-verify between Callback and View APIs
  EXPECT_EQ(callback_data_.seen_kernel_operation_ids.size(),
            callback_data_.view_kernel_id_to_corr_id.size())
      << "Kernel ID count mismatch between callback and view records";

  for (const auto& kernel_id : callback_data_.seen_kernel_operation_ids) {
    EXPECT_TRUE(callback_data_.view_kernel_id_to_corr_id.find(kernel_id) !=
                callback_data_.view_kernel_id_to_corr_id.end())
        << "Kernel operation_id " << kernel_id << " from callback not found in view records";
  }

  for (const auto& mem_id : callback_data_.seen_memory_operation_ids) {
    EXPECT_TRUE(callback_data_.view_memop_id_to_corr_id.find(mem_id) !=
                callback_data_.view_memop_id_to_corr_id.end())
        << "Memory operation_id " << mem_id << " from callback not found in view records";
  }

  std::cout << "  Unique kernel operation IDs: " << callback_data_.seen_kernel_operation_ids.size()
            << std::endl;
  std::cout << "  Unique memory operation IDs: " << callback_data_.seen_memory_operation_ids.size()
            << std::endl;
  std::cout << "  View kernel records matched: " << callback_data_.view_kernel_id_to_corr_id.size()
            << std::endl;
  std::cout << "  View memory records matched: " << callback_data_.view_memop_id_to_corr_id.size()
            << std::endl;

  std::cout << "====================================\n" << std::endl;

  // Print operation ID statistics
  PrintOperationIdStats(&callback_data_, "BasicSubscription");

  // Clean up
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
}

//
// Test subscription with null parameters
//
TEST_F(CallbackApiTest, SubscriptionWithNullParams) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test with null subscriber handle pointer
  EXPECT_NE(ptiCallbackSubscribe(nullptr, TestCallback, &callback_data_), PTI_SUCCESS);

  // Test with null callback function
  EXPECT_NE(ptiCallbackSubscribe(&subscriber, nullptr, &callback_data_), PTI_SUCCESS);

  // User data can be null, so this should succeed
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, nullptr), PTI_SUCCESS);
}

//
// Test domain enable and disable
//
TEST_F(CallbackApiTest, DomainEnableDisable) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe first
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
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

//
// Test enabling not implemented domains
//
TEST_F(CallbackApiTest, NotImplementedDomains) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe first
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
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

//
// Test multiple subscribers
//
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
}

//
// Test unsubscribe with invalid handle
//
TEST_F(CallbackApiTest, SubscribeWithNullParamsUnsubscribeInvalidHandle) {
  pti_callback_subscriber_handle subscriber = nullptr;

  // Test with null subscriber handle pointer
  EXPECT_NE(ptiCallbackSubscribe(nullptr, TestCallback, &callback_data_), PTI_SUCCESS);

  // Test with null callback function
  EXPECT_NE(ptiCallbackSubscribe(&subscriber, nullptr, &callback_data_), PTI_SUCCESS);

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

//
// Test helper functions for string conversion
//
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
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
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

//
// Test multi-threaded kernel execution with callback API
//
TEST_F(CallbackApiTest, MultiThreadedKernelExecution) {
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);

  // Enable cross-checking data consistency between Append phases and Complete phase
  callback_data_.append_complete_all_phases = true;
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe and enable callbacks
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

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
  const int num_threads = kDefaultThreadCount;
  const int kernels_per_thread = kDefaultKernelCount;
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

          LaunchMultipleGEMMKernels(queue, a, b, c, size, kDefaultKernelCount);
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

  // Stop Collection
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
  StopCollectionCommon();

  // Flush views to ensure callbacks are processed
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked for all kernels
  int expected_min_kernels = num_threads * kernels_per_thread;
  EXPECT_GE(callback_data_.completed_kernel_count.load(), expected_min_kernels)
      << "Expected at least " << expected_min_kernels << " kernel completions";

  // Verify we got callbacks from multiple threads
  {
    std::lock_guard<std::mutex> lock(callback_data_.thread_map_mutex);
    size_t num_threads_with_callbacks = callback_data_.thread_callback_counts.size();
    EXPECT_GT(num_threads_with_callbacks, 1)
        << "Expected callbacks from multiple threads, but got callbacks from "
        << num_threads_with_callbacks << " thread(s)";

    // Print thread callback distribution for debugging
    std::cout << "\n=== Thread Callback Distribution ===" << std::endl;
    for (const auto& [tid, count] : callback_data_.thread_callback_counts) {
      std::cout << "Thread ID " << tid << ": " << count << " callbacks" << std::endl;
    }

    std::cout << "\n=== Thread Kernel Distribution ===" << std::endl;
    for (const auto& [tid, count] : callback_data_.thread_kernel_counts) {
      std::cout << "Thread ID " << tid << ": " << count << " kernels" << std::endl;
    }
    std::cout << "====================================\n" << std::endl;
  }

  // Verify thread safety - no corrupted counters
  EXPECT_TRUE(callback_data_.all_callbacks_levelzero.load())
      << "All callbacks should have driver_api_group_id == PTI_API_GROUP_LEVELZERO";
  EXPECT_EQ(callback_data_.null_context_count.load(), 0)
      << "All callbacks should have non-null backend_context";
  EXPECT_EQ(callback_data_.null_device_handle_count.load(), 0)
      << "All GPU operation callbacks should have non-null _device_handle";

  // ============================================================================
  // NEW: Verify operation ID tracking in multi-threaded context
  // ============================================================================
  std::cout << "\n=== Multi-threaded Operation ID Verification ===" << std::endl;

  EXPECT_EQ(callback_data_.duplicate_kernel_ids.load(), 0)
      << "All kernel operation IDs should be unique in View records (multi-threaded)";

  EXPECT_EQ(callback_data_.duplicate_memory_ids.load(), 0)
      << "All memory operation IDs should be unique in View records (multi-threaded)";

  EXPECT_EQ(callback_data_.completed_without_appended.load(), 0)
      << "All completed operations should have been previously appended (multi-threaded)";

  VerifyAllAppendedCompleted(&callback_data_);
  EXPECT_EQ(callback_data_.appended_without_completed.load(), 0)
      << "All appended operations should eventually be completed (multi-threaded)";

  std::cout << "  Unique kernel IDs (multi-threaded): "
            << callback_data_.seen_kernel_operation_ids.size() << std::endl;
  std::cout << "  Unique memory IDs (multi-threaded): "
            << callback_data_.seen_memory_operation_ids.size() << std::endl;

  PrintOperationIdStats(&callback_data_, "MultiThreadedKernelExecution");
}

//
// Test concurrent queue submissions with synchronization
//
TEST_F(CallbackApiTest, ConcurrentQueueSubmissions) {
  // Enable additional ptiView
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);

  // Enable cross-checking data consistency between Append phases and Complete phase
  callback_data_.append_complete_all_phases = true;

  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe and enable callbacks
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
            PTI_SUCCESS);

  const int num_threads = kConcurrentThreadCount;
  const int submissions_per_thread = kConcurrentSubmissions;

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
        unsigned size = kSmallMatrixSize;
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
            cgh.copy(std::move(src_acc), std::move(dst_acc));
          });
        } else {
          // Kernel operation
          LaunchMultipleGEMMKernels(queue, a, b, c, size, kDefaultKernelCount);
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

  // Stop Collection
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  StopCollectionCommon();
  // Flush views
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // Verify callbacks were invoked
  EXPECT_GT(callback_data_.total_count.load(), 0) << "No callbacks were invoked";
  EXPECT_GT(callback_data_.appended_count.load(), 0) << "No APPENDED callbacks were invoked";

  // Verify both kernel and memory operations were seen
  EXPECT_TRUE(callback_data_.kernel_seen.load()) << "No kernel operations were detected";
  EXPECT_TRUE(callback_data_.memory_op_seen.load()) << "No memory operations were detected";

  // Print statistics
  std::cout << "\n=== Concurrent Submission Statistics ===" << std::endl;
  std::cout << "Total callbacks: " << callback_data_.total_count.load() << std::endl;
  std::cout << "Appended callbacks: " << callback_data_.appended_count.load() << std::endl;
  std::cout << "Completed callbacks: " << callback_data_.completed_count.load() << std::endl;
  std::cout << "Kernel operations: " << callback_data_.completed_kernel_count.load() << std::endl;
  std::cout << "Memory operations: " << callback_data_.completed_memcopy_count.load() << std::endl;

  {
    std::lock_guard<std::mutex> lock(callback_data_.thread_map_mutex);
    std::cout << "Unique threads with callbacks: " << callback_data_.thread_callback_counts.size()
              << std::endl;
  }
  std::cout << "========================================\n" << std::endl;

  // ============================================================================
  // NEW: Verify operation ID uniqueness under concurrent submissions
  // ============================================================================
  EXPECT_EQ(callback_data_.duplicate_kernel_ids.load(), 0)
      << "All kernel operation IDs should be unique in View records (concurrent)";

  EXPECT_EQ(callback_data_.duplicate_memory_ids.load(), 0)
      << "All memory operation IDs should be unique in View records (concurrent)";

  EXPECT_EQ(callback_data_.completed_without_appended.load(), 0)
      << "All completed operations should have been previously appended (concurrent)";

  VerifyAllAppendedCompleted(&callback_data_);
  EXPECT_EQ(callback_data_.appended_without_completed.load(), 0)
      << "All appended operations should eventually be completed (concurrent)";

  std::cout << "Unique kernel operation IDs (concurrent): "
            << callback_data_.seen_kernel_operation_ids.size() << std::endl;
  std::cout << "Unique memory operation IDs (concurrent): "
            << callback_data_.seen_memory_operation_ids.size() << std::endl;

  PrintOperationIdStats(&callback_data_, "ConcurrentQueueSubmissions");
}

//
// Test callback thread safety with shared queue
//
TEST_F(CallbackApiTest, CallbackThreadSafety) {
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

  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe with thread-safe callback
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, ThreadSafeCallback, thread_safe_data.get()),
            PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
            PTI_SUCCESS);

  // Create a shared queue for all threads
  sycl::device dev(sycl::gpu_selector_v);
  sycl::queue shared_queue(dev);

  const int num_threads = kThreadSafetyThreadCount;
  const int kernels_per_thread = kThreadSafetyKernelCount;
  std::vector<std::thread> threads;

  // Launch kernels from multiple threads using shared queue
  for (int tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      try {
        for (int i = 0; i < kernels_per_thread; ++i) {
          unsigned size = kSmallMatrixSize;
          std::vector<float> a(size * size, A_VALUE);
          std::vector<float> b(size * size, B_VALUE);
          std::vector<float> c(size * size, 0.0f);

          // Use the shared queue
          LaunchMultipleGEMMKernels(shared_queue, a, b, c, size, kDefaultKernelCount);

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

  // Stop Collection
  StopCollectionCommon();

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
}

//
// Test external correlation API when called from Append Enter/Exit callbacks
//
TEST_P(CallbackApiTest, ExternalCorrelationInAppendCallbacks) {
  std::cout << "\n=== Test: ExternalCorrelationInAppendCallbacks ===" << std::endl;

  callback_data_.do_external_correlation_test = true;
  pti_callback_subscriber_handle subscriber = nullptr;

  // Subscribe for Callbacks
  EXPECT_EQ(ptiCallbackSubscribe(&subscriber, TestCallback, &callback_data_), PTI_SUCCESS);
  EXPECT_NE(subscriber, nullptr);
  subscribers_.push_back(subscriber);

  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1),
            PTI_SUCCESS);

  // Set up ptiView callbacks (using existing BufferRequested/BufferCompleted)
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), PTI_SUCCESS);

  // Limit to GPU operation core APIs only (reduces noise)
  EXPECT_EQ(
      ptiViewEnableDriverApiClass(1, PTI_API_CLASS_GPU_OPERATION_CORE, PTI_API_GROUP_LEVELZERO),
      PTI_SUCCESS);

  EXPECT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION), PTI_SUCCESS);

  // Launch GPU kernels (single-threaded)
  try {
    command_list_immediate_ = GetParam();
    sycl::device dev(sycl::gpu_selector_v);
    if (SkipNonImmediateTestIfBMG(dev, command_list_immediate_)) {
      GTEST_SKIP() << "Skipping Non-immediate command list test on BMG";
    }
    // Important that queue is in order
    sycl::property_list prop;
    if (command_list_immediate_) {
      std::cout << " ** Immediate command list mode" << std::endl;
      prop = sycl::property_list{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::immediate_command_list()};
    } else {
      std::cout << " ** Non-immediate command list mode" << std::endl;
      prop = sycl::property_list{sycl::property::queue::in_order(),
                                 sycl::ext::intel::property::queue::no_immediate_command_list()};
    }
    sycl::queue queue(dev, sycl::async_handler{}, prop);

    unsigned size = kDefaultMatrixSize;
    std::vector<float> a(size * size, A_VALUE);
    std::vector<float> b(size * size, B_VALUE);
    std::vector<float> c(size * size, 0.0f);

    LaunchMultipleGEMMKernels(queue, a, b, c, size, kDefaultKernelCount);

    // Verify the last result
    float expected_result = A_VALUE * B_VALUE * size;
    float eps = Check(c, expected_result);
    EXPECT_LE(eps, MAX_EPS) << "GEMM kernel " << kDefaultKernelCount - 1 << " verification failed";

  } catch (const sycl::exception& e) {
    FAIL() << "SYCL exception during kernel execution: " << e.what();
  }

  // Stop Collection
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), PTI_SUCCESS);

  StopCollectionCommon();

  // Flush views to ensure callbacks are processed
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);

  // VERIFICATION

  std::cout << "\n========== External Correlation Test Verification ==========" << std::endl;

  const auto& external_corr_test_data_ = callback_data_.ext_correlation_data;
  // 1. Basic balance checks
  EXPECT_EQ(external_corr_test_data_.push_count.load(), external_corr_test_data_.pop_count.load())
      << "Push and pop operations should be balanced";
  EXPECT_EQ(external_corr_test_data_.push_errors.load(), 0) << "No push errors expected";
  EXPECT_EQ(external_corr_test_data_.pop_errors.load(), 0) << "No pop errors expected";

  std::cout << "Push count: " << external_corr_test_data_.push_count.load() << std::endl;
  std::cout << "Pop count: " << external_corr_test_data_.pop_count.load() << std::endl;

  // 2. For each external ID pushed in callbacks, verify view records
  for (const auto& [callback_corr_id, external_id] :
       external_corr_test_data_.callback_corr_to_external) {
    // 2a. External correlation record must exist
    EXPECT_TRUE(external_corr_test_data_.view_external_to_corr.find(external_id) !=
                external_corr_test_data_.view_external_to_corr.end())
        << "External correlation record not found for external_id: " << external_id;

    // 2b. Correlation ID from callback must match view record
    if (external_corr_test_data_.view_external_to_corr.find(external_id) !=
        external_corr_test_data_.view_external_to_corr.end()) {
      const auto& view_corr_id = external_corr_test_data_.view_external_to_corr.at(external_id);
      EXPECT_EQ(callback_corr_id, view_corr_id)
          << "Correlation ID mismatch: callback=" << callback_corr_id << " vs view=" << view_corr_id
          << " for external_id=" << external_id;
    }

    // 2c. This correlation_id should have a DRIVER API record (not runtime)
    EXPECT_TRUE(external_corr_test_data_.view_driver_api_records.find(callback_corr_id) !=
                external_corr_test_data_.view_driver_api_records.end())
        << "Driver API record not found for correlation_id: " << callback_corr_id;
  }

  // 3. Verify all external correlation records link to DRIVER API (not runtime)
  for (const auto& [external_id, corr_id] : external_corr_test_data_.view_external_to_corr) {
    EXPECT_TRUE(external_corr_test_data_.view_driver_api_records.find(corr_id) !=
                external_corr_test_data_.view_driver_api_records.end())
        << "External correlation (external_id=" << external_id
        << ") references non-existent Driver API record (correlation_id=" << corr_id << ")";

    EXPECT_TRUE(external_corr_test_data_.view_runtime_api_records.find(corr_id) !=
                external_corr_test_data_.view_runtime_api_records.end())
        << "Have not seen Runtime API record counterpart to Driver API record "
        << "(correlation_id=" << corr_id << ") which is expected to exist";
  }

  // 4. Verify proper ordering and API type constraints
  EXPECT_TRUE(external_corr_test_data_.ordering_violations.empty())
      << "Found " << external_corr_test_data_.ordering_violations.size() << " ordering violations";

  if (!external_corr_test_data_.ordering_violations.empty()) {
    std::cerr << "\nOrdering violations detected:" << std::endl;
    for (const auto& violation : external_corr_test_data_.ordering_violations) {
      std::cerr << "  Driver API without preceding external correlation: api_id="
                << violation.api_id << ", correlation_id=" << violation.correlation_id << std::endl;
    }
  }

  // 5. Verify expected operation count
  EXPECT_GE(external_corr_test_data_.push_count.load(), kDefaultKernelCount)
      << "Expected at least " << kDefaultKernelCount << " kernel operations";

  // 6. Print summary
  std::cout << "\n=== External Correlation Test Summary ===" << std::endl;
  std::cout << "External correlations pushed: " << external_corr_test_data_.push_count.load()
            << std::endl;
  std::cout << "External correlation records in view: "
            << external_corr_test_data_.view_external_to_corr.size() << std::endl;
  std::cout << "Driver API records: " << external_corr_test_data_.view_driver_api_records.size()
            << std::endl;
  std::cout << "Runtime API records: " << external_corr_test_data_.view_runtime_api_records.size()
            << std::endl;
  std::cout << "========================================\n" << std::endl;

  // ============================================================================
  // NEW: Verify operation IDs with external correlation enabled
  // ============================================================================
  EXPECT_EQ(callback_data_.duplicate_kernel_ids.load(), 0)
      << "All kernel operation IDs should be unique in View records (external correlation)";

  EXPECT_EQ(callback_data_.duplicate_memory_ids.load(), 0)
      << "All memory operation IDs should be unique in View records (external correlation)";

  EXPECT_EQ(callback_data_.completed_without_appended.load(), 0)
      << "All completed operations should have been previously appended (external correlation)";

  VerifyAllAppendedCompleted(&callback_data_);
  EXPECT_EQ(callback_data_.appended_without_completed.load(), 0)
      << "All appended operations should eventually be completed (external correlation)";

  std::cout << "  Kernel IDs seen in APPENDED: "
            << callback_data_.appended_kernel_id_to_corr_id.size() << std::endl;
  std::cout << "  Kernel IDs seen in COMPLETED: "
            << callback_data_.completed_kernel_id_to_corr_id.size() << std::endl;
  std::cout << "  Memory IDs seen in APPENDED: "
            << callback_data_.appended_memory_id_to_corr_id.size() << std::endl;
  std::cout << "  Memory IDs seen in COMPLETED: "
            << callback_data_.completed_memory_id_to_corr_id.size() << std::endl;

  PrintOperationIdStats(&callback_data_, "ExternalCorrelationInAppendCallbacks");
}

INSTANTIATE_TEST_SUITE_P(Parametrized, CallbackApiTest, ::testing::Values(false, true),
                         [](const testing::TestParamInfo<bool>& info) {
                           return fmt::format("Queue_Type_{}", info.param
                                                                   ? "ImmediateCommandList"
                                                                   : "NonImmediateCommandList");
                         });
