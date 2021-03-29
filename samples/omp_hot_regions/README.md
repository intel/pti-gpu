# OpenMP* Hot Regions
## Overview
This sample is a simple tool that allows to collect specific OpenMP* regions within an application along with their total execution time and call count. The tool is designed to work with `omp parallel` and `omp target` region types.

As a result, table like the following will be printed.
```
=== OpenMP Timing Results: ===

Total Execution Time (ns): 3243751492
Total Region Time (ns): 186811670

           Region ID,         Region Type,       Calls, Transferred (bytes),           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
             4203121,              Target,           4,                   0,           180761706,     96.76,            45190426,            44939097,            45505303
             4203122,    TransferToDevice,           8,            33554432,             4119470,      2.21,              514933,              364952,              628467
             4203123,  TransferFromDevice,           4,            16777216,             1930494,      1.03,              482623,              478523,              484546
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)

## Build and Run
### Linux
Run the following commands to build the sample (make sure you have Intel(R) C++ Compiler in `PATH` for building):
```sh
source <inteloneapi>/setvars.sh
cd <pti>/samples/omp_hot_regions
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
OMP_TOOL_LIBRARIES=./libomp_hot_regions.so <target_application>
```
One may use [omp_gemm](../omp_gemm) as target application:
```sh
OMP_TOOL_LIBRARIES=./libomp_hot_regions.so ../../omp_gemm/build/omp_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample (make sure you have Intel(R) C++ Compiler in `PATH` for building):
```sh
<inteloneapi>\setvars.sh
cd <pti>\samples\omp_hot_regions
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```
Use this command line to run the tool:
```sh
set OMP_TOOL_LIBRARIES=omp_hot_regions.dll
<target_application>
```
One may use [omp_gemm](../omp_gemm) as target application:
```sh
set OMP_TOOL_LIBRARIES=omp_hot_regions.dll
..\..\omp_gemm\build\omp_gemm.exe
```