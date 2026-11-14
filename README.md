# Profiling Tools Interfaces for GPU (PTI for GPU)

## About this fork

This fork is a maintained continuation of the PTI-GPU tools and samples that
upstream marked as no longer under active development. The PTI SDK and
`unitrace` are not part of this fork.

- Targets Linux with both the i915 and xe kernel drivers. Testing is
  currently done on i915; xe validation is in progress. The Windows build
  paths are kept, but Windows is not tested here.
- Tools: [onetrace](tools/onetrace/README.md), [oneprof](tools/oneprof/README.md),
  [ze_tracer](tools/ze_tracer/README.md), [cl_tracer](tools/cl_tracer/README.md),
  [gpuinfo](tools/gpuinfo/README.md), [instcount](tools/instcount/README.md),
  [memaccess](tools/memaccess/README.md), [sysmon](tools/sysmon/README.md).
- Also included: the [samples](#sample-tools--utilities), the profiling
  [chapters](#profiling-chapters), and the `tests/run.py` test driver.

## Overview

This repository describes the ways of collecting performance data for Intel(R)
Processor Graphics and provides a set of samples that help to start.

## License

Samples for Profiling Tools Interfaces for GPU (PTI for GPU) are distributed
under the MIT License.

You may obtain a copy of the License at <https://opensource.org/license/MIT>

## Supported OS

- Linux (i915 and xe kernel drivers): developed and tested (xe validation in progress)
- Windows: build paths kept, not tested in this fork

## Supported Platforms

- Intel® Processor Graphics Gen9 (formerly Skylake) and newer
- Intel® Iris® Xe Graphics
- Intel® Data Center GPU Flex Series
- Intel® Data Center GPU Max Series
- Intel® Arc™ Graphics

*Some samples may have higher hardware requirements.*

## Tested Configurations

- Ubuntu 24.04 with Intel® Data Center GPU Max Series (i915)

## Profiling Chapters

1. Runtime API Tracing
    - for [OpenCL(TM)](chapters/runtime_api_tracing/OpenCL.md)
    - for [oneAPI Level Zero (Level Zero)](chapters/runtime_api_tracing/LevelZero.md)
    - for [OpenMP*](chapters/runtime_api_tracing/OMPT.md)
2. Device Activity Tracing
    - for [OpenCL(TM)](chapters/device_activity_tracing/OpenCL.md)
    - for [oneAPI Level Zero (Level Zero)](chapters/device_activity_tracing/LevelZero.md)
    - for [SYCL/DPC++](chapters/device_activity_tracing/DPCXX.md)
3. Binary/Source Correlation
    - for [OpenCL(TM)](chapters/binary_source_correlation/OpenCL.md)
    - for [oneAPI Level Zero (Level Zero)](chapters/binary_source_correlation/LevelZero.md)
4. Metrics Collection
    - based on [oneAPI Level Zero (Level Zero) Metric API](chapters/metrics_collection/LevelZero.md)
    - based on [Intel(R) Metrics Discovery Application Programming Interface](chapters/metrics_collection/MetricsDiscoveryAPI.md)
    - based on [Performance Monitoring (PM) Register](chapters/metrics_collection/PerfMonReg.md)
5. Binary Instrumentation
    - based on [Graphics Technology Pin (GT Pin)](chapters/binary_instrumentation/GTPin.md)
    - based on [OpenCL(TM) built-in intrinsics](chapters/binary_instrumentation/OpenCLBuiltIn.md)
6. Code Annotation
    - based on [Instrumentation and Tracing Technology API (ITT API)](chapters/code_annotation/ITT.md)
7. System Management
    - for [oneAPI Level Zero (Level Zero)](chapters/system_management/LevelZero.md)

## Profiling & Debug Tools

- [onetrace](tools/onetrace/README.md) - host and device tracing tool for
  OpenCL(TM) and Level Zero backends (including DPC++ and OpenMP* GPU offload);
- [ze_tracer](tools/ze_tracer/README.md) - host and device tracing tool for
  Level Zero;
- [cl_tracer](tools/cl_tracer/README.md) - host and device tracing tool for
  OpenCL(TM);
- [oneprof](tools/oneprof/README.md) - GPU HW metrics collection tool;
- [gpuinfo](tools/gpuinfo/README.md) - provides basic information about the
  GPUs installed in a system, and the list of HW metrics one can collect
  for it;
- [instcount](tools/instcount/README.md) - prints GPU kernel assembly (GEN ISA)
  annotated by instruction execution count;
- [memaccess](tools/memaccess/README.md) - GT Pin based analysis of GPU kernel
  memory accesses;
- [sysmon](tools/sysmon/README.md) - Linux "top" like utility to monitor GPUs
  installed on a system;

## Sample Tools & Utilities

- tools for OpenCL(TM), DPC++ (with OpenCL(TM) backend) and OpenMP* GPU
  offload (with OpenCL(TM) backend):
  - [cl_hot_functions](samples/cl_hot_functions/README.md) - provides a list of
    hottest OpenCL(TM) API calls by backend (CPU and GPU);
  - [cl_hot_kernels](samples/cl_hot_kernels/README.md) - provides a list of
    hottest OpenCL(TM) kernels by backend (CPU and GPU);
  - [cl_debug_info](samples/cl_debug_info/README.md) - prints source and
    assembly (GEN ISA) for kernels on GPU;
  - [cl_gpu_metrics](samples/cl_gpu_metrics/README.md) - provides a list of
    hottest OpenCL(TM) GPU kernels along with percent of cycles it was active,
    stall and idle (based on continuous metrics collection mode);
  - [cl_gpu_query](samples/cl_gpu_query/README.md) - provides a list of hottest
    OpenCL(TM) GPU kernels along with percent of cycles it was active, stall
    and idle (based on query metrics collection mode);
- tools for Level Zero, DPC++ (with Level Zero backend) and OpenMP* GPU
  offload (with Level Zero backend):
  - [ze_hot_functions](samples/ze_hot_functions/README.md) - provides a list of
    hottest Level Zero API calls;
  - [ze_hot_kernels](samples/ze_hot_kernels/README.md) - provides a list of
    hottest Level Zero kernels;
  - [ze_debug_info](samples/ze_debug_info/README.md) - prints source and
    assembly (GEN ISA) for kernels on GPU;
  - [ze_metric_query](samples/ze_metric_query/README.md) - provides a list of
    hottest Level Zero GPU kernels along with percent of cycles it was active,
    stall and idle (metrics are collected in *query* mode);
  - [ze_metric_streamer](samples/ze_metric_streamer/README.md) - provides a
    list of hottest Level Zero GPU kernels along with percent of cycles it was
    active, stall and idle (metrics are collected in *streamer* mode);
- tools for OpenMP*:
  - [omp_hot_regions](samples/omp_hot_regions/README.md) - provides a list of
    hottest parallel (for CPU) and target (for GPU) OpenMP* regions;
- utilities:
  - [dpc_info](samples/dpc_info/README.md) - prints information on available
    platforms and devices in DPC++;
  - [ze_info](samples/ze_info/README.md) - prints information on available
    platforms and devices in Level Zero;
  - [ze_metric_info](samples/ze_metric_info/README.md) - prints the list of HW
    metrics one can collect with the help of Level Zero;
  - [gpu_perfmon_set](samples/gpu_perfmon_set/README.md) - allows to choose
    HW metric for collection in EU PerfMon register;

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
- [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html)
- [Intel(R) oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html)
- [libdrm](https://gitlab.freedesktop.org/mesa/drm)
  - on Ubuntu one may perform:

    ```sh
    sudo apt-get install libdrm-dev
    ```

More information of what is needed for particular sample can be found on
sample description page.

## Build and Run

### Building everything

The top-level CMake project builds every tool and sample, each as a
separate CMake sub-build in `build/tools/<name>` and `build/samples/<name>`:

```sh
source /opt/intel/oneapi/setvars.sh  # needed for the DPC++/OpenMP samples
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <install_dir>  # installs the tools into <install_dir>/bin
```

Options:

| Option | Default | Description |
|---|---|---|
| `PTI_GPU_BUILD_TOOLS` | `ON` | Build the tools |
| `PTI_GPU_BUILD_SAMPLES` | `ON` | Build the samples |
| `PTI_GPU_TOOLS` | all | List of tools to build, e.g. `"onetrace;sysmon"` |
| `PTI_GPU_SAMPLES` | all | List of samples to build |
| `PTI_GPU_SKIP` | empty | List of tools or samples to skip, e.g. `"instcount;memaccess"` |
| `BUILD_TESTING` | `OFF` | Add a `legacy` ctest that runs `tests/run.py` |

Samples that need `icpx` (DPC++ and OpenMP) are skipped automatically if the
compiler is not found, and the `*_debug_info` samples are skipped if the IGA
library (`libiga64.so`) is not found. `instcount` and `memaccess` download
GT Pin at configure time.

Per-tool packages can be created with CPack, e.g.:

```sh
cd build && cpack -G TGZ  # or DEB / RPM
```

### Building a single sample or tool

In general, to build samples one need to perform the following steps
(specific instructions for particular sample can be found on sample
description page):

```sh
cd <pti_root>/samples/<sample_root>
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

To point out to specific headers and libraries one may use
`-DCMAKE_INCLUDE_PATH` and `-DCMAKE_LIBRARY_PATH` options
correspondingly, e.g.:

```sh
cmake -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INCLUDE_PATH=/tmp/level_zero/include \
  -DCMAKE_LIBRARY_PATH=/tmp/level_zero/lib \
  ..
```

Run instructions may vary from sample to sample significantly, so they are
provided on particular sample description page.

## Testing

There is a way to build and test all the samples in one command, e.g.:

```sh
LD_LIBRARY_PATH=/usr/local/lib python <pti_root>/tests/run.py
```

In case of failed tests, error output will be available in `stderr.log` file.

It's also possible to test an exact sample or a group of samples, e.g.:

```sh
# build and test an exact sample "cl_hot_functions"
python <pti_root>/tests/run.py -s cl_hot_functions
# build and test all L0 samples
python <pti_root>/tests/run.py -s ze
```

To run testing in debug mode one may use `-d` option, e.g.:

```sh
python <pti_root>/tests/run.py -s ze_gemm -d
```

The script creates `build` directory inside each sample folder while testing.
To remove all of these folders, use:

```sh
python <pti_root>/tests/run.py -c
```

**Tested software versions one may find in [SOFTWARE](SOFTWARE) file.**

**(*) Other names and brands may be claimed as property of others.**
