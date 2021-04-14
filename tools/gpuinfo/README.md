# GPU Information
## Overview
This sample application queries the information about the Intel(R) Processor Graphics device, that includes general GPU properties and hardware metrics, available to collect through [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery).

This application could be used as the test to check if Intel(R) Metrics Discovery Application Programming Interface library is available on the system.
The following options are supported:
```
Usage: ./gpuinfo[.exe] <options>
Options:
-l        Print list of available devices
-i        Print general device info
-m        Print list of available mertrics
-h        Print this help message
```

Expected output should be the following:
* In case of printing the list of available GPU devices (`-l`):
    ```
    Device 0: Intel(R) Iris(R) Plus Graphics 655 (CFL GT3)
    ```
* In case of retrieving device information (`-i`) the following output is expected for all of the devices and subdevices:
    ```
    Device 0:
    ---- Name: Intel(R) Iris(R) Plus Graphics 655 (CFL GT3)
    ---- EuCoresTotalCount: 48
    ---- EuCoresPerSubsliceCount: 8
    ---- EuSubslicesTotalCount: 6
    ---- EuSubslicesPerSliceCount: 3
    ---- EuSlicesTotalCount: 2
    ---- EuThreadsCount: 7
    ---- SliceMask: 3
    ---- SubsliceMask: 63
    ---- SamplersTotalCount: 6
    ---- GpuMinFrequencyMHz: 300
    ---- GpuMaxFrequencyMHz: 1200
    ---- GpuCurrentFrequencyMHz: 300
    ---- PciDeviceId: 16037
    ---- SkuRevisionId: 1
    ---- PlatformIndex: 18
    ---- ApertureSize: 0
    ---- NumberOfRenderOutputUnits: 8
    ---- NumberOfShadingUnits: 28
    ---- OABufferMinSize: 16777216
    ---- OABufferMaxSize: 16777216
    ---- GpuTimestampFrequency: 12000000
    ---- MaxTimestamp: 357913941250
    ```
* In case of retrieving metrics information (`-m`) the following output is expected for all of the devices and subdevices:
    ```
    Device 0:
    ---- Metric Group 0: OcclusionQueryStats
    ------ Metric Set 0: RenderedPixelsStats (Rendered Pixels Statistics)
    -------- Metric 0: OcclusionQueryStats / RenderedPixelsStats / PixelsRendered (Depth passed pixels) [UINT64]
    ------ Metric Set 1: RenderedFragmentsStats (Rendered Fragments Statistics)
    -------- Metric 0: OcclusionQueryStats / RenderedFragmentsStats / PixelsRendered (Depth passed fragments) [UINT64]
    ...
    ---- Metric Group 2: PipelineStatistics
    ------ Metric Set 0: PipelineStats (Pipeline Statistics for OGL4)
    -------- Metric 0: PipelineStatistics / PipelineStats / IAVertices (Input vertices) [UINT64]
    -------- Metric 1: PipelineStatistics / PipelineStats / IAPrimitives (Input primitives) [UINT64]
    ...
    ---- Metric Group 3: OA
    ------ Metric Set 0: RenderBasic (Render Metrics Basic Gen9)
    -------- Metric 0: OA / RenderBasic / GpuTime (GPU Time Elapsed) [UINT64]
    -------- Metric 1: OA / RenderBasic / GpuCoreClocks (GPU Core Clocks) [UINT64]
    -------- Metric 2: OA / RenderBasic / AvgGpuCoreFrequencyMHz (AVG GPU Core Frequency) [UINT64]
    ...
    -------- Info 62: OA / RenderBasic / StreamMarker (Stream marker)
    ...
    ```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/gpuinfo
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./gpuinfo <options>
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./gpuinfo
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\tools\gpuinfo
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```
Use this command line to run the application:
```sh
gpuinfo.exe <options>
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
gpuinfo.exe
```