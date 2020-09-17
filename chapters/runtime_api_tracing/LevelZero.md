# Runtime API Tracing for oneAPI Level Zero (Level Zero)
## Overview
Level Zero API tracing allows to intercept standard API calls by injecting user-defined callbacks to function enter and exit points, as well as to read and modify traced function arguments and to read return value.

Level Zero runtime supports such an interface as part of its tools API.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [zet_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/tools/zet_api.h)

**Needed Libraries**:
- oneAPI Level Zero [libraries](https://github.com/intel/compute-runtime)

## How To Use
1. Before any work with Level Zero one must initialize its core and tools API:
```cpp
ze_result_t status = ZE_RESULT_SUCCESS;
status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
assert(status == ZE_RESULT_SUCCESS);
```
2. For each traced function on need to define the callbacks that will be called along with the traced API on enter and on exit, e.g. for `zeCommandListAppendLaunchKernel`:
```cpp
void OnEnterCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_user_data, void** instance_user_data) {
  std::cout << "Function zeCommandListAppendLaunchKernel is called on enter" <<
    std::endl;
}

void OnExitCommandListAppendLaunchKernel(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result, void* global_user_data, void** instance_user_data) {
  std::cout << "Function zeCommandListAppendLaunchKernel is called on exit" <<
    std::endl;
}
```
3. Create a tracer handle for the target device:
```cpp
  zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr,
      nullptr /* global user data */};

  zet_tracer_exp_handle_t tracer = nullptr;
  status = zetTracerExpCreate(context, &tracer_desc, &tracer);
  assert(status == ZE_RESULT_SUCCESS);
```
4. Set callbacks to Level Zero API functions that should be traced, e.g.:
```cpp
  zet_core_callbacks_t prologue_callbacks = {};
  zet_core_callbacks_t epilogue_callbacks = {};
  prologue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnEnterCommandListAppendLaunchKernel;
  epilogue_callbacks.CommandList.pfnAppendLaunchKernelCb =
    OnExitCommandListAppendLaunchKernel;

  status = zetTracerExpSetPrologues(tracer_, &prologue_callbacks);
  assert(status == ZE_RESULT_SUCCESS);
  status = zetTracerExpSetEpilogues(tracer, &epilogue_callbacks);
  assert(status == ZE_RESULT_SUCCESS);
```
5. Enable tracing session. After this call you will be notified on every traced API call:
```cpp
  status = zetTracerExpSetEnabled(tracer, true);
  assert(status == ZE_RESULT_SUCCESS);
```
6. To stop tracing, disable it. This call is non-blocking so some callbacks may be executing even after this function successfully returned:
```cpp
  status = zetTracerExpSetEnabled(tracer, false);
  assert(status == ZE_RESULT_SUCCESS);
```
7. Destroy tracing handle to release all the resources and wait for all callbacks, which may be still in the process of execution:
```cpp
  status = zetTracerExpDestroy(tracer);
  assert(status == ZE_RESULT_SUCCESS);
```

## Build and Run
To make tracing work one need to link the application with Level Zero ICD library (e.g. `libze_loader.so`) and run it as following:
```
ZET_ENABLE_API_TRACING_EXP=1 ./<application>
```

## Usage Details
- refer to oneAPI Level Zero API Tracing [documentation](https://spec.oneapi.com/level-zero/latest/tools/PROG.html#api-tracing) to learn more

## Samples
- [Level Zero Hot Functions](../../samples/ze_hot_functions)
- [Level Zero Hot Kernels](../../samples/ze_hot_kernels)
- [Level Zero Debug Info](../../samples/ze_debug_info)
- [Level Zero Metric Query](../../samples/ze_metric_query)
- [Level Zero Intercept](../../samples/ze_intercept)