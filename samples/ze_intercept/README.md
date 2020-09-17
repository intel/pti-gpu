# Level Zero Intercept
## Overview
This tool is an analogue of [Intercept Layer for OpenCL(TM) Applications](https://github.com/intel/opencl-intercept-layer) designed to support Level Zero.

Currently it has limited capabilities but expected to be fully functional eventually:
```
Usage: ./ze_intercept[.exe] [options] <application> <args>
Options:
--call-logging [-c]             Trace host API calls
--call-logging-timestamps [-t]  Show timestamps (in ns) for each host API call
                                (this option should be used along with --call-logging (-c))
--host-timing  [-h]             Report host API execution time
--device-timing [-d]            Report kernels exectucion time
```

**Call Logging** mode allows to grab full host API trace, e.g.:
```
...
>>>> zeDriverAllocDeviceMem: hDriver = 0x561d31a6fa20 device_desc = 0x7ffc2043467c size = 4194304 alignment = 64 hDevice = 0x561d31a6fb40 pptr = 0x7ffc20434638
<<<< zeDriverAllocDeviceMem -> ZE_RESULT_SUCCESS (0)
>>>> zeKernelSetGroupSize: hKernel = 0x561d32791a90 groupSizeX = 256 groupSizeY = 1 groupSizeZ = 1
<<<< zeKernelSetGroupSize -> ZE_RESULT_SUCCESS (0)
>>>> zeKernelSetArgumentValue: hKernel = 0x561d32791a90 argIndex = 0 argSize = 8 pArgValue = 0x7ffc20434628
<<<< zeKernelSetArgumentValue -> ZE_RESULT_SUCCESS (0)
...
```
**Call Logging with Timestamps** adds timestamp for each API call (should be used along with **Call Logging**, each timestamp is a duration in ns from application start till the current moment), e.g.:
```
...
>>>> [108341534] zeDriverAllocDeviceMem: hDriver = 0x5639f5f04a20 device_desc = 0x7ffef9d7741c size = 4194304 alignment = 64 hDevice = 0x5639f5f04b40 pptr = 0x7ffef9d773d8
<<<< [108383108] zeDriverAllocDeviceMem -> ZE_RESULT_SUCCESS (0)
>>>> [108403884] zeKernelSetGroupSize: hKernel = 0x5639f6c26a90 groupSizeX = 256 groupSizeY = 1 groupSizeZ = 1
<<<< [108450932] zeKernelSetGroupSize -> ZE_RESULT_SUCCESS (0)
>>>> [108484644] zeKernelSetArgumentValue: hKernel = 0x5639f6c26a90 argIndex = 0 argSize = 8 pArgValue = 0x7ffef9d773c8
<<<< [108534077] zeKernelSetArgumentValue -> ZE_RESULT_SUCCESS (0)
...
```
**Host Timing** mode collects duration for each API call and provides the summary for the whole application (may be used separately):
```
=== Host Timing Results: ===

Total time (ns): 222081726

                         Function,       Calls,       Time (ns),  Time (%),    Average (ns),        Min (ns),        Max (ns)
                   zeModuleCreate,           1,        92975907,     41.87,        92975907,        92975907,        92975907
zeCommandQueueExecuteCommandLists,           1,        84281391,     37.95,        84281391,        84281391,        84281391
        zeCommandQueueSynchronize,           1,        43885685,     19.76,        43885685,        43885685,        43885685
    zeCommandListAppendMemoryCopy,           3,          740759,      0.33,          246919,           54903,          628351
...
```
In case it is used along with **Call Logging**, duration in ns will be provided for each host API call, e.g.:
```
...
>>>> zeDriverAllocDeviceMem: hDriver = 0x55bb28938a20 device_desc = 0x7ffd76fd692c size = 4194304 alignment = 64 hDevice = 0x55bb28938b40 pptr = 0x7ffd76fd68e8
<<<< zeDriverAllocDeviceMem [6844 ns] -> ZE_RESULT_SUCCESS (0)
>>>> zeKernelSetGroupSize: hKernel = 0x55bb2965af00 groupSizeX = 256 groupSizeY = 1 groupSizeZ = 1
<<<< zeKernelSetGroupSize [1847 ns] -> ZE_RESULT_SUCCESS (0)
>>>> zeKernelSetArgumentValue: hKernel = 0x55bb2965af00 argIndex = 0 argSize = 8 pArgValue = 0x7ffd76fd68d8
<<<< zeKernelSetArgumentValue [2769 ns] -> ZE_RESULT_SUCCESS (0)
...
```
Note, that **Host Timing** duration will be more accurate comparing with similar duration one may calculate manualy based on **Call Logging Timestamps**.

**Device Timing** mode collects duration for each kernel on the device and provides the summary for the whole application (may be used separately):
```
=== Device Timing Results: ===

Total time (ns): 244823025

                     Function,       Calls,       Time (ns),  Time (%),    Average (ns),        Min (ns),        Max (ns)
                         GEMM,           4,       240085551,     98.06,        60021387,        42715867,       111690942
zeCommandListAppendMemoryCopy,          12,         4737474,      1.94,          394789,          282283,          495593
...
```

## Supported OS
- Linux
- Windows

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