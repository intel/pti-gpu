//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

/**
 * @file pc_sampling.cc
 * @brief PC Sampling API implementation for metric collection.
 *
 * This file contains the implementation of PC sampling lifecycle management:
 *   - Environment initialization (PTI view setup)
 *   - PC sampling enable/configure/start/stop/disable workflow
 *   - Buffer management for PC sampling data collection
 *   - Environment cleanup
 */

#include "pc_sampling_client.h"

#include <cstdlib>
#include <iostream>
#include <sycl/sycl.hpp>

#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"
#include "samples_utils.h"

namespace {

// PC sampling collection handle
pti_pc_sampling_handle_t g_pc_sampling_handle = nullptr;

// Buffer size for PTI view tracing
constexpr auto kRequestedBufferSize = 1024 * 1024;  // 1 MB buffer

// Buffer callback to provide memory for PTI tracing
void ProvideBuffer(unsigned char** buf, std::size_t* buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing" << std::endl;
    std::abort();
  }
  *buf_size = kRequestedBufferSize;
}

// Buffer callback to parse/deallocate PTI tracing data
void ParseBuffer(unsigned char* buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << std::endl;
    if (buf) {
      samples_utils::AlignedDealloc(buf);
    }
    return;
  }
  // Buffer parsing can be implemented here if needed.
  // For this sample, we just deallocate the buffer.
  samples_utils::AlignedDealloc(buf);
}

}  // namespace

bool InitializeProfilingEnvironment() {
  std::cout << "=== Initializing Profiling Environment ===" << std::endl;

  // Set up buffer callbacks for PTI view
  pti_result status = ptiViewSetCallbacks(ProvideBuffer, ParseBuffer);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to set view callbacks: " << ptiResultTypeToString(status) << std::endl;
    return false;
  }

  // Enable kernel view - required before PC sampling
  status = ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to enable kernel view: " << ptiResultTypeToString(status) << std::endl;
    return false;
  }

  std::cout << "Kernel view enabled successfully" << std::endl;
  return true;
}

bool EnablePcSampling() {
  std::cout << "\n=== Enabling PC Sampling ===" << std::endl;

  pti_result status = ptiPcSamplingEnable(&g_pc_sampling_handle);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to enable PC sampling: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return false;
  }

  std::cout << "PC Sampling enabled successfully" << std::endl;
  return true;
}

bool ConfigurePcSampling(const pti_device_handle_t* devices, size_t device_count,
                         uint32_t sampling_period_ns) {
  std::cout << "\n=== Configuring PC Sampling ===" << std::endl;

  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Call EnablePcSampling() first." << std::endl;
    return false;
  }

  // Configure PC sampling with specified devices and sampling period.
  // Pass devices=nullptr, device_count=0 to profile all available devices.
  // User can pass specific device handles to profile a subset of devices.
  pti_result status =
      ptiPcSamplingConfigure(g_pc_sampling_handle, devices, device_count, sampling_period_ns);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to configure PC sampling: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return false;
  }

  if (devices == nullptr) {
    std::cout << "PC Sampling configured for all available devices (sampling period: "
              << sampling_period_ns << " ns)" << std::endl;
  } else {
    std::cout << "PC Sampling configured for " << device_count
              << " device(s) (sampling period: " << sampling_period_ns << " ns)" << std::endl;
  }

  return true;
}

bool StartPcSamplingCollection() {
  std::cout << "\n=== Starting PC Sampling Collection ===" << std::endl;

  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Call EnablePcSampling() first." << std::endl;
    return false;
  }

  pti_result status = ptiPcSamplingStartCollection(g_pc_sampling_handle);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to start PC sampling collection: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return false;
  }

  std::cout << "PC Sampling collection started successfully" << std::endl;
  return true;
}

bool StopPcSamplingCollection() {
  std::cout << "\n=== Stopping PC Sampling Collection ===" << std::endl;

  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Nothing to stop." << std::endl;
    return false;
  }

  pti_result status = ptiPcSamplingStopCollection(g_pc_sampling_handle);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to stop PC sampling collection: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return false;
  }

  std::cout << "PC Sampling collection stopped successfully" << std::endl;
  return true;
}

bool DisablePcSampling() {
  std::cout << "\n=== Disabling PC Sampling ===" << std::endl;

  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Nothing to disable." << std::endl;
    return false;
  }

  pti_result status = ptiPcSamplingDisable(g_pc_sampling_handle);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to disable PC sampling: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return false;
  }

  std::cout << "PC Sampling disabled and resources freed successfully" << std::endl;
  g_pc_sampling_handle = nullptr;
  return true;
}

void CleanupProfilingEnvironment() {
  std::cout << "\n=== Cleaning Up Profiling Environment ===" << std::endl;

  // Disable kernel view
  pti_result status = ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to disable kernel view: " << ptiResultTypeToString(status) << std::endl;
  } else {
    std::cout << "Kernel view disabled successfully" << std::endl;
  }
  status = ptiFlushAllViews();
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to flush PTI views: " << ptiResultTypeToString(status) << std::endl;
  } else {
    std::cout << "PTI views flushed successfully" << std::endl;
  }
}
