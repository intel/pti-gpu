# Level Zero Metrics Info
## Overview
This sample application queries the information about the hardwre metrics for Intel(R) Processor Graphics, available to collect through Level Zero Metrics API.

Expected output should be the following (DN means the domain number, TBS and EBS are time or event based sampling correspondingly):
```
Target device: Intel(R) Gen9
Metric Group 0: D3 / TBS / RenderBasic (Render Metrics Basic Gen9)
    Metric 0: : D3 / TBS / RenderBasic / GpuTime (Time elapsed on the GPU during the measurement.)
    Metric 1: : D3 / TBS / RenderBasic / GpuCoreClocks (The total number of GPU core clocks elapsed during the measurement.)
    Metric 2: : D3 / TBS / RenderBasic / AvgGpuCoreFrequencyMHz (Average GPU Core Frequency in the measurement.)
    Metric 3: : D3 / TBS / RenderBasic / VsThreads (The total number of vertex shader hardware threads dispatched.)
    ...
    Metric 55: : D3 / TBS / RenderBasic / ReportReason (The reason of the report.)
    Metric 56: : D3 / TBS / RenderBasic / ContextId (The context tag in which report has been taken.)
Metric Group 1: D3 / EBS / RenderBasic (Render Metrics Basic Gen9)
    Metric 0: : D3 / EBS / RenderBasic / GpuTime (Time elapsed on the GPU during the measurement.)
    Metric 1: : D3 / EBS / RenderBasic / GpuCoreClocks (The total number of GPU core clocks elapsed during the measurement.)
    Metric 2: : D3 / EBS / RenderBasic / AvgGpuCoreFrequencyMHz (Average GPU Core Frequency in the measurement.)
    Metric 3: : D3 / EBS / RenderBasic / VsThreads (The total number of vertex shader hardware threads dispatched.)
...
```
## Supported OS
- Linux
- Windows (*under development*)

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
cd <pti>/samples/ze_metric_info
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./ze_metric_info
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./ze_metric_info
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_metric_info
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the application:
```sh
ze_metric_info.exe
```
Since Intel(R) Metrics Discovery Application Programming Interface and Metrics Library for Metrics Discovery API (Metrics Library for MD API) are loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
ze_metric_info.exe
```