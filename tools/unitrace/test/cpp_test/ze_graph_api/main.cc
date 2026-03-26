//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <level_zero/driver_experimental/zex_graph.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "ze_utils.h"

// Test configuration constants
constexpr int GRAPH_EXECUTION_COUNT = 3;
constexpr int MIN_CLI_ARGS = 2;  // program name + optional graph exec count
constexpr int DATA_SIZE = 64;    // number of int elements

// Graph extension function pointer typedefs
typedef ze_result_t(ZE_APICALL* pfn_zeGraphCreateExp)(ze_context_handle_t, ze_graph_handle_t*,
                                                      void*);
typedef ze_result_t(ZE_APICALL* pfn_zeGraphDestroyExp)(ze_graph_handle_t);
typedef ze_result_t(ZE_APICALL* pfn_zeExecutableGraphDestroyExp)(ze_executable_graph_handle_t);
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
static pfn_zeGraphCreateExp p_zeGraphCreateExp = nullptr;
static pfn_zeGraphDestroyExp p_zeGraphDestroyExp = nullptr;
static pfn_zeExecutableGraphDestroyExp p_zeExecutableGraphDestroyExp = nullptr;
static pfn_zeCommandListBeginCaptureIntoGraphExp p_zeCommandListBeginCaptureIntoGraphExp = nullptr;
static pfn_zeCommandListEndGraphCaptureExp p_zeCommandListEndGraphCaptureExp = nullptr;
static pfn_zeCommandListInstantiateGraphExp p_zeCommandListInstantiateGraphExp = nullptr;
static pfn_zeCommandListAppendGraphExp p_zeCommandListAppendGraphExp = nullptr;

#define ZE_CHECK(call)                                                               \
  do {                                                                               \
    ze_result_t _res = (call);                                                       \
    if (_res != ZE_RESULT_SUCCESS) {                                                 \
      std::cerr << "ERROR: " << #call << " failed with error 0x" << std::hex << _res \
                << std::dec << std::endl;                                            \
      return false;                                                                  \
    }                                                                                \
  } while (0)

