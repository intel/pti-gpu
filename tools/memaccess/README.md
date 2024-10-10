# GPU Memory Access Analysis

## Overview

This tool collects information about memory accesses using binary instrumentation. It provides next information: 
 * Access pattern (stride distribution)
 * Cache lines transferred between cache and compute units
 * Instruction count, SIMD lanes count for memory accesses
 * Percent cache line not aligned accesses
 * Addresses sample

As a result, list of memory access instructions with collected metrics will be printed for each kernel.
```console
=== GEMM (runs 2 times)
--------------------------------------------------------------------------------
0x000260 :         send.ugm (32|M0)         r32      r27     null:0  0x0            0x08200580           {A@1,$5} // wr:4+0, rd:2; load.ugm.d32.a64
 (no source info)
  * SIMD32 ExecSize_32 4 bytes X1 Scatter A64 Read
  * Instruction executed: 131072
  * SIMD lanes executed: 4194304
  * Cache line transferred: 262144 ( 16777216 bytes)
      100 % used/transferred ratio
  * Cache line not aligned: 0 % (0)
  * Stride distribution:
      100% (4063232) -> stride: 4 bytes (1 units)
  * Access addresses sample (SIMD32):
      Addr#  0   0xff007ffffffbfe00 0xff007ffffffbfe04 0xff007ffffffbfe08 0xff007ffffffbfe0c
      Addr#  4   0xff007ffffffbfe10 0xff007ffffffbfe14 0xff007ffffffbfe18 0xff007ffffffbfe1c
      Addr#  8   0xff007ffffffbfe20 0xff007ffffffbfe24 0xff007ffffffbfe28 0xff007ffffffbfe2c
      Addr# 12   0xff007ffffffbfe30 0xff007ffffffbfe34 0xff007ffffffbfe38 0xff007ffffffbfe3c
      Addr# 16   0xff007ffffffbfe40 0xff007ffffffbfe44 0xff007ffffffbfe48 0xff007ffffffbfe4c
      Addr# 20   0xff007ffffffbfe50 0xff007ffffffbfe54 0xff007ffffffbfe58 0xff007ffffffbfe5c
      Addr# 24   0xff007ffffffbfe60 0xff007ffffffbfe64 0xff007ffffffbfe68 0xff007ffffffbfe6c
      Addr# 28   0xff007ffffffbfe70 0xff007ffffffbfe74 0xff007ffffffbfe78 0xff007ffffffbfe7c

<...>
```

The following capabilities are available:
```console
--json-output                  Print results in JSON format
--kernel-run                   Kernel run to profile
--stride-min                   Minimal detected stride (bytes)
--stride-num                   Number of collected strides (buckets)
--stride-step                  Stride step (bytes)
--version                      Print version
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

Run the following commands to build the sample:

```sh
cd <pti>/samples/memaccess
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release [-DGTPIN_PATH=<gtpin>/Profilers] ..
make
```

Use this command line to run the tool:

```sh
./memaccess <target_application>
```

One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:

```sh
./memaccess ../../cl_gemm/build/cl_gemm
./memaccess ../../ze_gemm/build/ze_gemm
./memaccess ../../dpc_gemm/build/dpc_gemm
```

### Windows

Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:

```sh
cd <pti>\samples\memaccess
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```

Use this command line to run the tool:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
memaccess.exe <target_application>
```

CMake unpacks GTPin into "_deps\gtpin_package-src\Profilers":

```sh
set PATH=%PATH%;_deps\gtpin_package-src\Profilers\Lib\intel64
memaccess.exe <target_application>
```

One may use [cl_gemm](../cl_gemm), [ze_gemm](../ze_gemm) or [dpc_gemm](../dpc_gemm) as target application:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
memaccess.exe ..\..\cl_gemm\build\cl_gemm.exe
memaccess.exe ..\..\ze_gemm\build\ze_gemm.exe
memaccess.exe ..\..\dpc_gemm\build\dpc_gemm.exe
```

**Note**: to build this sample one may need to generate \*.lib file from IGA \*.dll (see [here](https://stackoverflow.com/questions/9946322/how-to-generate-an-import-library-lib-file-from-a-dll) for details) and provide the path to this \*.lib to cmake with `-DCMAKE_LIBRARY_PATH`.

Also one may need to add an actual path to IGA *.dll into PATH before sample run, e.g.:

```sh
set PATH=%PATH%;<gtpin>\Profilers\Lib\intel64
set PATH=%PATH%;<iga_dll_path>
memaccess.exe ..\..\cl_gemm\build\cl_gemm.exe
```
