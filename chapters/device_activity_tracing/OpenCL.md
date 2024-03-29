# Device Activity Tracing for OpenCL(TM)
## Overview
OpenCL(TM) device activity tracing is the standard feature of OpenCL(TM) runtime that allows to measure execution time for the kernels and memory transfers running on the device.

All the device activities, which should be placed into the queue with `clEnqueue*` call, have an event that allows to track their execution status (usually it's the last argument of `clEnqueue*` function).

The same event could be used to get additional profiling information for the device activity, such as:
 - time counter in nanoseconds when the command identified by event is enqueued in a command-queue by the host (`CL_PROFILING_COMMAND_QUEUED`);
 - time counter in nanoseconds when the command identified by event that has been enqueued is submitted by the host to the device associated with the command queue (`CL_PROFILING_COMMAND_SUBMIT`);
 - time counter in nanoseconds when the command identified by event starts execution on the device (`CL_PROFILING_COMMAND_START`);
 - time counter in nanoseconds when the command identified by event has finished execution on the device (`CL_PROFILING_COMMAND_END`).

Intel(R) Xeon(R) Processor / Intel(R) Core(TM) Processor (CPU) Runtimes use `QueryPerformanceCounter` on Windows and `CLOCK_MONOTONIC` on Linux as time sources for the counters described above. Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver also uses `QueryPerformanceCounter` on Windows but `CLOCK_MONOTONIC_RAW` on Linux.

**Supported Runtimes**:
- any OpenCL(TM) 1.0 and above

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Any

**Needed Headers**:
- OpenCL(TM) [headers](https://github.com/KhronosGroup/OpenCL-Headers)

**Needed Libraries**:
- OpenCL(TM) [libraries](https://github.com/intel/compute-runtime)

## How To Use
To retrieve the timestamps described above, one may:
1. Enable profiling mode for the command queue of the target device:
```cpp
cl_queue_properties props[3] = { CL_QUEUE_PROPERTIES,
                                 CL_QUEUE_PROFILING_ENABLE, 0 };
cl_int status = CL_SUCCESS;
cl_command_queue queue =
  clCreateCommandQueueWithProperties(context, device, props, &status);
assert(status == CL_SUCESS);
```
2. Introduce the event for the target device activity, e.g. for the kernel:
```cpp
cl_event event = nullptr;
status = clEnqueueNDRangeKernel(queue, kernel, dim, nullptr,
    global_size, local_size, 0, nullptr, &event);
assert(status == CL_SUCESS);
```
3. Set the callback to be notified when the activity is completed:
```cpp
status = clSetEventCallback(event, CL_COMPLETE, EventNotify, nullptr);
assert(status == CL_SUCESS);
```
4. Read the profiling data inside the callback:
```cpp
void CL_CALLBACK EventNotify(cl_event event,
                             cl_int event_status,
                             void* user_data) {
    cl_int status = CL_SUCCESS;
    cl_ulong start = 0, end = 0;
    
    assert(event_status == CL_COMPLETE);

    status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START,
                                     sizeof(cl_ulong), &start, nullptr);
    assert(status == CL_SUCCESS);
    status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,
                                     sizeof(cl_ulong), &end, nullptr);
    assert(status == CL_SUCCESS);
}
```
## Time Correlation
It's commonly needed to map OpenCL kernel timestamps to general CPU timeline. To solve this problem one should use `clGetDeviceAndHostTimer` function to get time sync point between host and device:
```cpp
cl_ulong device_timestamp = 0, host_timestamp = 0;
cl_int status = clGetDeviceAndHostTimer(
    device, device_timestamp, host_timestamp);
assert(status == CL_SUCCESS)
```
Note, that host timestamp in Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver is based on `CLOCK_MONOTONIC_RAW` on Linux and `QueryPerformanceCounter` on Windows (implementation specific, may be changed in future). Both timers are in nanoseconds.

## Usage Details
- refer to the documentation for the function [clGetEventProfilingInfo](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetEventProfilingInfo.html) to learn more on OpenCL(TM) profiling

## Samples
- [OpenCL(TM) GEMM](../../samples/cl_gemm)
- [OpenCL(TM) Hot Kernels](../../samples/cl_hot_kernels)
- [OpenCL(TM) GPU Metrics](../../samples/cl_gpu_metrics)

## Tools
- [OpenCL(TM) Tracer](../../tools/cl_tracer)
- [Tracing and Profiling Tool for Data Parallel C++ (DPC++)](../../tools/onetrace)
- [GPU Metrics Collection Tool for Data Parallel C++ (DPC++)](../../tools/oneprof)