# Level Zero Hot Functions
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect all called Level Zero API functions within an application along with their total execution time and call count.

As a result, table like the following will be printed.
```
=== API Timing Results: ===

Total Execution Time (ns): 439529295
Total API Time (ns): 430352157

                         Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
        zeCommandQueueSynchronize,           4,           245208971,     56.98,            61302242,            44288933,           111719100
                   zeModuleCreate,           1,            92922095,     21.59,            92922095,            92922095,            92922095
zeCommandQueueExecuteCommandLists,           4,            89169473,     20.72,            22292368,             1628943,            84087179
    zeCommandListAppendMemoryCopy,          12,             1753331,      0.41,              146110,               65386,              751061
            zeCommandQueueDestroy,           4,              259572,      0.06,               64893,               62835,               68716
             zeCommandListDestroy,           4,              219994,      0.05,               54998,               54714,               55321
                 zeMemAllocDevice,          12,              219467,      0.05,               18288,                8452,               34876
                        zeMemFree,          12,              187305,      0.04,               15608,               12315,               22998
             zeCommandQueueCreate,           4,              118216,      0.03,               29554,               26468,               31023
              zeCommandListCreate,           4,               99894,      0.02,               24973,               15703,               36353
         zeKernelSetArgumentValue,          16,               46716,      0.01,                2919,                 195,               18653
                zeEventPoolCreate,           4,               37682,      0.01,                9420,                8731,               10779
             zeKernelSetGroupSize,           4,               28111,      0.01,                7027,                1052,               22665
                  zeModuleDestroy,           1,               22883,      0.01,               22883,               22883,               22883
            zeDeviceGetProperties,           6,               11408,      0.00,                1901,                 393,                2663
  zeCommandListAppendLaunchKernel,           4,               10861,      0.00,                2715,                2520,                2894
                    zeEventCreate,           4,                8327,      0.00,                2081,                 584,                3285
         zeKernelSuggestGroupSize,           4,                8130,      0.00,                2032,                1200,                2439
               zeCommandListClose,           4,                4831,      0.00,                1207,                1155,                1281
                   zeKernelCreate,           1,                4598,      0.00,                4598,                4598,                4598
      zeEventQueryKernelTimestamp,           4,                3494,      0.00,                 873,                 721,                1081
                  zeKernelDestroy,           1,                2428,      0.00,                2428,                2428,                2428
       zeCommandListAppendBarrier,           8,                1749,      0.00,                 218,                 125,                 345
                   zeEventDestroy,           4,                1049,      0.00,                 262,                 237,                 282
                  zeContextCreate,           1,                 763,      0.00,                 763,                 763,                 763
                           zeInit,           1,                 303,      0.00,                 303,                 303,                 303
                      zeDeviceGet,           2,                 285,      0.00,                 142,                  76,                 209
                      zeDriverGet,           2,                 221,      0.00,                 110,                  66,                 155
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
cd <pti>/samples/ze_hot_functions
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_hot_functions <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./ze_hot_functions ../../ze_gemm/build/ze_gemm
./ze_hot_functions ../../dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_hot_functions
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_hot_functions.exe <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
ze_hot_functions.exe ..\..\ze_gemm\build\ze_gemm.exe
ze_hot_functions.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```