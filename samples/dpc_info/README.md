# DPC++ Info
## Overview
This sample application provides information on all platforms and devices available in DPC++ (similar to `clinfo` tool).

The following modes are implemented:
* Detailed information on each platform and device (`-a`):
    ```
        Number of  platforms                            5
        Platform Name                                   Intel(R) OpenCL
        Platform Vendor                                 Intel(R) Corporation
        Platform Profile                                FULL_PROFILE
        Platform Extensions                             cl_khr_icd cl_khr_global_int32_base_atomics cl_khr_global_int32_extended_atomics cl_khr_local_int32_base_atomics cl_khr_local_int32_extended_atomics ...
    ...
        Platform Name                                   Intel(R) OpenCL HD Graphics
        Platform Vendor                                 Intel(R) Corporation
        Platform Profile                                FULL_PROFILE
        Platform Extensions                             cl_khr_byte_addressable_store cl_khr_fp16 cl_khr_global_int32_base_atomics cl_khr_global_int32_extended_atomics cl_khr_icd cl_khr_local_int32_base_atomics ...
    ...

        Platform Name                                   Intel(R) OpenCL
    Number of devices                                   1
        Device Name                                     Intel(R) Core(TM) i7-6700K CPU @ 4.00GHz
        Device Vendor                                   Intel(R) Corporation
        Device vendor ID                                0x8086
        Device Version                                  OpenCL 2.1 (Build 0)
    ...
    ```

* List of available devices and platforms (`-l`):
    ```
    Platform #0: Intel(R) OpenCL
    `-- Device #0: Intel(R) Core(TM) i7-6700K CPU @ 4.00GHz
    Platform #1: Intel(R) FPGA Emulation Platform for OpenCL(TM)
    `-- Device #0: Intel(R) FPGA Emulation Device
    Platform #2: Intel(R) OpenCL HD Graphics
    `-- Device #0: Intel(R) Gen9 HD Graphics NEO
    Platform #3: Intel(R) Level-Zero
    `-- Device #0: Intel(R) Gen9
    Platform #4: SYCL host platform
    `-- Device #0: SYCL host device
    ```

## Supported OS
- Linux
- Windows (*under development*)

## Prerequisites
- [CMake](https://cmake.org/) (version 2.8 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
source <inteloneapi>/setvars.sh
cd <pti>/samples/dpc_info
mkdir build
cd build
CXX=dpcpp cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./dpc_info [-l|-a]
```
### Windows (manual build)
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
<inteloneapi>\setvars.bat
cd <pti>\samples\dpc_info
mkdir build
cd build
dpcpp [-O2|-O0 -g] ..\main.cc -o dpc_info.exe
```
Use this command line to run the application:
```sh
dpc_info.exe [-l|-a]
```