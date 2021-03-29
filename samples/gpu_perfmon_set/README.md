# GPU PerfMon Set
## Overview
This sample utility allows to tune Intel(R) Processor Graphics execution unit (EU) performance monitoring register to collect hardware events of some type.

The utility has a single parameter that contains the value one would like to set for events collection. Actual values are described in Programmer's Reference Manual for Intel(R) Processor Graphics, [Volume 14: Observability](https://01.org/sites/default/files/documentation/intel-gfx-prm-osrc-kbl-vol14-observability.pdf), e.g.:
```sh
./gpu_perfmon_set 5 # EU Stall Collection
```
On start the utility will configure the driver, and GPU will collect desired events until user press ENTER button. Expected output should be the following:
```
Configuration with the given GUID is already added
GPU PefMon configuration is completed
Press ENTER to deconfigure the driver...
```
To read the collected values for the event one should use binary instrumentation techniques or inline assembly.

## Supported OS
- Linux

## Prerequisites
- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 2.7 and above)
- [libdrm](https://gitlab.freedesktop.org/mesa/drm)

## Build and Run
### Linux
Run the following commands to build the sample:
```sh
cd <pti>/samples/gpu_perfmon_set
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Use this command line to run the utility:
```sh
./gpu_perfmon_set <perf_mon_reg_value>
```