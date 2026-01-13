//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file client.cc
 * @brief PTI Scope Metrics collection tool implementation
 *
 * This file demonstrates how to use the PTI Scope Metrics library to collect
 * GPU performance metrics during kernel execution.
 */

#include "client.h"

#include <iostream>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_metrics_scope.h"
#include "samples_utils.h"

/**
 * Terminologies:
 *  - Collection buffer(s) - buffer(s) contains raw collection data, owned by the PTI Metrics Scope
 * Interface; buffer size is setup by the user.
 *  - Metrics buffer(s)    - buffer(s) contains a set of uniform records
 * (pti_metrics_scope_record_t), owned by the user; those records are populated by the PTI Metrics
 * Scope Interface
 */
namespace {
// Tool resources
std::vector<pti_device_properties_t> devices;
uint32_t device_count = 0;
std::vector<pti_device_handle_t> device_handle_arr;
pti_scope_collection_handle_t scope_handle = nullptr;

}  // namespace

void StartProfiling() {
  // For now, need ptiView to enable ptiCallback
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

  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(dummy_provide, dummy_parse));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));

  // Get device count
  PTI_CHECK_SUCCESS(ptiMetricsGetDevices(nullptr, &device_count));

  // Allocate and get devices
  devices.resize(device_count);
  PTI_CHECK_SUCCESS(ptiMetricsGetDevices(devices.data(), &device_count));

  // Allocate and populate device handle array
  device_handle_arr.resize(device_count);
  for (uint32_t i = 0; i < device_count; ++i) {
    device_handle_arr[i] = devices[i]._handle;
  }

  // Step 1: Enable scope metrics collection
  PTI_CHECK_SUCCESS(ptiMetricsScopeEnable(&scope_handle));

  std::cout << "Scope metrics collection enabled \n\n";

  pti_metrics_scope_mode_t collection_mode = PTI_METRICS_SCOPE_AUTO_KERNEL;

  // Build metric names to collect (all must belong to a single metric group on the device)
  // Note: Replace with metrics available on your device/group, examples:
  // const char* metric_names1[] = { "SYSMEM_BYTE_READ", "SLM_BYTE_READ",
  // "LOAD_STORE_CACHE_BYTE_WRITE" }; //metrics not in the same group const char* metric_names2[] =
  // { "GpuTime", "GpuCoreClocks" }; //metrics spans in multiple groups

  // //metrics in one and only one group, but is not event-based - create query pool failed
  // const char* metric_names3[] = { "GpuTime", "GpuCoreClocks", "XveSliceFrequencyMHz",
  // "SYSMEM_BYTE_WRITE" }; const char* metric_names4[] = {"GpuTime", "Gpu_Core_Clocks"}; //metrics
  // spelled wrong
  const char* metric_names[] = {"GpuTime",
                                "GpuCoreClocks",
                                "AvgGpuCoreFrequencyMHz",
                                "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                "XVE_ACTIVE",
                                "XVE_STALL"};  // metrics in ComputeBasic group

  const size_t metric_count = static_cast<size_t>(sizeof(metric_names) / sizeof(metric_names[0]));

  // Step 2: Configure metrics
  // Note:    Passing a null pointer for devices_to_profile is not supported in this release
  //          and will return PTI_ERROR_NOT_IMPLEMENTED. Future releases may allow profiling
  //          all available devices when this argument is null.
  //          The current implementation supports profiling only a single device:
  //          the device pointed to by devices_to_profile.
  //          For example, passing &device_handle_arr[1] will profile the device at index 1;
  //          Passing device_handle_arr or &device_handle_arr[0] will profile device 0.
  PTI_CHECK_SUCCESS(ptiMetricsScopeConfigure(
      scope_handle, collection_mode, device_handle_arr.data(), 1, metric_names, metric_count));

  std::cout << "Scope metrics collection configured\n\n";

  // Step 3: Query estimated raw collection buffer size for expected number of kernels
  size_t expected_kernels = 5;  // Estimate based on your workload
  size_t estimated_collection_buffer_size = 0;
  PTI_CHECK_SUCCESS(ptiMetricsScopeQueryCollectionBufferSize(scope_handle, expected_kernels,
                                                             &estimated_collection_buffer_size));

  std::cout << std::dec << "Estimated raw collection buffer size for " << expected_kernels
            << " kernels: " << estimated_collection_buffer_size << " bytes\n";

  // Step 4: Set collection buffer size (capacity)
  PTI_CHECK_SUCCESS(
      ptiMetricsScopeSetCollectionBufferSize(scope_handle, estimated_collection_buffer_size));

  std::cout << "Collection buffer capacity set to " << estimated_collection_buffer_size
            << " bytes\n\n";

  // Step 5: Start metrics collection
  PTI_CHECK_SUCCESS(ptiMetricsScopeStartCollection(scope_handle));

  std::cout << "Scope metrics collection started\n\n";
  return;
}

