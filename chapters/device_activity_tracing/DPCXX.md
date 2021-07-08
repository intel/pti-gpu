# Device Activity Tracing for SYCL/DPC++
## Overview
One can measure kernel execution time directly from SYCL/DPC++ code using `sycl::event` object.

Three time points are available for each event:
 - `command_submit` - returns the time in nanoseconds when the associated command group was submitted to the queue;
 - `command_start` - returns the time in nanoseconds when the action associated with the command group (e.g. kernel invocation) started executing on the device;
 - `command_end` - returns the time in nanoseconds when the action associated with the command group (e.g. kernel invocation) finished executing on the device.

To make these time points available one should use `sycl::queue` object with enabled profiling mode.

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Any

**Needed Headers**:
- Standard SYCL/DPC++ [headers](https://github.com/intel/llvm/tree/sycl/sycl/include/CL)

**Needed Libraries**:
- Standard SYCL/DPC++ [runtime](https://github.com/intel/llvm)

## How To Use
1. Create a queue with enabled profling mode:
```cpp
#include <CL/sycl.hpp> // SYCL header
...
sycl::property_list prop_list{sycl::property::queue::enable_profiling()};
sycl::queue queue(sycl::gpu_selector{}, prop_list);
```
2. Use this queue to submit a kernel, return an event:
```cpp
sycl::event event = queue.submit([&](sycl::handler& cgh) {
  cgh.parallel_for<class MyKernel>(
      sycl::range<2>(size, size),
      [=](sycl::id<2> id) {
    // Kernel code
  });
});
```
3. Wait for kernel completion:
```cpp
event.wait(); // or queue.wait()
```
4. Read needed time points for the event:
```cpp
#define NSEC_IN_SEC 1000000000.0
...
uint64_t start =
  event.get_profiling_info<sycl::info::event_profiling::command_start>();
uint64_t end =
  event.get_profiling_info<sycl::info::event_profiling::command_end>();
duration = static_cast<double>(end - start) / NSEC_IN_SEC;
std::cout << "Kernel execution time: " << duration << " sec" << std::endl;
```

## Usage Details
- refer to the [SYCL 2020 standard](https://www.khronos.org/registry/SYCL/specs/sycl-2020/pdf/sycl-2020.pdf) to learn more on events and profiling queues.

## Samples
- [DPC++ GEMM](../../samples/dpc_gemm)