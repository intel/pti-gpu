# Tracing and Profiling Tool for Data Parallel C++ (DPC++)
## Overview
This tool provides basic tracing and profiling capabilities for the compute applications based on Intel runtimes for OpenCL(TM) and Level Zero, like DPC++, Intel(R) Implicit SPMD Program Compiler (Intel(R) ISPC) and OpenMP* GPU offload programs.

The following capabilities are available:
```
Usage: ./onetrace[.exe] [options] <application> <args>
Options:
--call-logging [-c]            Trace host API calls
--host-timing  [-h]            Report host API execution time
--device-timing [-d]           Report kernels execution time
--kernel-submission [-s]       Report append (queued), submit and execute intervals for kernels
--device-timeline [-t]         Trace device activities
--chrome-call-logging          Dump host API calls to JSON file
--chrome-device-timeline       Dump device activities to JSON file per command queue
--chrome-kernel-timeline       Dump device activities to JSON file per kernel name
--chrome-device-stages         Dump device activities by stages to JSON file
--verbose [-v]                 Enable verbose mode to show more kernel information
--demangle                     Demangle DPC++ kernel names
--kernels-per-tile             Dump kernel information per tile
--tid                          Print thread ID into host API trace
--pid                          Print process ID into host API and device activity trace
--output [-o] <filename>       Print console logs into the file
--conditional-collection       Enable conditional collection mode
--version                      Print version
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

Memory transfers for Level Zero are supplemented by transfer direction:
- "M" - system memory allocated with malloc or new;
- "H" - USM host memory allocated with `zeMemAllocHost`;
- "D" - USM device memory allocated with `zeMemAllocDevice`;
- "S" - USM shared memory allocated with `zeMemAllocShared`;
```
=== Device Timing Results: ===

                Total Execution Time (ns):            295236137
Total Device Time for L0 backend (ns):                177147822

== L0 Backend: ==

                            Kernel,       Calls,     Time (ns),  Time (%),     Average (ns),      Min (ns),      Max (ns)
                              GEMM,           4,     172104499,     97.15,         43026124,      42814000,      43484166
zeCommandListAppendMemoryCopy(M2D),           8,       2934831,      1.66,           366853,        286500,        585333
zeCommandListAppendMemoryCopy(D2M),           4,       2099164,      1.18,           524791,        497666,        559666
        zeCommandListAppendBarrier,           8,          9328,      0.01,             1166,          1166,          1166
```
**Kernel Submission** mode collects append (queued for OpenCL(TM)), submit and execute intervals for kernels and memory transfers:
```
=== Kernel Submission Results: ===

Total Execution Time (ns):            256576162
   Total Device Time (ns):            174582990

                            Kernel,       Calls,         Append (ns),  Append (%),         Submit (ns),  Submit (%),        Execute (ns), Execute (%),
                              GEMM,           4,              553087,       10.79,            12441082,        3.03,           169770832,       97.24,
zeCommandListAppendMemoryCopy(M2D),           8,             2898413,       56.53,            20843165,        5.08,             2843832,        1.63,
zeCommandListAppendMemoryCopy(D2M),           4,              534710,       10.43,           182217916,       44.43,             1957331,        1.12,
        zeCommandListAppendBarrier,           8,             1140561,       22.25,           194646664,       47.46,               10995,        0.01,
```
**Verbose** mode provides additional information per kernel (SIMD width, group count and group size for oneAPI Level Zero (Level Zero) and SIMD width, global and local size for OpenCL(TM)) and per transfer (bytes transferred). This option should be used in addition to others, e.g. for **Device Timing** mode one can get:
```
=== Device Timing Results: ===

                Total Execution Time (ns):            392681085
Total Device Time for CL GPU backend (ns):            177544981

== CL GPU Backend: ==

                                  Kernel,   Calls,   Time (ns),  Time (%),     Average (ns),      Min (ns),      Max (ns)
GEMM[SIMD32, {1024, 1024, 1}, {0, 0, 0}],       4,   172101915,     96.93,         43025478,      42804333,      43375416
     clEnqueueWriteBuffer[4194304 bytes],       8,     3217914,      1.81,           402239,        277416,        483750
      clEnqueueReadBuffer[4194304 bytes],       4      2225152,      1.25,           556288,        527122,        570898
```

**Device Timeline** mode dumps four timestamps for each device activity - *queued* to the host command queue for OpenCL(TM) or "append" to the command list for Level Zero, *submit* to device queue, *start* and *end* on the device (all the timestamps are in CPU nanoseconds):
```
Device Timeline: start time [ns] = 1632829416753742036
...
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueWriteBuffer [ns] = 317341082 (queued) 317355010 (submit) 317452332 (start) 317980165 (end)
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueWriteBuffer [ns] = 317789774 (queued) 317814558 (submit) 318160607 (start) 318492690 (end)
Device Timeline (queue: 0x55a9c7e51e70): GEMM [ns] = 318185764 (queued) 318200629 (submit) 318550014 (start) 361260930 (end)
Device Timeline (queue: 0x55a9c7e51e70): clEnqueueReadBuffer [ns] = 361479600 (queued) 361481387 (submit) 361482574 (start) 362155593 (end)
...
```
**Chrome Device Timeline** mode dumps timestamps for device activities per command queue to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool. Can't be used with **Chrome Kernel Timeline** and **Chrome Device Stages**.

**Chrome Kernel Timeline** mode dumps timestamps for device activities per kernel name to JSON format that can be opened in [chrome://tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool) browser tool. Can't be used with **Chrome Device Timeline**.

**Chrome Device Stages** mode provides alternative view for device queue where each kernel invocation is divided into stages: "queued" or "appended", "sumbitted" and "execution". Can't be used with **Chrome Device Timeline**.

**Conditional Collection** mode allows one to enable data collection for any target interval (by default collection will be disabled) using environment variable `PTI_ENABLE_COLLECTION`, e.g.:
```cpp
// Collection disabled
setenv("PTI_ENABLE_COLLECTION", "1", 1);
// Collection enabled
unsetenv("PTI_ENABLE_COLLECTION");
// Collection disabled
```
All the API calls and kernels, which submission happens while collection disabled interval, will be omitted from final results.

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime) to run on GPU
- [Intel(R) Xeon(R) Processor / Intel(R) Core(TM) Processor (CPU) Runtimes](https://software.intel.com/en-us/articles/opencl-drivers#cpu-section) to run on CPU

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/onetrace
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
./onetrace -c -h ../../../samples/dpc_gemm/build/dpc_gemm cpu
./onetrace -c -h ../../../samples/dpc_gemm/build/dpc_gemm gpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\tools\onetrace
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
onetrace.exe -c -h ..\..\..\samples\dpc_gemm\build\dpc_gemm.exe cpu
onetrace.exe -c -h ..\..\..\samples\dpc_gemm\build\dpc_gemm.exe gpu
```