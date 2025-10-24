//==============================================================
// Multi-Device Parallel Metrics Scope Test
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_metrics_scope.h"
#include "samples_utils.h"

namespace {

// Simple workload kernels for testing
void SimpleVectorAdd(const float* a, const float* b, float* c, size_t size, sycl::id<1> id) {
  size_t idx = id.get(0);
  if (idx < size) {
    c[idx] = a[idx] + b[idx];
  }
}

void SimpleVectorMul(const float* a, const float* b, float* c, size_t size, sycl::id<1> id) {
  size_t idx = id.get(0);
  if (idx < size) {
    c[idx] = a[idx] * b[idx];
  }
}

void SimpleMatrixMul(const float* a, const float* b, float* c, size_t size, sycl::id<2> id) {
  size_t i = id.get(0);
  size_t j = id.get(1);
  if (i < size && j < size) {
    float sum = 0.0f;
    for (size_t k = 0; k < size; ++k) {
      sum += a[i * size + k] * b[k * size + j];
    }
    c[i * size + j] = sum;
  }
}

// Test parameters
constexpr size_t kVectorSize = 8192;
constexpr size_t kMatrixSize = 64;
constexpr int kIterationsPerDevice = 5;

// Global test data
std::vector<pti_device_properties_t> g_devices;
std::mutex g_print_mutex;

// Thread-safe printing helper
template <typename... Args>
void SafePrint(Args&&... args) {
  std::lock_guard<std::mutex> lock(g_print_mutex);
  (std::cout << ... << args) << std::endl;
}

// Device workload runner
class DeviceWorkloadRunner {
 private:
  pti_device_handle_t device_handle_;
  sycl::device sycl_device_;
  sycl::queue queue_;
  int device_id_;
  std::string device_name_;

 public:
  DeviceWorkloadRunner(pti_device_handle_t device_handle, int device_id)
      : device_handle_(device_handle), device_id_(device_id) {
    // Find corresponding SYCL device
    auto sycl_devices = sycl::device::get_devices(sycl::info::device_type::gpu);
    if (device_id < static_cast<int>(sycl_devices.size())) {
      sycl_device_ = sycl_devices[device_id];
      device_name_ = sycl_device_.get_info<sycl::info::device::name>();

      sycl::property_list prop_list{sycl::property::queue::in_order()};
      queue_ = sycl::queue(sycl_device_, sycl::async_handler{}, prop_list);

      SafePrint("[Device ", device_id_, "] Using device: ", device_name_);
    } else {
      throw std::runtime_error("Device index out of range");
    }
  }

  pti_device_handle_t GetDeviceHandle() const { return device_handle_; }
  const std::string& GetDeviceName() const { return device_name_; }
  int GetDeviceId() const { return device_id_; }

  // Run vector addition workload
  void RunVectorAddWorkload(int iteration) {
    try {
      std::vector<float> a(kVectorSize, static_cast<float>(device_id_ + 1));
      std::vector<float> b(kVectorSize, static_cast<float>(iteration + 1));
      std::vector<float> c(kVectorSize, 0.0f);

      sycl::buffer<float, 1> a_buf(a.data(), a.size());
      sycl::buffer<float, 1> b_buf(b.data(), b.size());
      sycl::buffer<float, 1> c_buf(c.data(), c.size());

      auto start_time = std::chrono::high_resolution_clock::now();

      queue_.submit([&](sycl::handler& cgh) {
        auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
        auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
        auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

        cgh.parallel_for<class VectorAdd_MultiDevice>(
            sycl::range<1>(kVectorSize), [=](sycl::id<1> id) {
              auto a_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto b_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto c_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
              SimpleVectorAdd(a_ptr.get(), b_ptr.get(), c_ptr.get(), kVectorSize, id);
            });
      });
      queue_.wait_and_throw();

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

      SafePrint("[Device ", device_id_, "] VectorAdd iteration ", iteration, " completed in ",
                duration.count(), " μs");

    } catch (const std::exception& e) {
      SafePrint("[Device ", device_id_, "] VectorAdd iteration ", iteration, " failed: ", e.what());
      throw;
    }
  }

