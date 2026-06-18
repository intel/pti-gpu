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

#include <array>
#include <cstring>
#include <iostream>
#include <string>
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
pti_scope_collection_handle_t scope_handle = nullptr;

void PrintMetadata(const pti_metrics_scope_record_metadata_t& metadata) {
  std::cout << "Metrics Metadata set to " << metadata._struct_size << " bytes\n\n";
  std::cout << "Metadata for all records:\n";
  std::cout << "  Metrics count per record: " << metadata._metrics_count << "\n";
  for (uint32_t i = 0; i < metadata._metrics_count; ++i) {
    const char* units = metadata._metric_units[i] ? metadata._metric_units[i] : "";
    std::cout << "  [" << i << "] " << metadata._metric_names[i] << " (" << units << ")\n";
  }
}

void PrintMetricValue(const pti_metrics_scope_record_t& record, uint32_t metric_index,
                      const pti_metrics_scope_record_metadata_t& metadata) {
  std::cout << "      " << metadata._metric_names[metric_index] << ": ";
  switch (metadata._value_types[metric_index]) {
    case PTI_METRIC_VALUE_TYPE_UINT32:
      std::cout << record._metrics_values[metric_index].ui32;
      break;
    case PTI_METRIC_VALUE_TYPE_UINT64:
      std::cout << record._metrics_values[metric_index].ui64;
      break;
    case PTI_METRIC_VALUE_TYPE_FLOAT32:
      std::cout << record._metrics_values[metric_index].fp32;
      break;
    case PTI_METRIC_VALUE_TYPE_FLOAT64:
      std::cout << record._metrics_values[metric_index].fp64;
      break;
    case PTI_METRIC_VALUE_TYPE_BOOL8:
      std::cout << (record._metrics_values[metric_index].b8 ? "true" : "false");
      break;
    default:
      std::cout << " ";
  }
  if (metadata._metric_units[metric_index] && strlen(metadata._metric_units[metric_index]) > 0) {
    std::cout << " " << metadata._metric_units[metric_index];
  }
  std::cout << "\n";
}

void PrintRecord(size_t record_index, const pti_metrics_scope_record_t& record,
                 const pti_metrics_scope_record_metadata_t& metadata) {
  std::cout << "\n  Kernel " << record_index << ":\n";
  std::cout << "      ID: " << record._kernel_id << "\n";
  if (record._kernel_name) {
    std::cout << "      Kernel Name: " << record._kernel_name << "\n";
  }
  for (uint32_t m = 0; m < metadata._metrics_count; ++m) {
    PrintMetricValue(record, m, metadata);
  }
}

void ProcessCollectionBuffer(size_t buffer_index,
                             const pti_metrics_scope_record_metadata_t& metadata) {
  std::cout << "\n" << std::string(80, '-') << "\n";
  std::cout << "                    Processing Collection Buffer " << buffer_index << "\n";
  std::cout << std::string(80, '-') << "\n";

  void* collection_buffer = nullptr;
  size_t actual_collection_buffer_size = 0;
  PTI_CHECK_SUCCESS(ptiMetricsScopeGetCollectionBuffer(
      scope_handle, buffer_index, &collection_buffer, &actual_collection_buffer_size));

  pti_metrics_scope_collection_buffer_properties_t props;
  props._struct_size = sizeof(pti_metrics_scope_collection_buffer_properties_t);
  pti_result result =
      ptiMetricsScopeGetCollectionBufferProperties(scope_handle, collection_buffer, &props);
  if (result != PTI_SUCCESS) {
    std::cerr << "Failed to get buffer properties for buffer " << buffer_index
              << ", error: " << result << "\n";
    return;
  }

  std::cout << "Collection Buffer " << buffer_index << " properties:\n";
  std::cout << "  Device: " << reinterpret_cast<void*>(props._device_handle) << "\n";
  std::cout << "  Records/Scopes: " << props._num_scopes << "\n";
  std::cout << std::dec << "  Used Collection Buffer Size: " << actual_collection_buffer_size
            << " bytes\n";
  std::cout << "  Collected Group Name: " << props._metric_group_name << "\n\n\n";

  size_t required_metrics_buffer_size = 0;
  size_t records_count = 0;
  result = ptiMetricsScopeQueryMetricsBufferSize(scope_handle, collection_buffer,
                                                 &required_metrics_buffer_size, &records_count);
  if (result != PTI_SUCCESS) {
    std::cerr << "Failed to query buffer size for buffer " << buffer_index << ", error: " << result
              << "\n";
    return;
  }

  std::cout << "Required metrics buffer size: " << required_metrics_buffer_size << " bytes\n";
  std::cout << "Records count: " << records_count << "\n";

  if (records_count == 0) {
    return;
  }

  std::vector<uint8_t> metrics_buffer;
  try {
    metrics_buffer.resize(required_metrics_buffer_size);
  } catch (const std::exception&) {
    std::cerr << "Failed to allocate metrics buffer\n";
    return;
  }

  std::cout << "Successfully allocated metrics buffer size: " << required_metrics_buffer_size
            << " bytes\n\n";

  size_t actual_records_count = 0;
  result = ptiMetricsScopeCalculateMetrics(scope_handle, collection_buffer, metrics_buffer.data(),
                                           required_metrics_buffer_size, &actual_records_count);
  if (result != PTI_SUCCESS) {
    std::cout << "\n  Failed to calculate metrics for buffer " << buffer_index
              << ", error: " << result << "\n";
    return;
  }

  std::cout << "Calculated records: " << actual_records_count << "\n";
  std::cout << "\n";
  std::cout << "       FORMATTED METRICS DISPLAY\n";

  auto records = reinterpret_cast<pti_metrics_scope_record_t*>(metrics_buffer.data());
  for (size_t r = 0; r < actual_records_count; ++r) {
    PrintRecord(r, records[r], metadata);
  }
}

