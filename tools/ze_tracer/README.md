# Level Zero Tracer
## Overview
This tool is an analogue of [Intercept Layer for OpenCL(TM) Applications](https://github.com/intel/opencl-intercept-layer) designed to support Level Zero.

The following capabilities are available currently:
```
Usage: ./ze_tracer[.exe] [options] <application> <args>
Options:
--call-logging [-c]             Trace host API calls
--host-timing  [-h]             Report host API execution time
--device-timing [-d]            Report kernels exectucion time
--device-timeline [-t]          Trace device activities
--chrome-call-logging           Dump host API calls to JSON file
--chrome-device-timeline        Dump device activities to JSON file
--chrome-device-stages          Dump device activities by stages to JSON file
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

Total Execution Time (ns):    376807360
   Total Device Time (ns):    178294707

                       Kernel,       Calls, SIMD, Transferred (bytes),           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                         GEMM,           4,   32,                   0,           173655671,     97.40,            43413917,            43343928,            43517564
zeCommandListAppendMemoryCopy,          12,    0,            50331648,             4639036,      2.60,              386586,              271742,              553610
...
```
**Device Timeline** mode (***Linux kernel 5.0+ is required for accurate measurements***) dumps four timestamps for each device activity - *append* to the command list, *submit* to device queue, *start* and *end* on the device (all the timestamps are in CPU nanoseconds):
```
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 396835703 (append) 398002195 (submit) 399757026 (start) 400230526 (end)
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 397039340 (append) 398002195 (submit) 400231776 (start) 400547193 (end)
Device Timeline (queue: 0x556fa2318fc0): GEMM [ns] = 397513563 (append) 398002195 (submit) 400548943 (start) 443632026 (end)
Device Timeline (queue: 0x556fa2318fc0): zeCommandListAppendMemoryCopy [ns] = 397632053 (append) 398002195 (submit) 443633526 (start) 444084943 (end)
...
```
**Chrome Device Timeline** mode dumps timestamps for device activities to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool.

**Chrome Device Stages** mode provides alternative view for device queue where each kernel invocation is divided into stages: "appended", "sumbitted" and "execution". Can't be used in pair with **Chrome Device Timeline**.

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [libdrm](https://gitlab.freedesktop.org/mesa/drm)

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