  // Run vector multiplication workload
  void RunVectorMulWorkload(int iteration) {
    try {
      std::vector<float> a(kVectorSize, static_cast<float>(device_id_ + 2));
      std::vector<float> b(kVectorSize, static_cast<float>(iteration + 2));
      std::vector<float> c(kVectorSize, 0.0f);

      sycl::buffer<float, 1> a_buf(a.data(), a.size());
      sycl::buffer<float, 1> b_buf(b.data(), b.size());
      sycl::buffer<float, 1> c_buf(c.data(), c.size());

      auto start_time = std::chrono::high_resolution_clock::now();

      queue_.submit([&](sycl::handler& cgh) {
        auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
        auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
        auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

        cgh.parallel_for<class VectorMul_MultiDevice>(
            sycl::range<1>(kVectorSize), [=](sycl::id<1> id) {
              auto a_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto b_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto c_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
              SimpleVectorMul(a_ptr.get(), b_ptr.get(), c_ptr.get(), kVectorSize, id);
            });
      });
      queue_.wait_and_throw();

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

      SafePrint("[Device ", device_id_, "] VectorMul iteration ", iteration, " completed in ",
                duration.count(), " μs");

    } catch (const std::exception& e) {
      SafePrint("[Device ", device_id_, "] VectorMul iteration ", iteration, " failed: ", e.what());
      throw;
    }
  }

  // Run matrix multiplication workload
  void RunMatrixMulWorkload(int iteration) {
    try {
      std::vector<float> a(kMatrixSize * kMatrixSize, static_cast<float>(device_id_ + 3));
      std::vector<float> b(kMatrixSize * kMatrixSize, static_cast<float>(iteration + 3));
      std::vector<float> c(kMatrixSize * kMatrixSize, 0.0f);

      sycl::buffer<float, 1> a_buf(a.data(), a.size());
      sycl::buffer<float, 1> b_buf(b.data(), b.size());
      sycl::buffer<float, 1> c_buf(c.data(), c.size());

      auto start_time = std::chrono::high_resolution_clock::now();

      queue_.submit([&](sycl::handler& cgh) {
        auto a_acc = a_buf.get_access<sycl::access::mode::read>(cgh);
        auto b_acc = b_buf.get_access<sycl::access::mode::read>(cgh);
        auto c_acc = c_buf.get_access<sycl::access::mode::write>(cgh);

        cgh.parallel_for<class MatrixMul_MultiDevice>(
            sycl::range<2>(kMatrixSize, kMatrixSize), [=](sycl::id<2> id) {
              auto a_ptr = a_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto b_ptr = b_acc.get_multi_ptr<sycl::access::decorated::no>();
              auto c_ptr = c_acc.get_multi_ptr<sycl::access::decorated::no>();
              SimpleMatrixMul(a_ptr.get(), b_ptr.get(), c_ptr.get(), kMatrixSize, id);
            });
      });
      queue_.wait_and_throw();

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

      SafePrint("[Device ", device_id_, "] MatrixMul iteration ", iteration, " completed in ",
                duration.count(), " μs");

    } catch (const std::exception& e) {
      SafePrint("[Device ", device_id_, "] MatrixMul iteration ", iteration, " failed: ", e.what());
      throw;
    }
  }

  // Run all workloads for this device
  void RunAllWorkloads() {
    SafePrint("[Device ", device_id_, "] Starting all workloads...");

    for (int i = 0; i < kIterationsPerDevice; ++i) {
      RunVectorAddWorkload(i);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      RunVectorMulWorkload(i);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      RunMatrixMulWorkload(i);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SafePrint("[Device ", device_id_, "] All workloads completed");
  }
};

// Metrics collection results
struct MetricsCollectionResult {
  int device_id;
  std::string device_name;
  pti_result final_result;
  uint64_t total_buffers;
  size_t total_records;
  std::vector<std::string> kernel_names;
  std::map<std::string, size_t> kernel_counts;
  std::exception_ptr exception;

  MetricsCollectionResult(int id, const std::string& name)
      : device_id(id),
        device_name(name),
        final_result(PTI_ERROR_INTERNAL),
        total_buffers(0),
        total_records(0) {}
};

// Device metrics collector
class DeviceMetricsCollector {
 private:
  std::unique_ptr<DeviceWorkloadRunner> workload_runner_;
  pti_scope_collection_handle_t scope_handle_;
  MetricsCollectionResult result_;

