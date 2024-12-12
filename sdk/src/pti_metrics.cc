//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "pti/pti_metrics.h"

#include "metrics_handler.h"

namespace {
// TODO: maybe_unused because SPDLOG_ERROR not guarenteed to be there on release builds
void LogException([[maybe_unused]] const std::exception& excep) {
  SPDLOG_ERROR("Caught exception before return: {}", excep.what());
}
}  // namespace

/**
 * @brief Get the properties of all devices on the system on which metric collection can be done
 * usage: 1- Call ptiMetricsGetDevices(nullptr, device_count) to discover the required buffer
 *           size; the required buffer size will be written to device_count.
 *        2- Allocate devices buffer of size sizeof(pti_device_properties_t) * device_count
 *        3- Call ptiMetricsGetDevices(devices, device_count) to get the properties of the
 *           discovered devices written to devices
 *        WARNING: It is expected that devices buffer is sufficiently large to hold the
 *        properties for all discovered devices. Passing in a buffer that
 *        is not sufficiently large with incorrect device_count parameter will result is a
 *        buffer overrun
 *
 * @param[in/out] devices               Pointer to the array of devices. If nullptr, the number
 *                                          of devices is returned in *device_count
 *                                          if not nullptr, it should point to the buffer of size:
 *                                          sizeof(pti_device_description_t) * (*device_count)
 * @param[in/out] device_count          Number of devices
 *                                          in - if devices is nullptr or if device_count is
 *                                          less than the required minimum buffer size,
 *                                          device_count will contain the required buffer size
 *                                          out - devices buffer size used
 *                                          Note: device_count cannot be nullptr
 *
 * @return pti_result
 */
