//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// Level Zero Graph Extension API Test
// Exercises Graph APIs to verify unitrace tracing functionality.
// Uses lightweight compute-benchmarks kernel to minimize GPU simulation overhead.

#include <level_zero/driver_experimental/zex_graph.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "ze_utils.h"

// Test configuration constants
constexpr int KERNEL_OPERATIONS_COUNT = 10;
constexpr int GRAPH_EXECUTION_COUNT = 3;
constexpr int MIN_CLI_ARGS = 2;  // program name + optional graph exec count
constexpr int KERNEL_GROUP_SIZE = 1;
constexpr int KERNEL_ARG_INDEX = 0;

// Graph extension function pointer typedefs
typedef ze_result_t(ZE_APICALL* pfn_zeGraphCreateExp)(ze_context_handle_t, ze_graph_handle_t*,
                                                      void*);
typedef ze_result_t(ZE_APICALL* pfn_zeGraphDestroyExp)(ze_graph_handle_t);
typedef ze_result_t(ZE_APICALL* pfn_zeExecutableGraphDestroyExp)(ze_executable_graph_handle_t);
typedef ze_result_t(ZE_APICALL* pfn_zeCommandListBeginGraphCaptureExp)(ze_command_list_handle_t,
                                                                       void*);
typedef ze_result_t(ZE_APICALL* pfn_zeCommandListBeginCaptureIntoGraphExp)(ze_command_list_handle_t,
                                                                           ze_graph_handle_t,
                                                                           void*);
typedef ze_result_t(ZE_APICALL* pfn_zeCommandListEndGraphCaptureExp)(ze_command_list_handle_t,
                                                                     ze_graph_handle_t*, void*);
typedef ze_result_t(ZE_APICALL* pfn_zeCommandListInstantiateGraphExp)(ze_graph_handle_t,
                                                                      ze_executable_graph_handle_t*,
                                                                      void*);
typedef ze_result_t(ZE_APICALL* pfn_zeCommandListAppendGraphExp)(ze_command_list_handle_t,
                                                                 ze_executable_graph_handle_t,
                                                                 void*, ze_event_handle_t, uint32_t,
                                                                 ze_event_handle_t*);

// Global function pointers for graph extension
static pfn_zeGraphCreateExp pzeGraphCreateExp = nullptr;
static pfn_zeGraphDestroyExp pzeGraphDestroyExp = nullptr;
static pfn_zeExecutableGraphDestroyExp pzeExecutableGraphDestroyExp = nullptr;
static pfn_zeCommandListBeginGraphCaptureExp pzeCommandListBeginGraphCaptureExp = nullptr;
static pfn_zeCommandListBeginCaptureIntoGraphExp pzeCommandListBeginCaptureIntoGraphExp = nullptr;
static pfn_zeCommandListEndGraphCaptureExp pzeCommandListEndGraphCaptureExp = nullptr;
static pfn_zeCommandListInstantiateGraphExp pzeCommandListInstantiateGraphExp = nullptr;
static pfn_zeCommandListAppendGraphExp pzeCommandListAppendGraphExp = nullptr;

#define ZE_CHECK(call)                                                               \
  do {                                                                               \
    ze_result_t _res = (call);                                                       \
    if (_res != ZE_RESULT_SUCCESS) {                                                 \
      std::cerr << "ERROR: " << #call << " failed with error " << _res << std::endl; \
      return false;                                                                  \
    }                                                                                \
  } while (0)

#define LOAD_GRAPH_EXTENSION_FUNCTION(driver, func_name) \
  ZE_CHECK(zeDriverGetExtensionFunctionAddress(driver, #func_name, (void**)(&p##func_name)))

/// Loads graph extension function pointers from the Level Zero driver.
/// Returns true if all required functions are available, false otherwise.
static bool LoadGraphExtension(ze_driver_handle_t driver) {
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeGraphCreateExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeGraphDestroyExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeExecutableGraphDestroyExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListBeginGraphCaptureExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListBeginCaptureIntoGraphExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListEndGraphCaptureExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListInstantiateGraphExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListAppendGraphExp);

  std::cout << "Graph extension loaded successfully" << std::endl;
  return true;
}

/// Captures a kernel into a graph, instantiates it, and executes it multiple times.
static bool RunGraph(ze_kernel_handle_t kernel, ze_context_handle_t context,
                     ze_command_list_handle_t cmd_list, int graph_exec_count) {
  ze_graph_handle_t graph = nullptr;
  ze_executable_graph_handle_t exec_graph = nullptr;

  // Create empty graph
  ZE_CHECK(pzeGraphCreateExp(context, &graph, nullptr));

  // Begin capturing commands into the graph
  ZE_CHECK(pzeCommandListBeginCaptureIntoGraphExp(cmd_list, graph, nullptr));

  // Launch kernel (will be captured into graph)
  ze_group_count_t dim = {KERNEL_GROUP_SIZE, KERNEL_GROUP_SIZE, KERNEL_GROUP_SIZE};
  ZE_CHECK(zeCommandListAppendLaunchKernel(cmd_list, kernel, &dim, nullptr, 0, nullptr));

  // End graph capture and finalize the graph
  ZE_CHECK(pzeCommandListEndGraphCaptureExp(cmd_list, &graph, nullptr));

  // Instantiate the graph
  ZE_CHECK(pzeCommandListInstantiateGraphExp(graph, &exec_graph, nullptr));

  // Execute the instantiated graph multiple times
  for (int i = 0; i < graph_exec_count; ++i) {
    ZE_CHECK(pzeCommandListAppendGraphExp(cmd_list, exec_graph, nullptr, nullptr, 0, nullptr));
  }

  // Synchronize execution
  ZE_CHECK(zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr));
  ZE_CHECK(zeCommandListHostSynchronize(cmd_list, UINT64_MAX));

  // Cleanup resources
  pzeExecutableGraphDestroyExp(exec_graph);
  pzeGraphDestroyExp(graph);

  return true;
}

