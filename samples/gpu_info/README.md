# GPU Info
## Overview
This sample application queries the information about the Intel(R) Processor Graphics device, that includes general GPU properties and hardware metrics, available to collect through [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery).

This application could be used as the test to check if Intel(R) Metrics Discovery Application Programming Interface library is available on the system. Expected output should be the following:
* In case of retrieving device information (`-d`):
    ```
    Device Information:
        Device Name: Intel(R) HD Graphics 630 (Kaby Lake GT2)
        EuCoresTotalCount: 24
        EuCoresPerSubsliceCount: 8
        EuSubslicesTotalCount: 3
        EuSubslicesPerSliceCount: 3
        EuSlicesTotalCount: 1
        EuThreadsCount: 7
        SubsliceMask: 7
        SliceMask: 1
        SamplersTotalCount: 3
        GpuMinFrequencyMHz: 350
        GpuMaxFrequencyMHz: 1150
        GpuCurrentFrequencyMHz: 350
        PciDeviceId: 22802
        SkuRevisionId: 4
        PlatformIndex: 12
        ApertureSize: 0
        NumberOfRenderOutputUnits: 4
        NumberOfShadingUnits: 28
        OABufferMinSize: 16777216
        OABufferMaxSize: 16777216
        GpuTimestampFrequency: 12000000
        MaxTimestamp: 357913941250
    ```
* In case of retrieving metrics information (`-m`):
    ```
    Available Metrics:
        Metric Group 0: OcclusionQueryStats
            Metric Set 0: RenderedPixelsStats (Rendered Pixels Statistics)
                Metric 0: OcclusionQueryStats / RenderedPixelsStats / PixelsRendered (Depth passed pixels)
            Metric Set 1: RenderedFragmentsStats (Rendered Fragments Statistics)
                Metric 0: OcclusionQueryStats / RenderedFragmentsStats / PixelsRendered (Depth passed fragments)
        ...
        Metric Group 3: OA
            Metric Set 0: RenderBasic (Render Metrics Basic Gen9)
                Metric 0: OA / RenderBasic / GpuTime (GPU Time Elapsed)
                Metric 1: OA / RenderBasic / GpuCoreClocks (GPU Core Clocks)
                Metric 2: OA / RenderBasic / AvgGpuCoreFrequencyMHz (AVG GPU Core Frequency)
                ...
                Metric 51: OA / RenderBasic / SamplerBottleneck (Samplers Bottleneck)
                Info 52: OA / RenderBasic / QueryBeginTime (Query Begin Time)
                Info 53: OA / RenderBasic / CoreFrequencyMHz (GPU Core Frequency)
                Info 54: OA / RenderBasic / EuSliceFrequencyMHz (EU Slice Frequency)
                Info 55: OA / RenderBasic / ReportReason (Report Reason)
                Info 56: OA / RenderBasic / ContextId (Context ID)
                Info 57: OA / RenderBasic / CoreFrequencyChanged (GPU Core Frequency Changed)
                Info 58: OA / RenderBasic / QuerySplitOccurred (Query Split Occurred)
                Info 59: OA / RenderBasic / ReportId (Query report id)
                Info 60: OA / RenderBasic / ReportsCount (Query reports count)
                Info 61: OA / RenderBasic / OverrunOccured (Query Overrun Occurred)
            Metric Set 1: ComputeBasic (Compute Metrics Basic Gen9)
                Metric 0: OA / ComputeBasic / GpuTime (GPU Time Elapsed)
                ...
                Info 48: OA / ComputeBasic / OverrunOccured (Query Overrun Occurred)
    ...
    ```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/gpu_info
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./gpu_info [-d|-m]
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly, e.g.:
```sh
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib ./gpu_info
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\gpu_info
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```
Use this command line to run the application:
```sh
gpu_info.exe [-d|-m]
```
Since Intel(R) Metrics Discovery Application Programming Interface library is loaded at runtime, one may need to set its path explicitly (see the output of cmake), e.g.:
```sh
set PATH=%PATH%;C:\Windows\system32\DriverStore\FileRepository\igdlh64.inf_amd64_d59561bc9241aaf5
gpu_info.exe
```