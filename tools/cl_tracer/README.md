# OpenCL(TM) Tracer
## Overview
This tool is an analogue of [Intercept Layer for OpenCL(TM) Applications](https://github.com/intel/opencl-intercept-layer) designed based on internal [tracing mechanism](../../chapters/runtime_api_tracing/OpenCL.md) implemented in Intel runtimes for OpenCL(TM).

The following capabilities are available currently:
```
Usage: ./cl_tracer[.exe] [options] <application> <args>
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

          Total Execution Time (ns):    366500174
Total API Time for CPU backend (ns):        16851
Total API Time for GPU backend (ns):    357744252

== CPU Backend: ==

      Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
clGetDeviceIDs,           1,               16851,    100.00,               16851,               16851,               16851

== GPU Backend: ==

                          Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                          clFinish,           4,           174933263,     48.90,            43733315,            42966659,            44067629
                    clBuildProgram,           1,           172466699,     48.21,           172466699,           172466699,           172466699
              clEnqueueWriteBuffer,           8,             3788816,      1.06,              473602,              367912,              593802
            clEnqueueNDRangeKernel,           4,             3238743,      0.91,              809685,              208889,             2562605
...
```
**Device Timing** mode collects duration for each kernel on the device and provides the summary for the whole application:
```
=== Device Timing Results: ===

             Total Execution Time (ns):    366500174
Total Device Time for CPU backend (ns):            0
Total Device Time for GPU backend (ns):    180543441

== GPU Backend: ==

              Kernel,       Calls, SIMD, Transferred (bytes),           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                GEMM,           4,   32,                   0,           174210248,     96.49,            43552562,            42764416,            43851333
clEnqueueWriteBuffer,           8,    0,            33554432,             3683507,      2.04,              460438,              355983,              584325
 clEnqueueReadBuffer,           4,    0,            16777216,             2649686,      1.47,              662421,              607215,              702940
...
```
**Device Timeline** mode dumps four timestamps for each device activity - *queued* to the host command queue, *submit* to device queue, *start* and *end* on the device (all the timestamps are in CPU nanoseconds):
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
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime) to run on GPU
- [Intel(R) Xeon(R) Processor / Intel(R) Core(TM) Processor (CPU) Runtimes](https://software.intel.com/en-us/articles/opencl-drivers#cpu-section) to run on CPU

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/cl_tracer
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./cl_tracer [options] <target_application>
```
One may use [cl_gemm](../../samples/cl_gemm) or [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
./cl_tracer -c -h ../../../samples/cl_gemm/build/cl_gemm
./cl_tracer -c -h ../../../samples/dpc_gemm/build/dpc_gemm cpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\tools\cl_tracer
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
cl_tracer.exe [options] <target_application>
```
One may use [cl_gemm](../../samples/cl_gemm) or [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
cl_tracer.exe -c -h ..\..\..\samples\cl_gemm\build\cl_gemm.exe
cl_tracer.exe -c -h ..\..\..\samples\dpc_gemm\build\dpc_gemm.exe cpu
```