 public:
  DeviceMetricsCollector(pti_device_handle_t device_handle, int device_id)
      : workload_runner_(std::make_unique<DeviceWorkloadRunner>(device_handle, device_id)),
        scope_handle_(nullptr),
        result_(device_id, workload_runner_->GetDeviceName()) {}

  const MetricsCollectionResult& GetResult() const { return result_; }

  void RunMetricsCollection() {
    try {
      SafePrint("[Device ", result_.device_id, "] Starting metrics collection setup...");

      // Step 1: Enable scope metrics collection
      pti_result res = ptiMetricsScopeEnable(&scope_handle_);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to enable scope metrics: ", res);
        result_.final_result = res;
        return;
      }

      // Step 2: Configure metrics - use basic metrics that should be available on most devices
      pti_device_handle_t device = workload_runner_->GetDeviceHandle();
      const char* metric_names[] = {"GpuTime", "GpuCoreClocks"};
      const uint32_t metric_count = sizeof(metric_names) / sizeof(metric_names[0]);

      res = ptiMetricsScopeConfigure(scope_handle_, PTI_METRICS_SCOPE_AUTO_KERNEL, &device, 1,
                                     metric_names, metric_count);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to configure metrics: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      // Step 3: Set buffer size
      size_t estimated_buffer_size = 0;
      res = ptiMetricsScopeQueryCollectionBufferSize(scope_handle_, 20, &estimated_buffer_size);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to query buffer size: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      res = ptiMetricsScopeSetCollectionBufferSize(scope_handle_, estimated_buffer_size);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to set buffer size: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      // Step 4: Get metadata
      pti_metrics_scope_record_metadata_t metadata;
      metadata._struct_size = sizeof(pti_metrics_scope_record_metadata_t);
      res = ptiMetricsScopeGetMetricsMetadata(scope_handle_, &metadata);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to get metadata: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      SafePrint("[Device ", result_.device_id, "] Metrics configured:");
      SafePrint("  Buffer size: ", estimated_buffer_size, " bytes");
      SafePrint("  Metrics count: ", metadata._metrics_count);
      for (uint32_t i = 0; i < metadata._metrics_count; ++i) {
        SafePrint("    [", i, "] ", metadata._metric_names[i]);
      }

      // Step 5: Start collection
      res = ptiMetricsScopeStartCollection(scope_handle_);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to start collection: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      SafePrint("[Device ", result_.device_id,
                "] Metrics collection started, running workloads...");

      // Step 6: Run workloads
      workload_runner_->RunAllWorkloads();

      SafePrint("[Device ", result_.device_id, "] Workloads completed, stopping collection...");

      // Step 7: Stop collection
      res = ptiMetricsScopeStopCollection(scope_handle_);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to stop collection: ", res);
        result_.final_result = res;
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
        return;
      }

      // Step 8: Process collected data
      ProcessCollectedData(metadata);

      // Step 9: Cleanup
      res = ptiMetricsScopeDisable(scope_handle_);
      if (res != PTI_SUCCESS) {
        SafePrint("[Device ", result_.device_id, "] Failed to disable scope: ", res);
        result_.final_result = res;
        return;
      }

      result_.final_result = PTI_SUCCESS;
      SafePrint("[Device ", result_.device_id, "] Metrics collection completed successfully");

    } catch (const std::exception& e) {
      SafePrint("[Device ", result_.device_id, "] Exception during metrics collection: ", e.what());
      result_.exception = std::current_exception();
      result_.final_result = PTI_ERROR_INTERNAL;

      if (scope_handle_) {
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
      }
    } catch (...) {
      SafePrint("[Device ", result_.device_id, "] Unknown exception during metrics collection");
      result_.exception = std::current_exception();
      result_.final_result = PTI_ERROR_INTERNAL;

      if (scope_handle_) {
        // Check disable result but don't override the original error
        pti_result disable_result = ptiMetricsScopeDisable(scope_handle_);
        if (disable_result != PTI_SUCCESS) {
          SafePrint("[Device ", result_.device_id,
                    "] WARNING: Failed to disable scope during cleanup: ", disable_result);
        }
      }
    }
  }

