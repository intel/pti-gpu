# GPU Metrics Collection Tool for Data Parallel C++ (DPC++)
## Overview
This tool provides GPU hardware metrics collection capabilities for the compute applications based on Intel runtimes for OpenCL(TM) and Level Zero, like DPC++, Intel(R) Implicit SPMD Program Compiler (Intel(R) ISPC) and OpenMP* GPU offload programs.

The following capabilities are available:
```
Usage: ./oneprof[.exe] [options] <application> <args>
Options:
--raw-metrics [-m]               Collect raw metric stream for the device
--kernel-intrevals [-i]          Collect raw kernel intervals for the device
--kernel-metrics [-k]            Collect metrics for each kernel
--aggregation [-a]               Aggregate metrics for each kernel
--device [-d] <ID>               Target device for profiling (default is 0)
--group [-g] <NAME>              Target metric group to collect (default is ComputeBasic)
--sampling-interval [-s] <VALUE> Sampling interval for metrics collection in us (default is 1000 us)
--output [-o] <FILENAME>         Print console logs into the file
--raw-data-path [-p] <DIRECTORY> Path to store raw metic data into (default is process folder)
--device-list                    Print list of available devices
--metric-list                    Print list of available metrics
--version                        Print version
```

**Raw Metrics** mode allows to grab all the HW metric reports while application execution and dump them in CSV format for further investigation (doesn't depend on compute runtime), e.g.:
```
== Raw Metrics ==

SubDeviceId,HostTimestamp,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,343304199,682666,753618,1103,100,0,0,0,0,0,336,56.1465,43.8535,22.9954,36.4886,36.9501,1.45587,10.8754,99.9425,0,0,0,0,0,0,0,0,0,0,0,1966165,0,125834560,0,0,0,125785344,43008,6073216,44032,238066688000,1149,1149,1,32,1179654540,
0,343986865,682666,773375,1132,100,0,0,0,0,0,672,67.1219,32.8779,18.8542,40.5393,39.8573,1.30636,11.9009,99.8918,0,0,0,0,0,0,0,0,0,0,0,2207249,0,141263936,0,0,0,141191680,86016,6814912,43520,238067370666,1149,1149,1,32,1179654540,
0,344669532,682666,783253,1147,100,0,0,0,0,0,336,67.4071,32.5929,19.2494,40.8147,40.1636,1.31184,11.9925,99.9399,0,0,0,0,0,0,0,0,0,0,0,2253502,0,144224128,0,0,0,144171136,43008,6954560,86272,238068053333,1149,1149,1,32,1179654540,
0,345352199,682666,773433,1132,100,0,0,0,0,0,672,66.4875,33.5125,18.8932,40.1962,39.5619,1.31041,11.8066,99.8886,0,0,0,0,0,0,0,0,0,0,0,2189916,0,140154624,0,0,0,140070272,86016,6761088,86528,238068736000,1149,1149,1,32,1179654540,
0,346034865,682666,773375,1132,100,0,0,0,0,0,336,66.2652,33.7344,17.3913,39.4322,38.6375,1.28662,11.561,99.9126,0,0,0,0,0,0,0,0,0,0,0,2144985,0,137279040,0,0,0,137233664,43008,6620416,43520,238069418666,1149,1149,1,32,1179654540,
...
```

**Kernel Intervals** mode dumps execution intervals for all of the kernels and transfers on the device to be able to correlate raw metrics with kernels, e.g.:
```
== Raw Kernel Intervals (Level Zero) ==

Kernel,zeCommandListAppendMemoryCopy(M2D)[4194304 bytes],
SubDeviceId,Start,End,
0,106673485,106963818,

Kernel,zeCommandListAppendBarrier,
SubDeviceId,Start,End,
0,106963985,106965318,

Kernel,GEMM[SIMD32, {4; 1024; 1} {256; 1; 1}],
SubDeviceId,Start,End,
0,106965485,149496985,
...
```

**Kernel Metrics** mode automatically correlates metric reports to kernels giving an ability to track metrics bevahiour for a kernel over time:
```
== Kernel Metrics (OpenCL) ==

Kernel,clEnqueueWriteBufferclEnqueueWriteBuffer[4194304 bytes],
SubDeviceId,HostTimestamp,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,342796169,248333,260664,1049,89.4032,0,0,0,0,0,5000,2.97376,84.5846,0.0751366,2.04254,0.111094,1.03615,0.946629,79.7974,0,0,0,0,0,0,0,0,0,0,0,115437,0,7387968,0,0,0,4883072,2340224,2575616,2002560,29069653333,1149,1149,1,32,1179654540,

Kernel,GEMM[SIMD32, {1024, 1024, 1}, {0, 0, 0}],
SubDeviceId,HostTimestamp,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,344161502,590666,653470,1106,95.8942,0,0,0,0,0,672,52.4014,42.434,23.2914,34.7129,35.3936,1.49752,10.3669,94.677,0,0,0,0,0,0,0,0,0,0,0,1624753,0,103984192,0,0,0,103949056,43008,5054976,20736,29071018666,1149,1149,1,32,1179654540,
0,344844169,682666,783253,1147,100,0,0,0,0,0,336,69.2735,30.7265,19.4396,41.9169,41.3087,1.30476,12.3252,99.9455,0,0,0,0,0,0,0,0,0,0,0,2316056,0,148227584,0,0,0,148178304,43008,7149248,67328,29071701333,1149,1149,1,32,1179654540,
...
```

**Aggregation** mode provides a single metric report for a kernel to see cumulative results:
```
== Aggregated Metrics (Level Zero) ==

Kernel,zeCommandListAppendMemoryCopy(M2D)[4194304 bytes],
SubDeviceId,HostTimestamp,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,162137469,581750,565065,971,96.6676,0,0,0,0,0,10243,3.12565,88.8954,0.0217637,2.09571,0.0898717,1.01006,0.91929,82.8795,0,0,0,0,0,0,0,0,0,0,0,240969,0,15422016,0,0,0,9946880,4878464,5353472,4689728,70536192000,1149,1149,1,32,1179654540,

Kernel,GEMM[SIMD32, {4; 1024; 1} {256; 1; 1}],
SubDeviceId,HostTimestamp,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,162820135,43007958,49035563,1139,100,0,0,0,0,0,38909,74.4577,25.5021,29.1877,49.928,49.687,1.41558,11.3856,99.6933,0,0,0,0,0,0,0,0,0,0,0,133982149,0,8574857536,0,0,0,8571003008,7448448,417155712,7897472,70536874666,1149,1149,1,32,1179654540,

...
```

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)
    - one may need to install `libdrm-dev` package to build the library from sources
    - one may need to allow metrics collection for non-root users:
        ```sh
        sudo echo 0 > /proc/sys/dev/i915/perf_stream_paranoid
        ```
- [Metrics Library for Metrics Discovery API (Metrics Library for MD API)](https://github.com/intel/metrics-library)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/oneprof
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./oneprof [options] <target_application>
```
One may use e.g. [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
./oneprof -a -s 100 -g ComputeExtended ../../../samples/dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\tools\oneprof
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
oneprof.exe [options] <target_application>
```
One may use e.g. [dpc_gemm](../../samples/dpc_gemm) as target application, e.g.:
```sh
oneprof.exe a -s 100 -g ComputeExtended ..\..\..\samples\dpc_gemm\build\dpc_gemm.exe
```