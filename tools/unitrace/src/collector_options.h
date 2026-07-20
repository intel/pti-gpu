//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_
#define PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_

struct CollectorOptions {
  union {
    struct {
      // Core collector flags
      unsigned int device_timing : 1;
      unsigned int device_timeline : 1;
      unsigned int kernel_submission : 1;
      unsigned int host_timing : 1;
      unsigned int kernel_tracing : 1;
      unsigned int api_tracing : 1;
      unsigned int call_logging : 1;
      unsigned int need_tid : 1;
      unsigned int need_pid : 1;
      unsigned int verbose : 1;
      unsigned int demangle : 1;
      unsigned int kernels_per_tile : 1;
      unsigned int metric_query : 1;
      unsigned int metric_stream : 1;
      unsigned int stall_sampling : 1;
      unsigned int opencl : 1;
      unsigned int conditional_collection : 1;

      // Chrome logging flags
      unsigned int chrome_call_logging : 1;
      unsigned int chrome_kernel_logging : 1;
      unsigned int chrome_device_logging : 1;
      unsigned int chrome_syclrt_logging : 1;
      unsigned int chrome_ur_logging : 1;
      unsigned int chrome_itt_logging : 1;
      unsigned int chrome_no_thread_on_device : 1;
      unsigned int chrome_no_engine_on_device : 1;
      unsigned int chrome_mpi_logging : 1;
      unsigned int chrome_omp_logging : 1;

      // Other flags
      unsigned int log_to_file : 1;
      unsigned int output_dir_path : 1;
      unsigned int ccl_summary_report : 1;
    };
    uint64_t flags_ = 0;  // Default member initializer zeros all bitfields
  };

  void DeriveFlags() {
    // If no flags are set, default to host and device timing
    if (flags_ == 0) {
      host_timing = 1;
      device_timing = 1;
    }

    // Set kernel_tracing if any related features are enabled
    if (device_timing || device_timeline || kernel_submission ||
        chrome_device_logging || chrome_kernel_logging) {
      kernel_tracing = 1;
    }

    // Set api_tracing if any related features are enabled
    if (call_logging || chrome_call_logging || host_timing) {
      api_tracing = 1;
    }
  }
};

#endif //PTI_TOOLS_UNITRACE_COLLECTOR_OPTIONS_
