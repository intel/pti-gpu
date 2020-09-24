# Level Zero Hot Kernels
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect executed Level Zero kernels and buffer transfers within an application along with their total execution time and call count.

As a result, table like the following will be printed:
```
=== Device Timing Results: ===

Total Exectuion Time (ns): 443466433
Total Device Time (ns): 244619426

    Kernel,       Calls, SIMD,  Transfered (bytes),           Time (ns),  Time (%),        Average (ns)
      GEMM,           4,   32,                   0,           239549786,     97.93,            59887446
 HtoD/DtoH,          12,    0,            50331648,             5069640,      2.07,              422470

[INFO] Job is successfully completed
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
cd <pti>/samples/ze_hot_kernels
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_hot_kernels <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./ze_hot_kernels ../../ze_gemm/build/ze_gemm
./ze_hot_kernels ../../dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_hot_kernels
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_hot_kernels.exe <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
ze_hot_kernels.exe ..\..\ze_gemm\build\ze_gemm.exe
ze_hot_kernels.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```