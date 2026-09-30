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
 *   - PC sampling enable/configure/start/stop/retrieve/print/disable workflow
 *   - Buffer management for PC sampling data collection
 *   - Environment cleanup
 */

#include "pc_sampling_client.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"
#include "samples_utils.h"

namespace {

// PC sampling collection handle
pti_pc_sampling_handle_t g_pc_sampling_handle = nullptr;

// Buffer size for PTI view tracing
constexpr auto kRequestedBufferSize = 1024 * 1024;  // 1 MB buffer

size_t CountDecimalDigits(uint64_t value) {
  size_t digits = 1;
  while (value >= 10) {
    value /= 10;
    ++digits;
  }
  return digits;
}

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

bool InitializeProfilingEnvironment(std::ostream& output) {
  output << "=== Initializing Profiling Environment ===" << std::endl;

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

  output << "Kernel view enabled successfully" << std::endl;
  return true;
}

pti_result EnablePcSampling(std::ostream& output) {
  output << "\n=== Enabling PC Sampling ===" << std::endl;

  pti_result status = ptiPcSamplingEnable(&g_pc_sampling_handle);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to enable PC sampling: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return status;
  }

  output << "PC Sampling enabled successfully" << std::endl;
  return PTI_SUCCESS;
}

pti_result ConfigurePcSampling(const pti_device_handle_t* devices, size_t device_count,
                               uint32_t sampling_period_ns, std::ostream& output) {
  output << "\n=== Configuring PC Sampling ===" << std::endl;

  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Call EnablePcSampling() first." << std::endl;
    return PTI_ERROR_BAD_ARGUMENT;
  }

  // Configure PC sampling with specified devices and sampling period.
  // Pass devices=nullptr, device_count=0 to profile all available devices.
  // User can pass specific device handles to profile a subset of devices.
  pti_result status =
      ptiPcSamplingConfigure(g_pc_sampling_handle, devices, device_count, sampling_period_ns);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to configure PC sampling: " << ptiResultTypeToString(status) << " ("
              << static_cast<int>(status) << ")" << std::endl;
    return status;
  }

  if (devices == nullptr) {
    output << "PC Sampling configured for all available devices (sampling period: "
           << sampling_period_ns << " ns)" << std::endl;
  } else {
    output << "PC Sampling configured for " << device_count
           << " device(s) (sampling period: " << sampling_period_ns << " ns)" << std::endl;
  }

  return PTI_SUCCESS;
}

bool StartPcSamplingCollection(std::ostream& output) {
  output << "\n=== Starting PC Sampling Collection ===" << std::endl;

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

  output << "PC Sampling collection started successfully" << std::endl;
  return true;
}

bool StopPcSamplingCollection(std::ostream& output) {
  output << "\n=== Stopping PC Sampling Collection ===" << std::endl;

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

  output << "PC Sampling collection stopped successfully" << std::endl;
  return true;
}

bool DisablePcSampling(std::ostream& output) {
  output << "\n=== Disabling PC Sampling ===" << std::endl;

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

  output << "PC Sampling disabled and resources freed successfully" << std::endl;
  g_pc_sampling_handle = nullptr;
  return true;
}

bool HasObservedPcSamplingKernels() {
  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Cannot query observed kernels." << std::endl;
    return false;
  }

  size_t device_count = 0;
  pti_result status = ptiPcSamplingGetProfiledDevices(g_pc_sampling_handle, nullptr, &device_count);
  if (status != PTI_SUCCESS || device_count == 0) {
    return false;
  }

  std::vector<pti_device_handle_t> devices(device_count);
  status = ptiPcSamplingGetProfiledDevices(g_pc_sampling_handle, devices.data(), &device_count);
  if (status != PTI_SUCCESS) {
    return false;
  }

  for (pti_device_handle_t device : devices) {
    size_t kernel_count = 0;
    status =
        ptiPcSamplingGetObservedKernelHandles(g_pc_sampling_handle, device, nullptr, &kernel_count);
    if (status != PTI_SUCCESS) {
      return false;
    }
    if (kernel_count != 0) {
      return true;
    }
  }

  return false;
}