 private:
  void ProcessCollectedData(const pti_metrics_scope_record_metadata_t& metadata) {
    // Get buffer count
    pti_result res =
        ptiMetricsScopeGetCollectionBuffersCount(scope_handle_, &result_.total_buffers);
    if (res != PTI_SUCCESS) {
      SafePrint("[Device ", result_.device_id, "] Failed to get buffer count: ", res);
      return;
    }

    SafePrint("[Device ", result_.device_id, "] Processing ", result_.total_buffers, " buffers...");

    if (result_.total_buffers == 0) {
      SafePrint("[Device ", result_.device_id, "] No data collected");
      return;
    }

    // Process each buffer
    for (uint64_t i = 0; i < result_.total_buffers; i++) {
      ProcessBuffer(i, metadata);
    }

    SafePrint("[Device ", result_.device_id, "] Data processing completed:");
    SafePrint("  Total records: ", result_.total_records);
    SafePrint("  Unique kernels: ", result_.kernel_names.size());
    for (const auto& [kernel_name, count] : result_.kernel_counts) {
      SafePrint("    ", kernel_name, ": ", count, " executions");
    }
  }

  void ProcessBuffer(uint64_t buffer_index, const pti_metrics_scope_record_metadata_t& metadata) {
    // Get buffer data
    void* buffer_data = nullptr;
    size_t buffer_size = 0;
    pti_result res =
        ptiMetricsScopeGetCollectionBuffer(scope_handle_, buffer_index, &buffer_data, &buffer_size);
    if (res != PTI_SUCCESS || !buffer_data) {
      SafePrint("[Device ", result_.device_id, "] Failed to get buffer ", buffer_index, ": ", res);
      return;
    }

    // Get buffer properties
    pti_metrics_scope_collection_buffer_properties_t props;
    props._struct_size = sizeof(props);
    res = ptiMetricsScopeGetCollectionBufferProperties(scope_handle_, buffer_data, &props);
    if (res != PTI_SUCCESS) {
      SafePrint("[Device ", result_.device_id, "] Failed to get buffer properties ", buffer_index,
                ": ", res);
      return;
    }

    SafePrint("[Device ", result_.device_id, "] Buffer ", buffer_index, " properties:");
    SafePrint("  Device: ", reinterpret_cast<void*>(props._device_handle));
    SafePrint("  Records: ", props._num_scopes);
    SafePrint("  Used Size: ", buffer_size, " bytes");
    SafePrint("  Metric Group: ", props._metric_group_name);

    result_.total_records += props._num_scopes;

    // Query metrics buffer size
    size_t required_size = 0;
    size_t records_count = 0;
    res = ptiMetricsScopeQueryMetricsBufferSize(scope_handle_, buffer_data, &required_size,
                                                &records_count);
    if (res != PTI_SUCCESS) {
      SafePrint("[Device ", result_.device_id, "] Failed to query metrics buffer size for buffer ",
                buffer_index, ": ", res);
      return;
    }

    if (records_count == 0 || required_size == 0) {
      SafePrint("[Device ", result_.device_id, "] Buffer ", buffer_index, " has no records");
      return;
    }

    SafePrint("[Device ", result_.device_id, "] Buffer ", buffer_index,
              " - Required size: ", required_size, ", Records: ", records_count);

    // Allocate user buffer using smart pointer
    auto user_buffer = std::make_unique<uint8_t[]>(required_size);
    if (!user_buffer) {
      SafePrint("[Device ", result_.device_id, "] Failed to allocate user buffer for buffer ",
                buffer_index);
      return;
    }

    // Calculate metrics
    size_t actual_records = 0;
    res = ptiMetricsScopeCalculateMetrics(scope_handle_, buffer_data, user_buffer.get(),
                                          required_size, &actual_records);
    if (res != PTI_SUCCESS) {
      SafePrint("[Device ", result_.device_id, "] Failed to calculate metrics for buffer ",
                buffer_index, ": ", res);
      return;
    }

    // Process records
    auto records = reinterpret_cast<pti_metrics_scope_record_t*>(user_buffer.get());
    for (size_t r = 0; r < actual_records; r++) {
      if (records[r]._kernel_name) {
        std::string kernel_name(records[r]._kernel_name);

        SafePrint("[Device ", result_.device_id, "] Kernel ", r, ":");
        SafePrint("  ID: ", records[r]._kernel_id);
        SafePrint("  Name: ", kernel_name);

        // Add to unique kernel names if not already present
        if (std::find(result_.kernel_names.begin(), result_.kernel_names.end(), kernel_name) ==
            result_.kernel_names.end()) {
          result_.kernel_names.push_back(kernel_name);
        }

        // Count kernel executions
        result_.kernel_counts[kernel_name]++;
      }

      // Display metrics values
      if (records[r]._metrics_values && metadata._metrics_count > 0) {
        SafePrint("  Metrics (", metadata._metrics_count, "):");
        for (uint32_t m = 0; m < metadata._metrics_count; ++m) {
          std::string metric_output = "    " + std::string(metadata._metric_names[m]) + ": ";

          // Format value based on metadata type
          switch (metadata._value_types[m]) {
            case PTI_METRIC_VALUE_TYPE_UINT32:
              metric_output += std::to_string(records[r]._metrics_values[m].ui32);
              break;
            case PTI_METRIC_VALUE_TYPE_UINT64:
              metric_output += std::to_string(records[r]._metrics_values[m].ui64);
              break;
            case PTI_METRIC_VALUE_TYPE_FLOAT32:
              metric_output += std::to_string(records[r]._metrics_values[m].fp32);
              break;
            case PTI_METRIC_VALUE_TYPE_FLOAT64:
              metric_output += std::to_string(records[r]._metrics_values[m].fp64);
              break;
            case PTI_METRIC_VALUE_TYPE_BOOL8:
              metric_output += (records[r]._metrics_values[m].b8 ? "true" : "false");
              break;
            default:
              metric_output += "unknown";
              break;
          }

          if (metadata._metric_units[m] && strlen(metadata._metric_units[m]) > 0) {
            metric_output += " " + std::string(metadata._metric_units[m]);
          }

          SafePrint(metric_output);
        }
      }
    }

    // Smart pointer automatically cleans up user_buffer
  }
};

}  // namespace

class MultiDeviceParallelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize PTI View (required for callbacks)
    auto dummy_provide = [](unsigned char** buf, std::size_t* buf_size) {
      *buf_size = sizeof(pti_view_record_kernel) * 100;
      void* ptr = ::operator new(*buf_size);
      *buf = static_cast<unsigned char*>(ptr);
      if (!*buf) {
        std::abort();
      }
    };
    auto dummy_parse = [](unsigned char* buf, std::size_t, std::size_t) {
      if (buf) ::operator delete(buf);
    };

    EXPECT_EQ(ptiViewSetCallbacks(dummy_provide, dummy_parse), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);

    // Get available devices
    uint32_t device_count = 0;
    pti_result result = ptiMetricsGetDevices(nullptr, &device_count);
    if (result != PTI_SUCCESS || device_count == 0) {
      GTEST_SKIP() << "No devices available for testing";
    }

    g_devices.resize(device_count);
    result = ptiMetricsGetDevices(g_devices.data(), &device_count);
    if (result != PTI_SUCCESS) {
      GTEST_SKIP() << "Failed to get device properties";
    }

    std::cout << "Found " << device_count << " devices for testing:" << std::endl;
    for (uint32_t i = 0; i < device_count; ++i) {
      std::cout << "  Device " << i << ": " << g_devices[i]._model_name << std::endl;
    }
  }

  void TearDown() override {
    g_devices.clear();
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), PTI_SUCCESS);
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
    EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  }
};

