//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <CL/tracing_api.h>

#include "cl_utils.h"
#include "pti_assert.h"

// Pointers to tracing functions
static decltype(clCreateTracingHandleINTEL)*  clCreateTracingHandle  = nullptr;
static decltype(clSetTracingPointINTEL)*      clSetTracingPoint      = nullptr;
static decltype(clDestroyTracingHandleINTEL)* clDestroyTracingHandle = nullptr;
static decltype(clEnableTracingINTEL)*        clEnableTracing        = nullptr;
static decltype(clDisableTracingINTEL)*       clDisableTracing       = nullptr;

// Tracing handle
static cl_tracing_handle tracer = nullptr;

// Function maps & mutex
static std::map<std::string, uint64_t> function_time_map;
static std::map<std::string, uint64_t> function_count_map;
std::mutex lock;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_hot_functions[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {}

// Internal Tool Functionality ////////////////////////////////////////////////

static bool LoadTracingFunctions(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  cl_int status = CL_SUCCESS;

  cl_platform_id platform = nullptr;
  status = clGetDeviceInfo(
      device, CL_DEVICE_PLATFORM, sizeof(platform), &platform, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  clCreateTracingHandle =
    reinterpret_cast<decltype(clCreateTracingHandleINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
        platform, "clCreateTracingHandleINTEL"));
  clSetTracingPoint =
    reinterpret_cast<decltype(clSetTracingPointINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
        platform, "clSetTracingPointINTEL"));
  clDestroyTracingHandle =
    reinterpret_cast<decltype(clDestroyTracingHandleINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
        platform, "clDestroyTracingHandleINTEL"));
  clEnableTracing =
    reinterpret_cast<decltype(clEnableTracingINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
        platform, "clEnableTracingINTEL"));
  clDisableTracing =
    reinterpret_cast<decltype(clDisableTracingINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
        platform, "clDisableTracingINTEL"));

  if (clCreateTracingHandle == nullptr ||
      clSetTracingPoint == nullptr ||
      clDestroyTracingHandle == nullptr ||
      clEnableTracing == nullptr ||
      clDisableTracing == nullptr) {
    return false;
  }

  return true;
}

static void Callback(
    cl_function_id function,
    cl_callback_data* callback_data,
    void* user_data) {
  PTI_ASSERT(callback_data != nullptr);
  PTI_ASSERT(callback_data->correlationData != nullptr);

  // Get current time point
  std::chrono::duration<uint64_t, std::nano> time =
    std::chrono::steady_clock::now().time_since_epoch();

  if (callback_data->site == CL_CALLBACK_SITE_ENTER) { // Before the function
    uint64_t& start_time = *reinterpret_cast<uint64_t*>(
        callback_data->correlationData);
    start_time = time.count();
  } else { // After the function
    uint64_t end_time = time.count();
    uint64_t& start_time = *reinterpret_cast<uint64_t*>(
        callback_data->correlationData);

    {
      const std::lock_guard<std::mutex> guard(lock);

      if (function_time_map.count(callback_data->functionName) == 0) {
        function_time_map[callback_data->functionName] =
          end_time - start_time;
      } else {
        function_time_map[callback_data->functionName] +=
          end_time - start_time;
      }

      if (function_count_map.count(callback_data->functionName) == 0) {
        function_count_map[callback_data->functionName] = 1;
      } else {
        function_count_map[callback_data->functionName] += 1;
      }
    }
  }
}

static void PrintResults() {
  if (function_time_map.empty()) {
    return;
  }

  size_t function_length = 0;
  for (auto& item : function_time_map) {
    auto& name = item.first;
    if (name.size() > function_length) {
      function_length = name.size();
    }
  }
  PTI_ASSERT(function_length > 0);

  std::cerr << std::endl;
  std::cerr << std::setw(function_length) << "Function" << "," <<
      std::setw(12) << "Calls" << "," <<
      std::setw(20) << "Time (ns)" << "," <<
      std::setw(20) << "Average (ns)" << std::endl;

  for (auto& item : function_time_map) {
    auto& name = item.first;
    uint64_t time = item.second;
    PTI_ASSERT(function_count_map.count(name) == 1);
    uint64_t count = function_count_map[name];
    std::cerr << std::setw(function_length) << name << "," <<
      std::setw(12) << count << "," <<
      std::setw(20) << time << "," <<
      std::setw(20) << time / count << std::endl;
  }
  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  cl_int status = CL_SUCCESS;

  // Get GPU device
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cerr <<
      "[WARNING] Unable to find GPU device for tracing" << std::endl;
    return;
  }

  // Get pointers for tracing functions
  bool loaded = LoadTracingFunctions(device);
  if (!loaded) {
    std::cerr <<
      "[WARNING] Unable to load pointers for tracing functions" << std::endl;
    return;
  }

  // Create tracing handle
  status = clCreateTracingHandle(device, Callback, nullptr, &tracer);
  PTI_ASSERT(status == CL_SUCCESS);

  // Switch on tracing for all of the functions
  for (int fid = 0; fid < CL_FUNCTION_COUNT; ++fid) {
    status = clSetTracingPoint(
        tracer, static_cast<cl_function_id>(fid), CL_TRUE);
    PTI_ASSERT(status == CL_SUCCESS);
  }

  // Enable tracing
  status = clEnableTracing(tracer);
  PTI_ASSERT(status == CL_SUCCESS);
}

void DisableProfiling() {
  if (tracer == nullptr) {
    return;
  }

  cl_int status = CL_SUCCESS;

  // Disable tracing
  status = clDisableTracing(tracer);
  PTI_ASSERT(status == CL_SUCCESS);

  // Destroy tracing handle
  status = clDestroyTracingHandle(tracer);
  PTI_ASSERT(status == CL_SUCCESS);

  PrintResults();
}