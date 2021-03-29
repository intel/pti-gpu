# GPU Instruction Count
## Overview
This sample is a simple LD_PRELOAD based tool that allows to collect dynamic execution count for every OpenCL(TM) kernel instruction.

As a result, assembly listing annotated with dynamic instruction count for each kernel will be printed.
```
=== GEMM (runs 4 times) ===
[     32768] 0x0000: (W)      mov (8|M0)               r5.0<1>:ud    r0.0<1;1,0>:ud
[     32768] 0x0010: (W)      or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   0x4C0:uw         {Switch}
[     32768] 0x0020: (W)      mul (1|M0)               r6.0<1>:d     r9.3<0;1,0>:d     r5.6<0;1,0>:d
[     32768] 0x0030: (W)      mul (1|M0)               r6.1<1>:d     r9.2<0;1,0>:d     r5.1<0;1,0>:d
[     32768] 0x0040: (W)      cmp (16|M0)   (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
[     32768] 0x0050: (W)      cmp (16|M16)  (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w
[     32768] 0x0060:          add (16|M0)              r10.0<1>:d    r6.0<0;1,0>:d     r3.0<16;16,1>:uw
[     32768] 0x0070:          add (16|M16)             r3.0<1>:d     r6.0<0;1,0>:d     r4.0<16;16,1>:uw
[     32768] 0x0080:          add (16|M0)              r14.0<1>:d    r6.1<0;1,0>:d     r1.0<16;16,1>:uw
[     32768] 0x0090:          add (16|M16)             r12.0<1>:d    r6.1<0;1,0>:d     r2.0<16;16,1>:uw
[     32768] 0x00A0:          add (16|M0)              r10.0<1>:d    r10.0<8;8,1>:d    r7.1<0;1,0>:d    {Compacted}
[     32768] 0x00A8:          add (16|M16)             r3.0<1>:d     r3.0<8;8,1>:d     r7.1<0;1,0>:d   
[     32768] 0x00B8:          add (16|M0)              r26.0<1>:d    r14.0<8;8,1>:d    r7.0<0;1,0>:d    {Compacted}
[     32768] 0x00C0:          add (16|M16)             r16.0<1>:d    r12.0<8;8,1>:d    r7.0<0;1,0>:d   
[     32768] 0x00D0:          mul (16|M0)              r20.0<1>:d    r10.0<8;8,1>:d    r8.6<0;1,0>:d    {Compacted}
[     32768] 0x00D8:          mul (16|M16)             r14.0<1>:d    r3.0<8;8,1>:d     r8.6<0;1,0>:d
[     32768] 0x00E8: (W&f1.0) jmpi                                 L296
[         0] 0x00F8:          mov (16|M0)              r116.0<1>:f   0x0:f
[         0] 0x0108:          mov (16|M16)             r18.0<1>:f    0x0:f
[         0] 0x0118: (W)      jmpi                                 L656
[     32768] 0x0128:          mov (16|M0)              r116.0<1>:f   0x0:f
[     32768] 0x0138:          mov (16|M16)             r18.0<1>:f    0x0:f
[     32768] 0x0148: (W)      mov (1|M0)               r4.0<1>:d     0:w
[  33554432] 0x0158: (W)      mul (1|M0)               r4.1<1>:d     r4.0<0;1,0>:d     r8.6<0;1,0>:d
[  33554432] 0x0168:          add (16|M0)              r10.0<1>:d    r20.0<8;8,1>:d    r4.0<0;1,0>:d    {Compacted}
[  33554432] 0x0170:          add (16|M16)             r2.0<1>:d     r14.0<8;8,1>:d    r4.0<0;1,0>:d
[  33554432] 0x0180: (W)      add (1|M0)               r4.0<1>:d     r4.0<0;1,0>:d     1:w
[  33554432] 0x0190:          add (16|M0)              r12.0<1>:d    r4.1<0;1,0>:d     r26.0<8;8,1>:d   {Compacted}
[  33554432] 0x0198:          add (16|M16)             r6.0<1>:d     r4.1<0;1,0>:d     r16.0<8;8,1>:d
[  33554432] 0x01A8:          shl (16|M0)              r10.0<1>:d    r10.0<8;8,1>:d    2:w
[  33554432] 0x01B8:          shl (16|M16)             r2.0<1>:d     r2.0<8;8,1>:d     2:w
[  33554432] 0x01C8:          shl (16|M0)              r12.0<1>:d    r12.0<8;8,1>:d    2:w
[  33554432] 0x01D8:          shl (16|M16)             r6.0<1>:d     r6.0<8;8,1>:d     2:w
[  33554432] 0x01E8:          add (16|M0)              r10.0<1>:d    r10.0<8;8,1>:d    r8.7<0;1,0>:d    {Compacted}
[  33554432] 0x01F0:          add (16|M16)             r2.0<1>:d     r2.0<8;8,1>:d     r8.7<0;1,0>:d
[  33554432] 0x0200:          add (16|M0)              r12.0<1>:d    r12.0<8;8,1>:d    r9.0<0;1,0>:d    {Compacted}
[  33554432] 0x0208:          add (16|M16)             r6.0<1>:d     r6.0<8;8,1>:d     r9.0<0;1,0>:d
[  33554432] 0x0218:          send (16|M0)             r22:w    r10     0xC         0x4205E00  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 0
[  33554432] 0x0228: (W)      cmp (16|M0)   (lt)f0.0   null<1>:d     r4.0<0;1,0>:d     r8.6<0;1,0>:d    {Compacted}
[  33554432] 0x0230:          send (16|M16)            r24:w    r2      0xC         0x4205E00  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 0
[  33554432] 0x0240:          send (16|M0)             r109:w   r12     0xC         0x4205E01  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 1
[  33554432] 0x0250:          send (16|M16)            r107:w   r6      0xC         0x4205E01  //  wr:2+?, rd:2, Untyped Surface Read msc:30, to bti 1
[  33554432] 0x0260: (W)      cmp (16|M16)  (lt)f0.0   null<1>:d     r4.0<0;1,0>:d     r8.6<0;1,0>:d
[  33554432] 0x0270:          mad (16|M0)              r116.0<1>:f   r116.0<2;1>:f     r22.0<2;1>:f      r109.0<1>:f      {Compacted}
[  33554432] 0x0278:          mad (16|M16)             r18.0<1>:f    r18.0<2;1>:f      r24.0<2;1>:f      r107.0<1>:f      {Compacted}
[  33554432] 0x0280: (W&f0.0) jmpi                                 L344
[     32768] 0x0290:          add (16|M0)              r6.0<1>:d     r20.0<8;8,1>:d    r26.0<8;8,1>:d   {Compacted}
[     32768] 0x0298:          add (16|M16)             r2.0<1>:d     r14.0<8;8,1>:d    r16.0<8;8,1>:d
[     32768] 0x02A8: (W)      mov (8|M0)               r112.0<1>:ud  r5.0<8;8,1>:ud                   {Compacted}
[     32768] 0x02B0:          shl (16|M0)              r6.0<1>:d     r6.0<8;8,1>:d     2:w
[     32768] 0x02C0:          shl (16|M16)             r2.0<1>:d     r2.0<8;8,1>:d     2:w
[     32768] 0x02D0:          add (16|M0)              r6.0<1>:d     r6.0<8;8,1>:d     r9.1<0;1,0>:d    {Compacted}
[     32768] 0x02D8:          add (16|M16)             r2.0<1>:d     r2.0<8;8,1>:d     r9.1<0;1,0>:d
[     32768] 0x02E8:          sends (16|M0)            null:w   r6      r116    0x8C        0x4025E02  //  wr:2+2, rd:0, Untyped Surface Write msc:30, to bti 2
[     32768] 0x02F8:          sends (16|M16)           null:w   r2      r18     0x8C        0x4025E02  //  wr:2+2, rd:0, Untyped Surface Write msc:30, to bti 2
[     32768] 0x0308: (W)      send (8|M0)              null     r112    0x27        0x2000010  {EOT} //  wr:1+?, rd:0,  end of thread
[     32768] 0x0318:          illegal
[     32768] 0x0328:          illegal
[     32768] 0x0338:          illegal
[     32768] 0x0348:          illegal
[     32768] 0x0358:          illegal
[     32768] 0x0368:          illegal
[     32768] 0x0378:          illegal
[     32768] 0x0388:          illegal
[     32768] 0x0398:          illegal
[     32768] 0x03A8:          illegal
```
## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/gpu_inst_count
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release [-DGTPIN_PATH=<gtpin>/Profilers] ..
make
```
Use this command line to run the tool:
```sh
./gpu_inst_count <target_application>
```
One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
./gpu_inst_count ../../cl_gemm/build/cl_gemm
./gpu_inst_count ../../ze_gemm/build/ze_gemm
./gpu_inst_count ../../dpc_gemm/build/dpc_gemm
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\gpu_inst_count
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DGTPIN_PATH=<gtpin>\Profilers -DCMAKE_LIBRARY_PATH=<iga_lib_path> ..
nmake
```
Use this command line to run the tool:
```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
gpu_inst_count.exe <target_application>
```
One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:
```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
gpu_inst_count.exe ..\..\cl_gemm\build\cl_gemm.exe
gpu_inst_count.exe ..\..\ze_gemm\build\ze_gemm.exe
gpu_inst_count.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```
**Note**: to build this sample one may need to generate *.lib file from IGA *.dll (see [here](https://stackoverflow.com/questions/9946322/how-to-generate-an-import-library-lib-file-from-a-dll) for details) and provide the path to this *.lib to cmake with `-DCMAKE_LIBRARY_PATH`.

Also one may need to add an actual path to IGA *.dll into PATH before sample run, e.g.:
```
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
set PATH=%PATH%;<iga_dll_path>
gpu_inst_count.exe ..\..\cl_gemm\build\cl_gemm.exe
```
### Additional Notes
GEN12 is currently supported in an experimental mode. To enable it, use the environment variable `PTI_GEN12`:
```sh
PTI_GEN12=1 ./gpu_inst_count <target_application>
```