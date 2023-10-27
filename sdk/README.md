# Performance Tools Interfaces SDK 

## Overview

This is PTI SDK Proof of Concept.

**PTI SDK** will be a framework for developing profiling tools for applications built on top of oneAPI and running on Intel XPUs. 

Today **SDK** POC provides *pti_view* library with an API to trace various tasks of application running on Intel GPU. While the library implementation uses low-level tracing APIs of SYCL run-time and Level-Zero, its own API is  high level. 

**SDK** POC is being built on the experience of **PTI-GPU** tools and samples and reuses that code with some modification. 

As for the project organization here – SDK folder is self-contained and independent from the rest of repository. 

One of the objectives is to extend functionality of PTI SDK and with the time to transform today’s **PTI-GPU** project to an **SDK**. 


This project is in active development. We decided to open it at this early stage to benefit from feedback and critisim of interested parties and early adopters.

## How to contribute 

At this stage we do not accept pull requests. We will change this soon. 

For now - please, submit issues or start a discussion. 

Please, check [TODO](TODO.md) list. We are aware of many shortcomings and inefficiencies and are working on them. 

## Supported OS

- Linux

## Regularly Tested Configurations

- Ubuntu 20.04 with Intel(R) Data Center GPU Max 1550

## Prerequisites 

- [CMake](https://cmake.org/) (version 3.12 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)

## Build

Build the pti library + tests and samples 

```console
>> source <path_to_oneapi>/setvars.sh
>> cd sdk
>> mkdir build
>> cd build
>> cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/icpx_toolchain.cmake ..
>> make -j
```

## Sample

From `build` directory:

```console
>> ./samples/vector_sq_add/vec_sqadd
```

## Test

```console
>> make test
```

## Install

```console
>> mkdir build
>> cd build
>> cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/icpx_toolchain.cmake -DBUILD_TESTING=OFF ..
>> make -j
>> cmake --install . --config Release --prefix "../out"
```

## Linking

### find_package

```cmake
# set Pti_DIR if you install in a nonstandard location.
set(Pti_DIR <path-to-pti>/lib/cmake/pti)

find_package(Pti X.Y.Z)

target_link_libraries(stuff PUBLIC Pti::pti_view)
```

## Usage

Use the `samples/` as a guide to developing with this library.

Note:

- Before `ptiViewEnable()` is called, please define
callbacks and register them with `ptiViewSetCallbacks()`.
