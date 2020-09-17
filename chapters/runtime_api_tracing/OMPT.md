# Runtime API Tracing for OpenMP*
## Overview
OpenMP* standard version 5.0+ has a special interface for first-party tools that provides the following capabilities:
- a mechanism to initialize a first-party tool;
- routines that enable a tool to determine the capabilities of an OpenMP implementation;
- routines that enable a tool to examine OpenMP state information associated with a thread;
- mechanisms that enable a tool to map implementation-level calling contexts back to their source-level representations;
- a callback interface that enables a tool to receive notification of OpenMP events;
- a tracing interface that enables a tool to trace activity on OpenMP target devices;
- a runtime library routine that an application can use to control a tool.

**Supported Runtimes**:
- Any OpenMP* version 5.0+ runtime

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Any

**Needed Headers**:
- omp-tools.h (*a part of compiler headers*)

## How To Use
1. Make a dynamic library with `ompt_start_tool` function definition:
```cpp
#include <omp-tools.h>

ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version, const char* runtime_version) {
  /* Start-up code here */
}
```
2. Declare tool initialization and finalization routines and return their pointers as part of `ompt_start_tool_result_t` structure from `ompt_start_tool` function:
```cpp
int Initialize(ompt_function_lookup_t lookup,
               int initial_device_num,
               ompt_data_t* data) {
  /* Tool initialization code here */
}

void Finalize(ompt_data_t* data) {
  assert(data->ptr != nullptr);
  ompt_start_tool_result_t* result =
    static_cast<ompt_start_tool_result_t*>(data->ptr);
  delete result;
}

ompt_start_tool_result_t* ompt_start_tool(
    unsigned int omp_version, const char* runtime_version) {
  ompt_start_tool_result_t* result = new ompt_start_tool_result_t;
  result->initialize = Initialize;
  result->finalize = Finalize;
  result->tool_data.ptr = result;

  return result;
}
```
3. Most of the interfaces OMPT provides are not defined as global function symbols. Instead, they are defined as runtime entry points that a tool can only identify through the `lookup` function that is provided as an argument to the tool initializer. One normally need to grab the pointer to `ompt_set_callback` function first:
```cpp
int Initialize(ompt_function_lookup_t lookup,
               int initial_device_num,
               ompt_data_t* data) {
  ompt_set_callback_t ompt_set_callback =
    reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
  assert(ompt_set_callback != nullptr);

  /* More tool initialization code here */
}
```
4. Finally one can set tracing callbacks to monitor application execution process in terms of various OpenMP* pragmas. To enable the tool  one have to return non-zero value from initialization routine:
```cpp
void ParallelBeginCallback(ompt_data_t* task_data,
                           const ompt_frame_t* task_frame,
                           ompt_data_t* parallel_data,
                           unsigned int requested_parallelism,
                           int flags,
                           const void* codeptr_ra) {
  std::cout << "Parallel Region Is Started" << std::endl;
}

void ParallelEndCallback(ompt_data_t* parallel_data,
                         ompt_data_t* task_data,
                         int flags,
                         const void* codeptr_ra) {
  std::cout << "Parallel Region Is Ended" << std::endl;
}

void TargetCallback(ompt_target_t kind,
                    ompt_scope_endpoint_t endpoint,
                    int device_num,
                    ompt_data_t* task_data,
                    ompt_id_t target_id,
                    const void* codeptr_ra) {
  if (endpoint == ompt_scope_begin) {
    std::cout << "Target Region Is Started" << std::endl;
  } else {
    std::cout << "Target Region Is Ended" << std::endl;
  }
}

int Initialize(ompt_function_lookup_t lookup,
               int initial_device_num,
               ompt_data_t* data) {
  ompt_set_callback_t ompt_set_callback =
    reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
  assert(ompt_set_callback != nullptr);

  ompt_set_result_t result = ompt_set_error;

  result = ompt_set_callback(ompt_callback_parallel_begin,
    reinterpret_cast<ompt_callback_t>(ParallelBeginCallback));
  assert(result == ompt_set_always);
  result = ompt_set_callback(ompt_callback_parallel_end,
    reinterpret_cast<ompt_callback_t>(ParallelEndCallback));
  assert(result == ompt_set_always);

  result = ompt_set_callback(ompt_callback_target,
    reinterpret_cast<ompt_callback_t>(TargetCallback));
  assert(result == ompt_set_always);

  return 1; // Don't forget to return non-zero value to enable tracing
}
```
5. One may use `OMP_TOOL_LIBRARIES` environment variable to run the tool for target application:
```sh
OMP_TOOL_LIBRARIES=./libmy_omp_tool.so <application>
```

## Usage Details
- refer to [OpenMP API Specs Version 5.0](https://www.openmp.org/wp-content/uploads/OpenMP-API-Specification-5.0.pdf) (Chapter 4) to learn more on OMPT intreface

## Samples
- [OpenMP* Hot Regions](../../samples/omp_hot_regions)