pti_result ptiMetricsGetDevices(pti_device_properties_t* devices, uint32_t* device_count) {
  try {
    // device_count cannot be null
    if (device_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    uint32_t num_devices = 0;
    pti_result result = PTI_SUCCESS;

    /* Step 1: discover number of device */
    if (devices == nullptr) {
      result = MetricsCollectorInstance().GetDeviceCount(&num_devices);
      if (result == PTI_SUCCESS) {
        *device_count = num_devices;
        if (num_devices == 0) {
          SPDLOG_WARN("No supported devices found");
        }
      } else {
        SPDLOG_WARN("Failed to discover device count");
      }
    } else {
      /* Step 2: populate devices buffer */
      num_devices = *device_count;
      result = MetricsCollectorInstance().GetDevices(devices, &num_devices);
      if (result != PTI_SUCCESS) {
        SPDLOG_WARN("Failed to discover devices");
      }
    }
    return result;
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get the properties of the metric groups supported by the device
 * Usage: 1- Call ptiMetricsGetMetricGroups(device_handle, null, metrics_group_count) to discover
 * the required buffer size; the required buffer size will be written to metrics_group_count. 2-
 * Allocate metrics_groups buffer of size sizeof(pti_metrics_group_properties_t) *
 * metrics_group_count 3- Call ptiMetricsGetMetricGroups(device_handle, metrics_groups,
 * metrics_group_count) again to get the properties of the discovered metric groups written to
 * the supplied buffer WARNING: It is expected that metrics_groups is sufficiently large to hold
 * propertied for all discovered metric groups. Passing in a buffer that is not suffiently large
 * will result in a buffer overrun
 *
 * @param[in] device_handle                      Device handle
 * @param[in/out] metrics_groups              buffer where to save metric group properties for
 *                                               the metric groups that can be collected. Set to
 * null to discover the required buffer size
 * @param[in/out] metrics_group_count         if metrics_groups is set to null or
 *                                               metrics_group_count is less than the required
 * minimum buffer size, metrics_group_count will contain the metric group count Otherwise,
 * metrics_group_count will contain actual metric group count. Note: metrics_group_count
 * cannot be null
 *
 * @return pti_result
 */
pti_result ptiMetricsGetMetricGroups(pti_device_handle_t device_handle,
                                     pti_metrics_group_properties_t* metrics_groups,
                                     uint32_t* metrics_group_count) {
  try {
    if (device_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    // metrics_group_count cannot be null
    if (metrics_group_count == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    uint32_t group_count = 0;
    pti_result result = PTI_SUCCESS;

    /* step 1: Get number of metric groups */
    if (metrics_groups == nullptr) {
      result = MetricsCollectorInstance().GetMetricGroupCount(device_handle, &group_count);
      if (result == PTI_SUCCESS) {
        *metrics_group_count = group_count;
        if (group_count == 0) {
          SPDLOG_WARN("No metric group found for specified device");
        }
      } else {
        SPDLOG_WARN("Failed to discover metric group count");
      }
    } else {
      /* Step 2: populate metrics_groups buffer */
      group_count = *metrics_group_count;
      result =
          MetricsCollectorInstance().GetMetricGroups(device_handle, metrics_groups, &group_count);
      if (result != PTI_SUCCESS) {
        SPDLOG_WARN("Failed to discover metric groups for specified device");
      }
    }

    return result;
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Get properties for all metrics in a metric group.
 * Usage: 1- Get available metric groups on a specified device using ptiMetricsGetMetricGroups
 *        2- In the metric group properties structure of the metric group of interest, allocate
 * buffer _metric_properties of size sizeof(pti_metric_properties_t) * _metric_count
 *        3- call ptiMetricsGetMetricsProperties(metrics_group_handle, _metric_properties) to get
 * the metric properties written to the supplied buffer WARNING: It is expected that
 * _metric_properties buffer is sufficiently large to hold properties for all metrics in the
 * specified metric group. Passing in a buffer that is not suffitiently large will result in a
 * buffer overrun
 *
 * @param[in] metrics_group_handle            Metric group handle
 * @param[in/out] metrics                Buffer where to save metric properties for the
 *                                           specified metric group
 *
 * @return pti_result
 */
pti_result ptiMetricsGetMetricsProperties(pti_metrics_group_handle_t metrics_group_handle,
                                          pti_metric_properties_t* metrics) {
  try {
    if (metrics_group_handle == nullptr || metrics == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    pti_result result = PTI_SUCCESS;

    /* Populate metrics buffer */
    result = MetricsCollectorInstance().GetMetrics(metrics_group_handle, metrics);
    if (result != PTI_SUCCESS) {
      SPDLOG_WARN("Failed to discover metrics for specified metric group");
    }

    return result;
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Configure metric groups of interest.
 * Note: only 1 metric group of type PTI_METRIC_GROUP_TYPE_TIME_BASED can be specified at this time.
 * TODO: add support for multiple metric groups and different types
 *
 * @param[in] device_handle                  Device handle
 * @param[in] metric_config_params       Buffer of input parameters structures. Note: only 1 is
 * supported at this time
 * @param[in] metrics_group_count     Number of configuration structures in the configuration
 * buffer
 *
 * @return pti_result
 */
pti_result PTI_EXPORT ptiMetricsConfigureCollection(
    pti_device_handle_t device_handle, pti_metrics_group_collection_params_t* metric_config_params,
    uint32_t metrics_group_count) {
  try {
    if (device_handle == nullptr || metric_config_params == nullptr ||
        metric_config_params->_group_handle == nullptr || metrics_group_count == 0) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    if (metrics_group_count > 1) {
      SPDLOG_ERROR(
          "pti metric collector currently supports the collection of "
          "only one metric group at a time");
      return PTI_ERROR_NOT_IMPLEMENTED;
    }

    return MetricsCollectorInstance().ConfigureMetricGroups(device_handle, metric_config_params,
                                                            metrics_group_count);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Start metrics collection on specified device
 * Note: ptiMetricsConfigureCollection must be called first to configure the metric group(s) of
 * interest
 *
 * @param[in] device_handle                        Device handle
 *
 * @return pti_result
 */
pti_result ptiMetricsStartCollection(pti_device_handle_t device_handle) {
  try {
    return MetricsCollectorInstance().StartCollection(device_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Start metrics collection on specified device in paused mode
 * Note: ptiMetricsConfigureCollection must be called first to configure the metric group(s) of
 * interest
 *
 * @param[in] device_handle                        Device handle
 *
 * @return pti_result
 */
pti_result ptiMetricsStartCollectionPaused(pti_device_handle_t device_handle) {
  try {
    return MetricsCollectorInstance().StartCollectionPaused(device_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Pause metrics collection on specified device
 * Note: Collection  must be started first
 *
 * @param[in] device_handle                        Device handle
 *
 * @return pti_result
 */
pti_result ptiMetricsPauseCollection(pti_device_handle_t device_handle) {
  try {
    return MetricsCollectorInstance().PauseCollection(device_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Resume metrics collection on specified device
 * Note: Collection must be started and paused
 *
 * @param[in] device_handle                        Device handle
 *
 * @return pti_result
 */
pti_result ptiMetricsResumeCollection(pti_device_handle_t device_handle) {
  try {
    return MetricsCollectorInstance().ResumeCollection(device_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief Stop metrics collection on specified device
 * Note: ptiMetricsStartCollection must be called first to Start the collection
 * This function terminates the collection but does not process the data
 * @param[in] device_handle                        Device handle
 *
 * @return pti_result
 */
pti_result ptiMetricsStopCollection(pti_device_handle_t device_handle) {
  try {
    return MetricsCollectorInstance().StopCollection(device_handle);
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}

/**
 * @brief process and dump collected data on specified device
 * Note: ptiMetricsStopCollection must be called first to process collected data
 * ptiMetricGetCalculatedData can only be called once after the collection is stopped and cannot be
called between pa use and resume
 *
 * usage: 1- Call ptiMetricGetCalculatedData(device_handle, metrics_group_handle, NULL,
metrics_values_count) to discover the required buffer size for
 *           data collected for the specified metric group on on the specified device ;
 *           the required buffer size will be written to metrics_values_count in multiples of
pti_value_t.
 *        2- Allocate metrics_values_buffer for holding  metrics_values_count values
 *        3- Call ptiMetricGetCalculatedData(device_handle, metrics_group_handle,
metrics_values_buffer, metrics_values_count) to get the values written to buffer
 * A sample contains a 64bit value container for each metric in the metric group.
 * based on the metric's value type, the 64bit value container should be converted appropriately.
 *
 * @param[in] device_handle                 Device handle
 * @param[in] metrics_group_handle          Metric Group handle
 * @param[in/out] metrics_values_buffer     Buffer where to save collected samples
 *                                          Set to null to discover the required buffer size
 * @param[in/out] metrics_values_count      If metrics_values_buffer is set to null,
metrics_values_count will contain the number of pti_value_t
 *                                          values the buffer needs to be able to hold
 * @return pti_result
 */
pti_result ptiMetricGetCalculatedData(pti_device_handle_t device_handle,
                                      pti_metrics_group_handle_t metrics_group_handle,
                                      pti_value_t* metrics_values_buffer,
                                      uint32_t* metrics_values_count) {
  try {
    // metrics_values_count cannot be null
    if (metrics_values_count == nullptr || device_handle == nullptr ||
        metrics_group_handle == nullptr) {
      return PTI_ERROR_BAD_ARGUMENT;
    }

    uint32_t values_count = 0;
    pti_result result = PTI_SUCCESS;

    /* Step 1: discover the required buffer size */
    if (metrics_values_buffer == nullptr) {
      result = MetricsCollectorInstance().GetCalculatedData(device_handle, metrics_group_handle,
                                                            nullptr, &values_count);
      if (result == PTI_SUCCESS) {
        *metrics_values_count = values_count;
        if (values_count == 0) {
          SPDLOG_WARN("No samples found");
        }
      } else {
        SPDLOG_WARN("Failed to determine required buffer size");
      }
    } else {
      /* Step 2: populate buffer */
      values_count = *metrics_values_count;
      result = MetricsCollectorInstance().GetCalculatedData(device_handle, metrics_group_handle,
                                                            metrics_values_buffer, &values_count);
      if (result != PTI_SUCCESS) {
        SPDLOG_WARN("Failed to save values in buffer");
      }
    }
    return result;
  } catch (const std::exception& e) {
    LogException(e);
    return pti_result::PTI_ERROR_INTERNAL;
  } catch (...) {
    return pti_result::PTI_ERROR_INTERNAL;
  }
}
