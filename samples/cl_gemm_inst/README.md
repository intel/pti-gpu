# OpenCL(TM) GEMM with Manual Code Instrumentation
## Overview
This sample application performs general matrix multiplication using OpenCL(TM) GPU device and provides some statistics based on manual source code instrumentation with OpenCL built-in intrinsics.
```
OpenCL Matrix Multiplication (matrix size: 1024 x 1024, repeats 4 times)
Target device: Intel(R) Gen9 HD Graphics NEO
Matrix multiplication time: 0.073987 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.069171 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.0688272 sec
Results are CORRECT with accuracy: 4.90573e-06
Matrix multiplication time: 0.0686009 sec
Results are CORRECT with accuracy: 4.90573e-06
Total execution time: 0.56824 sec
======== GEMM Instrumentation Results ========
Samples collected: 131072 (8.32% of bad samples)
Estimated average HW thread duration: ~ 391 us (391549 cycles)
Estimated number of HW threads per EU: 7
Estimated number of EUs per subslice: 8
Estimated number of subslices per slice: 3
Estimated number of slices: 1
Estimated total number EUs: 24
Estimated total number of HW threads: 168
==============================================
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime) (should be built in `Debug` or `ReleaseInternal` mode)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/cl_gemm_inst
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the application:
```sh
./bin/cl_gemm_inst [matrix_size] [repeat_count]
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\cl_gemm_inst
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the application:
```sh
cl_gemm_inst.exe [matrix_size] [repeats_count]