#define LOAD_GRAPH_EXTENSION_FUNCTION(driver, func_name) \
  ZE_CHECK(zeDriverGetExtensionFunctionAddress(driver, #func_name, (void**)(&p_##func_name)))

/// Loads graph extension function pointers from the Level Zero driver.
static bool LoadGraphExtension(ze_driver_handle_t driver) {
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeGraphCreateExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeGraphDestroyExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeExecutableGraphDestroyExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListBeginCaptureIntoGraphExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListEndGraphCaptureExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListInstantiateGraphExp);
  LOAD_GRAPH_EXTENSION_FUNCTION(driver, zeCommandListAppendGraphExp);

  std::cout << "Graph extension loaded successfully" << std::endl;
  return true;
}

/// Captures a graph with sequential fork/join pattern using 3 command lists.
/// Primary: H2D -> add_one -[fork1]-> wait[join1] -[fork2]-> wait[join2] -> multiply_two -> D2H
/// Fork1:   wait[fork1] -> D2D(dev->staging) -> D2D(staging->dev) -[join1]
/// Fork2:   wait[fork2] -> add_one(dev_buf) -[join2]
/// Both forks join back to primary (no nesting per spec).
/// Result: (i + 1 + 1) * 2 = 2i + 4
static bool CaptureGraphWithForks(
    ze_kernel_handle_t kernel_add_one,
    ze_kernel_handle_t kernel_multiply_two,
    ze_kernel_handle_t kernel_add_one_fork,
    ze_context_handle_t context,
    ze_command_list_handle_t primary_cmdlist,
    ze_command_list_handle_t fork1_cmdlist,
    ze_command_list_handle_t fork2_cmdlist,
    void* dev_buf, void* host_input, void* staging_buf, void* host_output,
    size_t buf_size,
    ze_event_handle_t fork_event1, ze_event_handle_t join_event1,
    ze_event_handle_t fork_event2, ze_event_handle_t join_event2,
    uint32_t group_count_x,
    ze_graph_handle_t& out_graph,
    ze_executable_graph_handle_t& out_exec_graph) {

  ze_graph_handle_t graph = nullptr;
  ZE_CHECK(p_zeGraphCreateExp(context, &graph, nullptr));

  // Begin capture on primary cmdlist
  ZE_CHECK(p_zeCommandListBeginCaptureIntoGraphExp(primary_cmdlist, graph, nullptr));

  // PRIMARY: H2D copy
  ZE_CHECK(zeCommandListAppendMemoryCopy(primary_cmdlist, dev_buf, host_input, buf_size,
                                         nullptr, 0, nullptr));

  // PRIMARY: add_one on dev_buf, signal fork_event1 (first fork point)
  ze_group_count_t dim = {group_count_x, 1, 1};
  ZE_CHECK(zeCommandListAppendLaunchKernel(primary_cmdlist, kernel_add_one, &dim,
                                           fork_event1, 0, nullptr));

  // FORK1: wait fork_event1, D2D round-trip (dev->staging->dev), signal join_event1
  ZE_CHECK(zeCommandListAppendMemoryCopy(fork1_cmdlist, staging_buf, dev_buf, buf_size,
                                         nullptr, 1, &fork_event1));
  ZE_CHECK(zeCommandListAppendMemoryCopy(fork1_cmdlist, dev_buf, staging_buf, buf_size,
                                         join_event1, 0, nullptr));

  // PRIMARY: wait join_event1 (join fork1), signal fork_event2 (second fork point)
  ZE_CHECK(zeCommandListAppendWaitOnEvents(primary_cmdlist, 1, &join_event1));
  ZE_CHECK(zeCommandListAppendSignalEvent(primary_cmdlist, fork_event2));

  // FORK2: wait fork_event2, add_one on dev_buf, signal join_event2
  ZE_CHECK(zeCommandListAppendLaunchKernel(fork2_cmdlist, kernel_add_one_fork, &dim,
                                           join_event2, 1, &fork_event2));

  // PRIMARY: wait join_event2 (join fork2), multiply_two, D2H copy
  ZE_CHECK(zeCommandListAppendLaunchKernel(primary_cmdlist, kernel_multiply_two, &dim,
                                           nullptr, 1, &join_event2));
  ZE_CHECK(zeCommandListAppendMemoryCopy(primary_cmdlist, host_output, dev_buf, buf_size,
                                         nullptr, 0, nullptr));

  // End capture on primary
  ZE_CHECK(p_zeCommandListEndGraphCaptureExp(primary_cmdlist, &graph, nullptr));

  // Instantiate executable graph
  ze_executable_graph_handle_t exec_graph = nullptr;
  ZE_CHECK(p_zeCommandListInstantiateGraphExp(graph, &exec_graph, nullptr));

  out_graph = graph;
  out_exec_graph = exec_graph;
  return true;
}

/// Runs graph with fork/join pattern: 3 command lists (primary + 2 forks)
static bool RunGraphPipeline(ze_kernel_handle_t kernel_add_one,
                              ze_kernel_handle_t kernel_multiply_two,
                              ze_kernel_handle_t kernel_add_one_fork,
                              ze_context_handle_t context,
                              ze_device_handle_t device,
                              int graph_exec_count) {
  const size_t buf_size = DATA_SIZE * sizeof(int);

  // Allocate buffers
  void* host_input = nullptr;
  void* host_output = nullptr;
  void* dev_buf = nullptr;
  void* staging_buf = nullptr;

  ze_host_mem_alloc_desc_t host_desc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};
  ze_device_mem_alloc_desc_t dev_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};

  ZE_CHECK(zeMemAllocHost(context, &host_desc, buf_size, 0, &host_input));
  ZE_CHECK(zeMemAllocHost(context, &host_desc, buf_size, 0, &host_output));
  ZE_CHECK(zeMemAllocDevice(context, &dev_desc, buf_size, 0, device, &dev_buf));
  ZE_CHECK(zeMemAllocDevice(context, &dev_desc, buf_size, 0, device, &staging_buf));

  // Initialize input data: [0, 1, 2, ..., DATA_SIZE-1]
  int* input = static_cast<int*>(host_input);
  for (int i = 0; i < DATA_SIZE; ++i) {
    input[i] = i;
  }
  memset(host_output, 0, buf_size);

  // Set kernel arguments - each kernel instance gets its own buffer binding
  int count = DATA_SIZE;
  ZE_CHECK(zeKernelSetArgumentValue(kernel_add_one, 0, sizeof(void*), &dev_buf));
  ZE_CHECK(zeKernelSetArgumentValue(kernel_add_one, 1, sizeof(int), &count));
  ZE_CHECK(zeKernelSetArgumentValue(kernel_multiply_two, 0, sizeof(void*), &dev_buf));
  ZE_CHECK(zeKernelSetArgumentValue(kernel_multiply_two, 1, sizeof(int), &count));
  // Fork2 kernel operates on dev_buf (sequential fork, runs after fork1 completes)
  ZE_CHECK(zeKernelSetArgumentValue(kernel_add_one_fork, 0, sizeof(void*), &dev_buf));
  ZE_CHECK(zeKernelSetArgumentValue(kernel_add_one_fork, 1, sizeof(int), &count));

  // Create 3 immediate command lists: primary + 2 forks
  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

  ze_command_list_handle_t primary_cmdlist = nullptr;
  ze_command_list_handle_t fork1_cmdlist = nullptr;
  ze_command_list_handle_t fork2_cmdlist = nullptr;

  ZE_CHECK(zeCommandListCreateImmediate(context, device, &cmd_queue_desc, &primary_cmdlist));
  ZE_CHECK(zeCommandListCreateImmediate(context, device, &cmd_queue_desc, &fork1_cmdlist));
  ZE_CHECK(zeCommandListCreateImmediate(context, device, &cmd_queue_desc, &fork2_cmdlist));

  // Create 4 events: fork1, join1, fork2, join2
  constexpr uint32_t EVENT_POOL_SIZE = 4;
  ze_event_pool_desc_t pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                    ZE_EVENT_POOL_FLAG_HOST_VISIBLE, EVENT_POOL_SIZE};
  ze_event_pool_handle_t event_pool = nullptr;
  ZE_CHECK(zeEventPoolCreate(context, &pool_desc, 1, &device, &event_pool));

  ze_event_handle_t events[EVENT_POOL_SIZE];
  for (uint32_t i = 0; i < EVENT_POOL_SIZE; ++i) {
    ze_event_desc_t evt_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, i,
                                ZE_EVENT_SCOPE_FLAG_SUBDEVICE, ZE_EVENT_SCOPE_FLAG_SUBDEVICE};
    ZE_CHECK(zeEventCreate(event_pool, &evt_desc, &events[i]));
  }

  ze_event_handle_t fork_event1 = events[0];
  ze_event_handle_t join_event1 = events[1];
  ze_event_handle_t fork_event2 = events[2];
  ze_event_handle_t join_event2 = events[3];

  // Capture graph with fork/join
  ze_graph_handle_t graph = nullptr;
  ze_executable_graph_handle_t exec_graph = nullptr;

  // Use group_size=1 so group_count=DATA_SIZE to ensure all work items execute
  // (some simulators don't support SIMD dispatch within a work group)
  uint32_t group_size_x = 1;
  ZE_CHECK(zeKernelSetGroupSize(kernel_add_one, group_size_x, 1, 1));
  ZE_CHECK(zeKernelSetGroupSize(kernel_multiply_two, group_size_x, 1, 1));
  ZE_CHECK(zeKernelSetGroupSize(kernel_add_one_fork, group_size_x, 1, 1));
  uint32_t group_count_x = DATA_SIZE / group_size_x;
  std::cout << "Kernel group size: " << group_size_x << ", group count: " << group_count_x << std::endl;

  if (!CaptureGraphWithForks(kernel_add_one, kernel_multiply_two, kernel_add_one_fork,
                              context, primary_cmdlist, fork1_cmdlist, fork2_cmdlist,
                              dev_buf, host_input, staging_buf, host_output, buf_size,
                              fork_event1, join_event1, fork_event2, join_event2,
                              group_count_x, graph, exec_graph)) {
    std::cerr << "ERROR: Failed to capture graph with forks" << std::endl;
    return false;
  }

  std::cout << "Graph captured successfully with sequential forks: Fork1->Primary->Fork2->Primary" << std::endl;

  // Execute the graph multiple times
  for (int i = 0; i < graph_exec_count; ++i) {
    ZE_CHECK(p_zeCommandListAppendGraphExp(primary_cmdlist, exec_graph, nullptr,
                                          nullptr, 0, nullptr));
  }

  // Synchronize
  ZE_CHECK(zeCommandListHostSynchronize(primary_cmdlist, UINT64_MAX));

  // Verify results for single iteration
  int* output = static_cast<int*>(host_output);
  if (graph_exec_count == 1) {
    bool correct = true;
    // Debug: print first 8 values
    std::cout << "Output[0..7]: ";
    for (int i = 0; i < 8 && i < DATA_SIZE; ++i) {
      std::cout << output[i] << " ";
    }
    std::cout << std::endl;

    // Expected: (i + 2) * 2 = 2i + 4
    // Steps: add_one(i)=i+1, fork1 D2D round-trip (no-op), fork2 add_one=(i+1)+1=i+2,
    //        multiply_two=(i+2)*2=2i+4
    for (int i = 0; i < DATA_SIZE; ++i) {
      int expected = (i + 2) * 2;
      if (output[i] != expected) {
        std::cerr << "MISMATCH at [" << i << "]: expected=" << expected
                  << " got=" << output[i] << std::endl;
        correct = false;
        break;
      }
    }
    std::cout << "Result verification: " << (correct ? "PASSED" : "FAILED") << std::endl;
  } else {
    std::cout << "Result verification: skipped (multi-iteration)" << std::endl;
  }

  // Cleanup
  p_zeExecutableGraphDestroyExp(exec_graph);
  p_zeGraphDestroyExp(graph);

  for (uint32_t i = 0; i < EVENT_POOL_SIZE; ++i) {
    zeEventDestroy(events[i]);
  }
  zeEventPoolDestroy(event_pool);

  zeCommandListDestroy(fork2_cmdlist);
  zeCommandListDestroy(fork1_cmdlist);
  zeCommandListDestroy(primary_cmdlist);

  zeMemFree(context, staging_buf);
  zeMemFree(context, dev_buf);
  zeMemFree(context, host_output);
  zeMemFree(context, host_input);

  return true;
}


