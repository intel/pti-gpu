# GPU Metrics Collection Tool for Data Parallel C++ (DPC++)
## Overview
This tool provides GPU hardware metrics collection capabilities for the compute applications based on Intel runtimes for OpenCL(TM) and Level Zero, like DPC++, Intel(R) Implicit SPMD Program Compiler (Intel(R) ISPC) and OpenMP* GPU offload programs.

The following capabilities are available:
```
Usage: ./oneprof[.exe] [options] <application> <args>
Options:
--raw-metrics [-m]               Collect raw metric stream for the device
--kernel-intervals [-i]          Collect raw kernel intervals for the device
--kernel-metrics [-k]            Collect over-time metrics for each kernel instance
--aggregation [-a]               Collect aggregated metrics for each kernel instance in time-based mode
--kernel-query [-q]              Collect aggregated metrics for each kernel instance in query-based mode
--device [-d] <ID>               Target device for profiling (default is 0)
--group [-g] <NAME>              Target metric group to collect (default is ComputeBasic)
--sampling-interval [-s] <VALUE> Sampling interval for metrics collection in us (default is 1000 us)
--output [-o] <FILENAME>         Print console logs into the file
--raw-data-path [-p] <DIRECTORY> Path to store raw metic data into (default is process folder)
--finalize [-f] <FILENAME>       Print output from collected result file
--no-finalize                    Do not finalize and do not report collection results
--device-list                    Print list of available devices
--metric-list                    Print list of available metrics
--version                        Print version
```

