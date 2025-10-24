//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <pti/pti_metrics_scope.h>
#include <spdlog/spdlog.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "metrics_handler.h"
#include "pti/pti_callback.h"
#include "pti_assert.h"
#include "pti_metrics_scope_buffer.h"
#include "pti_metrics_scope_buffer_handler.h"
#include "pti_metrics_scope_helper.h"

namespace {

// Global map to manage scope collection handles
std::unordered_map<pti_scope_collection_handle_t, std::unique_ptr<_pti_scope_collection_handle_t>>
    g_scope_handles;

// Mutex to protect the global map, adding/remoing objects from this
// this map also serves to validate that handle passed by user is valid
std::shared_mutex g_scope_handles_mutex;

// For now - most of scope operations are serialized
// TODO - do it by Scope to allow more parallelism
std::mutex g_scope_ops_mutex;

// TODO: maybe_unused because SPDLOG_ERROR not guaranteed to be there on release builds
void LogException([[maybe_unused]] const std::exception& excep) {
  SPDLOG_ERROR("Caught exception before return: {}", excep.what());
}

bool IsOurHandle(pti_scope_collection_handle_t handle) {
  std::shared_lock<std::shared_mutex> lock(g_scope_handles_mutex);
  return g_scope_handles.find(handle) != g_scope_handles.end();
}

/**
 * @brief Callback function for scope-based metrics collection
 * This function handles GPU operation events and injects metric queries automatically
 *
 * @param[in] domain                    Callback domain (GPU operation events)
 * @param[in] driver_group_id           Driver group identifier
 * @param[in] driver_api_id             Driver API identifier
 * @param[in] backend_context           Backend context handle
 * @param[in] cb_data                   Callback data containing operation details
 * @param[in] user_data                 User data (scope collection handle)
 * @param[in/out] instance_user_data    Instance-specific user data
 */
void MetricsScopeCallback(pti_callback_domain domain,
                          [[maybe_unused]] pti_api_group_id driver_group_id,
                          [[maybe_unused]] uint32_t driver_api_id,
                          pti_backend_ctx_t backend_context, void* cb_data, void* user_data,
                          [[maybe_unused]] void** instance_user_data) {
  SPDLOG_TRACE(
      "========================= MetricsScopeCallback triggered =================================");
  SPDLOG_TRACE("Domain: {}", ptiCallbackDomainTypeToString(domain));
  SPDLOG_TRACE("Driver Group ID: {}", static_cast<uint64_t>(driver_group_id));
  SPDLOG_TRACE("Driver API ID: {}", driver_api_id);
  SPDLOG_TRACE("Instance user data: {}", static_cast<void*>(instance_user_data));

  _pti_scope_collection_handle_t* scope_collection_handle =
      static_cast<_pti_scope_collection_handle_t*>(user_data);

  if (!scope_collection_handle || !scope_collection_handle->is_collection_active_) {
    SPDLOG_TRACE("MetricsScopeCallback: Collection not active, returning");
    return;
  }
  auto* callback_data = static_cast<pti_callback_gpu_op_data*>(cb_data);
  if (callback_data == nullptr) {
    SPDLOG_TRACE("MetricsScopeCallback: Callback data is null. Skipping ",
                 ptiCallbackDomainTypeToString(domain));
    return;
  }

  // Delegate to handle method for cleaner separation
  HandleKernelEvent(scope_collection_handle, callback_data, backend_context, domain);
}

}  // namespace

/**
 * @brief Collect query data for a completed kernel operation
 * This function retrieves raw metric data from the query associated with the kernel
 * and stores it in the collection buffer for later processing
 *
 * @param[in] scope_collection_handle    Scope collection handle
 * @param[in] kernel_id                  Unique identifier for the kernel
 * @param[in] kernel_name                Name of the kernel
 * @param[in] context                    Level Zero context handle
 * @param[in] queue                      Level Zero command queue handle
 * @param[in] submit_type                Type of submission operation
 */