/// Initializes Level Zero resources, loads kernels, and runs the graph pipeline.
static bool RunGraphTest(ze_device_handle_t device, ze_driver_handle_t driver,
                          int graph_exec_count) {
  const std::string kernel_module = "graph_kernels.spv";
  std::string kernel_path = utils::GetExecutablePath();
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

  // Create add_one kernel (used on primary cmdlist)
  ze_kernel_desc_t kernel_desc_add = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "add_one"};
  ze_kernel_handle_t kernel_add_one = nullptr;
  ZE_CHECK(zeKernelCreate(module, &kernel_desc_add, &kernel_add_one));

  // Create multiply_two kernel (used on primary cmdlist)
  ze_kernel_desc_t kernel_desc_mul = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "multiply_two"};
  ze_kernel_handle_t kernel_multiply_two = nullptr;
  ZE_CHECK(zeKernelCreate(module, &kernel_desc_mul, &kernel_multiply_two));

  // Create separate add_one kernel instance for fork2 cmdlist
  // (separate handle avoids any kernel state sharing across cmdlists)
  ze_kernel_handle_t kernel_add_one_fork = nullptr;
  ZE_CHECK(zeKernelCreate(module, &kernel_desc_add, &kernel_add_one_fork));

  // Enable data collection
  utils::SetEnv("PTI_ENABLE_COLLECTION", "1");

  if (!RunGraphPipeline(kernel_add_one, kernel_multiply_two, kernel_add_one_fork,
                        context, device, graph_exec_count)) {
    std::cerr << "ERROR: Graph pipeline failed" << std::endl;
    return false;
  }

  // Disable data collection
  utils::SetEnv("PTI_ENABLE_COLLECTION", "");

  // Cleanup
  zeKernelDestroy(kernel_add_one_fork);
  zeKernelDestroy(kernel_multiply_two);
  zeKernelDestroy(kernel_add_one);
  zeModuleDestroy(module);
  zeContextDestroy(context);

  return true;
}

