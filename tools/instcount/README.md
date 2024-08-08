# GPU Instruction Count

## Overview

This tool allows to collect dynamic instruction execution count and SIMD active count for every GPU kernel instruction.

As a result, assembly listing annotated with dynamic instruction count and SIMD active lanes for each kernel will be printed.

```console

[INFO] : [ Instruction count | SIMD active lanes count ] total for all invocations
=== GEMM(runs 4 times) ===
[   2048|   16384] 0x000000 : (W)     mov (8|M0)               r5.0<1>:ud    r0.0<1;1,0>:ud                  
[   2048|    2048] 0x000010 : (W)     or (1|M0)                cr0.0<1>:ud   cr0.0<0;1,0>:ud   0x4C0:uw              {@1}
[   2048|    2048] 0x000020 : (W)     mul (1|M0)               acc0.0<1>:d   r9.2<0;1,0>:d     r5.2<0;1,0>:uw   {@1}
[   2048|    2048] 0x000030 : (W)     mach (1|M0)              r6.0<1>:d     r9.2<0;1,0>:d     r5.1<0;1,0>:d   
[   2048|    2048] 0x000040 : (W)     mul (1|M0)               acc0.0<1>:d   r9.3<0;1,0>:d     r5.12<0;1,0>:uw 
[   2048|    2048] 0x000050 : (W)     mach (1|M0)              r8.0<1>:d     r9.3<0;1,0>:d     r5.6<0;1,0>:d   
[   2048|   32768] 0x000060 : (W)     cmp (16|M0)   (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w              
[   2048|   32768] 0x000070 :         add (16|M0)              r18.0<1>:d    r8.0<0;1,0>:d     r3.0<16;16,1>:uw {@2}
[   2048|   32768] 0x000080 :         add (16|M16)             r20.0<1>:d    r8.0<0;1,0>:d     r4.0<16;16,1>:uw
[   2048|   32768] 0x000090 :         add (16|M0)              r18.0<1>:d    r18.0<8;8,1>:d    r7.1<0;1,0>:d    {Compacted,@2}
[   2048|   32768] 0x000098 :         add (16|M16)             r20.0<1>:d    r20.0<8;8,1>:d    r7.1<0;1,0>:d    {Compacted,@2}
[   2048|   32768] 0x0000a0 : (W)     cmp (16|M16)  (gt)f1.0   null<1>:d     r8.6<0;1,0>:d     0:w              
[   2048|   16384] 0x0000b0 : (W)     mul (8|M0)               acc0.0<1>:d   r18.0<8;8,1>:d    r8.12<0;1,0>:uw  {Compacted,@3}
[   2048|   32768] 0x0000b8 :         add (16|M0)              r10.0<1>:d    r6.0<0;1,0>:d     r1.0<16;16,1>:uw {@7}
[   2048|   32768] 0x0000c8 :         add (16|M16)             r12.0<1>:d    r6.0<0;1,0>:d     r2.0<16;16,1>:uw
[   2048|   16384] 0x0000d8 :         mach (8|M0)              r22.0<1>:d    r18.0<8;8,1>:d    r8.6<0;1,0>:d    {Compacted}
[   2048|   16384] 0x0000e0 : (W)     mul (8|M8)               acc0.0<1>:d   r19.0<8;8,1>:d    r8.12<0;1,0>:uw 
[   2048|   16384] 0x0000f0 :         mach (8|M8)              r23.0<1>:d    r19.0<8;8,1>:d    r8.6<0;1,0>:d    {Compacted}
[   2048|   16384] 0x0000f8 : (W)     mul (8|M16)              acc0.0<1>:d   r20.0<8;8,1>:d    r8.12<0;1,0>:uw  {@7}
[   2048|   16384] 0x000108 :         mach (8|M16)             r24.0<1>:d    r20.0<8;8,1>:d    r8.6<0;1,0>:d    {Compacted}
[   2048|   16384] 0x000110 : (W)     mul (8|M24)              acc0.0<1>:d   r21.0<8;8,1>:d    r8.12<0;1,0>:uw 
[   2048|   32768] 0x000120 :         add (16|M0)              r14.0<1>:d    r10.0<8;8,1>:d    r7.0<0;1,0>:d    {Compacted,@7}
[   2048|   32768] 0x000128 :         add (16|M16)             r16.0<1>:d    r12.0<8;8,1>:d    r7.0<0;1,0>:d    {Compacted,@7}
[   2048|   16384] 0x000130 :         mach (8|M24)             r25.0<1>:d    r21.0<8;8,1>:d    r8.6<0;1,0>:d    {Compacted}
[   2048|    8192] 0x000138 : (W)     csel (4|M0)   (eq)f0.0   r1.0<1>:w     r1.0<4;1>:w       r1.0<4;1>:w       r1.0<1>:w       
[   2048|       0] 0x000148 : (~f1.0) if (32|M0)                           48                  368                
[      0|       0] 0x000158 :         mov (16|M0)              r26.0<1>:f    0x0:f                               {Compacted}
[      0|       0] 0x000160 :         mov (16|M16)             r28.0<1>:f    0x0:f                               {Compacted}
[      0|       0] 0x000168 :         else (32|M0)                         336                  336                
[   2048|   32768] 0x000178 :         mov (16|M0)              r26.0<1>:f    0x0:f                               {Compacted}
[   2048|   32768] 0x000180 :         mov (16|M16)             r28.0<1>:f    0x0:f                               {Compacted}
[   2048|    2048] 0x000188 : (W)     mov (1|M0)               r6.1<1>:f     0x0:f                               {Compacted}
[ 262144|  262144] 0x000190 : (W)     mul (1|M0)               acc0.0<1>:d   r6.1<0;1,0>:d     r8.12<0;1,0>:uw  {@1}
[ 262144|  262144] 0x0001a0 : (W)     mach (1|M0)              r34.0<1>:d    r6.1<0;1,0>:d     r8.6<0;1,0>:d   
[ 262144| 4194304] 0x0001b0 :         add (16|M0)              r30.0<1>:d    r22.0<8;8,1>:d    r6.1<0;1,0>:d    {Compacted}
[ 262144| 4194304] 0x0001b8 :         add (16|M16)             r32.0<1>:d    r24.0<8;8,1>:d    r6.1<0;1,0>:d    {Compacted}
[ 262144| 4194304] 0x0001c0 :         add (16|M0)              r39.0<1>:d    r34.0<0;1,0>:d    r14.0<8;8,1>:d   {Compacted,@3}
[ 262144| 4194304] 0x0001c8 :         add (16|M16)             r41.0<1>:d    r34.0<0;1,0>:d    r16.0<8;8,1>:d   {Compacted}
[ 262144| 4194304] 0x0001d0 :         shl (16|M0)              r30.0<1>:d    r30.0<8;8,1>:d    2:w               {Compacted,@4}
[ 262144| 4194304] 0x0001d8 :         shl (16|M16)             r32.0<1>:d    r32.0<8;8,1>:d    2:w               {Compacted,@4}
[ 262144| 4194304] 0x0001e0 :         shl (16|M0)              r39.0<1>:d    r39.0<8;8,1>:d    2:w               {Compacted,@4}
[ 262144| 4194304] 0x0001e8 :         shl (16|M16)             r41.0<1>:d    r41.0<8;8,1>:d    2:w               {Compacted,@4}
[ 262144| 4194304] 0x0001f0 :         add (16|M0)              r30.0<1>:d    r30.0<8;8,1>:d    r8.7<0;1,0>:d    {Compacted,@4}
[ 262144| 4194304] 0x0001f8 :         add (16|M16)             r32.0<1>:d    r32.0<8;8,1>:d    r8.7<0;1,0>:d    {Compacted,@4}
[ 262144| 4194304] 0x000200 :         add (16|M0)              r39.0<1>:d    r39.0<8;8,1>:d    r9.0<0;1,0>:d    {Compacted,@4}
[ 262144| 4194304] 0x000208 :         add (16|M16)             r41.0<1>:d    r41.0<8;8,1>:d    r9.0<0;1,0>:d    {Compacted,@4}
[ 262144| 4194304] 0x000210 :         send.dc1 (16|M0)         r35      r30     null    0x0            0x04205E00           {@4,$0} // wr:2+0, rd:2; untyped surface read with x
[ 262144| 4194304] 0x000220 :         send.dc1 (16|M16)        r37      r32     null    0x0            0x04205E00           {@3,$1} // wr:2+0, rd:2; untyped surface read with x
[ 262144| 4194304] 0x000230 :         send.dc1 (16|M0)         r44      r39     null    0x0            0x04205E01           {@2,$2} // wr:2+0, rd:2; untyped surface read with x
[ 262144| 4194304] 0x000240 :         send.dc1 (16|M16)        r46      r41     null    0x0            0x04205E01           {@1,$3} // wr:2+0, rd:2; untyped surface read with x
[ 262144|  262144] 0x000250 : (W)     add (1|M0)               r6.1<1>:d     r6.1<0;1,0>:d     1:w               {Compacted}
[ 262144| 4194304] 0x000258 : (W)     cmp (16|M0)   (lt)f0.0   null<1>:d     r6.1<0;1,0>:d     r8.6<0;1,0>:d    {@1}
[ 262144| 4194304] 0x000268 : (W)     cmp (16|M16)  (lt)f0.0   null<1>:d     r6.1<0;1,0>:d     r8.6<0;1,0>:d   
[ 262144|  262144] 0x000278 :         sync.nop                             null                             {Compacted,$2.dst}
[ 262144| 4194304] 0x000280 :         mad (16|M0)              r26.0<1>:f    r26.0<8;1>:f      r35.0<8;1>:f      r44.0<1>:f       {Compacted,$0.dst}
[ 262144|  262144] 0x000288 :         sync.nop                             null                             {Compacted,$3.dst}
[ 262144| 4194304] 0x000290 :         mad (16|M16)             r28.0<1>:f    r28.0<8;1>:f      r37.0<8;1>:f      r46.0<1>:f       {Compacted,$1.dst}
[ 262144| 1048576] 0x000298 : (W)     csel (4|M0)   (eq)f0.0   r1.0<1>:w     r1.0<4;1>:w       r1.0<4;1>:w       r1.0<1>:w       
[ 262144| 8323072] 0x0002a8 : (f0.0)  while (32|M0)                        -280                                
[   2048|   65536] 0x0002b8 :         endif (32|M0)                        16                                
[   2048|   32768] 0x0002c8 :         add (16|M0)              r48.0<1>:d    r22.0<8;8,1>:d    r14.0<8;8,1>:d   {Compacted}
[   2048|   32768] 0x0002d0 :         add (16|M16)             r50.0<1>:d    r24.0<8;8,1>:d    r16.0<8;8,1>:d   {Compacted}
[   2048|   32768] 0x0002d8 :         shl (16|M0)              r48.0<1>:d    r48.0<8;8,1>:d    2:w               {Compacted,@2}
[   2048|   32768] 0x0002e0 :         shl (16|M16)             r50.0<1>:d    r50.0<8;8,1>:d    2:w               {Compacted,@2}
[   2048|   32768] 0x0002e8 :         add (16|M0)              r48.0<1>:d    r48.0<8;8,1>:d    r9.1<0;1,0>:d    {Compacted,@2}
[   2048|   32768] 0x0002f0 :         add (16|M16)             r50.0<1>:d    r50.0<8;8,1>:d    r9.1<0;1,0>:d    {Compacted,@2}
[   2048|   32768] 0x0002f8 :         send.dc1 (16|M0)         null     r48     r26     0x80            0x04025E02           {@2,$4} // wr:2+2, rd:0; untyped surface write with x
[   2048|   32768] 0x000308 :         send.dc1 (16|M16)        null     r50     r28     0x80            0x04025E02           {@1,$5} // wr:2+2, rd:0; untyped surface write with x
[   2048|   16384] 0x000318 : (W)     mov (8|M0)               r127.0<1>:f   r5.0<8;8,1>:f                    {Compacted}
[   2048|   16384] 0x000320 : (W)     send.dc0 (8|M0)          r43      r5      null    0x0            0x0219E000           {$6} // wr:1h+0, rd:1; synchronized global fence flushing
[   2048|   16384] 0x000330 : (W)     mov (8|M0)               null<1>:ud    r43.0<8;8,1>:ud                  {$6.dst}
[   2048|   32768] 0x000340 : (W)     mov (16|M0)              acc0.0<1>:f   0x0:f                              
[   2048|    2048] 0x000350 : (W)     send.ts (1|M0)           null     r127    null    0x0            0x02000010           {EOT,@1} // wr:1+0, rd:0; end of thread
[      0|       0] 0x000360 :         nop                    
```