bool PrintPcSamplingData(std::ostream& output) {
  if (!g_pc_sampling_handle) {
    std::cerr << "PC sampling handle is null. Cannot print stall data." << std::endl;
    return false;
  }

  output << "\n=== PC Sampling Stall Data ===" << std::endl;

  // Print Stall Reasons types and descriptions
  size_t reason_count = 0;
  pti_result status = ptiPcSamplingGetStallReasons(g_pc_sampling_handle, nullptr, &reason_count);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to query stall reason count: " << ptiResultTypeToString(status)
              << std::endl;
    return false;
  }

  std::vector<pti_pc_sampling_stall_reason_info_t> reasons(reason_count);
  for (auto& r : reasons) {
    r._struct_size = sizeof(pti_pc_sampling_stall_reason_info_t);
  }
  status = ptiPcSamplingGetStallReasons(g_pc_sampling_handle, reasons.data(), &reason_count);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to retrieve stall reasons: " << ptiResultTypeToString(status) << std::endl;
    return false;
  }

  output << "Stall reasons (" << reason_count << "):" << std::endl;
  for (size_t i = 0; i < reason_count; ++i) {
    const char* name = reasons[i]._name ? reasons[i]._name : "<null>";
    const char* desc = reasons[i]._description ? reasons[i]._description : "<null>";
    output << "  [" << i << "] " << name << ": " << desc << std::endl;
  }

  // Print profiled devices
  size_t device_count = 0;
  status = ptiPcSamplingGetProfiledDevices(g_pc_sampling_handle, nullptr, &device_count);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to query profiled device count: " << ptiResultTypeToString(status)
              << std::endl;
    return false;
  }

  if (device_count == 0) {
    output << "No profiled devices found." << std::endl;
    return true;
  }

  std::vector<pti_device_handle_t> devices(device_count);
  status = ptiPcSamplingGetProfiledDevices(g_pc_sampling_handle, devices.data(), &device_count);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to retrieve profiled devices: " << ptiResultTypeToString(status)
              << std::endl;
    return false;
  }

  // Print device specific data
  for (size_t d = 0; d < device_count; ++d) {
    output << "\n--- Device " << (d + 1) << "/" << device_count << " ---" << std::endl;

    // Print device status
    pti_pc_sampling_device_status_t device_status{};
    device_status._struct_size = sizeof(device_status);
    status = ptiPcSamplingGetDeviceStatus(g_pc_sampling_handle, devices[d], &device_status);
    if (status == PTI_SUCCESS) {
      output << "  Total samples collected : " << device_status._total_sample_count << std::endl;
      output << "  Total PCs sampled       : " << device_status._total_pc_count << std::endl;
      output << "  Samples dropped         : " << (device_status._samples_dropped ? "Yes" : "No")
             << std::endl;
    } else {
      std::cerr << "  Warning: Failed to get device status: " << ptiResultTypeToString(status)
                << std::endl;
    }

    // Print kernel amount
    size_t kernel_count = 0;
    status = ptiPcSamplingGetObservedKernelHandles(g_pc_sampling_handle, devices[d], nullptr,
                                                   &kernel_count);
    if (status != PTI_SUCCESS) {
      std::cerr << "  Failed to query kernel handle count: " << ptiResultTypeToString(status)
                << std::endl;
      continue;
    }

    if (kernel_count == 0) {
      output << "  No kernels with samples found for this device." << std::endl;
      continue;
    }

    std::vector<uint64_t> kernel_handles(kernel_count);
    status = ptiPcSamplingGetObservedKernelHandles(g_pc_sampling_handle, devices[d],
                                                   kernel_handles.data(), &kernel_count);
    if (status != PTI_SUCCESS) {
      std::cerr << "  Failed to retrieve kernel handles: " << ptiResultTypeToString(status)
                << std::endl;
      continue;
    }

    output << "  Kernels with samples: " << kernel_count << std::endl;

    // Print per kernel data
    for (size_t k = 0; k < kernel_count; ++k) {
      // Print kernel info
      pti_pc_sampling_kernel_info_t kernel_info{};
      kernel_info._struct_size = sizeof(kernel_info);
      status = ptiPcSamplingGetObservedKernelInfo(g_pc_sampling_handle, devices[d],
                                                  kernel_handles[k], &kernel_info);
      if (status != PTI_SUCCESS) {
        std::cerr << "  Failed to retrieve kernel info: " << ptiResultTypeToString(status)
                  << std::endl;
        continue;
      }

      const size_t kernel_reason_count = kernel_info._reason_count;
      std::vector<uint64_t> aggregated_samples(kernel_reason_count, 0);
      kernel_info._aggregated_samples = aggregated_samples.data();
      status = ptiPcSamplingGetObservedKernelInfo(g_pc_sampling_handle, devices[d],
                                                  kernel_handles[k], &kernel_info);
      if (status != PTI_SUCCESS) {
        std::cerr << "  Failed to retrieve aggregated kernel samples: "
                  << ptiResultTypeToString(status) << std::endl;
        continue;
      }

      const char* kernel_name = kernel_info._kernel_name ? kernel_info._kernel_name : "<unknown>";
      output << "\n  Kernel [" << (k + 1) << "/" << kernel_count << "]: " << kernel_name
             << std::endl;
      output << "    Instructions with samples: " << kernel_info._instructions_with_samples_count
             << std::endl;

      // Aggregated stall counts across all instructions in this kernel
      output << "    Aggregated stall samples:" << std::endl;
      bool any_nonzero = false;
      for (size_t r = 0; r < kernel_reason_count; ++r) {
        if (aggregated_samples[r] > 0) {
          const char* rname = r < reason_count && reasons[r]._name ? reasons[r]._name : "<unknown>";
          output << "      " << rname << ": " << aggregated_samples[r] << std::endl;
          any_nonzero = true;
        }
      }
      if (!any_nonzero) {
        output << "      (no stall samples recorded)" << std::endl;
      }

      const size_t instr_count = kernel_info._instructions_with_samples_count;
      if (instr_count == 0 || kernel_reason_count == 0) {
        continue;
      }

      // Print per-instruction stall breakdown
      std::vector<pti_pc_sampling_instruction_t> instructions(instr_count);
      std::vector<uint64_t> samples(instr_count * kernel_reason_count, 0);
      status = ptiPcSamplingGetSamplesPerInstruction(g_pc_sampling_handle, devices[d],
                                                     kernel_handles[k], instructions.data(),
                                                     instr_count, samples.data(), samples.size());
      if (status != PTI_SUCCESS) {
        std::cerr << "    Failed to retrieve per-instruction samples: "
                  << ptiResultTypeToString(status) << std::endl;
        continue;
      }

      // Pre-compute per-column width: max(header name length, max value digits)
      std::vector<size_t> col_widths(kernel_reason_count);
      for (size_t r = 0; r < kernel_reason_count; ++r) {
        const char* rname = r < reason_count && reasons[r]._name ? reasons[r]._name : "<unknown>";
        col_widths[r] = std::strlen(rname);
      }
      for (size_t i = 0; i < instr_count; ++i) {
        for (size_t r = 0; r < kernel_reason_count; ++r) {
          size_t val_width = CountDecimalDigits(samples[i * kernel_reason_count + r]);
          col_widths[r] = std::max(col_widths[r], val_width);
        }
      }

      output << "    Per-instruction stall samples:" << std::endl;

      // Header row
      output << "      " << std::left << std::setw(10) << "Offset";
      for (size_t r = 0; r < kernel_reason_count; ++r) {
        const char* rname = r < reason_count && reasons[r]._name ? reasons[r]._name : "<unknown>";
        output << " | " << std::left << std::setw(col_widths[r]) << rname;
      }
      output << std::endl;

      // Separator row
      output << "      " << std::string(10, '-');
      for (size_t r = 0; r < kernel_reason_count; ++r) {
        output << "-+-" << std::string(col_widths[r], '-');
      }
      output << std::endl;

      // Data rows
      for (size_t i = 0; i < instr_count; ++i) {
        output << "      0x" << std::right << std::hex << std::setw(8) << std::setfill('0')
               << instructions[i]._instruction_offset << std::setfill(' ') << std::dec;
        if (instructions[i]._source_info && instructions[i]._source_info->_file_path) {
          output << " [" << instructions[i]._source_info->_file_path << ":"
                 << instructions[i]._source_info->_file_line << "]";
        }
        for (size_t r = 0; r < kernel_reason_count; ++r) {
          output << " | " << std::right << std::setw(col_widths[r])
                 << samples[i * kernel_reason_count + r];
        }
        output << std::endl;
      }
    }
  }

  return true;
}

void CleanupProfilingEnvironment(std::ostream& output) {
  output << "\n=== Cleaning Up Profiling Environment ===" << std::endl;

  // Disable kernel view
  pti_result status = ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to disable kernel view: " << ptiResultTypeToString(status) << std::endl;
  } else {
    output << "Kernel view disabled successfully" << std::endl;
  }
  status = ptiFlushAllViews();
  if (status != PTI_SUCCESS) {
    std::cerr << "Failed to flush PTI views: " << ptiResultTypeToString(status) << std::endl;
  } else {
    output << "PTI views flushed successfully" << std::endl;
  }
}
