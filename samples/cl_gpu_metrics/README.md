# GPU Metrics for OpenCL(TM)
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect such GPU hardware metrics as execution unit (EU) active, stall and idle ratios attributed to OpenCL(TM) kernels.

As a result, table like the following will be printed. For each kernel its call count, total time and metric values will be shown.
```
=== Device Metrics: ===

Total Execution Time (ns): 414222992
Total Kernel Time (ns): 177950165

    Kernel,       Calls,           Time (ns),        Time (%),        Average (ns),   EU Active (%),    EU Stall (%),     EU Idle (%)
      GEMM,           4,           177950165,          100.00,            44487541,           61.59,           38.35,            0.06
```
To set target device and sub-device to collect metrics from one can specify `PTI_DEVICE_ID` and `PTI_SUB_DEVICE_ID` environment variables.

## Supported OS
- Linux
- Windows

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/cl_gpu_metrics
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./cl_gpu_metrics <target_application>
```
One may use [cl_gemm](../cl_gemm) as target application:
```sh
./cl_gpu_metrics ../../cl_gemm/build/cl_gemm gpu
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./cl_gpu_metrics ../../cl_gemm/build/cl_gemm gpu
```
On Linux one may need to enable metrics collection for non-root users:
```sh
sudo echo 0 > /proc/sys/dev/i915/perf_stream_paranoid
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\cl_gpu_metrics
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
cl_gpu_metrics.exe <target_application>
```
One may use [cl_gemm](../cl_gemm) as target application:
```sh
cl_gpu_metrics.exe ..\..\cl_gemm\build\cl_gemm.exe
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
cl_gpu_metrics.exe ..\..\cl_gemm\build\cl_gemm.exe
```