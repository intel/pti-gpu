# Binary/Source Correlation for OpenCL(TM)
## Overview
This chapter describes the interfaces and solutions one can use to deal with GPU OpenCL(TM) kernel sources, binaries and symbols.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- OpenCL(TM) [headers](https://github.com/KhronosGroup/OpenCL-Headers)
- Intel(R) Processor Graphics Assembler (IGA) library [headers](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary/api)
- [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h)
- [program_debug_data.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h)

**Needed Libraries**:
- OpenCL(TM) [libraries](https://github.com/intel/compute-runtime)
- Intel(R) Processor Graphics Assembler (IGA) [library](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary), can be installed as part of [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## How To Use
### Source Capture
Source grabbing is the standard feature of OpenCL(TM) that can be employed through [`clGetProgramInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetProgramInfo.html) call. The line returned by this call contains the source code for the whole program, so in case one need some particular kernel, one should find it out manually, e.g. highlight only needed lines based on kernel symbols information:
```cpp
size_t source_length = 0;
status = clGetProgramInfo(program, CL_PROGRAM_SOURCE,
                          0, nullptr, &source_length);
assert(status == CL_SUCCESS);

char* source = new char[source_length];
status = clGetProgramInfo(program, CL_PROGRAM_SOURCE,
                          source_length, source, nullptr);
assert(status == CL_SUCCESS);
```

### Binary Capture
Kernel binary grabbing is the feature of OpenCL(TM) for Intel(R) Processor Graphics that allows one to get final binary code.

There are two ways to retrieve kernel binaries supported by Intel(R) Processor Graphics.

The first way is Intel(R) specific. Standard [`clGetKernelInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetKernelInfo.html) function is used, but with a specific flag:
```cpp
#define CL_KERNEL_BINARY_PROGRAM_INTEL 0x407D

// ...

size_t binary_size = 0;
status = clGetKernelInfo(kernel, CL_KERNEL_BINARY_PROGRAM_INTEL,
                         0, nullptr, &binary_size);
assert(status == CL_SUCCESS);

unsigned char* binary = new unsigned char[binary_size];
status = clGetKernelInfo(kernel, CL_KERNEL_BINARY_PROGRAM_INTEL,
                         binary_size, binary, nullptr);
assert(status == CL_SUCCESS);
```
In that case raw GEN binaries will be retrieved, so one may decode/disassemble them directly as described [here](#binary-decoding-and-disassembling).

The second way is more standard and based on [`clGetProgramInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetProgramInfo.html) call:
```cpp
size_t* binary_size = new size_t[device_count];
status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
                          device_count * sizeof(size_t),
                          binary_size, nullptr);
assert(status == CL_SUCCESS);

uint8_t** binary = new uint8_t* [device_count];
for (size_t i = 0; i < device_count; ++i) {
  binary[i] = new uint8_t[binary_size[i]];
}

status = clGetProgramInfo(program, CL_PROGRAM_BINARIES,
                          device_count * sizeof(uint8_t*),
                          binary, nullptr);
assert(status == CL_SUCCESS);
```
Here one will get binaries in ELF format that should be parsed as described in the [next section](#binary-parsing).

### Binary Parsing
Program binary obtained by [`clGetProgramInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetProgramInfo.html) is stored as `"Intel(R) OpenCL Device Binary"` section of ELF file in a special internal format described in [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h).

To retrieve raw GEN binary for a kernel with a specific name, one should perform the next two steps.

The first step is to parse binary data in standard ELF64 format to get the content of `"Intel(R) OpenCL Device Binary"` section (let's call it `igc_binary`).

The second step is to parse this `igc_binary` that is stored in an internal [Intel(R) Processor Graphics Compiler](https://github.com/intel/intel-graphics-compiler) format described in [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h):
```cpp
#include <patch_list.h>

const SProgramBinaryHeader* header =
  reinterpret_cast<const SProgramBinaryHeader*>(igc_binary.data());
assert(header->Magic == MAGIC_CL);

const uint8_t* ptr = reinterpret_cast<const uint8_t*>(header) +
  sizeof(SProgramBinaryHeader) + header->PatchListSize;
for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
  const SKernelBinaryHeaderCommon* kernel_header =
    reinterpret_cast<const SKernelBinaryHeaderCommon*>(ptr);

  ptr += sizeof(SKernelBinaryHeaderCommon);
  const char* kernel_name = reinterpret_cast<const char*>(ptr);

  ptr += kernel_header->KernelNameSize;
  if (kernel_name == "SomeKernel") {
    std::vector<uint8_t> raw_binary(kernel_header->KernelHeapSize);
    memcpy(raw_binary.data(), ptr,
           kernel_header->KernelHeapSize * sizeof(uint8_t));
  }

  ptr += kernel_header->PatchListSize +
    kernel_header->KernelHeapSize +
    kernel_header->GeneralStateHeapSize + kernel_header->DynamicStateHeapSize +
    kernel_header->SurfaceStateHeapSize;
}
```

### Binary Decoding And Disassembling
To decode and/or disassemble GEN binaries one should use [Intel(R) Processor Graphics Assembler (IGA)](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary) library described [here](GenBinaryDecoding.md).

### Symbols Capture
Symbols grabbing is also the specific feature of Intel(R) Processor Graphics, that provides various debug information for GPU programs and kernels. To get it, one can use standard
[`clGetProgramInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetProgramInfo.html)
function with specific flags.

Symbols may be returned for each device associated with the OpenCL(TM) program object, so one should prepare the list of arrays by number of devices the program connected with. To get this number one may use [`clGetProgramInfo`](https://www.khronos.org/registry/OpenCL/sdk/2.1/docs/man/xhtml/clGetProgramInfo.html) call with `CL_PROGRAM_NUM_DEVICES` flag.

Note, that symbols are available only if the OpenCL kernel is compiled with `-gline-tables-only` flag:
```cpp
#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100

// ...

size_t* debug_info_size = new size_t[device_count];
status = clGetProgramInfo(program, CL_PROGRAM_DEBUG_INFO_SIZES_INTEL,
                          device_count * sizeof(size_t),
                          debug_info_size, nullptr);
assert(status == CL_SUCCESS);

uint8_t** debug_info = new uint8_t* [device_count];
for (size_t i = 0; i < device_count; ++i) {
  debug_info[i] = new uint8_t[debug_info_size[i]];
}

status = clGetProgramInfo(program, CL_PROGRAM_DEBUG_INFO_INTEL,
                          device_count * sizeof(uint8_t*),
                          debug_info, nullptr);
assert(status == CL_SUCCESS);
```

### Symbols Parsing
To decode debug symbols for GPU modules one should refer to [Intel(R) Processor Graphics Compiler (IGC)](https://github.com/intel/intel-graphics-compiler) internal formats described [here](GenSymbolsDecoding.md).

## Usage Details
- look into GEN binary decoding [chapter](GenBinaryDecoding.md) to learn more on GEN binary decoding/disassembling interfaces
- refer to the IGC [header](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h) to learn more on debug symbols layout

## Samples
- [OpenCL(TM) Debug Info](../../samples/cl_debug_info)