## Supported OS

- Ubuntu 22.04 (last check: GTPin v4.0, driver 1.3.27912)
- Windows 11 (last check: GTPin v4.0, driver 31.0.101.4887)

## Prerequisites

- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html)

## Build and Run

### Linux

If required, use "-DGTPIN_PATH=<gtpin>/Profilers" cmake config option to point on specific GTPin directory. Or use "-DGTPIN_LINK=" to specify link to GTPin package.

Run the following commands to build the sample:

```sh
cd <pti>/tools/instcount
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release [-DGTPIN_PATH=<gtpin>/Profilers] ..
make
```

Use this command line to run the tool:

```sh
./instcount <target_application>
```

One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:

```sh
./instcount ../../cl_gemm/build/cl_gemm
./instcount ../../ze_gemm/build/ze_gemm
./instcount ../../dpc_gemm/build/dpc_gemm
```

### Windows

Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:

```sh
cd <pti>\tools\instcount
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```

Use this command line to run the tool:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
instcount.exe <target_application>
```

CMake unpacks GTPin into "_deps\gtpin_package-src\Profilers":

```sh
set PATH=%PATH%;_deps\gtpin_package-src\Profilers\Lib\intel64
instcount.exe <target_application>
```

One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
instcount.exe ..\..\cl_gemm\build\cl_gemm.exe
instcount.exe ..\..\ze_gemm\build\ze_gemm.exe
instcount.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```

**Note**: to build this sample one may need to generate \*.lib file from IGA \*.dll (see [here](https://stackoverflow.com/questions/9946322/how-to-generate-an-import-library-lib-file-from-a-dll) for details) and provide the path to this \*.lib to cmake with `-DCMAKE_LIBRARY_PATH`.

Also one may need to add an actual path to IGA *.dll into PATH before sample run, e.g.:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
set PATH=%PATH%;<iga_dll_path>
instcount.exe ..\..\cl_gemm\build\cl_gemm.exe
```

The options can be one or more of the following:
```console
--disable-simd                 Disable SIMD active lanes collection
--json-output                  Print results in JSON format
--version                      Print version
```
