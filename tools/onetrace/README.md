# Tracing and Profiling Tool for Data Parallel C++ (DPC++)
## Overview
This tool provides basic tracing and profiling capabilities for the compute applications based on Intel runtimes for OpenCL(TM) and Level Zero, like DPC++ and OpenMP* GPU offload programs.

The following capabilities are available:
```
Usage: ./onetrace[.exe] [options] <application> <args>
Options:
--call-logging [-c]             Trace host API calls
--host-timing  [-h]             Report host API execution time
--device-timing [-d]            Report kernels exectucion time
--device-timeline [-t]          Trace device activities
--chrome-device-timeline        Dump device activities to JSON file
--chrome-call-logging           Dump host API calls to JSON file
```

**Call Logging** mode allows to grab full host API trace, e.g.:
```
...
>>>> [271632470] clCreateBuffer: context = 0x5591dba3f860 flags = 4 size = 4194304 hostPtr = 0 errcodeRet = 0x7ffd334b2f04
<<<< [271640078] clCreateBuffer [7608 ns] result = 0x5591dbaa5760 -> CL_SUCCESS (0)
>>>> [272171119] clEnqueueWriteBuffer: commandQueue = 0x5591dbf4be70 buffer = 0x5591dbaa5760 blockingWrite = 1 offset = 0 cb = 4194304 ptr = 0x5591dc92af90 numEventsInWaitList = 0 eventWaitList = 0 event = 0
<<<< [272698660] clEnqueueWriteBuffer [527541 ns] -> CL_SUCCESS (0)
>>>> [272716922] clSetKernelArg: kernel = 0x5591dc500c60 argIndex = 0 argSize = 8 argValue = 0x7ffd334b2f10
<<<< [272724034] clSetKernelArg [7112 ns] -> CL_SUCCESS (0)
>>>> [272729938] clSetKernelArg: kernel = 0x5591dc500c60 argIndex = 1 argSize = 8 argValue = 0x7ffd334b2f18
<<<< [272733712] clSetKernelArg [3774 ns] -> CL_SUCCESS (0)
...
```
**Chrome Call Logging** mode dumps API calls to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool.

**Host Timing** mode collects duration for each API call and provides the summary for the whole application:
```
=== API Timing Results: ===

             Total Execution Time (ns):   372547856
    Total API Time for L0 backend (ns):   355680113
Total API Time for CL CPU backend (ns):        7119
Total API Time for CL GPU backend (ns):        2550

== L0 Backend: ==

                              Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                zeEventHostSynchronize,          32,           181510841,     51.03,             5672213,                  72,            45327080
                        zeModuleCreate,           1,            96564991,     27.15,            96564991,            96564991,            96564991
     zeCommandQueueExecuteCommandLists,           8,            76576727,     21.53,             9572090,               20752,            76024831
...

== CL CPU Backend: ==

         Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
  clGetDeviceInfo,           6,                3094,     43.46,                 515,                 216,                1295
clGetPlatformInfo,           2,                1452,     20.40,                 726,                 487,                 965
   clGetDeviceIDs,           4,                 987,     13.86,                 246,                  93,                 513
...

== CL GPU Backend: ==

         Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
   clGetDeviceIDs,           4,                 955,     37.45,                 238,                 153,                 352
  clGetDeviceInfo,           6,                 743,     29.14,                 123,                  65,                 244
  clReleaseDevice,           2,                 331,     12.98,                 165,                 134,                 197
...
```
**Device Timing** mode collects duration for each kernel on the device and provides the summary for the whole application:
```
=== Device Timing Results: ===

                Total Execution Time (ns):    362771691
Total Device Time for CL GPU backend (ns):    176849013

== CL GPU Backend: ==

              Kernel,       Calls, SIMD, Transferred (bytes),           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                GEMM,           4,   32,                   0,           171239582,     96.83,            42809895,            42567333,            43121083
clEnqueueWriteBuffer,           8,    0,            33554432,             3362082,      1.90,              420260,              298500,              532500
 clEnqueueReadBuffer,           4,    0,            16777216,             2247349,      1.27,              561837,              556520,              565134
...
```
**Device Timeline** mode dumps four timestamps for each device activity - *queued* to the host command queue for OpenCL(TM) or "append" to the command list for Level Zero, *submit* to device queue, *start* and *end* on the device (all the timestamps are in CPU nanoseconds):
```
...
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueWriteBuffer [ns] = 317341082 (queued) 317355010 (submit) 317452332 (start) 317980165 (end)
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueWriteBuffer [ns] = 317789774 (queued) 317814558 (submit) 318160607 (start) 318492690 (end)
Device Timeline (queue: 0x55a9c7e51e70): GEMM [ns] = 318185764 (queued) 318200629 (submit) 318550014 (start) 361260930 (end)
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueReadBuffer [ns] = 361479600 (queued) 361481387 (submit) 361482574 (start) 362155593 (end)
...
```
**Chrome Device Timeline** mode dumps timestamps for device activities to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool.

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime) to run on GPU
- [Intel(R) Xeon(R) Processor / Intel(R) Core(TM) Processor (CPU) Runtimes](https://software.intel.com/en-us/articles/opencl-drivers#cpu-section) to run on CPU
- [libdrm](https://gitlab.freedesktop.org/mesa/drm)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/onetrace
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./onetrace [options] <target_application>
```
One may use e.g. [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
./onetrace -c -h ../../dpc_gemm/build/dpc_gemm cpu
./onetrace -c -h ../../dpc_gemm/build/dpc_gemm gpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\onetrace
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
onetrace.exe [options] <target_application>
```
One may use e.g. [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
onetrace.exe -c -h ..\..\dpc_gemm\build\dpc_gemm.exe cpu
onetrace.exe -c -h ..\..\dpc_gemm\build\dpc_gemm.exe gpu
```