/// Initializes Level Zero resources, loads the lightweight kernel, and runs graph executions.
static bool RunGraphTest(ze_device_handle_t device, ze_driver_handle_t driver, int graph_exec_count) {
  // Load the lightweight "eat_time" kernel from compute-benchmarks
  const std::string kernel_module = "api_overhead_benchmark_eat_time.spv";
  std::string kernel_path = utils::GetExecutablePath();
  // Ensure path separator if GetExecutablePath() doesn't end with /
  if (!kernel_path.empty() && kernel_path.back() != '/') {
    kernel_path += '/';
  }
  kernel_path += kernel_module;

  std::vector<uint8_t> binary = utils::LoadBinaryFile(kernel_path);
  if (binary.empty()) {
    std::cerr << "ERROR: Unable to load kernel module: " << kernel_path << std::endl;
    return false;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);
  if (context == nullptr) {
    std::cerr << "ERROR: Failed to get Level Zero context" << std::endl;
    return false;
  }

  // Create module from SPIR-V binary
  ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                  nullptr,
                                  ZE_MODULE_FORMAT_IL_SPIRV,
                                  static_cast<uint32_t>(binary.size()),
                                  binary.data(),
                                  nullptr,
                                  nullptr};
  ze_module_handle_t module = nullptr;
  ZE_CHECK(zeModuleCreate(context, device, &module_desc, &module, nullptr));

  // Create kernel (eat_time kernel has signature: kernel void eat_time(int operationsCount))
  ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "eat_time"};
  ze_kernel_handle_t kernel = nullptr;
  ZE_CHECK(zeKernelCreate(module, &kernel_desc, &kernel));

  // Set kernel group size
  ZE_CHECK(zeKernelSetGroupSize(kernel, KERNEL_GROUP_SIZE, KERNEL_GROUP_SIZE, KERNEL_GROUP_SIZE));

  // Set kernel argument
  ZE_CHECK(
      zeKernelSetArgumentValue(kernel, KERNEL_ARG_INDEX, sizeof(int), &KERNEL_OPERATIONS_COUNT));

  // Create immediate command list for submitting commands to the device
  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
  ze_command_list_handle_t cmd_list = nullptr;
  ZE_CHECK(zeCommandListCreateImmediate(context, device, &cmd_queue_desc, &cmd_list));

  // Enable data collection for profiling
  utils::SetEnv("PTI_ENABLE_COLLECTION", "1");

  if (RunGraph(kernel, context, cmd_list, graph_exec_count) == false) {
    std::cerr << "ERROR: Graph execution failed" << std::endl;
    return false;
  }

  std::cout << "\n=== Completed "<< graph_exec_count
            << " graph executions with "
            << KERNEL_OPERATIONS_COUNT << " kernel operations each ===\n"
            << std::endl;

  // Disable data collection
  utils::SetEnv("PTI_ENABLE_COLLECTION", "");

  // Cleanup resources in reverse order of creation
  zeCommandListDestroy(cmd_list);
  zeKernelDestroy(kernel);
  zeModuleDestroy(module);
  zeContextDestroy(context);

  return true;
}

int main(int argc, char* argv[]) {
  // Initialize Level Zero
  if (zeInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) {
    std::cerr << "ERROR: Failed to initialize Level Zero" << std::endl;
    return 1;
  }

  // Get GPU device and driver
  ze_device_handle_t device = utils::ze::GetGpuDevice();
  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  if (device == nullptr || driver == nullptr) {
    std::cerr << "ERROR: Unable to find GPU device" << std::endl;
    return 1;
  }

  // Load graph extension
  if (!LoadGraphExtension(driver)) {
    std::cerr << "ERROR: Graph extension not available on this device/driver" << std::endl;
    return 1;
  }

  // Parse command line arguments
  int graph_exec_count = GRAPH_EXECUTION_COUNT;
  if (argc >= MIN_CLI_ARGS) {
    try {
      graph_exec_count = std::stoi(argv[1]);
      if (graph_exec_count <= 0) {
        std::cerr << "ERROR: Graph execution count must be positive (got " << graph_exec_count << ")"
                  << std::endl;
        return 1;
      }
    } catch (const std::exception& e) {
      std::cerr << "ERROR: Invalid graph execution count: " << argv[1] << std::endl;
      return 1;
    }
  }

  // Display test configuration
  std::cout << "Level Zero Graph API Test" << std::endl;
  std::cout << "Target device: " << utils::ze::GetDeviceName(device) << std::endl;
  std::cout << "Graph executions: " << graph_exec_count << std::endl;

  // Run the test and measure execution time
  auto start = std::chrono::steady_clock::now();
  bool success = RunGraphTest(device, driver, graph_exec_count);
  auto end = std::chrono::steady_clock::now();
  if (!success) {
    std::cerr << "ERROR: Graph test failed" << std::endl;
    return 1;
  }
  std::chrono::duration<float> elapsed = end - start;

  std::cout << "\nTotal execution time: " << elapsed.count() << " seconds" << std::endl;

  return 0;
}
