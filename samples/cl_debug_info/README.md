# OpenCL(TM) Debug Info
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect OpenCL kernel sources, binaries and symbols to correlate them. This tool works for Intel(R) Processor Graphics only.

As a result, listing like the following will be printed for each kernel (once per kernel creation).
```
===== Kernel: GEMM =====
=== File: Unknown ===
                [0x00370]         illegal
=== File: Kernel Source ===
                [0x000C8]         mul (16|M0)              r118.0<1>:d   r120.0<8;8,1>:d   r8.6<0;1,0>:d    {Compacted}
                [0x000D0]         mul (16|M16)             r16.0<1>:d    r14.0<8;8,1>:d    r8.6<0;1,0>:d
[    1] __kernel void GEMM(__global float* a, __global float* b,
                [0x00000] (W)     mov (8|M0)               r5.0<1>:ud    r0.0<1;1,0>:ud
                [0x00010] (W)     or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   0x4C0:uw              {Switch}
                [0x00020] (W)     mul (1|M0)               r6.0<1>:d     r9.1<0;1,0>:d     r5.6<0;1,0>:d
                [0x00030] (W)     mul (1|M0)               r126.0<1>:d   r9.0<0;1,0>:d     r5.1<0;1,0>:d    {Compacted}
[    2]                    __global float* c, unsigned size) {
[    3]   int j = get_global_id(0);
[    4]   int i = get_global_id(1);
[    5]   float sum = 0.0f;
[    6]   for (unsigned k = 0; k < size; ++k) {
                [0x00038] (W)     cmp (16|M0)   (eq)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
                [0x00048] (W)     cmp (16|M16)  (eq)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
                [0x00058]         add (16|M0)              r120.0<1>:d   r6.0<0;1,0>:d     r3.0<16;16,1>:uw
                [0x00068]         add (16|M16)             r14.0<1>:d    r6.0<0;1,0>:d     r4.0<16;16,1>:uw
                [0x00078]         add (16|M0)              r10.0<1>:d    r126.0<0;1,0>:d   r1.0<16;16,1>:uw
                [0x00088]         add (16|M16)             r124.0<1>:d   r126.0<0;1,0>:d   r2.0<16;16,1>:uw
                [0x00098]         add (16|M0)              r120.0<1>:d   r120.0<8;8,1>:d   r7.1<0;1,0>:d    {Compacted}
                [0x000A0]         add (16|M16)             r14.0<1>:d    r14.0<8;8,1>:d    r7.1<0;1,0>:d
                [0x000B0]         add (16|M0)              r12.0<1>:d    r10.0<8;8,1>:d    r7.0<0;1,0>:d    {Compacted}
                [0x000B8]         add (16|M16)             r122.0<1>:d   r124.0<8;8,1>:d   r7.0<0;1,0>:d
                [0x000E0] (W&~f1.0) jmpi                               L288
                [0x000F0]         mov (16|M0)              r116.0<1>:f   0.0:f
                [0x00100]         mov (16|M16)             r18.0<1>:f    0.0:f
                [0x00110] (W)     jmpi                                 L608
                [0x00120]         mov (16|M0)              r116.0<1>:f   0.0:f
                [0x00130]         mov (16|M16)             r18.0<1>:f    0.0:f
                [0x00140] (W)     mov (1|M0)               r126.1<1>:d   0:w
                [0x00178] (W)     add (1|M0)               r126.1<1>:d   r126.1<0;1,0>:d   1:w
                [0x001F0] (W)     cmp (16|M0)   (lt)f0.0   null<1>:d     r126.1<0;1,0>:ud  r8.6<0;1,0>:ud
                [0x00230] (W)     cmp (16|M16)  (lt)f0.0   null<1>:d     r126.1<0;1,0>:ud  r8.6<0;1,0>:ud
                [0x00250] (W&f0.0) jmpi                                L336
[    7]     sum += a[i * size + k] * b[k * size + j];
                [0x00150] (W)     mul (1|M0)               r126.2<1>:d   r126.1<0;1,0>:d   r8.6<0;1,0>:d
                [0x00160]         add (16|M0)              r20.0<1>:d    r118.0<8;8,1>:d   r126.1<0;1,0>:d  {Compacted}
                [0x00168]         add (16|M16)             r114.0<1>:d   r16.0<8;8,1>:d    r126.1<0;1,0>:d
                [0x00188]         add (16|M0)              r26.0<1>:d    r126.2<0;1,0>:d   r12.0<8;8,1>:d   {Compacted}
                [0x00190]         add (16|M16)             r112.0<1>:d   r126.2<0;1,0>:d   r122.0<8;8,1>:d
                [0x001A0]         shl (16|M0)              r20.0<1>:d    r20.0<8;8,1>:d    2:w
                [0x001B0]         shl (16|M16)             r114.0<1>:d   r114.0<8;8,1>:d   2:w
                [0x001C0]         shl (16|M0)              r26.0<1>:d    r26.0<8;8,1>:d    2:w
                [0x001D0]         shl (16|M16)             r112.0<1>:d   r112.0<8;8,1>:d   2:w
                [0x001E0]         send (16|M0)             r22:w    r20     0xC            0x04205E00           // wr:2+0, rd:2; hdc.dc1; untyped surface read with x
                [0x00200]         send (16|M16)            r24:w    r114    0xC            0x04205E00           // wr:2+0, rd:2; hdc.dc1; untyped surface read with x
                [0x00210]         send (16|M0)             r109:w   r26     0xC            0x04205E01           // wr:2+0, rd:2; hdc.dc1; untyped surface read with x
                [0x00220]         send (16|M16)            r107:w   r112    0xC            0x04205E01           // wr:2+0, rd:2; hdc.dc1; untyped surface read with x
                [0x00240]         mad (16|M0)              r116.0<1>:f   r116.0<2;1>:f     r22.0<2;1>:f      r109.0<1>:f      {Compacted}
                [0x00248]         mad (16|M16)             r18.0<1>:f    r18.0<2;1>:f      r24.0<2;1>:f      r107.0<1>:f      {Compacted}
[    8]   }
[    9]   c[i * size + j] = sum;
                [0x00260]         add (16|M0)              r28.0<1>:d    r118.0<8;8,1>:d   r12.0<8;8,1>:d   {Compacted}
                [0x00268]         add (16|M16)             r105.0<1>:d   r16.0<8;8,1>:d    r122.0<8;8,1>:d
                [0x00280]         shl (16|M0)              r28.0<1>:d    r28.0<8;8,1>:d    2:w
                [0x00290]         shl (16|M16)             r105.0<1>:d   r105.0<8;8,1>:d   2:w
                [0x002A0]         sends (16|M0)            null:w   r28     r116    0x8C            0x04025E02           // wr:2+2, rd:0; hdc.dc1; untyped surface write with x
                [0x002B0]         sends (16|M16)           null:w   r105    r18     0x8C            0x04025E02           // wr:2+2, rd:0; hdc.dc1; untyped surface write with x
[   10] }
                [0x00278] (W)     mov (8|M0)               r127.0<1>:ud  r5.0<8;8,1>:ud                   {Compacted}
                [0x002C0] (W)     send (8|M0)              null     r127    0x27            0x02000010           {EOT} // wr:1+0, rd:0; spawner; end of thread
                [0x002D0]         illegal
                [0x002E0]         illegal
                [0x002F0]         illegal
                [0x00300]         illegal
                [0x00310]         illegal
                [0x00320]         illegal
                [0x00330]         illegal
                [0x00340]         illegal
                [0x00350]         illegal
                [0x00360]         illegal
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [OpenCL(TM) ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/cl_debug_info
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the tool:
```sh
./cl_debug_info <target_application>
```
One may use [cl_gemm](../cl_gemm) as target application:
```sh
./cl_debug_info ../../cl_gemm/build/cl_gemm gpu
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\cl_debug_info
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<opencl_icd_lib_path>;<iga_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
cl_debug_info.exe <target_application>
```
One may use [cl_gemm](../cl_gemm) as target application:
```sh
cl_debug_info.exe ..\..\cl_gemm\build\cl_gemm.exe
```
**Note**: to build this sample one may need to generate *.lib file from IGA *.dll (see [here](https://stackoverflow.com/questions/9946322/how-to-generate-an-import-library-lib-file-from-a-dll) for details) and provide the path to this *.lib to cmake with `-DCMAKE_LIBRARY_PATH`.

Also one may need to add an actual path to IGA *.dll into PATH before sample run, e.g.:
```
set PATH=%PATH%;<iga_dll_path>
cl_debug_info.exe ..\..\cl_gemm\build\cl_gemm.exe
```