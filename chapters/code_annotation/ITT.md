# Instrumentation and Tracing Technology API (ITT API)
## Overview
The [Instrumentation and Tracing Technology API (ITT API)](https://software.intel.com/content/www/us/en/develop/documentation/vtune-help/top/api-support/instrumentation-and-tracing-technology-apis.html) enables the application to generate and control the collection of tracing data during its execution.
It is intended for use with [Intel(R) VTune(TM) Analyzer](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html).

Instrumentation and Tracing Technology API (ITT API) provides the following capabilities:
- Enable user to control collection;
- Enable user to set marks during the execution of the specific code;
- Enable user to mark frames and tasks;
- Enable user to specify custom synchronization primitives implemented without standard system APIs;
- Support applications in C/C++ environments.

User applications/modules linked to the static user API library do not have a runtime dependency on a dynamic library. Therefore, they can be executed without Intel(R) VTune(TM) Analyzer with close-to-zero overhead.

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Any

**Needed Headers**:
- [ittnotify.h](https://github.com/intel/ittapi/blob/master/include/ittnotify.h)

**Needed Libraries**:
- Instrumentation and Tracing Technology (ITT) [library](https://github.com/intel/ittapi), can be installed as part of [Intel(R) VTune(TM) Analyzer](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html)

## How To Use
The following steps should be performed to enable ITT based code annotation for target application:
1. Get ITT source code from [github](https://github.com/intel/ittapi).
2. Call ITT functions to annotate the regions-of-interest inside the source code:
```cpp
#include <ittnotify.h>

int main() {
  __itt_domain* domain = __itt_domain_create("Domain.Global");
  assert(domain != nullptr);

  // Place a new frame
  __itt_frame_begin_v3(domain, nullptr);
  {
    // Annotate the first task
    __itt_string_handle* first_task_handle =
      __itt_string_handle_create("FirstTask");
    __itt_task_begin(domain, __itt_null, __itt_null, first_task_handle);
    {
      /* First Task Body */
    }
    __itt_task_end(domain);

    // Annotate the second task
    __itt_string_handle* second_task_handle =
      __itt_string_handle_create("SecondTask");
    __itt_task_begin(domain, __itt_null, __itt_null, second_task_handle);
    {
      /* Second Task Body */
    }
    __itt_task_end(domain);
  }
  __itt_frame_end_v3(domain, nullptr);

  return 0;
}
```
3. Build the application and link it with ITT library implementation. One may build ITT static library first, and then link the application with it. Another way is to add ITT sources (in particular, [ittnotify_static.c](https://github.com/intel/ittapi/blob/master/src/ittnotify/ittnotify_static.c) file) into the application directly.
4. Run the application under [Intel(R) VTune(TM) Analyzer](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html) to see the result.

## Usage Details
- refer to Instrumentation and Tracing Technology API (ITT API) [documentation](https://software.intel.com/content/www/us/en/develop/documentation/vtune-help/top/api-support/instrumentation-and-tracing-technology-apis.html) documentation to learn more on programming interfaces
- refer to Instrumentation and Tracing Technology API (ITT API) [code repository](https://github.com/intel/ittapi) to learn more on implementation details

## Samples
- [OpenCL(TM) GEMM with Code Annotation](../../samples/cl_gemm_itt)