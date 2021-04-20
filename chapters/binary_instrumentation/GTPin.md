# Binary Instrumentation with Graphics Technology Pin (GT Pin)
## Overview
This chapter describes **Graphics Technology Pin (GT Pin)** -  the solution for binary code instrumentation on Intel(R) Processor Graphics. It is a [Pin](http://pintool.intel.com/) like system that instruments Gen execution unit (EU) binaries and is used to build architectural and software tools to analyze shaders or compute kernels running on Gen EUs.

Graphics Technology Pin (GT Pin) works based on three callbacks:
- **on kernel build** - called for every GPU kernel while it is building, right place to add any instrumentation into kernel binary;
- **on kernel run** - called for every instance of the kernel before its execution. One can choose which version of the kernel (instrumented or native) should be running;
- **on kernel complete** - called for every instrumented instance of the kernel after completion, right place to grab profiling results.

Unlike Pin, Graphics Technology Pin (GT Pin) does not support arbitrary instrumentation. Instead it provides several types of probes that could be inserted in any place inside a binary. E.g. one may use ***latency*** probe to measure instruction blocks, ***opcodeprof*** probe to grab instruction execution count or ***simdprof*** to count the number of effective SIMD operations.

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html) headers

**Needed Libraries**:
- [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html) libraries

## How To Use
To start using Graphics Technology Pin (GT Pin), one need to initialize it first. Note, that such an initialization should be performed ***before*** any work with GPU runtimes (like OpenCL) - otherwise some kernels may not be captured.

Graphics Technology Pin (GT Pin) initialization requires three callbacks described above.
```cpp
void KnobAddBool(const char* name, bool value) {
  GTPinKnob knob = KNOB_FindArg(name);
  assert(knob != nullptr);
  KnobValue knob_value;
  knob_value.value._bool = value;
  knob_value.type = KNOB_TYPE_BOOL;
  KNOB_STATUS status = KNOB_AddValue(knob, &knob_value);
  assert(status == KNOB_STATUS_SUCCESS);
}

void GtpinInit() {
  KnobAddBool("silent_warnings", true);

  GTPin_OnKernelBuild(OnKernelBuild, nullptr);
  GTPin_OnKernelRun(OnKernelRun, nullptr);
  GTPin_OnKernelComplete(OnKernelComplete, nullptr);

  GTPIN_Start();
}
```
The first callback, `OnKernelBuild`, should be used to create an instrumented version of the kernel. Code fragment below injects *opcodeprof* probes into the kernel binary to compute its dynamic basic block count (how many times each block has been executed):
```cpp
void OnKernelBuild(GTPinKernel kernel, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

  for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block);
    block = GTPin_BBLNext(block)) {
    GTPinINS head = GTPin_InsHead(block);
    int32_t offset =  GTPin_InsOffset(head);

    GTPinMem mem = nullptr;
    status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

    status = GTPin_OpcodeprofInstrument(head, mem);
    assert(status == GTPINTOOL_STATUS_SUCCESS);

      // Save mem object to get profiling results on kernel completion
  }
}
```
The second callback, `OnKernelRun`, allows to enable or disable instrumentation for the current kernel instance:
```cpp
void OnKernelRun(GTPinKernelExec kernelExec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernelExec, 1);
  assert(status == GTPINTOOL_STATUS_SUCCESS);
}
```
And the third callback, `OnKernelComplete`, is normally used to grab the results collecting while execution of instrumented code.

These profiling results are represented by `GTPinMem` objects, each of them corresponds to a previously set probe. E.g. for the case of dynamic block count, there is a probe per basic block, so one can grab an execution count for a block. Graphics Technology Pin (GT Pin) provides a value per EU hardware thread, so since each block may be executed inside different threads, one need to accumulate these per-thread values into a total sum.
```cpp
static void OnKernelComplete(GTPinKernelExec kernelExec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

  // Previously stored list of GTPinMem objects
  std::vector<GTPinMem> block_memory_list;

  for (auto block : block_memory_list) {
    uint32_t thread_count = GTPin_MemSampleLength(block);
    assert(thread_count > 0);

    uint32_t total = 0, value = 0;
    for (uint32_t tid = 0; tid < threadCount; ++tid) {
      status = GTPin_MemRead(block, tid, sizeof(uint32_t), (char*)&value, nullptr);
      assert(status == GTPINTOOL_STATUS_SUCCESS);
      total += value;
    }

    // Print execution count for the current basic block
  }
}
```

**Note**, that to use GT-Pin tools with oneAPI Level Zero (Level Zero) one should set the following environment variables:
```
ZET_ENABLE_API_TRACING_EXP=1
ZET_ENABLE_PROGRAM_INSTRUMENTATION=1
```

## Usage Details
- refer to [Graphics Technology Pin (GT Pin)](https://software.intel.com/content/www/us/en/develop/articles/gtpin.html) project page to learn more on binary instrumentation techniques for Intel(R) Processor Graphics
- refer to
[Compute Architecture Manuals](https://software.intel.com/en-us/articles/intel-graphics-developers-guides) to learn more on Intel(R) Processor Graphics Architecture
- refer to the paper [Introduction to GEN Assembly](https://software.intel.com/en-us/articles/introduction-to-gen-assembly) to learn more on Intel(R) Processor Graphics instruction set

## Samples
- [GPU Instruction Count](../../samples/gpu_inst_count)
- [GPU PerfMon Read](../../samples/gpu_perfmon_read)