void CollectQueryDataForKernel(pti_scope_collection_handle_t scope_collection_handle,
                               uint64_t kernel_id, const char* kernel_name,
                               ze_context_handle_t context, ze_command_queue_handle_t queue,
                               pti_backend_command_list_type submit_type) {
  std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

  SPDLOG_TRACE("CollectQueryDataForKernel: Collecting query data for kernel ID: {}", kernel_id);

  // Get query from profiler (single source of truth)
  zet_metric_query_handle_t query =
      scope_collection_handle->query_profiler_->GetQueryForKernel(kernel_id);
  ze_event_handle_t completion_event =
      scope_collection_handle->query_profiler_->GetEventForQuery(query);

  if (!ValidateQueryAndEvent(query, completion_event, kernel_id)) {
    return;
  }

  // Get raw metric data
  std::vector<uint8_t> raw_data;
  if (!GetRawMetricData(query, raw_data, kernel_name)) {
    return;
  }

  // Create kernel data record
  auto kernel_data =
      CreateKernelMetricData(kernel_id, kernel_name, utils::GetTime(), utils::GetTime(),
                             raw_data.data(), raw_data.size(), context, queue, submit_type);

  if (!kernel_data) return;

  // Store the data in buffer
  bool success = StoreKernelData(scope_collection_handle, std::move(kernel_data), raw_data.size());
  if (success) {
    SPDLOG_TRACE(
        "CollectQueryDataForKernel: Successfully collected {} bytes of metric data for kernel: {}",
        raw_data.size(), kernel_name);
  }

  // Clean up
  scope_collection_handle->query_profiler_->RemoveKernelQuery(kernel_id);

  ze_result_t destroy_status = zeEventDestroy(completion_event);
  if (destroy_status != ZE_RESULT_SUCCESS) {
    SPDLOG_WARN("CollectQueryDataForKernel: Failed to destroy completion event: 0x{:x}",
                static_cast<uint32_t>(destroy_status));
  }
}

/**
 * @brief Allocate and initialize the scope collection handle
 * Usage: Call this function first to create a scope collection handle before configuring metrics
 * collection
 *
 * @param[out] scope_collection_handle       Pointer to store the scope collection handle
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeEnable(pti_scope_collection_handle_t* scope_collection_handle) {
  try {
    if (scope_collection_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    auto handle = std::make_unique<_pti_scope_collection_handle_t>();

    // Create the handle
    pti_scope_collection_handle_t new_handle = handle.get();

    // Protect global map access with mutex
    {
      std::lock_guard<std::shared_mutex> lock(g_scope_handles_mutex);
      g_scope_handles[new_handle] = std::move(handle);
    }

    *scope_collection_handle = new_handle;

    SPDLOG_TRACE("ptiMetricsScopeEnable: Scope metrics collection handle enabled");
    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Configure scope collection for a single device
 * This function resolves the metric group from metric names and initializes the query profiler
 *
 * @param[in] scope_collection_handle    Scope collection handle to configure
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeConfigureSingleDevice(
    pti_scope_collection_handle_t scope_collection_handle) {
  try {
    if (scope_collection_handle == nullptr) {
      SPDLOG_DEBUG("MScopeConfigOneDevice: scope_collection_handle is null");
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result res = ResolveGroupFromMetricNames(scope_collection_handle);
    if (res != PTI_SUCCESS) {
      SPDLOG_DEBUG(
          "ptiMetricsScopeConfigureSingleDevice: Failed to resolve metric group from names");
      return res;
    }

    // Create the query profiler for this device/group
    scope_collection_handle->query_profiler_ = std::make_unique<PtiQueryMetricsProfiler>(
        scope_collection_handle->device_handle_, scope_collection_handle->metrics_group_handle_);

    SPDLOG_TRACE("Created query profiler:");
    SPDLOG_TRACE("---------------------------------");
    SPDLOG_TRACE("device handle: {}", static_cast<void*>(scope_collection_handle->device_handle_));
    SPDLOG_TRACE("group handle: {}",
                 static_cast<void*>(scope_collection_handle->metrics_group_handle_));
    std::string metric_names_str;
    for (size_t i = 0; i < scope_collection_handle->requested_metric_properties_.size(); ++i) {
      metric_names_str += GetMetricName(scope_collection_handle, i);
      metric_names_str += " ";
    }
    SPDLOG_TRACE("metric names: {}", metric_names_str);
    SPDLOG_TRACE("---------------------------------");

    // Initialize the profiler
    pti_result start_res = scope_collection_handle->query_profiler_->StartProfiling(false);
    if (start_res != PTI_SUCCESS) {
      std::cerr << "Failed to initialize query profiler" << std::endl;
      scope_collection_handle->query_profiler_.reset();
      return start_res;
    }

    scope_collection_handle->is_configured_ = true;

    scope_collection_handle->buffer_manager_ = std::make_unique<PtiMetricsScopeBufferHandle>();

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Configure MetricsScope collection
 *
 * @param[in] scope_collection_handle        Scope collection handle obtained from
 * ptiMetricsScopeEnable
 * @param[in] collection_mode                Collection mode;
 *                                           currently only PTI_METRICS_SCOPE_AUTO_KERNEL is
 * supported
 * @param[in] devices_to_profile             Array of device handles for target devices;
 *                                           currently only one device per MetricsScope is supported
 * @param[in] device_count                   Number of devices in the devices_to_profile array
 * @param[in] metric_names                   Array of metric names to collect
 * @param[in] metric_count                   Number of metric names in the metric_names array
 *
 * @return pti_result
 *
 */
