# Level Zero Debug Info
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect oneAPI Level Zero kernel sources, binaries and symbols to correlate them. This tool works for Intel(R) Processor Graphics only.

As a result, listing like the following will be printed for each kernel (once per kernel creation):
```
=== Kernel: GEMM ===
                [0x010] (W)      or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   0x4C0:uw         {Switch}
                [0x020] (W)      mul (1|M0)               r6.0<1>:d     r9.3<0;1,0>:d     r5.6<0;1,0>:d
                [0x030] (W)      mul (1|M0)               r126.0<1>:d   r9.2<0;1,0>:d     r5.1<0;1,0>:d
                [0x060]          add (16|M0)              r120.0<1>:d   r6.0<0;1,0>:d     r3.0<16;16,1>:uw
                [0x070]          add (16|M16)             r14.0<1>:d    r6.0<0;1,0>:d     r4.0<16;16,1>:uw
                [0x080]          add (16|M0)              r10.0<1>:d    r126.0<0;1,0>:d   r1.0<16;16,1>:uw
                [0x090]          add (16|M16)             r124.0<1>:d   r126.0<0;1,0>:d   r2.0<16;16,1>:uw
                [0x0A0]          add (16|M0)              r120.0<1>:d   r120.0<8;8,1>:d   r7.1<0;1,0>:d    {Compacted}
                [0x0A8]          add (16|M16)             r14.0<1>:d    r14.0<8;8,1>:d    r7.1<0;1,0>:d
                [0x0B8]          add (16|M0)              r12.0<1>:d    r10.0<8;8,1>:d    r7.0<0;1,0>:d    {Compacted}
                [0x0C0]          add (16|M16)             r122.0<1>:d   r124.0<8;8,1>:d   r7.0<0;1,0>:d
                [0x0D0]          mul (16|M0)              r118.0<1>:d   r120.0<8;8,1>:d   r8.6<0;1,0>:d    {Compacted}
                [0x0D8]          mul (16|M16)             r16.0<1>:d    r14.0<8;8,1>:d    r8.6<0;1,0>:d
[  1] __kernel void GEMM(__global float* a, __global float* b,
                [0x000] (W)      mov (8|M0)               r5.0<1>:ud    r0.0<1;1,0>:ud
                [0x2B0] (W)      mov (8|M0)               r127.0<1>:ud  r5.0<8;8,1>:ud                   {Compacted}
                [0x310] (W)      send (8|M0)              null     r127    0x27        0x2000010  {EOT} //  wr:1+?, rd:0,  end of thread
                [0x320]          illegal
                [0x330]          illegal
                [0x340]          illegal
                [0x350]          illegal
                [0x360]          illegal
                [0x370]          illegal
                [0x380]          illegal
                [0x390]          illegal
                [0x3A0]          illegal
[  2]                     __global float* c, int size) {
[  3]   int j = get_global_id(0);
[  4]   int i = get_global_id(1);
[  5]   float sum = 0.0f;
[  6]   for (int k = 0; k < size; ++k) {
                [0x040] (W)      cmp (16|M0)   (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
                [0x050] (W)      cmp (16|M16)  (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
                [0x0E8] (W&f1.0) jmpi                                 L296
                [0x0F8]          mov (16|M0)              r116.0<1>:f   0x0:f
                [0x108]          mov (16|M16)             r18.0<1>:f    0x0:f
                [0x118] (W)      jmpi                                 L664
                [0x128]          mov (16|M0)              r116.0<1>:f   0x0:f
                [0x138]          mov (16|M16)             r18.0<1>:f    0x0:f
                [0x148] (W)      mov (1|M0)               r126.1<1>:d   0:w
                [0x180] (W)      add (1|M0)               r126.1<1>:d   r126.1<0;1,0>:d   1:w
                [0x228] (W)      cmp (16|M0)   (lt)f0.0   null<1>:d     r126.1<0;1,0>:d   r8.6<0;1,0>:d
                [0x268] (W)      cmp (16|M16)  (lt)f0.0   null<1>:d     r126.1<0;1,0>:d   r8.6<0;1,0>:d
                [0x288] (W&f0.0) jmpi                                 L344
[  7]     sum += a[i * size + k] * b[k * size + j];
                [0x158] (W)      mul (1|M0)               r126.2<1>:d   r126.1<0;1,0>:d   r8.6<0;1,0>:d
                [0x168]          add (16|M0)              r20.0<1>:d    r118.0<8;8,1>:d   r126.1<0;1,0>:d  {Compacted}
                [0x170]          add (16|M16)             r114.0<1>:d   r16.0<8;8,1>:d    r126.1<0;1,0>:d
                [0x190]          add (16|M0)              r26.0<1>:d    r126.2<0;1,0>:d   r12.0<8;8,1>:d   {Compacted}
                [0x198]          add (16|M16)             r112.0<1>:d   r126.2<0;1,0>:d   r122.0<8;8,1>:d
                [0x1A8]          shl (16|M0)              r20.0<1>:d    r20.0<8;8,1>:d    2:w
                [0x1B8]          shl (16|M16)             r114.0<1>:d   r114.0<8;8,1>:d   2:w
                [0x1C8]          shl (16|M0)              r26.0<1>:d    r26.0<8;8,1>:d    2:w
                [0x1D8]          shl (16|M16)             r112.0<1>:d   r112.0<8;8,1>:d   2:w
                [0x1E8]          add (16|M0)              r20.0<1>:d    r20.0<8;8,1>:d    r8.7<0;1,0>:d    {Compacted}
                [0x1F0]          add (16|M16)             r114.0<1>:d   r114.0<8;8,1>:d   r8.7<0;1,0>:d
                [0x200]          add (16|M0)              r26.0<1>:d    r26.0<8;8,1>:d    r9.0<0;1,0>:d    {Compacted}
                [0x208]          add (16|M16)             r112.0<1>:d   r112.0<8;8,1>:d   r9.0<0;1,0>:d
                [0x218]          send (16|M0)             r22:w    r20     0xC         0x4205E00  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 0
                [0x238]          send (16|M16)            r24:w    r114    0xC         0x4205E00  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 0
                [0x248]          send (16|M0)             r109:w   r26     0xC         0x4205E01  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 1
                [0x258]          send (16|M16)            r107:w   r112    0xC         0x4205E01  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 1
                [0x278]          mad (16|M0)              r116.0<1>:f   r116.0<2;1>:f     r22.0<2;1>:f      r109.0<1>:f      {Compacted}
                [0x280]          mad (16|M16)             r18.0<1>:f    r18.0<2;1>:f      r24.0<2;1>:f      r107.0<1>:f      {Compacted}
[  8]   }
[  9]   c[i * size + j] = sum;
                [0x298]          add (16|M0)              r28.0<1>:d    r118.0<8;8,1>:d   r12.0<8;8,1>:d   {Compacted}
                [0x2A0]          add (16|M16)             r105.0<1>:d   r16.0<8;8,1>:d    r122.0<8;8,1>:d
                [0x2B8]          shl (16|M0)              r28.0<1>:d    r28.0<8;8,1>:d    2:w
                [0x2C8]          shl (16|M16)             r105.0<1>:d   r105.0<8;8,1>:d   2:w
                [0x2D8]          add (16|M0)              r28.0<1>:d    r28.0<8;8,1>:d    r9.1<0;1,0>:d    {Compacted}
                [0x2E0]          add (16|M16)             r105.0<1>:d   r105.0<8;8,1>:d   r9.1<0;1,0>:d
                [0x2F0]          sends (16|M0)            null:w   r28     r116    0x8C        0x4025E02  //  wr:2+2, rd:0, Untyped Surface Write msc:30, to bti 2
                [0x300]          sends (16|M16)           null:w   r105    r18     0x8C        0x4025E02  //  wr:2+2, rd:0, Untyped Surface Write msc:30, to bti 2
[ 10] }
```
**Note:** to collect debug information for DPC++ application one need to compile it with `-gline-tables-only` flag.

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/ze_debug_info
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./ze_debug_info <target_application>
```
One may use [ze_gemm](../ze_gemm) as target application:
```sh
./ze_debug_info ../../ze_gemm/build/ze_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_debug_info
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib;<iga_lib_path> -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the tool:
```sh
ze_debug_info.exe <target_application>
```
One may use [ze_gemm](../ze_gemm) as target application:
```sh
ze_debug_info.exe ..\..\ze_gemm\build\ze_gemm.exe
```
**Note**: to build this sample one may need to generate *.lib file from IGA *.dll (see [here](https://stackoverflow.com/questions/9946322/how-to-generate-an-import-library-lib-file-from-a-dll) for details) and provide the path to this *.lib to cmake with `-DCMAKE_LIBRARY_PATH`.

Also one may need to add an actual path to IGA *.dll into PATH before sample run, e.g.:
```
set PATH=%PATH%;<iga_dll_path>
ze_debug_info.exe ..\..\ze_gemm\build\ze_gemm.exe
```