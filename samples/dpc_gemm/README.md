# DPC++ GEMM
## Overview
This sample application performs general matrix multiplication using DPC++ on CPU or GPU, so it can be used as a target for OpenCL(TM) and Level Zero profiling and tracing tools.
```
DPC++ Matrix Multiplication (matrix size: 1024 x 1024, repeats 4 times)
Target device: Intel(R) Gen9
Matrix multiplication time: 0.0429941 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.0431165 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.0433001 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.0428462 sec
Results are CORRECT with accuracy: 4.90573e-06
Total execution time: 0.373728 sec
```
## Supported OS
- Linux
- Windows

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)

## Build and Run
### Linux
Run the following commands to build the sample ((make sure you have oneAPI DPC++ Compiler in `PATH` for building)):
```sh
source <inteloneapi>/setvars.sh
cd <pti>/samples/dpc_gemm
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the application:
```sh
./dpc_gemm [cpu|gpu|host] [matrix_size] [repeat_count]
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample (make sure you have oneAPI DPC++ Compiler in `PATH` for building):
```sh
<inteloneapi>\setvars.bat
cd <pti>\samples\dpc_gemm
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
Use this command line to run the application:
```sh
cd Release
dpc_gemm.exe [cpu|gpu|host] [matrix_size] [repeats_count]
```