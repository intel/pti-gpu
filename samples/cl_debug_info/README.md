# OpenCL(TM) Debug Info
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect OpenCL kernel sources, binaries and symbols to correlate them. This tool works for Intel(R) Processor Graphics only.

As a result, listing like the following will be printed for each kernel (once per kernel creation).
```
======== gemm ========
[ 1] __kernel void gemm(__global float* a, __global float* b, __global float* c, int size) {
                [0x000] (W)      mov (8|M0)               r3.0<1>:ud    r0.0<1;1,0>:ud                  
                [0x010] (W)      or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   0x4C0:uw         {Switch}
[ 2]     int j = get_global_id(0);
[ 3]     int i = get_global_id(1);
[ 4]     float sum = 0.0f;
[ 5]     for (int k = 0; k < size; ++k) {
                [0x050] (W)      cmp (8|M0)    (gt)f0.0   null<1>:d     r5.6<0;1,0>:d     0:w             
                [0x060]          add (8|M0)               r124.0<1>:d   r6.5<0;1,0>:d     r124.0<8;8,1>:d  {Compacted}
                [0x068]          add (8|M0)               r7.0<1>:d     r126.0<0;1,0>:d   r1.0<8;8,1>:uw  
                [0x078]          add (8|M0)               r124.0<1>:d   r124.0<8;8,1>:d   r4.1<0;1,0>:d    {Compacted}
                [0x080]          add (8|M0)               r125.0<1>:d   r7.0<8;8,1>:d     r4.0<0;1,0>:d    {Compacted}
                [0x090] (W&f0.0) jmpi                                 L192                            
                [0x0A0]          mov (8|M0)               r123.0<1>:f   0x0:f                           
                [0x0B0] (W)      jmpi                                 L392                            
                [0x0C0] (W)      mov (1|M0)               r6.6<1>:d     0:w                             
                [0x0D0]          mov (8|M0)               r123.0<1>:f   0x0:f                           
                [0x0F8] (W)      add (1|M0)               r6.6<1>:d     r6.6<0;1,0>:d     1:w             
                [0x120] (W)      cmp (8|M0)    (lt)f0.1   null<1>:d     r6.6<0;1,0>:d     r5.6<0;1,0>:d   
                [0x178] (W&f0.1) jmpi                                 L224                            
[ 6]         sum += a[i * size + k] * b[k * size + j];
                [0x0E0] (W)      mul (1|M0)               r126.1<1>:d   r6.6<0;1,0>:d     r5.6<0;1,0>:d   
                [0x0F0]          add (8|M0)               r122.0<1>:d   r8.0<8;8,1>:d     r6.6<0;1,0>:d    {Compacted}
                [0x108]          add (8|M0)               r9.0<1>:d     r126.1<0;1,0>:d   r125.0<8;8,1>:d  {Compacted}
                [0x110]          shl (8|M0)               r122.0<1>:d   r122.0<8;8,1>:d   2:w             
                [0x130]          shl (8|M0)               r9.0<1>:d     r9.0<8;8,1>:d     2:w             
                [0x140]          add (8|M0)               r122.0<1>:d   r122.0<8;8,1>:d   r5.7<0;1,0>:d    {Compacted}
                [0x148]          add (8|M0)               r9.0<1>:d     r9.0<8;8,1>:d     r6.0<0;1,0>:d    {Compacted}
                [0x150]          send (8|M0)              r10:f    r122    0xC         0x2106E00  //  wr:1+?, rd:1, Untyped Surface Read msc:46, to bti 0
                [0x160]          send (8|M0)              r121:f   r9      0xC         0x2106E01  //  wr:1+?, rd:1, Untyped Surface Read msc:46, to bti 1
                [0x170]          mad (8|M0)               r123.0<1>:f   r123.0<2;1>:f     r10.0<2;1>:f      r121.0<1>:f      {Compacted}
[ 7]     }
[ 8]     c[i * size + j] = sum;
                [0x188]          add (8|M0)               r11.0<1>:d    r8.0<8;8,1>:d     r125.0<8;8,1>:d  {Compacted}
                [0x198]          shl (8|M0)               r11.0<1>:d    r11.0<8;8,1>:d    2:w             
                [0x1A8]          add (8|M0)               r11.0<1>:d    r11.0<8;8,1>:d    r6.1<0;1,0>:d    {Compacted}
                [0x1B0]          sends (8|M0)             null:ud  r11     r123    0x4C        0x2026E02  //  wr:1+1, rd:0, Untyped Surface Write msc:46, to bti 2
[ 9] }
                [0x190] (W)      mov (8|M0)               r127.0<1>:ud  r3.0<8;8,1>:ud                   {Compacted}
                [0x1C0] (W)      send (8|M0)              null     r127    0x27        0x2000010  {EOT} //  wr:1+?, rd:0,  end of thread
                [0x1D0]          illegal                
                [0x1E0]          illegal                
                [0x1F0]          illegal                
                [0x200]          illegal                
                [0x210]          illegal                
                [0x220]          illegal                
                [0x230]          illegal                
                [0x240]          illegal                
                [0x250]          illegal                
                [0x260]          illegal                
======================
```
## Supported OS
- Linux
- Windows

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