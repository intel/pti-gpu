# OpenCL(TM) Hot Functions
## Overview
This is a simple LD_PRELOAD based tool that allows to collect all called OpenCL(TM) API functions within an application along with their total execution time and call count for GPU device.

As a result, table like the following will be printed.
```
                          Function,       Calls,           Time (ns),        Average (ns)
                    clBuildProgram,           1,           183549198,           183549198
                    clCreateBuffer,          12,              108285,                9023
clCreateCommandQueueWithProperties,           1,                1265,                1265
                   clCreateContext,           1,                9322,                9322
                    clCreateKernel,           1,                3428,                3428
         clCreateProgramWithSource,           1,                3219,                3219
            clEnqueueNDRangeKernel,           4,             2237845,              559461
               clEnqueueReadBuffer,           4,             2358133,              589533
              clEnqueueWriteBuffer,           8,             5719781,              714972
                          clFinish,           4,           174064236,            43516059
                    clGetDeviceIDs,           2,                 362,                 181
                   clGetDeviceInfo,           2,                 354,                 177
           clGetEventProfilingInfo,           8,               14198,                1774
                   clGetKernelInfo,           4,                2411,                 602
             clReleaseCommandQueue,           1,                1046,                1046
                  clReleaseContext,           1,                 173,                 173
                   clReleaseKernel,           1,                2741,                2741
                clReleaseMemObject,          12,              110922,                9243
                  clReleaseProgram,           1,               11561,               11561
                    clSetKernelArg,          16,               75282,                4705
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

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
One may use [cl_gemm](../cl_gemm) as target application:
```sh
./cl_hot_functions ../../cl_gemm/build/cl_gemm
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
One may use [cl_gemm](../cl_gemm) as target application:
```sh
cl_hot_functions.exe ..\..\cl_gemm\build\cl_gemm.exe
```