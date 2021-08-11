# Level Zero Tracer
## Overview
This tool is an analogue of [Intercept Layer for OpenCL(TM) Applications](https://github.com/intel/opencl-intercept-layer) designed to support Level Zero.

The following capabilities are available currently:
```
Usage: ./ze_tracer[.exe] [options] <application> <args>
Options:
--call-logging [-c]            Trace host API calls
--host-timing  [-h]            Report host API execution time
--device-timing [-d]           Report kernels execution time
--device-timing-verbose [-v]   Report kernels execution time with SIMD width and global/local sizes
--device-timeline [-t]         Trace device activities
--output [-o] <filename>       Print console logs into the file
--chrome-call-logging          Dump host API calls to JSON file
--chrome-device-timeline       Dump device activities to JSON file per command queue
--chrome-kernel-timeline       Dump device activities to JSON file per kernel name
--chrome-device-stages         Dump device activities by stages to JSON file
--tid                          Print thread ID into host API trace
--pid                          Print process ID into host API and device activity trace
--version                      Print version
```

**Call Logging** mode allows to grab full host API trace, e.g.:
```
...
>>>> [99194698] zeMemAllocDevice: hContext = 0x55c087b2ca80 device_desc = 0x7ffcd388ecc0 size = 4194304 alignment = 64 hDevice = 0x55c0878f3bd0 pptr = 0x7ffcd388ec08 (ptr = 0)
<<<< [99259883] zeMemAllocDevice [65185 ns] ptr = 0xffffffffff3f0000 -> ZE_RESULT_SUCCESS (0)
>>>> [99307794] zeKernelSetGroupSize: hKernel = 0x55c087b5a4d0 groupSizeX = 256 groupSizeY = 1 groupSizeZ = 1
<<<< [99335067] zeKernelSetGroupSize [27273 ns] -> ZE_RESULT_SUCCESS (0)
>>>> [99389764] zeKernelSetArgumentValue: hKernel = 0x55c087b5a4d0 argIndex = 0 argSize = 8 pArgValue = 0x7ffcd388ebf8
<<<< [99435142] zeKernelSetArgumentValue [45378 ns] -> ZE_RESULT_SUCCESS (0)
...
```
**Chrome Call Logging** mode dumps API calls to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool.

**Host Timing** mode collects duration for each API call and provides the summary for the whole application:
```
=== API Timing Results: ===

Total Execution Time (ns):    418056422
      Total API Time (ns):    407283268

                         Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
        zeCommandQueueSynchronize,           4,           182529847,     44.82,            45632461,            45271728,            46364532
                   zeModuleCreate,           1,           111687828,     27.42,           111687828,           111687828,           111687828
zeCommandQueueExecuteCommandLists,           4,           108593458,     26.66,            27148364,             1756304,           102803947
    zeCommandListAppendMemoryCopy,          12,             2493748,      0.61,              207812,               62061,             1037087
...
```
**Device Timing** mode collects duration for each kernel on the device and provides the summary for the whole application:
```
=== Device Timing Results: ===

Total Execution Time (ns):             95693944
   Total Device Time (ns):              2470823

                       Kernel,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                         GEMM,           4,             2202998,     89.16,              550749,              509666,              578833
zeCommandListAppendMemoryCopy,          12,              257495,     10.42,               21457,               12666,               50166
   zeCommandListAppendBarrier,           8,               10330,      0.42,                1291,                1166,                1500
...
```
**Device Timing Verbose** mode provides additional information per kernel (SIMD width, group count and group size) and per transfer (bytes transferred):
```
=== Device Timing Results: ===

Total Execution Time (ns):             95831439
   Total Device Time (ns):              2414157

                                     Kernel,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
     GEMM[SIMD32, {1, 256, 1}, {256, 1, 1}],           4,             2155831,     89.30,              538957,              508666,              606166
zeCommandListAppendMemoryCopy[262144 bytes],           8,              213831,      8.86,               26728,               12500,               49833
zeCommandListAppendMemoryCopy[131072 bytes],           4,               34499,      1.43,                8624,                8000,               10000
                 zeCommandListAppendBarrier,           8,                9996,      0.41,                1249,                1166,                1333
```

**Device Timeline** mode (***Linux kernel 5.0+ is required for accurate measurements***) dumps four timestamps for each device activity - *append* to the command list, *submit* to device queue, *start* and *end* on the device (all the timestamps are in CPU nanoseconds):
```
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 396835703 (append) 398002195 (submit) 399757026 (start) 400230526 (end)
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 397039340 (append) 398002195 (submit) 400231776 (start) 400547193 (end)
Device Timeline (queue: 0x556fa2318fc0): GEMM [ns] = 397513563 (append) 398002195 (submit) 400548943 (start) 443632026 (end)
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 397632053 (append) 398002195 (submit) 443633526 (start) 444084943 (end)
...
```
**Chrome Device Timeline** mode dumps timestamps for device activities per command queue to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool. Can't be used with **Chrome Kernel Timeline** and **Chrome Device Stages**.

**Chrome Kernel Timeline** mode dumps timestamps for device activities per kernel name to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool. Can't be used with **Chrome Device Timeline**.

**Chrome Device Stages** mode provides alternative view for device queue where each kernel invocation is divided into stages: "appended", "sumbitted" and "execution". Can't be used with **Chrome Device Timeline**.

To enable `high_resolution_clock` timestamps instead of `steady_clock` used by default, one may set `CLOCK_HIGH_RESOLUTION` variable for CMake:
```sh
cmake -DCLOCK_HIGH_RESOLUTION=1 ..
```

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/ze_tracer
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_tracer [options] <target_application>
```
One may use [ze_gemm](../../samples/ze_gemm) or [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
./ze_tracer -c -h ../../../samples/ze_gemm/build/ze_gemm
./ze_tracer -c -h ../../../samples/dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\tools\ze_tracer
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_tracer.exe [options] <target_application>
```
One may use [ze_gemm](../../samples/ze_gemm) or [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
ze_tracer.exe -c -h ..\..\..\samples\ze_gemm\build\ze_gemm.exe
ze_tracer.exe -c -h ..\..\..\samples\dpc_gemm\build\dpc_gemm.exe
```