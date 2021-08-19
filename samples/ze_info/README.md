# Level Zero Info
## Overview
This sample application provides information on all platforms and devices available through oneAPI Level Zero (Level Zero), similar to `clinfo` tool.

* Detailed information on each driver and device (`-a`):
    ```
    Number of Drivers                                 1
      Driver API Version                              1.0 (latest)
      Driver Version                                  16795188

      Driver                                          0
    Number devices                                    1
      Device Name                                     Intel(R) Gen9
      Device Type                                     GPU
      Vendor ID                                       0x8086
      Device ID                                       0x1912
      Subdevice ID                                    0x0
      Core Clock Rate                                 1150MHz
      Maximum Memory Allocation Size                  4294959104 (4.00GiB)
      Maximum Hardware Contexts                       2
      Maximum Command Queue Priority                  0
      Number Threads Per EU                           7
      Physical EU SIMD Width                          8
      Number EU Per SubSlice                          8
      Number SubSlices Per Slice                      3
      Number Slices                                   1
      Timer Resolution                                83ns
      ...
    ```
* List of available drivers and platforms (`-l`):
    ```
      Driver #0: API Version 1.0 (latest)
      -- Device #0: Intel(R) Gen9
    ```
## Supported OS
- Linux
- Windows (*under development*)

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
cd <pti>/samples/ze_info
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./ze_info [-l|-a]
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_info
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the application:
```sh
ze_info.exe
```