int main(int argc, char* argv[]) {
  if (zeInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) {
    std::cerr << "ERROR: Failed to initialize Level Zero" << std::endl;
    return 1;
  }

  ze_device_handle_t device = utils::ze::GetGpuDevice();
  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  if (device == nullptr || driver == nullptr) {
    std::cerr << "ERROR: Unable to find GPU device" << std::endl;
    return 1;
  }

  if (!LoadGraphExtension(driver)) {
    std::cerr << "ERROR: Graph extension not available" << std::endl;
    return 1;
  }

  int graph_exec_count = GRAPH_EXECUTION_COUNT;
  if (argc >= MIN_CLI_ARGS) {
    try {
      graph_exec_count = std::stoi(argv[1]);
      if (graph_exec_count <= 0) {
        std::cerr << "ERROR: Count must be positive" << std::endl;
        return 1;
      }
    } catch (const std::exception& e) {
      std::cerr << "ERROR: Invalid count: " << argv[1] << std::endl;
      return 1;
    }
  }

  std::cout << "Level Zero Graph API Test (Sequential Fork/Join Pattern)" << std::endl;
  std::cout << "Target device: " << utils::ze::GetDeviceName(device) << std::endl;
  std::cout << "Graph executions: " << graph_exec_count << std::endl;
  std::cout << "Data size: " << DATA_SIZE << " elements" << std::endl;
  std::cout << "Pattern: Primary -> Fork1 -> Primary -> Fork2 -> Primary (sequential, both join to primary)" << std::endl;

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
