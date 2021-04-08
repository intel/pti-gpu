# Level Zero System Management
## Overview
This sample application provides basic static and dynamic information for GPU device:

Output should look like the following:
```
Device: Intel(R) Iris(R) Plus Graphics 655 [0x3ea5]
-- Subdevice Count: 0
-- Driver Version: A7F72C54A5788663395C411
-- PCI Bus: 0000:00:02.0
-- Frequency Domains: 1
---- [0] Clock EU Freq Range (MHz): 300 - 1200 (changeable)
---- [0] Current Clock EU Freq (MHz): 300
-- Temperature Sensors: 5
---- [2] Core Temperature (C): 34
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
cd <pti>/samples/ze_sysman
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./ze_sysman
```
### Windows
Use Microsoft* Visual Studio x64 command prompt to run the following commands and build the sample:
```sh
cd <pti>\samples\ze_sysman
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_LIBRARY_PATH=<level_zero_loader>\lib -DCMAKE_INCLUDE_PATH=<level_zero_loader>\include ..
nmake
```
Use this command line to run the application:
```sh
ze_sysman.exe
```