void StopProfiling() {
  // Step 1: Stop metrics collection
  PTI_CHECK_SUCCESS(ptiMetricsScopeStopCollection(scope_handle));

  std::cout << "Scope metrics collection stopped\n";

  // Get metadata once per scope (reusable for all buffers/records)
  pti_metrics_scope_record_metadata_t metadata;

  // Set struct size
  metadata._struct_size = sizeof(pti_metrics_scope_record_metadata_t);

  // Populate metadata
  PTI_CHECK_SUCCESS(ptiMetricsScopeGetMetricsMetadata(scope_handle, &metadata));

  std::cout << "Metrics Metadata set to " << metadata._struct_size << " bytes\n\n";

  std::cout << "Metadata for all records:\n";
  std::cout << "  Metrics count per record: " << metadata._metrics_count << "\n";
  for (uint32_t i = 0; i < metadata._metrics_count; ++i) {
    std::cout << "  [" << i << "] " << metadata._metric_names[i] << " ("
              << metadata._metric_units[i] << ")\n";
  }

  // Step 2: Get collection buffer information
  size_t collection_buffer_count = 0;
  PTI_CHECK_SUCCESS(
      ptiMetricsScopeGetCollectionBuffersCount(scope_handle, &collection_buffer_count));

  std::cout << "\nNumber of collection buffers used: " << collection_buffer_count << "\n";

  if (collection_buffer_count == 0) {
    std::cout << "No data collected - this might indicate:\n";
    std::cout << "  1. Callbacks were not triggered\n";
    std::cout << "  2. No GPU kernels were detected\n";
    std::cout << "  3. Query data collection failed\n";
  } else {
    // Step 3: Process each collection buffer individually
    for (size_t i = 0; i < collection_buffer_count; i++) {
      std::cout << "\n" << std::string(80, '-') << "\n";
      std::cout << "                    Processing Collection Buffer " << i << "\n";
      std::cout << std::string(80, '-') << "\n";

      // Get collection buffer data
      void* collection_buffer = nullptr;
      size_t actual_collection_buffer_size = 0;
      PTI_CHECK_SUCCESS(ptiMetricsScopeGetCollectionBuffer(scope_handle, i, &collection_buffer,
                                                           &actual_collection_buffer_size));

      // Step 4: Get collection buffer properties
      pti_metrics_scope_collection_buffer_properties_t collection_buffer_props;
      collection_buffer_props._struct_size =
          sizeof(pti_metrics_scope_collection_buffer_properties_t);
      pti_result result = ptiMetricsScopeGetCollectionBufferProperties(
          scope_handle, collection_buffer, &collection_buffer_props);
      if (result != PTI_SUCCESS) {
        std::cerr << "Failed to get buffer properties for buffer " << i << ", error: " << result
                  << "\n";
        continue;
      }

      std::cout << "Collection Buffer " << i << " properties:\n";
      std::cout << "  Device: " << reinterpret_cast<void*>(collection_buffer_props._device_handle)
                << "\n";
      std::cout << "  Records/Scopes: " << collection_buffer_props._num_scopes << "\n";
      std::cout << std::dec << "  Used Collection Buffer Size: " << actual_collection_buffer_size
                << " bytes\n";
      std::cout << "  Collected Group Name: " << collection_buffer_props._metric_group_name
                << "\n\n\n";

      // Step 5: Calculate metrics for this collection buffer
      size_t required_metrics_buffer_size = 0;
      size_t records_count = 0;

      // Query for required metrics buffer size
      result = ptiMetricsScopeQueryMetricsBufferSize(scope_handle, collection_buffer,
                                                     &required_metrics_buffer_size, &records_count);

      if (result != PTI_SUCCESS) {
        std::cerr << "Failed to query buffer size for buffer " << i << ", error: " << result
                  << "\n";
        continue;
      }

      std::cout << "Required metrics buffer size: " << required_metrics_buffer_size << " bytes\n";
      std::cout << "Records count: " << records_count << "\n";

      if (records_count > 0) {
        // User allocates the metrics buffer
        std::vector<uint8_t> metrics_buffer;

        try {
          metrics_buffer.resize(required_metrics_buffer_size);
        } catch (const std::exception&) {
          std::cerr << "Failed to allocate metrics buffer\n";
          continue;
        }

        std::cout << "Successfully allocated metrics buffer size: " << required_metrics_buffer_size
                  << " bytes\n\n";

        // Calculate metrics from collection buffer into user metrics buffer
        size_t actual_records_count = 0;
        result =
            ptiMetricsScopeCalculateMetrics(scope_handle, collection_buffer, metrics_buffer.data(),
                                            required_metrics_buffer_size, &actual_records_count);

        if (result == PTI_SUCCESS) {
          std::cout << "Calculated records: " << actual_records_count << "\n";

          std::cout << "\n";
          std::cout << "       FORMATTED METRICS DISPLAY"
                    << "\n";

          // Access records directly from the metrics buffer
          auto records = reinterpret_cast<pti_metrics_scope_record_t*>(metrics_buffer.data());

          // Display calculated metrics for each kernel in this buffer
          for (size_t r = 0; r < actual_records_count; r++) {
            std::cout << "\n  Kernel " << r << ":\n";
            std::cout << "      ID: " << records[r]._kernel_id << "\n";
            if (records[r]._kernel_name) {
              std::cout << "      Kernel Name: " << records[r]._kernel_name << "\n";
            }

            // Direct access to requested metrics using metadata
            for (uint32_t m = 0; m < metadata._metrics_count; ++m) {
              std::cout << "      " << metadata._metric_names[m] << ": ";

              // Format value based on metadata type
              switch (metadata._value_types[m]) {
                case PTI_METRIC_VALUE_TYPE_UINT32:
                  std::cout << records[r]._metrics_values[m].ui32;
                  break;
                case PTI_METRIC_VALUE_TYPE_UINT64:
                  std::cout << records[r]._metrics_values[m].ui64;
                  break;
                case PTI_METRIC_VALUE_TYPE_FLOAT32:
                  std::cout << records[r]._metrics_values[m].fp32;
                  break;
                case PTI_METRIC_VALUE_TYPE_FLOAT64:
                  std::cout << records[r]._metrics_values[m].fp64;
                  break;
                case PTI_METRIC_VALUE_TYPE_BOOL8:
                  std::cout << (records[r]._metrics_values[m].b8 ? "true" : "false");
                  break;
                default:
                  std::cout << " ";
              }

              if (metadata._metric_units[m] && strlen(metadata._metric_units[m]) > 0) {
                std::cout << " " << metadata._metric_units[m];
              }
              std::cout << "\n";
            }
          }
        } else {
          std::cout << "\n  Failed to calculate metrics for buffer " << i << ", error: " << result
                    << "\n";
        }
      }
    }
  }

  // Step 6: Disable scope collection
  PTI_CHECK_SUCCESS(ptiMetricsScopeDisable(scope_handle));

  std::cout << "\n\nScope metrics collection disabled\n";

  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiFlushAllViews());

  std::cout << "Example completed successfully\n";
  return;
}
