# Binary/Source Correlation for oneAPI Level Zero (Level Zero)
## Overview
This chapter describes the interfaces and solutions one can use to deal with oneAPI Level Zero kernel sources, binaries and symbols.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [zet_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/zet_api.h)
- [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h)
- [program_debug_data.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h)
- Intel(R) Processor Graphics Assembler (IGA) [headers](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary/api)

**Needed Libraries**:
- oneAPI Level Zero [libraries](https://github.com/intel/compute-runtime)
- Intel(R) Processor Graphics Assembler (IGA) [library](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary), can be installed as part of [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## How To Use
### Binary Capture
Kernel binary grabbing is the feature of oneAPI Level Zero core API that is implemented by `zeModuleGetNativeBinary` function:
```cpp
ze_result_t status = ZE_RESULT_SUCCESS;
size_t binary_size = 0;
status = zeModuleGetNativeBinary(module, &binary_size, nullptr);
assert(status == ZE_RESULT_SUCCESS);

std::vector<uint8_t> binary(binary_size);
status = zeModuleGetNativeBinary(module, &binary_size, binary.data());
assert(status == ZE_RESULT_SUCCESS);
```

### Binary Parsing
Module binary obtained by `zeModuleGetNativeBinary` is stored as `"Intel(R) OpenCL Device Binary"` section of ELF file in a special internal format described in [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h).

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

### Binary Decoding/Disassembling
To decode and/or disassemble GEN binaries one should use [Intel(R) Processor Graphics Assembler (IGA)](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary) library described [here](../../chapters/binary_source_correlation/GenBinaryDecoding.md).

### Symbols Capture
Debug symbols grabbing for a module is a feature of Level Zero Tools API that is implemented by function `zetModuleGetDebugInfo`:
```cpp
size_t debug_info_size = 0;
status = zetModuleGetDebugInfo(
    module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
    &debug_info_size, nullptr);
assert(status == ZE_RESULT_SUCCESS);
assert(debug_info_size > 0);

std::vector<uint8_t> debug_info(debug_info_size);
status = zetModuleGetDebugInfo(
    module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
    &debug_info_size, debug_info.data());
assert(status == ZE_RESULT_SUCCESS);
```

### Symbols Parsing
To decode debug symbols for GPU modules one should refer to [Intel(R) Processor Graphics Compiler (IGC)](https://github.com/intel/intel-graphics-compiler) internal formats described [here](GenSymbolsDecoding.md).

## Usage Details
- refer to oneAPI Level Zero [documentation](https://spec.oneapi.com/level-zero/latest/index.html) to learn more
- look into GEN binary decoding [chapter](GenBinaryDecoding.md) to learn more on GEN binary decoding/disassembling interfaces
- look into GEN symbols decoding [chapter](GenSymbolsDecoding.md) to learn more on symbols format
- refer to the IGC [patch_list.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/patch_list.h) header to learn more on module binary layout
- refer to the IGC [program_debug_data.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h) header to learn more on debug symbols layout

## Samples
- [Level Zero Debug Info](../../samples/ze_debug_info)