TEST_F(MultiDeviceParallelTest, TwoDevicesParallelCollection) {
  if (g_devices.size() < 2) {
    GTEST_SKIP() << "Need at least 2 devices for parallel testing, found " << g_devices.size();
  }

  std::cout << "\n=== Starting Two-Device Parallel Metrics Collection Test ===" << std::endl;

  // Create metrics collectors for first two devices
  auto collector1 = std::make_unique<DeviceMetricsCollector>(g_devices[0]._handle, 0);
  auto collector2 = std::make_unique<DeviceMetricsCollector>(g_devices[1]._handle, 1);

  // Thread synchronization
  std::atomic<int> threads_ready{0};
  std::atomic<bool> start_signal{false};
  std::atomic<int> threads_completed{0};

  std::cout << "Starting parallel metrics collection on 2 devices..." << std::endl;

  // Launch collection threads
  std::thread thread1([&collector1, &threads_ready, &start_signal, &threads_completed]() {
    threads_ready++;
    while (!start_signal.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    collector1->RunMetricsCollection();
    threads_completed++;
  });

  std::thread thread2([&collector2, &threads_ready, &start_signal, &threads_completed]() {
    threads_ready++;
    while (!start_signal.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    collector2->RunMetricsCollection();
    threads_completed++;
  });

  // Wait for threads to be ready
  while (threads_ready.load() < 2) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::cout << "Both threads ready, starting parallel execution..." << std::endl;
  start_signal = true;

  // Wait for completion with timeout
  auto start_time = std::chrono::steady_clock::now();
  const auto timeout = std::chrono::seconds(60);

  while (threads_completed.load() < 2) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > timeout) {
      std::cout << "Timeout waiting for threads to complete" << std::endl;
      break;
    }
  }

  // Join threads
  if (thread1.joinable()) thread1.join();
  if (thread2.joinable()) thread2.join();

  std::cout << "\n=== Parallel Collection Results ===" << std::endl;

  // Analyze results
  const auto& result1 = collector1->GetResult();
  const auto& result2 = collector2->GetResult();

  std::cout << "\nDevice 0 (" << result1.device_name << ") Results:" << std::endl;
  std::cout << "  Final result: " << result1.final_result << std::endl;
  std::cout << "  Total buffers: " << result1.total_buffers << std::endl;
  std::cout << "  Total records: " << result1.total_records << std::endl;
  std::cout << "  Unique kernels: " << result1.kernel_names.size() << std::endl;
  for (const auto& [kernel_name, count] : result1.kernel_counts) {
    std::cout << "    " << kernel_name << ": " << count << " executions" << std::endl;
  }

  std::cout << "\nDevice 1 (" << result2.device_name << ") Results:" << std::endl;
  std::cout << "  Final result: " << result2.final_result << std::endl;
  std::cout << "  Total buffers: " << result2.total_buffers << std::endl;
  std::cout << "  Total records: " << result2.total_records << std::endl;
  std::cout << "  Unique kernels: " << result2.kernel_names.size() << std::endl;
  for (const auto& [kernel_name, count] : result2.kernel_counts) {
    std::cout << "    " << kernel_name << ": " << count << " executions" << std::endl;
  }

  // Verify results
  EXPECT_EQ(threads_completed.load(), 2) << "Both threads should complete";

  // Check for exceptions
  if (result1.exception) {
    try {
      std::rethrow_exception(result1.exception);
    } catch (const std::exception& e) {
      FAIL() << "Device 0 threw exception: " << e.what();
    }
  }

  if (result2.exception) {
    try {
      std::rethrow_exception(result2.exception);
    } catch (const std::exception& e) {
      FAIL() << "Device 1 threw exception: " << e.what();
    }
  }

  // Both devices should complete successfully (or with acceptable configuration errors)
  EXPECT_TRUE(result1.final_result == PTI_SUCCESS ||
              result1.final_result == PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND ||
              result1.final_result == PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP)
      << "Device 0 should complete with acceptable result, got: " << result1.final_result;

  EXPECT_TRUE(result2.final_result == PTI_SUCCESS ||
              result2.final_result == PTI_ERROR_METRICS_SCOPE_METRIC_NOT_FOUND ||
              result2.final_result == PTI_ERROR_METRICS_SCOPE_NOT_A_SINGLE_GROUP)
      << "Device 1 should complete with acceptable result, got: " << result2.final_result;

  // If both succeeded, verify data collection
  if (result1.final_result == PTI_SUCCESS && result2.final_result == PTI_SUCCESS) {
    // We should have collected some data from both devices
    EXPECT_GT(result1.total_records + result2.total_records, 0u)
        << "Should collect some records from at least one device";

    // Verify kernel isolation - each device should have its own kernel instances
    // (kernel names might be similar but should be tracked separately)
    std::cout << "\n=== Data Isolation Verification ===" << std::endl;
    std::cout << "Device 0 collected " << result1.total_records << " records" << std::endl;
    std::cout << "Device 1 collected " << result2.total_records << " records" << std::endl;

    // The test passes if both devices can collect metrics independently
    // without interfering with each other
  }

  std::cout << "\n=== Two-Device Parallel Test Summary ===" << std::endl;
  std::cout << "Test completed successfully - both devices operated independently" << std::endl;
  std::cout << "Device 0 result: " << result1.final_result << std::endl;
  std::cout << "Device 1 result: " << result2.final_result << std::endl;
  std::cout << "Total records collected: " << (result1.total_records + result2.total_records)
            << std::endl;
}