# OpenCL(TM) Hot Functions
## Overview
This is a simple LD_PRELOAD based tool that allows to collect all called OpenCL(TM) API functions within an application along with their total execution time and call count.

As a result, table like the following will be printed.
```
=== API Timing Results: ===

Total Execution Time (ns): 363687486
Total API Time for CPU backend (ns): 524
Total API Time for GPU backend (ns): 355355363

== CPU Backend: ==

      Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
clGetDeviceIDs,           1,                 524,    100.00,                 524,                 524,                 524

== GPU Backend: ==

                          Function,       Calls,           Time (ns),  Time (%),        Average (ns),            Min (ns),            Max (ns)
                    clBuildProgram,           1,           173888026,     48.93,           173888026,           173888026,           173888026
                          clFinish,           4,           172908147,     48.66,            43227036,            42711785,            44318785
              clEnqueueWriteBuffer,           8,             4636256,      1.30,              579532,              207825,             1864890
               clEnqueueReadBuffer,           4,             2051244,      0.58,              512811,              498662,              542971
            clEnqueueNDRangeKernel,           4,             1623139,      0.46,              405784,              236120,              609050
                clReleaseMemObject,          12,               95182,      0.03,                7931,                3525,               16436
                    clCreateBuffer,          12,               81056,      0.02,                6754,                2511,               16990
                    clSetKernelArg,          16,               24515,      0.01,                1532,                 141,                7038
           clGetEventProfilingInfo,           8,               13139,      0.00,                1642,                 103,                3288
                   clCreateContext,           1,               12680,      0.00,               12680,               12680,               12680
                  clReleaseProgram,           1,                9503,      0.00,                9503,                9503,                9503
         clCreateProgramWithSource,           1,                3880,      0.00,                3880,                3880,                3880
                    clCreateKernel,           1,                2941,      0.00,                2941,                2941,                2941
                   clReleaseKernel,           1,                1679,      0.00,                1679,                1679,                1679
                   clGetKernelInfo,           4,                1617,      0.00,                 404,                 190,                 552
clCreateCommandQueueWithProperties,           1,                1388,      0.00,                1388,                1388,                1388
                    clGetDeviceIDs,           2,                 311,      0.00,                 155,                 138,                 173
             clReleaseCommandQueue,           1,                 270,      0.00,                 270,                 270,                 270
                   clGetDeviceInfo,           2,                 227,      0.00,                 113,                 103,                 124
                  clReleaseContext,           1,                 163,      0.00,                 163,                 163,                 163
```
## Supported OS
- Linux
- Windows

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
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
cl_hot_functions.exe ..\..\dpc_gemm\build\Release\dpc_gemm.exe cpu
```