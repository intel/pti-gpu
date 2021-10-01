# Device Activity Tracing with oneAPI Level Zero (Level Zero)
## Overview
Device activity tracing allows to measure execution time for the kernels and memory transfers running on the device. This capability is a part of Level Zero core API and does not require to enable the Instrumentation Layer.

There are also two alternative ways to get kernel execution time based on Metrics API covered in appropriate [chapter](../metrics_collection/LevelZero.md):
1. Based on ***metric queries***;
2. Based on ***stream markers***.

General approach for all the options is to find (or instrument with [Tracing API](../runtime_api_tracing/LevelZero.md)) the functions that append some activity into command list (e.g. `zeCommandListAppendLaunchKernel`) and inject an additional time measurements before and after this activity.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [ze_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/ze_api.h)
- [zet_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/zet_api.h)

**Needed Libraries**:
- oneAPI Level Zero [libraries](https://github.com/intel/compute-runtime)

## How To Use

Device activity tracing is a part of Level Zero core functionality that allows to collect timestamps for the events created from the pool with enabled profiling capabilities.

To use it one should create an event pool with timestamps support and an event from it:
```cpp
// Create an event pool
ze_event_pool_desc_t event_pool_desc = {
    ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
    ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool contain profiling information
    1}; // number of events in pool
ze_event_pool_handle_t event_pool = nullptr;
zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);
assert(event_pool != nullptr);

// Create an event
ze_event_desc_t event_desc = {
    ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
    ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};
ze_event_handle_t event = nullptr;
zeEventCreate(event_pool, &event_desc, &event);
assert(event != nullptr);
```
The next step is to intercept target device activity and use the created event to measure its execution time, e.g.:
```cpp
zeCommandListAppendLaunchKernel(cmd_list, kernel, global_size,
                                event /* profiling event */, 0, nullptr);
```
To get device activity timestamps one may use the following functions (should be called only after the activity will be completed):
```cpp
ze_kernel_timestamp_result_t timestamp{};
zeEventQueryKernelTimestamp(event, &timestamp);
```
Finally to compute actual activity duration in nanoseconds one should retrieve timer resolution for the device and perform time scaling:
```cpp
ze_device_properties_t props{};
props.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
status = zeDeviceGetProperties(state->device, &props);
assert(status == ZE_RESULT_SUCCESS);

uint64_t time_ns = (timestamp.context.kernelEnd - timestamp.context.kernelStart) * props.timerResolution;
```
There are two types of timestamps one may retrieve:
* `global` - wall-clock time start/end in GPU clocks for event, should be used to map kernel to global application timeline;
* `context` - context time start/end in GPU clocks for event, only includes time while HW context is actively running on GPU, may be used to calculate precise kernel duration.

The major difference between context and global timestamps is that global time will include time of activity preemption and context will not.

## Time Correlation
Common problem while kernel timestamps collection is to map these timestamps to general CPU timeline. Since Level Zero provides kernel timestamps in GPU clocks, one may need to convert them to some CPU time. Starting from Level Zero 1.1, new function `zeDeviceGetGlobalTimestamps` is available. Using this function, one can get correlated host (CPU) and device (GPU) timestamps for any particular device:
```cpp
uint64_t host_timestamp = 0, device_timestamp = 0;
ze_result_t status = zeDeviceGetGlobalTimestamps(
    device, &host_timestamp, &device_timestamp);
assert(status == ZE_RESULT_SUCCESS);
```
Note, that host timestamp value corresponds to `CLOCK_MONOTONIC_RAW` on Linux or `QueryPerformanceCounter` on Windows, while device timestamp for GPU is collected in raw GPU cycles. Also note that not all bits of device timestamp are valid, to get exact number of valid bits use `timestampValidBits` field from `ze_device_properties_t` structure, e.g.:
```cpp
ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
ze_result_t status = zeDeviceGetProperties(device, &props);
assert(status == ZE_RESULT_SUCCESS);

uint64_t mask = (1ull << props.kernelTimestampValidBits) - 1ull;
uint64_t kernel_timestamp = (device_timestamp & mask);
```
The same valid bits mask should be applied to `global` kernel timestamps.

To convert GPU cycles into seconds one may use `timerResolution` field from `ze_device_properties_t` structure, that represents cycles per second starting from Level Zero 1.1:
```cpp
ze_device_properties_t props{};
props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
ze_result_t status = zeDeviceGetProperties(device, &props);
assert(status == ZE_RESULT_SUCCESS);

const uint64_t NSEC_IN_SEC = 1000000000;
uint64_t device_timestamp_ns = NSEC_IN_SEC * device_timestamp / props.timerResolution;
```

## Build and Run
Event pool profiling does not require any additional environment variables to be set, simply run the application as is:
```
./<application>
```

## Usage Details
- refer to oneAPI Level Zero [documentation](https://spec.oneapi.com/level-zero/latest/index.html) to learn more

## Samples
- [Level Zero GEMM](../../samples/ze_gemm)
- [Level Zero Hot Kernels](../../samples/ze_hot_kernels)

## Tools
- [Level Zero Tracer](../../tools/ze_tracer)
- [Tracing and Profiling Tool for Data Parallel C++ (DPC++)](../../tools/onetrace)
- [GPU Metrics Collection Tool for Data Parallel C++ (DPC++)](../../tools/oneprof)