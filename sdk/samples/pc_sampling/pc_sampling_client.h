//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef PC_SAMPLING_H_
#define PC_SAMPLING_H_

#include <cstddef>
#include <cstdint>

#include "pti/pti.h"

// Initialize PTI view subsystem required for profiling.
// Must be called before EnablePcSampling().
// Note: This is a temporary requirement which will be removed in future when
//  PC sampling can be enabled without enabling kernel view from the app.
bool InitializeProfilingEnvironment();

// Enable PC sampling and create a collection handle.
// This corresponds to ptiPcSamplingEnable().
// Must be called before ConfigurePcSampling().
bool EnablePcSampling();

// Configure PC sampling with device selection and sampling period.
// Parameters:
//   devices: Array of device handles to profile, or nullptr to profile all available devices.
//   device_count: Number of devices in the array, or 0 when devices is nullptr.
//   sampling_period_ns: Sampling period in nanoseconds (e.g., 50000 for 50µs).
// This corresponds to ptiPcSamplingConfigure().
// Must be called after EnablePcSampling() and before StartPcSamplingCollection().
bool ConfigurePcSampling(const pti_device_handle_t* devices, size_t device_count,
                         uint32_t sampling_period_ns);

// Start PC sampling collection.
// This corresponds to ptiPcSamplingStartCollection().
// Must be called after ConfigurePcSampling().
bool StartPcSamplingCollection();

// Stop PC sampling collection.
// This corresponds to ptiPcSamplingStopCollection().
// Must be called after StartPcSamplingCollection() and before DisablePcSampling().
bool StopPcSamplingCollection();

// Disable PC sampling and free resources.
// This corresponds to ptiPcSamplingDisable().
// Must be called after StopPcSamplingCollection().
bool DisablePcSampling();

// Retrieve and print PC sampling data collected during the last sampling session.
// Must be called after StopPcSamplingCollection() and before DisablePcSampling().
// Prints stall reason descriptions, per-device collection statistics, per-kernel
// aggregated stall counts, and per-instruction stall breakdowns.
bool PrintPcSamplingData();

// Cleanup PTI view subsystem.
// Should be called at the end of the program.
void CleanupProfilingEnvironment();

#endif  // PC_SAMPLING_H_
