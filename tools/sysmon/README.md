# System Monitoring Utility
## Overview
This utility provides basic static and dynamic information about the GPU, including the list of processes that uses this GPU at the moment - similar to Linux `top`.

The following options are supported:
```
Usage: ./sysmon [options]
Options:
--processes [-p]    Print short device information and running processes (default)
--list [-l]         Print list of devices and subdevices
--details [-d]      Print detailed information for all of the devices and subdevices
--help [-h]         Print help message
--version           Print version
```

**Processes** mode dumps short information about all the available GPUs and running processes (default mode):
```
=====================================================================================
GPU 0: Intel(R) Iris(R) Plus Graphics 655 [0x3ea5]    PCI Bus: 0000:00:02.0
Vendor: Intel(R) Corporation    Driver Version: 1.1.20043    Subdevices: 0
EU Count: 48    Threads Per EU: 7    EU SIMD Width: 8    Total Memory(MB): 25431.6
Core Frequency(MHz): 300.0 of 1200.0    Core Temperature(C): 36
=====================================================================================
Running Processes: 2
     PID,  Device Memory Used(MB),  Shared Memory Used(MB),  GPU Engines, Executable
   22246,                  770.23,                     0.0,        OTHER, ./ze_gemm
   22258,                     0.1,                     0.0,        OTHER, ./sysmon
```
*Note*, that the list of processes may be unavailable for integrated graphics. Core temperature is available only for *root* user.

**Device List** mode prints the list of devices and subdevices on the system:
```
Driver #0: API Version 1.1 (latest)
-- Device #0: Intel(R) Iris(R) Plus Graphics 655 [0x3ea5]
```

**Details** mode dumps all available information for all devices and subdevices:
```
=====================================================================================
GPU 0
=====================================================================================
Name,                                   Intel(R) Iris(R) Plus Graphics 655 [0x3ea5]
Number Of Slices,                       2
Number Of Subslices Per Slice,          3
Number Of EU Per Subslice,              8
Number Of Threads Per EU,               7
Total EU Count,                         48
Physical EU SIMD Width,                 8
Kernel Timestamp Valid Bits,            32
Max Command Queue Priority,             0
Timer Resolution(clks),                 12000000
Timestamp Valid Bits,                   36
Max Hardware Contexts,                  65536
Max Memory Allocation Size(MB),         4096.0
Brand Number,                           unknown
Brand Name,                             Intel(R) Corporation
Kernel Driver Version,                  C46E946D4B88EAAF868F519
Serial Number,                          unknown
Model Name,                             Intel(R) Iris(R) Plus Graphics 655
Vendor,                                 Intel(R) Corporation
Subdevices,                             0
PCI Bus,                                0000:00:20.0
PCI Generation,                         unknown
PCI Max Brandwidth(GB/s),               unknown
PCI Width,                              unknown
Level Zero GPU Driver Version,          1.1.20043
Current Frequency(MHz),                 300
Changeable Frequency,                   Yes
Max Core Frequency(MHz),                1200
Min Core Frequency(MHz),                300
Current Voltage(V),                     unknown
Current Frequency Request(MHz),         unknown
Efficient Min Frequency(MHz),           300.0
Max Frequency For Current TDP(MHz),     unknown
Memory Name,                            DDR
Memory Max Clock Rate(MHz),             0
Memory Max Bus Width,                   64
Memory Total Size(MB),                  25431.6
Engines,                                RENDER_SINGLE(1);MEDIA_DECODE_SINGLE(2);COPY_SINGLE(1);MEDIA_ENHANCEMENT_SINGLE(1)
```

## Supported OS
- Linux

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/tools/sysmon
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./sysmon
```
To run this utility in `top` like mode, one can use the following command:
```sh
watch -n 1 ./sysmon # Call sysmon every 1 second
```