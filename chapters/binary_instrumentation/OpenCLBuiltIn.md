# OpenCL(TM) Built-In Intrinsics
## Overview
This chapter describes OpenCL(TM) built-in intrinsics one may use inside the kernel code to retrieve some additional instruction-level information from Intel(R) Processor Graphics execution unit (EU).

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- OpenCL(TM) [headers](https://github.com/KhronosGroup/OpenCL-Headers)

**Needed Libraries**:
- OpenCL(TM) [libraries](https://github.com/intel/compute-runtime)

## How To Use
The following instrinsics are currently available:
```cpp
/* Returns the value from GPU EU timestamp register */
ulong __attribute__((overloadable)) intel_get_cycle_counter( void );

/* Returns active channel mask for current GPU hardware thread */
uint __attribute__((overloadable)) intel_get_active_channel_mask( void );

/* Returns the value for target control register */
uint __attribute__((overloadable)) intel_get_control_register( uint reg );

/* Returns global GPU hardware thread ID across the device */
uint __attribute__((overloadable)) intel_get_hw_thread_id( void );

/* Returns global GPU slice ID */
uint __attribute__((overloadable)) intel_get_slice_id( void );

/* Returns GPU subslice ID for current slice */
uint __attribute__((overloadable)) intel_get_subslice_id( void );

/* Returns GPU EU ID for current subslice */
uint __attribute__((overloadable)) intel_get_eu_id( void );

/* Returns GPU hardware thread ID for current EU */
uint __attribute__((overloadable)) intel_get_eu_thread_id( void );

/* Pauses current GPU hardware thread for (32 * value) cycles  */
void __attribute__((overloadable)) intel_eu_thread_pause( uint value );
```

To use them inside an OpenCL(TM) kernel, one need to declare their prototypes first and then use as normal functions:
```cpp
ulong __attribute__((overloadable)) intel_get_cycle_counter( void );

__kernel kernel(/*...*/) {
  ulong thread_start = intel_get_cycle_counter();
  /* do some computations */
  ulong thread_end = intel_get_cycle_counter();
}
```

Note, that described instrinsics will work correctly only with `Debug` or `ReleaseInternal` builds of [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime) and [Intel(R) Processor Graphics Compiler](https://github.com/intel/intel-graphics-compiler).

## Usage Details
- refer to [Intel(R) Processor Graphics Compiler](https://github.com/intel/intel-graphics-compiler) project to get more details on OpenCL(TM) built-in instrinsicts support ([this](https://github.com/intel/intel-graphics-compiler/blob/1c28cd3cd4cf11fa603cf41dbf8ce852c32f1f23/IGC/BiFModule/Languages/OpenCL/IBiF_Impl.cl) file contains its implementations)
- refer to the paper [Introduction to GEN Assembly](https://software.intel.com/en-us/articles/introduction-to-gen-assembly) to learn more on Intel(R) Processor Graphics instruction set
- refer to
[Compute Architecture Manuals](https://software.intel.com/en-us/articles/intel-graphics-developers-guides) to learn more on Intel(R) Processor Graphics Architecture

## Samples
- [OpenCL(TM) GEMM with Manual Code Instrumentation](../../samples/cl_gemm_inst)