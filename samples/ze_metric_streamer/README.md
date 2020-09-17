# Level Zero Metric Streamer

## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect such GPU hardware metrics as execution unit (EU) active, stall and idle ratios attributed to Level Zero kernels. The sample is based on continuous mode for metrics collection.

As a result, table like the following will be printed. For each kernel its call count, total time and metric values will be shown.
```
+------------------------------------------------------------------------------------------------------+
| Kernel                       | Call Count | Total Time, ms | EU Active, % | EU Stall, % | EU Idle, % |
+------------------------------------------------------------------------------------------------------+
| GEMM                         |          4 |         181.06 |        64.78 |       35.06 |       0.16 |
+------------------------------------------------------------------------------------------------------+
```
## Supported OS
- Linux
- Windows

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
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
cd <pti>/samples/ze_metric_streamer
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_metric_streamer <target_application>
```
One may use [ze_gemm](../ze_gemm) as target application:
```sh
./ze_metric_streamer ../../ze_gemm/build/ze_gemm
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./ze_metric_streamer ../../ze_gemm/build/ze_gemm
```
On Linux one may need to enable collection for non-root users:
```sh
sudo echo 0 > /proc/sys/dev/i915/perf_stream_paranoid
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_metric_streamer
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_metric_streamer.exe <target_application>
```
One may use [ze_gemm](../ze_gemm) as target application:
```sh
ze_metric_streamer.exe ..\..\ze_gemm\build\ze_gemm.exe
```
Since Intel(R) Metrics Discovery Application Programming Interface and Metrics Library for Metrics Discovery API (Metrics Library for MD API) are loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
ze_metric_streamer.exe ..\..\ze_gemm\build\ze_gemm.exe
```