pti_result ptiMetricsScopeConfigure(pti_scope_collection_handle_t scope_collection_handle,
                                    pti_metrics_scope_mode_t collection_mode,
                                    pti_device_handle_t* devices_to_profile, uint32_t device_count,
                                    const char** metric_names, size_t metric_count) {
  try {
    std::lock_guard<std::mutex> lock(g_scope_ops_mutex);

    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Validate all input arguments
    auto result = ValidateConfigurationArguments(devices_to_profile, device_count, metric_names,
                                                 metric_count);
    if (result != PTI_SUCCESS) {
      return result;
    }

    // Validate the target device
    pti_device_handle_t target_device = *devices_to_profile;
    result = ValidateTargetDevice(target_device);
    if (result != PTI_SUCCESS) {
      return result;
    }

    // Set up the scope collection handle
    scope_collection_handle->device_handle_ = target_device;

    result = SetupMetricProperties(scope_collection_handle, metric_names, metric_count);
    if (result != PTI_SUCCESS) {
      return result;
    }

    switch (collection_mode) {
      case PTI_METRICS_SCOPE_AUTO_KERNEL:
        // Automatic per-kernel profiling
        return ptiMetricsScopeConfigureSingleDevice(scope_collection_handle);

      case PTI_METRICS_SCOPE_USER:
        SPDLOG_DEBUG("ptiMetricsScopeConfigure: User scope mode not implemented");
        return PTI_ERROR_NOT_IMPLEMENTED;

      default:
        return PTI_ERROR_BAD_ARGUMENT;
    }
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Query the estimated collection buffer size required for collecting metrics for the
 * specified scope count
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] scopes_number                  Number of scopes to estimate collection buffer size
 * @param[out] estimated_buffer_size         Pointer to store the estimated collection buffer size
 * in bytes
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeQueryCollectionBufferSize(
    pti_scope_collection_handle_t scope_collection_handle, size_t scopes_number,
    size_t* estimated_buffer_size) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->is_configured_) {
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    // TODO: as soon as there is Level-Zero API that allows to Query metrcis recornd size
    // - we should use it here
    size_t estimated_per_scope = kEstimatedSizeOfRawRecord;

    if (scopes_number > kMaxNumberOfRawRecordsPerCollectionBuffer) {
      SPDLOG_WARN(
          "ptiMetricsScopeQueryCollectionBufferSize: Requested scopes_number ({}) exceeds "
          "maximum per buffer ({}). Capping to maximum.",
          scopes_number, kMaxNumberOfRawRecordsPerCollectionBuffer);
      scopes_number = kMaxNumberOfRawRecordsPerCollectionBuffer;
      return PTI_WARN_METRICS_SCOPE_PARTIAL_BUFFER;
    }

    PTI_ASSERT(scopes_number <= kMaxNumberOfRawRecordsPerCollectionBuffer);
    size_t buffer_size = scopes_number * estimated_per_scope;

    *estimated_buffer_size =
        (buffer_size > kMinCollectionBufferSize) ? buffer_size : kMinCollectionBufferSize;

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Set the collection buffer size to be used during collection
 * Note: As soon as the first such buffer is full, PTI will allocate a second one and so on.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] buffer_size                    Size of the collection buffer in bytes for later
 * allocation
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeSetCollectionBufferSize(
    pti_scope_collection_handle_t scope_collection_handle, size_t buffer_size) {
  try {
    std::lock_guard<std::mutex> lock(g_scope_ops_mutex);

    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{} could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->is_configured_) {
      SPDLOG_DEBUG("{}: Scope collection handle not configured", __func__);
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    if (scope_collection_handle->is_collection_active_) {
      return PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED;
    }

    if (!scope_collection_handle->buffer_manager_) {
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    if (buffer_size == 0) {
      // Auto-calculate reasonable default
      size_t default_size = 0;
      pti_result est_result = ptiMetricsScopeQueryCollectionBufferSize(
          scope_collection_handle, 10, &default_size);  // Default for 10 kernels
      if (est_result == PTI_SUCCESS) {
        buffer_size = default_size;
        SPDLOG_WARN(
            "ptiMetricsScopeSetCollectionBufferSize: Zero buffer size specified, "
            "using auto-calculated size: {} bytes",
            buffer_size);
      } else {
        buffer_size = 4096;  // Fallback minimum
        SPDLOG_WARN(
            "ptiMetricsScopeSetCollectionBufferSize: Zero buffer size specified, "
            "using fallback size: {} bytes",
            buffer_size);
      }
    }

    scope_collection_handle->configured_buffer_size_ = buffer_size;

    // Pre-allocate the first collection buffer
    pti_result result = scope_collection_handle->buffer_manager_->CreateBuffer(
        buffer_size, scope_collection_handle->device_handle_,
        scope_collection_handle->next_buffer_id_++);

    if (result != PTI_SUCCESS) {
      return PTI_ERROR_METRICS_SCOPE_OUT_OF_MEMORY;
    }

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Begin the metrics collection
 *
 * @param[in] scope_collection_handle    Scope collection handle
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeStartCollection(pti_scope_collection_handle_t scope_collection_handle) {
  try {
    std::lock_guard<std::mutex> lock(g_scope_ops_mutex);

    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->is_configured_) {
      SPDLOG_DEBUG("{}: Scope collection handle not configured", __func__);
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    if (scope_collection_handle->is_collection_active_) {
      SPDLOG_DEBUG("ptiMetricsScopeStartCollection: Collection already active");
      return PTI_ERROR_METRICS_COLLECTION_ALREADY_ENABLED;
    }

    // Register the callback for automatic query injection with the handle as user_data
    pti_result result = ptiCallbackSubscribe(&scope_collection_handle->callback_subscriber_,
                                             MetricsScopeCallback, scope_collection_handle);
    if (result != PTI_SUCCESS) {
      SPDLOG_DEBUG("ptiMetricsScopeStartCollection: Failed to subscribe to callback: {}",
                   static_cast<int>(result));
      return result;
    }

    // Enable GPU operation completion domain
    result = ptiCallbackEnableDomain(scope_collection_handle->callback_subscriber_,
                                     PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED, 1, 1);
    if (result != PTI_SUCCESS) {
      SPDLOG_DEBUG(
          "ptiMetricsScopeStartCollection: Failed to enable GPU_OPERATION_COMPLETED domain: {}",
          static_cast<int>(result));
      ptiCallbackUnsubscribe(scope_collection_handle->callback_subscriber_);
      scope_collection_handle->callback_subscriber_ = 0;
      return result;
    }

    // Enable driver GPU operation append for query injection
    result = ptiCallbackEnableDomain(scope_collection_handle->callback_subscriber_,
                                     PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED, 1, 1);
    if (result != PTI_SUCCESS) {
      SPDLOG_DEBUG(
          "ptiMetricsScopeStartCollection: Failed to enable DRIVER_GPU_OPERATION_APPEND domain: {}",
          static_cast<int>(result));
      ptiCallbackUnsubscribe(scope_collection_handle->callback_subscriber_);
      scope_collection_handle->callback_subscriber_ = 0;
      return result;
    }

    scope_collection_handle->is_collection_active_ = true;

    SPDLOG_TRACE(
        "ptiMetricsScopeStartCollection: Scope metrics collection started with callback "
        "registration");
    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

static pti_result InternalMetricsScopeStopCollectionNoHandleCheck(
    pti_scope_collection_handle_t scope_collection_handle) {
  if (!scope_collection_handle->is_collection_active_) {
    SPDLOG_DEBUG("{}: Collection not active", __func__);
    return PTI_ERROR_METRICS_COLLECTION_NOT_ENABLED;
  }
  // Disable callback domains first
  if (scope_collection_handle->callback_subscriber_ != 0) {
    pti_result result = ptiCallbackDisableDomain(scope_collection_handle->callback_subscriber_,
                                                 PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_COMPLETED);
    if (result != PTI_SUCCESS) {
      SPDLOG_WARN("{}: Failed to disable GPU_OPERATION_COMPLETED domain: {}", __func__,
                  static_cast<int>(result));
    }
    result = ptiCallbackDisableDomain(scope_collection_handle->callback_subscriber_,
                                      PTI_CB_DOMAIN_DRIVER_GPU_OPERATION_APPENDED);
    if (result != PTI_SUCCESS) {
      SPDLOG_WARN("{}: Failed to disable DRIVER_GPU_OPERATION_APPEND domain: {}", __func__,
                  static_cast<int>(result));
    }

    // Unregister the callback
    result = ptiCallbackUnsubscribe(scope_collection_handle->callback_subscriber_);
    if (result != PTI_SUCCESS) {
      SPDLOG_WARN("ptiMetricsScopeStopCollection: Failed to unsubscribe callback: {}",
                  static_cast<int>(result));
    }
    scope_collection_handle->callback_subscriber_ = 0;
    // Move current buffer to collection if it has data
    if (scope_collection_handle->buffer_manager_ &&
        scope_collection_handle->buffer_manager_->HasCurrentBufferWithData()) {
      pti_result result = scope_collection_handle->buffer_manager_->FinalizeCurrentBuffer();
      if (result != PTI_SUCCESS) {
        SPDLOG_WARN("{}: Failed to finalize current buffer: {}", __func__,
                    static_cast<int>(result));
      }
    }

    scope_collection_handle->is_collection_active_ = false;
    SPDLOG_TRACE("{}: Scope metrics collection stopped and callback unregistered", __func__);

    return PTI_SUCCESS;
  }
  return PTI_ERROR_INTERNAL;
}

/**
 * @brief Stop metrics scope collection
 *
 * @param[in] scope_collection_handle        Scope collection handle
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeStopCollection(pti_scope_collection_handle_t scope_collection_handle) {
  try {
    std::lock_guard<std::mutex> lock(g_scope_ops_mutex);

    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }
    return InternalMetricsScopeStopCollectionNoHandleCheck(scope_collection_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get metadata for user's metrics buffer in a scope collection
 * This function provides type and unit information for all requested metrics.
 * Call this function to get metadata per scope that applies to all records in all buffers.
 *
 * The metadata structure contains direct pointers to metric information stored within the
 * scope collection handle. These pointers remain valid until ptiMetricsScopeDisable is called.
 *
 * Usage:
 *  - User must set metadata->_struct_size = sizeof(pti_metrics_scope_record_metadata_t) before
 * calling
 *  - Function populates metadata->_metrics_count and the three array pointers
 *  - No memory allocation is performed by this function
 *  - Returned pointers reference internal scope collection data (no copying)
 *
 * @param[in] scope_collection_handle    Scope collection handle
 * @param[out] metadata                  Metadata structure to populate
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeGetMetricsMetadata(pti_scope_collection_handle_t scope_collection_handle,
                                             pti_metrics_scope_record_metadata_t* metadata) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!metadata) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Validate that user set _struct_size
    if (metadata->_struct_size == 0) {
      SPDLOG_TRACE("{}: Metadata struct size is 0/not set", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->is_configured_) {
      return PTI_ERROR_METRICS_BAD_COLLECTION_CONFIGURATION;
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    size_t metrics_count =
        static_cast<size_t>(scope_collection_handle->requested_metric_properties_.size());

    // Populate metadata struct fields with direct pointers
    metadata->_metrics_count = metrics_count;

    if (metrics_count > 0) {
      metadata->_value_types = scope_collection_handle->requested_value_types_.data();
      metadata->_metric_names = scope_collection_handle->requested_metric_names_.data();
      metadata->_metric_units = scope_collection_handle->requested_metric_units_.data();
    } else {
      // No metrics case
      metadata->_value_types = nullptr;
      metadata->_metric_names = nullptr;
      metadata->_metric_units = nullptr;
    }

    return PTI_SUCCESS;

  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get the number of collection buffers available
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[out] buffer_count                  Pointer to store the number of available collection
 * buffers
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeGetCollectionBuffersCount(
    pti_scope_collection_handle_t scope_collection_handle, size_t* buffer_count) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }
    if (!buffer_count) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (scope_collection_handle->is_collection_active_) {
      return PTI_ERROR_METRICS_COLLECTION_NOT_DISABLED;
    }

    if (!scope_collection_handle->buffer_manager_) {
      *buffer_count = 0;
      return PTI_SUCCESS;
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    *buffer_count = scope_collection_handle->buffer_manager_->GetBufferCount();

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get the collection buffer of the specified index and its size
 * Note: The size might be handy for future usage when such buffers might be stored by the user
 * for fully offline processing.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] buffer_index                   Index of the collection buffer to retrieve
 * @param[out] buffer                        Pointer to store the collection buffer address
 * @param[out] buffer_size                   Pointer to store the collection buffer size in bytes
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeGetCollectionBuffer(pti_scope_collection_handle_t scope_collection_handle,
                                              size_t buffer_index, void** buffer,
                                              size_t* buffer_size) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!buffer || !buffer_size) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->buffer_manager_) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    PtiMetricsScopeBuffer* target_buffer =
        scope_collection_handle->buffer_manager_->GetBuffer(buffer_index);
    if (!target_buffer) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    *buffer = target_buffer->GetRawBuffer();
    *buffer_size = target_buffer->GetUsedSize();

    return PTI_SUCCESS;

  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get information about the collection buffer
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] collection_buffer              Collection buffer to query properties for
 * @param[in/out] props                      Pointer to store the collection buffer properties;
 *                                           user must not forget to initialize props->_struct_size
 * prior to the call
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeGetCollectionBufferProperties(
    pti_scope_collection_handle_t scope_collection_handle, void* collection_buffer,
    pti_metrics_scope_collection_buffer_properties_t* props) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!collection_buffer || !props) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Validate structure size.
    // To be used in further version to distinguish between versions of this structure
    if (props->_struct_size < sizeof(pti_metrics_scope_collection_buffer_properties_t)) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->buffer_manager_) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    PtiMetricsScopeBuffer* found_buffer =
        scope_collection_handle->buffer_manager_->FindBufferByRawPointer(collection_buffer);
    if (!found_buffer) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Fill properties using Buffer methods
    props->_device_handle = found_buffer->GetDeviceHandle();
    props->_num_scopes = found_buffer->GetRecordCount();
    props->_buffer_size = found_buffer->GetUsedSize();

    // TODO: Add timing information if needed
    props->_host_time_first_scope_append = 0;
    props->_host_time_last_scope_append = 0;
    props->_metric_group_name = scope_collection_handle->collected_metrics_group_name_;

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Query for the required metrics buffer size for storing calculated metrics records
 *
 * This function calculates the exact metrics buffer size needed to store all records
 * from a collection buffer, including space for strings and metric values.
 *
 * @param[in] scope_collection_handle           Scope collection handle
 * @param[in] collection_buffer                 Collection buffer to query
 * @param[out] required_metrics_buffer_size     Required metrics buffer size in bytes
 * @param[out] records_count                    Number of records that will be stored
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeQueryMetricsBufferSize(
    pti_scope_collection_handle_t scope_collection_handle, void* collection_buffer,
    size_t* required_metrics_buffer_size, size_t* records_count) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!collection_buffer || !required_metrics_buffer_size || !records_count) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    PtiMetricsScopeBuffer* found_buffer =
        scope_collection_handle->buffer_manager_->FindBufferByRawPointer(collection_buffer);
    if (!found_buffer) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    *records_count = found_buffer->GetRecordCount();
    // this is indeed self-check.
    PTI_ASSERT(*records_count <= kMaxNumberOfRawRecordsPerCollectionBuffer);
    if (*records_count == 0) {
      *required_metrics_buffer_size = 0;
      return PTI_SUCCESS;
    }

    size_t total_size = 0;

    // Space for record structs (aligned)
    size_t records_array_size = *records_count * sizeof(pti_metrics_scope_record_t);
    records_array_size = AlignUp(records_array_size, 8);
    total_size += records_array_size;

    // Calculate space needed for each record's data
    // TODO: maybe can be optimized if the size of each record is the same
    for (size_t i = 0; i < *records_count; ++i) {
      const kernel_metric_data* kernel_data = found_buffer->GetRecord(i);
      if (!kernel_data) continue;

      // Space for metric values array
      uint32_t num_metrics = scope_collection_handle->requested_metric_properties_.size();
      if (num_metrics > 0) {
        size_t values_size = num_metrics * sizeof(pti_value_t);
        total_size += AlignUp(values_size, 8);
      }
    }

    *required_metrics_buffer_size = total_size;
    return PTI_SUCCESS;

  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Calculate metrics from collection data and populate into user-provided metrics buffer
 * Usage:   The user must first call ptiMetricsScopeQueryMetricsBufferSize to determine
 *          the required metrics buffer size, then allocate the metrics buffer and call this
 * function.
 *
 * @param[in] scope_collection_handle        Scope collection handle
 * @param[in] collection_buffer              Collection buffer containing raw metrics data
 * @param[in] metrics_buffer                 User metrics buffer for storing records
 * @param[in] metrics_buffer_size            Size of metrics_buffer in bytes
 * @param[out] records_count                 Number of records that will be/were written to the
 * metrics buffer
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeCalculateMetrics(pti_scope_collection_handle_t scope_collection_handle,
                                           void* collection_buffer, void* metrics_buffer,
                                           size_t metrics_buffer_size, size_t* records_count) {
  try {
    if (!IsOurHandle(scope_collection_handle)) {
      SPDLOG_DEBUG("{}: could not find a scope_collection_handle", __func__);
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // TODO: Validate collection_buffer - that it is indeed our buffer
    if (!collection_buffer || !metrics_buffer || !records_count) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (!scope_collection_handle->buffer_manager_) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Get the total size needed for all records
    size_t total_size_needed = 0;
    size_t total_records = 0;
    pti_result size_result = ptiMetricsScopeQueryMetricsBufferSize(
        scope_collection_handle, collection_buffer, &total_size_needed, &total_records);
    if (size_result != PTI_SUCCESS) {
      return size_result;
    }

    if (total_records == 0) {
      *records_count = 0;
      return PTI_ERROR_METRICS_SCOPE_INVALID_COLLECTION_BUFFER;
    }

    // Calculate size per record
    size_t size_per_record = total_size_needed / total_records;

    // Calculate how many records can fit
    size_t records_that_fit = metrics_buffer_size / size_per_record;

    // Make sure we don't exceed the available records
    if (records_that_fit > total_records) {
      records_that_fit = total_records;
    }

    if (records_that_fit == 0) {
      *records_count = 0;
      SPDLOG_DEBUG(
          "ptiMetricsScopeCalculateMetrics: Buffer too small ({} bytes) for even one record "
          "(need {} bytes). Minimum buffer size required: {} bytes",
          metrics_buffer_size, size_per_record, size_per_record);
      return PTI_ERROR_METRICS_SCOPE_COLLECTION_BUFFER_TOO_SMALL;
    }

    if (records_that_fit < total_records) {
      SPDLOG_WARN(
          "ptiMetricsScopeCalculateMetrics: Buffer can fit {} of {} records. "
          "For complete data, allocate {} bytes (current: {} bytes)",
          records_that_fit, total_records, total_size_needed, metrics_buffer_size);
      // Continue processing what fits
    }

    std::lock_guard<std::mutex> lock(scope_collection_handle->data_mutex_);

    // Find the buffer
    PtiMetricsScopeBuffer* found_buffer =
        scope_collection_handle->buffer_manager_->FindBufferByRawPointer(collection_buffer);
    if (!found_buffer) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // Layout the buffer
    uint8_t* buffer_ptr = static_cast<uint8_t*>(metrics_buffer);
    uint8_t* current_pos = buffer_ptr;

    // Records array at the beginning (sized for records_that_fit)
    size_t final_records_array_size =
        AlignUp(records_that_fit * sizeof(pti_metrics_scope_record_t), 8);
    pti_metrics_scope_record_t* records =
        reinterpret_cast<pti_metrics_scope_record_t*>(current_pos);
    current_pos += final_records_array_size;

    zet_metric_group_handle_t metric_group =
        static_cast<zet_metric_group_handle_t>(scope_collection_handle->metrics_group_handle_);

    // Process each kernel record, ensuring we do not overflow the buffer
    size_t records_written = 0;
    for (size_t i = 0; i < total_records; ++i) {
      uint8_t* next_pos = ProcessSingleRecord(scope_collection_handle, found_buffer, records,
                                              metric_group, i, current_pos);
      if ((next_pos - buffer_ptr) > static_cast<ptrdiff_t>(metrics_buffer_size)) {
        // Not enough space for this record, stop writing
        break;
      }
      current_pos = next_pos;
      ++records_written;
    }

    *records_count = records_written;

    // Return a warning if we couldn't fit all records
    if (records_written < total_records) {
      return PTI_WARN_METRICS_SCOPE_PARTIAL_BUFFER;
    }

    return PTI_SUCCESS;
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Disable MetricsScope and free all associated resources
 *
 * @param[in] scope_collection_handle        Scope collection handle to cleanup
 *
 * @return pti_result
 */
pti_result ptiMetricsScopeDisable(pti_scope_collection_handle_t scope_collection_handle) {
  try {
    std::lock_guard<std::mutex> lock(g_scope_ops_mutex);

    // Remove handle from global registry under exclusive lock
    {
      std::lock_guard<std::shared_mutex> lock(g_scope_handles_mutex);

      // Validate handle exists in the registry
      auto it = g_scope_handles.find(scope_collection_handle);
      if (it == g_scope_handles.end()) {
        return PTI_ERROR_BAD_ARGUMENT;
      }
      PTI_ASSERT(it != g_scope_handles.end());

      // Stop collection if still active
      pti_result stop_result = PTI_SUCCESS;
      if (scope_collection_handle->is_collection_active_) {
        stop_result = InternalMetricsScopeStopCollectionNoHandleCheck(scope_collection_handle);
        if (stop_result != PTI_SUCCESS) {
          SPDLOG_WARN("Failed to stop collection during disable: {}",
                      static_cast<int>(stop_result));
        }
      }
      // Remove handle from registry
      g_scope_handles.erase(it);
      SPDLOG_TRACE("Scope metrics collection handle disabled");
      return stop_result;
    }
  } catch (const std::exception& e) {
    LogException(e);
    return PTI_ERROR_INTERNAL;
  } catch (...) {
    return PTI_ERROR_INTERNAL;
  }
}
