# OpenMP* GEMM
## Overview
This sample application performs general matrix multiplication using OpenMP* on CPU or GPU, so it can be used as a target for OpenMP* profiling and tracing tools.
```
OpenMP Matrix Multiplication (matrix size: 2048 x 2048, repeats 4 times)
Target device: GPU
Matrix multiplication time: 1.55599 sec
Results are CORRECT with accuracy: 5.86231e-06
Matrix multiplication time: 1.27369 sec
Results are CORRECT with accuracy: 5.86231e-06
Matrix multiplication time: 1.27024 sec
Results are CORRECT with accuracy: 5.86231e-06
Matrix multiplication time: 1.27694 sec
Results are CORRECT with accuracy: 5.86231e-06
Total execution time: 5.37699 sec
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)

## Build and Run
### Linux
Run the following commands to build the sample (make sure you use Intel(R) C++ Compiler for building):
```sh
source <inteloneapi>/setvars.sh
cd <pti>/samples/omp_gemm
mkdir build
cd build
CXX=icpx cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the application:
```sh
./omp_gemm [cpu|gpu] [matrix_size] [repeat_count]
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
<inteloneapi>\setvars.bat
cd <pti>\samples\omp_gemm
mkdir build
cd build
set CXX=icl
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```
Use this command line to run the application:
```sh
omp_gemm.exe [cpu|gpu] [matrix_size] [repeats_count]
```