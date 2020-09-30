# Level Zero Intercept
## Overview
This tool is an analogue of [Intercept Layer for OpenCL(TM) Applications](https://github.com/intel/opencl-intercept-layer) designed to support Level Zero.

Currently it has limited capabilities but expected to be fully functional eventually:
```
Usage: ./ze_intercept[.exe] [options] <application> <args>
Options:
--call-logging [-c]             Trace host API calls
--host-timing  [-h]             Report host API execution time
--device-timing [-d]            Report kernels exectucion time
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
**Host Timing** mode collects duration for each API call and provides the summary for the whole application:
```
=== API Timing Results: ===

Total Execution Time (ns): 418056422
Total API Time (ns): 407283268

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

Total Execution Time (ns): 376807360
Total Device Time (ns): 178294707

                       Kernel,       Calls, SIMD, Transferred (bytes),           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                         GEMM,           4,   32,                   0,           173655671,     97.40,            43413917,            43343928,            43517564
zeCommandListAppendMemoryCopy,          12,    0,            50331648,             4639036,      2.60,              386586,              271742,              553610
...
```

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/ze_intercept
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_intercept [options] <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application, e.g.:
```sh
./ze_intercept -c -h ../../ze_gemm/build/ze_gemm
./ze_intercept -c -h ../../dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_intercept
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_intercept.exe [options] <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application, e.g.:
```sh
ze_intercept.exe -c -h ..\..\ze_gemm\build\ze_gemm.exe
ze_intercept.exe -c -h ..\..\dpc_gemm\build\dpc_gemm.exe
```