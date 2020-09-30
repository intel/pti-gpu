# OpenMP* Hot Regions
## Overview
This sample is a simple tool that allows to collect specific OpenMP* regions within an application along with their total execution time and call count. The tool is designed to work with `omp parallel` and `omp target` region types.

As a result, table like the following will be printed.
```
+------------------------------------------------------------------------------+
| Region ID  |     Region Type    | Call Count | Avg Time, ms | Total Time, ms |
+------------------------------------------------------------------------------+
|     403291 |             Target |          4 |        0.074 |          0.297 |
|     403292 |   TransferToDevice |         40 |        0.000 |          0.019 |
|     403293 | TransferFromDevice |          4 |        0.001 |          0.002 |
+------------------------------------------------------------------------------+
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) oneAPI HPC Toolkit](https://software.intel.com/en-us/oneapi/hpc-kit) (version beta05 and above)

## Build and Run
### Linux
Run the following commands to build the sample (make sure you use Intel(R) C++ Compiler for building):
```sh
source <inteloneapi>/setvars.sh
cd <pti>/samples/omp_hot_regions
mkdir build
cd build
CXX=icpc cmake -DCMAKE_BUILD_TYPE=Release ..
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
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
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