**Raw Metrics** mode allows to grab all the HW metric reports while application execution and dump them in CSV format for further investigation (doesn't depend on compute runtime), e.g.:
```
== Raw Metrics ==

SubDeviceId,GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],EuSliceFrequencyMHz[MHz],ReportReason,ContextId,StreamMarker,
0,665083,631139,948,96.7231,0,0,0,0,0,4768,48.1844,38.0917,28.6887,37.3213,37.2607,1.62512,8.75791,82.9885,0,0,0,0,0,0,0,0,0,0,0,1372563,0,87844032,0,0,0,85624064,2114560,6231488,2143424,66413568000,1149,1149,1,32,1179654540,
0,682666,783219,1147,100,0,0,0,0,0,1344,81.9616,18.0384,38.7397,58.4375,58.7402,1.49389,13.3814,99.8482,0,0,0,0,0,0,0,0,0,0,0,2511983,0,160766912,0,0,0,160627072,172032,3802560,129280,66414250666,1149,1149,1,32,1179654540,
0,682666,783253,1147,100,0,0,0,0,0,1035,79.8366,20.1634,36.3135,56.0759,56.3012,1.47741,12.8364,99.8778,0,0,0,0,0,0,0,0,0,0,0,2410378,0,154264192,0,0,0,154127104,134528,3567168,172096,66414933333,1149,1149,1,32,1179654540,
0,682666,783254,1147,100,0,0,0,0,0,1317,78.7735,21.2265,35.3096,55.0285,55.194,1.47134,12.5856,99.8278,0,0,0,0,0,0,0,0,0,0,0,2362583,0,151205312,0,0,0,151031168,166528,3526336,129472,66415616000,1149,1149,1,32,1179654540,
0,682666,773283,1132,100,0,0,0,0,0,1008,77.721,22.279,34.449,54.0922,54.2257,1.46635,12.3713,99.876,0,0,0,0,0,0,0,0,0,0,0,2293428,0,146779392,0,0,0,146652416,129024,3415872,172480,66416298666,1149,1149,1,32,1179654540,
```

**Kernel Intervals** mode dumps execution intervals for all of the kernels and transfers on the device to be able to correlate raw metrics with kernels, e.g.:
```
== Raw Kernel Intervals ==

Kernel,SubDeviceId,Time[ns],Start[ns],End[ns],
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,4875166,66413228499,66418103665,

Kernel,SubDeviceId,Time[ns],Start[ns],End[ns],
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,4914833,66419571333,66424486166,

Kernel,SubDeviceId,Time[ns],Start[ns],End[ns],
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,4894833,66425926333,66430821166,
```

**Kernel Metrics** mode automatically correlates metric reports to kernels giving an ability to track metrics bevahiour for a kernel over time:
```
== Kernel Metrics ==

Kernel,SubDeviceId,GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],EuSliceFrequencyMHz[MHz],ReportReason,ContextId,StreamMarker,
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,636000,729679,1147,97.3827,0,0,0,0,0,5104,56.9211,34.0079,32.6102,43.4698,43.4986,1.59991,10.139,88.376,0,0,0,0,0,0,0,0,0,0,0,1820732,0,116526848,0,0,0,114358784,2182144,6848832,2142592,66432682666,1149,1149,1,32,1179654540,
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,682666,783242,1147,100,0,0,0,0,0,1344,79.9666,20.0334,36.5073,56.2568,56.4681,1.47899,12.8727,99.8256,0,0,0,0,0,0,0,0,0,0,0,2416416,0,154650624,0,0,0,154475648,172032,3639680,172224,66433365333,1149,1149,1,32,1179654540,
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,682666,783254,1147,100,0,0,0,0,0,1008,78.7907,21.2093,35.5036,55.0709,55.3467,1.47392,12.6116,99.8635,0,0,0,0,0,0,0,0,0,0,0,2368212,0,151565568,0,0,0,151426304,129024,3530112,129344,66434048000,1149,1149,1,32,1179654540,
GEMM[SIMD32 {2; 512; 1} {256; 1; 1}],0,682666,773363,1132,100,0,0,0,0,0,1008,77.7469,22.2531,35.1834,54.3703,54.6638,1.47641,12.4527,99.8311,0,0,0,0,0,0,0,0,0,0,0,2308793,0,147762752,0,0,0,147626496,129024,3429376,172544,66434730666,1149,1149,1,32,1179654540,
```

**Aggregation** mode provides a single metric report for a kernel to see cumulative results. These are results of automatic aggregation for the metrics, collected in time-based mode:
```
== Aggregated Kernel Metrics ==

Kernel,SubDeviceId,KernelTime[ns],GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],EuSliceFrequencyMHz[MHz],ReportReason,ContextId,StreamMarker,
GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],0,42523000,43007960,49099595,1141,100,0,0,0,0,0,43326,74.529,25.3546,29.2023,50.0025,49.7394,1.41551,11.4035,99.3923,0,0,0,0,0,0,0,0,0,0,0,134419343,0,8602837952,0,0,0,8593193984,9804288,426876416,10557504,357781162666,1149,1149,1,32,40,

Kernel,SubDeviceId,KernelTime[ns],GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],EuSliceFrequencyMHz[MHz],ReportReason,ContextId,StreamMarker,
GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],0,42675500,42325292,48219155,1139,100,0,0,0,0,0,32592,75.3883,24.5697,29.646,50.6168,50.3917,1.41555,11.5397,99.8522,0,0,0,0,0,0,0,0,0,0,0,133463067,0,8541636288,0,0,0,8537506816,4128768,411957312,4760000,357828949333,1149,1149,1,32,40,
```

**Kernel Query** mode also provides a single metric report for a kernel instance, but it is based on query mode:
```
== Query Kernel Metrics ==

Kernel,SubDeviceId,GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],CoreFrequencyChanged,QuerySplitOccurred,ReportId,ReportsCount,OverrunOccured,ReportError,ReportInconsistent,ReportCtxSwitchLost,ReportWithoutWorkload,
GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],0,42345936,48278766,1140,100,0,0,0,0,0,32768,75.488,24.4516,30.0049,50.8641,50.6392,1.41966,11.5963,99.5197,0,0,0,0,0,0,0,0,0,0,0,134283264,0,8594128896,0,0,0,8610906880,4204544,415981504,4992896,214977194916,1150,0,0,1,1,0,0,0,8,0,

Kernel,SubDeviceId,GpuTime[ns],GpuCoreClocks[cycles],AvgGpuCoreFrequencyMHz[MHz],GpuBusy[%],VsThreads[threads],HsThreads[threads],DsThreads[threads],GsThreads[threads],PsThreads[threads],CsThreads[threads],EuActive[%],EuStall[%],EuFpuBothActive[%],Fpu0Active[%],Fpu1Active[%],EuAvgIpcRate[number],EuSendActive[%],EuThreadOccupancy[%],RasterizedPixels[pixels],HiDepthTestFails[pixels],EarlyDepthTestFails[pixels],SamplesKilledInPs[pixels],PixelsFailingPostPsTests[pixels],SamplesWritten[pixels],SamplesBlended[pixels],SamplerTexels[texels],SamplerTexelMisses[texels],SlmBytesRead[bytes],SlmBytesWritten[bytes],ShaderMemoryAccesses[messages],ShaderAtomics[messages],L3ShaderThroughput[bytes],ShaderBarriers[messages],TypedBytesRead[bytes],TypedBytesWritten[bytes],UntypedBytesRead[bytes],UntypedBytesWritten[bytes],GtiReadThroughput[bytes],GtiWriteThroughput[bytes],QueryBeginTime[ns],CoreFrequencyMHz[MHz],CoreFrequencyChanged,QuerySplitOccurred,ReportId,ReportsCount,OverrunOccured,ReportError,ReportInconsistent,ReportCtxSwitchLost,ReportWithoutWorkload,
GEMM[SIMD32 {4; 1024; 1} {256; 1; 1}],0,42713211,48514430,1135,100,0,0,0,0,0,32768,75.1907,24.7165,29.808,50.615,50.3952,1.41864,11.54,99.5191,0,0,0,0,0,0,0,0,0,0,0,134283264,0,8594128896,0,0,0,8601469696,4199936,416482240,4996864,215024800416,1150,0,0,1,1,0,0,0,8,0,
```

***Note, that metric names provided by the tool may be not obvious.*** To learn what each metric exactly means, use `--metric-list` option. E.g. `Throughput` metrics may be represented in bytes, and to calculate actual throughput  one should devide it to `GpuTime`:
```
Group 1: ComputeBasic (Compute Metrics Basic set)
    Metric 0: GpuTime[ns] (Time elapsed on the GPU during the measurement.) [UINT64, DURATION, ComputeBasic]
    ...
    Metric 37: GtiReadThroughput[bytes] (The total number of GPU memory bytes read from GTI.) [UINT64, THROUGHPUT, ComputeBasic]
    Metric 38: GtiWriteThroughput[bytes] (The total number of GPU memory bytes written to GTI.) [UINT64, THROUGHPUT, ComputeBasic]
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