void DisableViews() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY));
  PTI_CHECK_SUCCESS(ptiFlushAllViews());
}

}  // namespace

void StartProfiling(const std::vector<pti_device_handle_t>& device_handles) {
  samples_utils::CheckEnvVarOrSuggest("ZET_ENABLE_METRICS", "1");

  // For now, need ptiView to enable ptiCallback
  auto dummy_provide = [](unsigned char** buf, std::size_t* buf_size) {
    *buf_size = sizeof(pti_view_record_kernel) * 100;
    void* ptr = ::operator new(*buf_size);
    *buf = static_cast<unsigned char*>(ptr);
  };
  auto dummy_parse = [](unsigned char* buf, std::size_t, std::size_t) {
    if (buf) {
      ::operator delete(buf);
    }
  };

  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(dummy_provide, dummy_parse));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY));

  // Step 1: Enable scope metrics collection
  PTI_CHECK_SUCCESS(ptiMetricsScopeEnable(&scope_handle));

  std::cout << "Scope metrics collection enabled \n\n";

  pti_metrics_scope_mode_t collection_mode = PTI_METRICS_SCOPE_AUTO_KERNEL;

  // Build metric names to collect (all must belong to a single metric group on each device).
  // In auto-detect mode all devices must share the same model (uniformity check) so the same
  // metric group resolves on every device.
  auto metric_names = std::array{"GpuTime",
                                 "GpuCoreClocks",
                                 "AvgGpuCoreFrequencyMHz",
                                 "XVE_INST_EXECUTED_ALU0_ALL_UTILIZATION",
                                 "XVE_ACTIVE",
                                 "XVE_STALL"};  // metrics in ComputeBasic group

  // Step 2: Configure metrics.
  // Two modes are supported:
  //  - Auto-detect: pass devices=nullptr, count=0 to profile every available GPU.
  //    All devices must be the same model; otherwise
  //    PTI_ERROR_METRICS_SCOPE_DEVICE_TYPE_NOT_UNIFORM is returned.
  //  - Explicit:    pass a non-null array of one or more device handles to profile that subset.
  if (device_handles.empty()) {
    PTI_CHECK_SUCCESS(ptiMetricsScopeConfigure(scope_handle, collection_mode, nullptr, 0,
                                               metric_names.data(),
                                               static_cast<uint32_t>(metric_names.size())));
    std::cout << "Scope metrics collection configured (auto-detect: all available devices)\n\n";
  } else {
    auto mutable_handles = device_handles;
    PTI_CHECK_SUCCESS(
        ptiMetricsScopeConfigure(scope_handle, collection_mode, mutable_handles.data(),
                                 static_cast<uint32_t>(mutable_handles.size()), metric_names.data(),
                                 static_cast<uint32_t>(metric_names.size())));
    std::cout << "Scope metrics collection configured (explicit: " << mutable_handles.size()
              << " device(s))\n\n";
  }

  // Step 3: Query estimated raw collection buffer size for expected number of kernels per device.
  // The capacity set in step 4 is the per-collection-buffer size; the library will rotate
  // through additional buffers per (device, thread) as needed during collection.
  size_t expected_kernels = 5;
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

  // Step 2: Get metadata once per scope (reusable for all buffers/records)
  pti_metrics_scope_record_metadata_t metadata;
  metadata._struct_size = sizeof(pti_metrics_scope_record_metadata_t);
  PTI_CHECK_SUCCESS(ptiMetricsScopeGetMetricsMetadata(scope_handle, &metadata));
  PrintMetadata(metadata);

  // Step 3: Iterate collection buffers
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
    for (size_t i = 0; i < collection_buffer_count; ++i) {
      ProcessCollectionBuffer(i, metadata);
    }
  }

  // Step 4: Disable scope collection and views
  PTI_CHECK_SUCCESS(ptiMetricsScopeDisable(scope_handle));
  std::cout << "\n\nScope metrics collection disabled\n";

  DisableViews();

  std::cout << "Example completed successfully\n";
}
