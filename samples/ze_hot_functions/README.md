# Level Zero Hot Functions
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect all called Level Zero API functions within an application along with their total execution time and call count.

As a result, table like the following will be printed.
```
+------------------------------------------------------------------------------------------------+
| Function                                          | Call Count | Avg Time, ms | Total Time, ms |
+------------------------------------------------------------------------------------------------+
| zeEventHostSynchronize                            |       1768 |         0.10 |         175.04 |
| zeCommandListAppendMemoryCopy                     |         16 |         6.22 |          99.48 |
| zeModuleCreate                                    |          1 |        88.09 |          88.09 |
| zeCommandQueueExecuteCommandLists                 |          8 |         0.17 |           1.37 |
| zeDriverAllocDeviceMem                            |         12 |         0.04 |           0.50 |
| zeCommandListCreate                               |          8 |         0.03 |           0.22 |
| zeCommandListDestroy                              |          8 |         0.02 |           0.20 |
| zeDriverFreeMem                                   |         12 |         0.01 |           0.15 |
| zeCommandListCreateImmediate                      |          2 |         0.03 |           0.05 |
| zeEventCreate                                     |          8 |         0.01 |           0.05 |
| zeCommandListAppendLaunchKernel                   |          4 |         0.01 |           0.04 |
| zeKernelSetArgumentValue                          |         52 |         0.00 |           0.04 |
| zeKernelSetGroupSize                              |          4 |         0.00 |           0.02 |
| zeCommandQueueCreate                              |          1 |         0.01 |           0.01 |
| zeCommandQueueDestroy                             |          1 |         0.01 |           0.01 |
| zeKernelSuggestGroupSize                          |          4 |         0.00 |           0.01 |
| zeCommandListClose                                |          8 |         0.00 |           0.01 |
| zeKernelCreate                                    |          1 |         0.00 |           0.00 |
| zeEventDestroy                                    |          8 |         0.00 |           0.00 |
| zeCommandListAppendWaitOnEvents                   |          4 |         0.00 |           0.00 |
| zeDeviceGetKernelProperties                       |          8 |         0.00 |           0.00 |
| zeEventPoolCreate                                 |          1 |         0.00 |           0.00 |
| zeDeviceGetCacheProperties                        |          8 |         0.00 |           0.00 |
| zeEventGetTimestamp                               |          8 |         0.00 |           0.00 |
| zeDeviceGetProperties                             |          2 |         0.00 |           0.00 |
| zeDeviceGetMemoryProperties                       |         16 |         0.00 |           0.00 |
| zeKernelSetAttribute                              |         12 |         0.00 |           0.00 |
| zeDriverGetProperties                             |          1 |         0.00 |           0.00 |
| zeDeviceGetImageProperties                        |          8 |         0.00 |           0.00 |
| zeInit                                            |          2 |         0.00 |           0.00 |
| zeDeviceGet                                       |          8 |         0.00 |           0.00 |
| zeModuleBuildLogDestroy                           |          1 |         0.00 |           0.00 |
| zeDriverGet                                       |          2 |         0.00 |           0.00 |
| zeDeviceGetComputeProperties                      |          2 |         0.00 |           0.00 |
| zeDriverGetApiVersion                             |          1 |         0.00 |           0.00 |
+------------------------------------------------------------------------------------------------+
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