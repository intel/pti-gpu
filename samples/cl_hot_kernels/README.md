# OpenCL(TM) Hot Functions
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect all executed OpenCL(TM) kernels and buffer transfers within an application along with their total execution time and call count.

As a result, table like the following will be printed.
```
+------------------------------------------------------------------------------------------------+
|                                           | Call  | SIMD  | Total MBytes |   Avg    |  Total   |
| Kernel                                    | Count | Width |  Transfered  | Time, ms | Time, ms |
+------------------------------------------------------------------------------------------------+
| GEMM                                      |     4 |    32 |            - |    42.70 |   170.80 |
| HtoD                                      |     8 |     - |        32.00 |     0.45 |     3.63 |
| DtoH                                      |     4 |     - |        16.00 |     0.84 |     3.36 |
+------------------------------------------------------------------------------------------------+
```
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
cd <pti>/samples/cl_hot_kernels
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./cl_hot_kernels <target_application>
```
One may use [cl_gemm](../cl_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./cl_hot_kernels ../../cl_gemm/build/cl_gemm
./cl_hot_kernels ../../dpc_gemm/build/dpc_gemm cpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\cl_hot_kernels
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
cl_hot_kernels.exe <target_application>
```
One may use [cl_gemm](../cl_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
cl_hot_kernels.exe ..\..\cl_gemm\build\cl_gemm.exe
cl_hot_kernels.exe ..\..\dpc_gemm\build\dpc_gemm.exe cpu
```