# System Monitoring Utility
## Overview
This utility provides basic static and dynamic information about the GPU, including the list of processes that uses this GPU at the moment - similar to Linux `top`.

**Note**, that the list of processes may be unavailable for integrated graphics. Core temperature is available only for *root* user.
```
====================================================================================================
GPU 0: Intel(R) Iris(R) Plus Graphics 655 [0x3ea5]    PCI bus: 0000:00:02.0
Vendor: Intel(R) Corporation    Driver version: A7F72C54A5788663395C411    Subdevices: 0
EU Count: 48    Threads per EU: 7    EU SIMD Width: 8    Total Memory(MB): 25432.418
Core Frequency(MHz): 300 of 1200    Core Temperature(C): 36
====================================================================================================
Running Processes: 2
     PID,  Device Memory Used(MB),  Shared Memory Used(MB),  GPU Engines, Executable
   22246,                 770.250,                   0.000,        OTHER, ./ze_gemm
   22258,                   0.125,                   0.000,        OTHER, ./sysmon
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