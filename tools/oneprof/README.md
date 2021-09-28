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
--output [-o] <filename>         Print console logs into the file
--device-list                    Print list of available devices
--metric-list                    Print list of available metrics
--version                        Print version
```

**Raw Metrics** mode allows to grab all the HW metric reports while application execution and dump them in CSV format for further investigation (doesn't depend on compute runtime), e.g.:
```
== Raw Metrics ==

SubDeviceId,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,682666,759081,1111,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,768,78019584000,1149,1149,1,1048575,69165123,
0,682666,783253,1147,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,384,78020266666,1149,1149,1,1048575,69165123,
0,682666,783231,1147,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,256,78020949333,1149,1149,1,1048575,69165123,
0,682666,783242,1147,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,256,78021632000,1149,1149,1,1048575,69165123,
0,671166,770086,1147,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,256,78022303166,1149,1149,8,32,69165123,
0,11500,13167,1144,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9408,704,78022314666,1149,1149,1,32,69165123,
0,682666,783231,1147,99.2111,0,0,0,0,0,16720,8.7586,88.6453,2.93694,6.15843,4.01323,1.40595,1.95425,89.3107,0,0,0,0,0,0,0,0,0,0,0,555467,0,35549888,0,0,0,27271424,8444928,9089280,8205056,78022997333,1149,1149,1,32,69165123,
0,682666,783230,1147,100,0,0,0,0,0,336,72.8647,27.1342,35.0761,51.0768,52.328,1.51334,11.8151,99.9455,0,0,0,0,0,0,0,0,0,0,0,2220109,0,142086976,0,0,0,142055680,43008,6853120,228096,78023680000,1149,1149,1,32,69165123,
...
```

**Kernel Intervals** mode dumps execution intervals for all of the kernels and transfers on the device to be able to correlate raw metrics with kernels, e.g.:
```
== Raw Kernel Intervals (Level Zero) ==

SubDeviceId,Name,Start,End,
0,zeCommandListAppendMemoryCopy[4194304 bytes],333082304833,333082625166,
0,zeCommandListAppendMemoryCopy[4194304 bytes],333082625333,333082914166,
0,zeCommandListAppendBarrier,333082914333,333082915999,
0,GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],333082916166,333125617666,
0,zeCommandListAppendBarrier,333125617833,333125618999,
0,zeCommandListAppendMemoryCopy[4194304 bytes],333125619166,333126081832,
...
```

**Kernel Metrics** mode automatically correlates metric reports to kernels giving an ability to track metrics bevahiour for a kernel over time:
```
== Kernel Metrics (OpenCL) ==

Kernel,clEnqueueWriteBuffer[4194304 bytes],
SubDeviceId,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,374750,364111,971,93.864,0,0,0,0,0,6280,2.71498,90.43,0.0452701,1.86351,0.0731233,1.02394,0.860377,85.2366,0,0,0,0,0,0,0,0,0,0,0,145957,0,9341248,0,0,0,6262528,3028480,3227200,2642816,259520606666,1149,1149,8,32,0,

Kernel,GEMM[SIMD32, {1024, 1024, 1}, {0, 0, 0}],
SubDeviceId,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,246666,248846,1008,100,0,0,0,0,0,2248,29.8595,62.0995,12.2179,19.2157,19.4196,1.46249,6.01644,88.0508,0,0,0,0,0,0,0,0,0,0,0,382460,0,24477440,0,0,0,23375744,1162752,2120704,1556480,259520853333,1149,1149,1,32,0,
0,682666,783231,1147,100,0,0,0,0,0,336,64.3564,35.6436,22.5158,39.9458,41.0742,1.38486,11.9982,99.9483,0,0,0,0,0,0,0,0,0,0,0,2254528,0,144289792,0,0,0,144233472,43008,6960768,43264,259521536000,1149,1149,1,32,0,
0,682666,759046,1111,100,0,0,0,0,0,672,64.7454,35.2543,18.8366,39.1944,38.9525,1.31759,11.5678,99.8654,0,0,0,0,0,0,0,0,0,0,0,2105639,0,134760896,0,0,0,134665472,86016,6498496,86784,259522218666,1149,1149,1,32,0,
0,682666,783230,1147,100,0,0,0,0,0,672,65.9991,34.0009,18.9462,39.8569,39.5415,1.31341,11.7531,99.871,0,0,0,0,0,0,0,0,0,0,0,2207611,0,141287104,0,0,0,141199616,86016,6814400,43264,259522901333,1149,1149,1,32,0,
...
```

**Aggregation** mode provides a single metric report for a kernel to see cumulative results:
```
== Aggregated Metrics (Level Zero) ==

Kernel,zeCommandListAppendMemoryCopy[4194304 bytes],
SubDeviceId,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,105416,103059,977,80.2298,0,0,0,0,0,1821,2.8716,62.56,0.0550251,1.97421,0.135966,1.02677,0.785917,58.3937,0,0,0,0,0,0,0,0,0,0,0,38684,0,2475776,0,0,0,1713920,776704,900288,370112,344182442666,1099,1099,1,32,428071356,

Kernel,GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],
SubDeviceId,GpuTime,GpuCoreClocks,AvgGpuCoreFrequencyMHz,GpuBusy,VsThreads,HsThreads,DsThreads,GsThreads,PsThreads,CsThreads,EuActive,EuStall,EuFpuBothActive,Fpu0Active,Fpu1Active,EuAvgIpcRate,EuSendActive,EuThreadOccupancy,RasterizedPixels,HiDepthTestFails,EarlyDepthTestFails,SamplesKilledInPs,PixelsFailingPostPsTests,SamplesWritten,SamplesBlended,SamplerTexels,SamplerTexelMisses,SlmBytesRead,SlmBytesWritten,ShaderMemoryAccesses,ShaderAtomics,L3ShaderThroughput,ShaderBarriers,TypedBytesRead,TypedBytesWritten,UntypedBytesRead,UntypedBytesWritten,GtiReadThroughput,GtiWriteThroughput,QueryBeginTime,CoreFrequencyMHz,EuSliceFrequencyMHz,ReportReason,ContextId,StreamMarker,
0,43690626,49697972,1137,100,0,0,0,0,0,47331,73.4137,26.532,28.8329,49.2144,48.9965,1.41675,11.2316,99.6994,0,0,0,0,0,0,0,0,0,0,0,134052413,0,8579354432,0,0,0,8568335616,11876864,420841984,12214336,344183127166,1149,1149,1,32,428071356,

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