# Profiling Tools Interfaces SDK

## Overview

This is PTI SDK Proof of Concept.

**PTI SDK** will be a framework for developing profiling tools for applications built on top of oneAPI and running on Intel XPUs.

Today **SDK** POC provides *pti_view* library with an API to trace various tasks of application running on Intel GPU. While the library implementation uses low-level tracing APIs of SYCL run-time and Level-Zero, its own API is  high level.

**SDK** POC is being built on the experience of **PTI-GPU** tools and samples and reuses that code with some modification.

As for the project organization here – SDK folder is self-contained and independent from the rest of repository.

One of the objectives is to extend functionality of PTI SDK and with the time to transform today’s **PTI-GPU** project to an **SDK**.


This project is in active development. We decided to open it at this early stage to benefit from feedback and critisim of interested parties and early adopters.

## Recent (version 0.7.0) update

Starting version 0.7.0 **PTI SDK** implements the new functionality of Local collection. It enables starting and stopping collection anytime-anywhere in an application when run on the system with installed Level-Zero runtime supporting [1.9.0 specification](https://spec.oneapi.io/releases/index.html#level-zero-v1-9-0) and higher.

Local collection functionality is transparent and controlled via `ptiViewEnable` and `ptiViewDisable` calls, where the first `ptiViewEnable` (or several of them) called at any place start the Local collection and the last `ptiViewDisable` (or several of them, paired with preceding `ptiViewEnable` calls) stop the Local collection.
Outside of Local collection regions of interest, PTI SDK maintains zero overhead by not issuing any calls or collecting any data.

On systems with Level-Zero version lower than 1.9.0 **PTI SDK** still operates as before its version 0.7.0: tracing runtime calls and causing the overhead outside of `ptiViewEnable` - `ptiViewDisable` regions, but reporting data only for `ptiViewEnable` - `ptiViewDisable` regions.

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
>> cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/icpx_toolchain.cmake ..
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
>> cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/icpx_toolchain.cmake -DBUILD_TESTING=OFF ..
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
