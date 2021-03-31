# Level Zero Metrics Query
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect such GPU hardware metrics as execution unit (EU) active, stall and idle ratios attributed to Level Zero kernels. The sample is based on query mode for metrics collection.

As a result, table like the following will be printed. For each kernel its call count, total time and metric values will be shown.
```
=== Device Metrics: ===

Total Execution Time (ns): 396137394
Total Kernel Time (ns): 171688405

    Kernel,       Calls,           Time (ns),        Time (%),        Average (ns),   EU Active (%),    EU Stall (%),     EU Idle (%)
      GEMM,           4,           171688405,          100.00,            42922101,           73.71,           26.12,            0.17
```
To set target device and sub-device to collect metrics from one can specify `PTI_DEVICE_ID` and `PTI_SUB_DEVICE_ID` environment variables.

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)
- [Metrics Library for Metrics Discovery API (Metrics Library for MD API)](https://github.com/intel/metrics-library)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/ze_metric_query
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_metric_query <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./ze_metric_query ../../ze_gemm/build/ze_gemm
./ze_metric_query ../../dpc_gemm/build/dpc_gemm
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./ze_metric_query ../../ze_gemm/build/ze_gemm
```
On Linux one may need to enable collection for non-root users:
```sh
sudo echo 0 > /proc/sys/dev/i915/perf_stream_paranoid
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_metric_query
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_metric_query.exe <target_application>
```
One may use [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
ze_metric_query.exe ..\..\ze_gemm\build\ze_gemm.exe
```
Since Intel(R) Metrics Discovery Application Programming Interface and Metrics Library for Metrics Discovery API (Metrics Library for MD API) are loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
ze_metric_query.exe ..\..\ze_gemm\build\ze_gemm.exe
ze_metric_query.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```