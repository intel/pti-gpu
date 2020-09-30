# OpenCL(TM) Hot Functions
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect all called OpenCL(TM) API functions within an application along with their total execution time and call count.

As a result, table like the following will be printed.
```
+----------------------------------------------------------------------------------------------+
| Function                                        | Call Count | Avg Time, ms | Total Time, ms |
+----------------------------------------------------------------------------------------------+
| clFinish                                        |          4 |        43.86 |         175.46 |
| clBuildProgram                                  |          1 |       171.89 |         171.89 |
| clEnqueueWriteBuffer                            |          8 |         0.62 |           5.00 |
| clEnqueueReadBuffer                             |          4 |         0.96 |           3.84 |
| clEnqueueNDRangeKernel                          |          4 |         0.40 |           1.60 |
| clReleaseMemObject                              |         12 |         0.01 |           0.08 |
| clCreateBuffer                                  |         12 |         0.01 |           0.07 |
| clSetKernelArg                                  |         16 |         0.00 |           0.03 |
| clGetEventProfilingInfo                         |          8 |         0.00 |           0.01 |
| clReleaseProgram                                |          1 |         0.01 |           0.01 |
| clCreateContext                                 |          1 |         0.01 |           0.01 |
| clCreateProgramWithSource                       |          1 |         0.00 |           0.00 |
| clCreateKernel                                  |          1 |         0.00 |           0.00 |
| clReleaseKernel                                 |          1 |         0.00 |           0.00 |
| clGetKernelInfo                                 |          4 |         0.00 |           0.00 |
| clCreateCommandQueueWithProperties              |          1 |         0.00 |           0.00 |
| clGetDeviceIDs                                  |          2 |         0.00 |           0.00 |
| clGetDeviceInfo                                 |          2 |         0.00 |           0.00 |
| clReleaseCommandQueue                           |          1 |         0.00 |           0.00 |
| clReleaseContext                                |          1 |         0.00 |           0.00 |
+----------------------------------------------------------------------------------------------+
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
cd <pti>/samples/cl_hot_functions
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./cl_hot_functions <target_application>
```
One may use [cl_gemm](../cl_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./cl_hot_functions ../../cl_gemm/build/cl_gemm
./cl_hot_functions ../../dpc_gemm/build/dpc_gemm cpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\cl_hot_functions
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
cl_hot_functions.exe <target_application>
```
One may use [cl_gemm](../cl_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
cl_hot_functions.exe ..\..\cl_gemm\build\cl_gemm.exe
cl_hot_functions.exe ..\..\dpc_gemm\build\dpc_gemm.exe cpu
```