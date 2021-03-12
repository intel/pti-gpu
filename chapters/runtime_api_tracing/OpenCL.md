# Runtime API Tracing for OpenCL(TM)
## Overview
OpenCL(TM) API tracing allows to intercept standard API calls by injecting user-defined callback to function enter and exit points, as well as to read and modify traced function arguments and return value.

OpenCL(TM) runtimes for Intel(R) Processor Graphics, Intel(R) Core(TM) Processor and Intel(R) Xeon(R) CPU support special tracing extension functions that can be obtained by [`clGetExtensionFunctionAddressForPlatform`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetExtensionFunctionAddressForPlatform.html) call.

The following functions are exposed:

```cpp
//! Function creates a tracing handle object
cl_int CL_API_CALL clCreateTracingHandleINTEL(
    cl_device_id device,          //!< OpenCL device to create a tracing handle for
    cl_tracing_callback callback, //!< User-defined callback that will be called along with traced
                                  //!< API functions
    void *userData,               //!< Pointer to any data user would like to pass to the callback,
                                  //!< can be NULL
    cl_tracing_handle *handle);   //!< Tracing handle object that describes current tracing session

//! Function allows to specify which target API call should be traced
cl_int CL_API_CALL clSetTracingPointINTEL(
    cl_tracing_handle handle,  //!< Tracing handle object
    cl_function_id functionId, //!< Identifier of the API function from the list
    cl_bool enable);           //!< Flag that shows if the API function should be traced or not

//! Function destroys the tracing handle object and releases all the associated resources
cl_int CL_API_CALL clDestroyTracingHandleINTEL(
    cl_tracing_handle handle); //!< Tracing handle object

//! Function enables the tracing process for the handle
cl_int CL_API_CALL clEnableTracingINTEL(
    cl_tracing_handle handle); //!< Tracing handle object

//! Function disables the tracing process for the handle
cl_int CL_API_CALL clDisableTracingINTEL(
    cl_tracing_handle handle); //!< Tracing handle object

//!  Function requests the tracing state for the handle (if it is enabled or disabled)
cl_int CL_API_CALL clGetTracingStateINTEL(
    cl_tracing_handle handle, //!< Tracing handle object
    cl_bool *enable);         //!< Returns TRUE if tracing handle is enabled and FALSE otherwise
```

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)
- [Intel(R) Xeon(R) Processor / Intel(R) Core(TM) Processor (CPU) Runtimes](https://software.intel.com/en-us/articles/opencl-drivers#cpu-section)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Any

**Needed Headers**:
- OpenCL(TM) [headers](https://github.com/KhronosGroup/OpenCL-Headers)
- [tracing_types.h](https://github.com/intel/compute-runtime/blob/master/opencl/source/tracing/tracing_types.h)
- [tracing_api.h](https://github.com/intel/compute-runtime/blob/master/opencl/source/tracing/tracing_api.h)

**Needed Libraries**:
- OpenCL(TM) [libraries](https://github.com/intel/compute-runtime)

## How To Use
1. Get a pointer to the function using
[`clGetExtensionFunctionAddressForPlatform`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetExtensionFunctionAddressForPlatform.html)
call, e.g.:
```cpp
decltype(clCreateTracingHandleINTEL)* _clCreateTracingHandleINTEL =
  reinterpret_cast<decltype(clCreateTracingHandleINTEL)*>(
      clGetExtensionFunctionAddressForPlatform(
          platform, "clCreateTracingHandleINTEL"));
```
2. Define the function that will be called along with the traced API, e.g.:
```cpp
void Callback(cl_function_id fid,
              cl_callback_data* callback_data,
              void* user_data) {
  std::count << "Function " << callback_data->functionName << " is called (" <<
    (callback_data->site == CL_CALLBACK_SITE_ENTER ? "enter" : "exit") <<
    ")" << std::endl;
}
```
3. Create a tracing handle for the target device:
```cpp
  cl_tracing_handle handle = nullptr;
  cl_int status = _clCreateTracingHandleINTEL(device, callback,
                                              nullptr, &handle);
  assert(status == CL_SUCCESS);
```
4. Set OpenCL(TM) API functions that should be traced, e.g.:
```cpp
  status = _clSetTracingPointINTEL(
      handle, CL_FUNCTION_clEnqueueNDRangeKernel, CL_TRUE);
  assert(status == CL_SUCCESS);
```
5. Enable tracing session. Then you will be notified on every traced API call:
```cpp
  status = _clEnableTracingINTEL(handle);
  assert(status == CL_SUCCESS);
```
6. To stop tracing, disable it. This call is blocked in a sense it waits for all callbacks, which may be in the process of execution:
```cpp
  status = _clDisableTracingINTEL(handle);
  assert(status == CL_SUCCESS);
```
7. Destroy tracing handle to release all the resources:
```cpp
  status = _clDestroyTracingHandleINTEL(handle);
  assert(status == CL_SUCCESS);
```

## Usage Details
- refer to OpenCL(TM) runtime API tracing [headers](https://github.com/intel/compute-runtime/blob/master/opencl/source/tracing) to learn more on tracing interfaces and data types

## Samples
- [OpenCL(TM) Hot Functions](../../samples/cl_hot_functions)
- [OpenCL(TM) Hot Kernels](../../samples/cl_hot_kernels)
- [OpenCL(TM) Debug Info](../../samples/cl_debug_info)
- [OpenCL(TM) GPU Metrics](../../samples/cl_gpu_metrics)