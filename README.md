# Profiling Tools Interfaces for GPU (PTI for GPU)

## Overview

This repository provides tools and a library for collecting performance data
on Intel(R) Graphics: the unitrace and sysmon tools, and the PTI SDK
profiling library.

## License

Profiling Tools Interfaces for GPU (PTI for GPU) is distributed
under the MIT License.

You may obtain a copy of the License at <https://opensource.org/license/MIT>

## Supported OS

- Linux
- Windows

## Supported Platforms

- Intel® Processor Graphics Gen9 (formerly Skylake) and newer
- Intel® Iris® Xe Graphics
- Intel® Data Center GPU Flex Series
- Intel® Data Center GPU Max Series
- Intel® Arc™ Graphics

## Regularly Tested Configurations

- Ubuntu 24.04 with Intel® Arc™ B580 Graphics
- Ubuntu 24.04 with Intel® Data Center GPU Max Series
- Windows 11 with Intel® Arc™ B580 Graphics

## Tools and SDK

- [unitrace](tools/unitrace/README.md) - unified tracing and profiling tool. In
  addition to Level Zero and/or OpenCL, this tool is capable of profiling
  software layers in the software stack, for example, SYCL and plugins, oneCCL,
  MPI etc., for scale-up and scale-out applications. It also supports profiling
  hardware metrics (including instruction-level EU stalls) and software events
  at the same time.
- [sysmon](tools/sysmon/README.md) - Linux "top" like utility to monitor GPUs
  installed on a system;
- [sdk](sdk/README.md) - profiling library one can link against to gain the
  capability of profiling SYCL and Level Zero; used by PyTorch Profiler for the
  Intel® XPU Activity.

## Prerequisites

- [CMake](https://cmake.org/) (version 3.22 and above)
- [Git](https://git-scm.com/) (version 1.8 and above)
- [Python](https://www.python.org/) (version 3.9 and above)
- On Linux one has to be a part of the `video` (Ubuntu 18 and below) or
  `render` (Ubuntu 19 and above) user group to do any computations on
  Intel(R) Processor Graphics:

  ```sh
  sudo usermod -a -G video <username>
  # OR
  sudo usermod -a -G render <username>
  ```

- OpenCL(TM) [ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader)
  and [Headers](https://github.com/KhronosGroup/OpenCL-Headers)
  - to use non-standard path to OpenCL ICD library one may add it into
    `LD_LIBRARY_PATH`:

    ```sh
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<path-to-opencl>
    ```

- [oneAPI Level Zero loader](https://github.com/oneapi-src/level-zero)
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [Intel(R) Metrics Discovery Application Programming Interface](https://github.com/intel/metrics-discovery)
  - one may need to install `libdrm-dev` package to build the library
    from sources
  - one may need to allow metrics collection for non-root users
    - if using the i915 kernel module (e.g. PVC):

      ```sh
      echo "0" | sudo tee /proc/sys/dev/i915/perf_stream_paranoid
      ```

    - if using the xe kernel module (e.g. BMG):

      ```sh
      echo "0" | sudo tee /proc/sys/dev/xe/observation_paranoid
      ```

- [Metrics Library for Metrics Discovery API (Metrics Library for MD API)](https://github.com/intel/metrics-library)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)
- [libdrm](https://gitlab.freedesktop.org/mesa/drm)
  - on Ubuntu one may perform:

    ```sh
    sudo apt-get install libdrm-dev
    ```

More information on what each tool needs can be found in its README.

## Build

The top-level CMakeLists.txt builds Unitrace and PTI SDK together. To build
them, use CMake in the root of this repository.

```console
source /opt/intel/oneapi/setvars.sh
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Tested software versions one may find in [SOFTWARE](SOFTWARE) file.**

**(*) Other names and brands may be claimed as property of others.**
