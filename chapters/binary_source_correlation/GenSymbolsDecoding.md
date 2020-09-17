# GEN Symbols Decoding
## Overview
This chapter describes the way of decodig symbol information that comes from [Intel(R) Processor Graphics Compiler (IGC)](https://github.com/intel/intel-graphics-compiler) and is stored in a special IGC internal format with ELF/DWARF sections inside.

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [program_debug_data.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h)

## How To Use
Symbols one can grab for OpenCL(TM) program or oneAPI Level Zero (Level Zero) module are in the special format described in
[program_debug_data.h](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h) file:
```
+--------------------------------------------------------------------------+
|   SProgramDebugDataHeaderIGC                                             |
+--------------------------------------------------------------------------+
|    Program Kernel Data Table:                                            |
|    (All kernels have debug data entries in here. If kernel has no debug  |
|     info the debug info size will be zero)                               |
|     --->   (IGC) Program Kernel Data 1                                   |
|     --->   ...                                                           |
|     --->   (IGC) Program Kernel Data n                                   |
+--------------------------------------------------------------------------+
```
Each kernel data section has the following format:
```
+--------------------------------------------------------------------------+
|   SKernelDebugDataHeaderIGC                                              |
+--------------------------------------------------------------------------+
|   Kernel name:                                                           |
|    (NULL terminated string aligned on sizeof(DWORD))                     |
+--------------------------------------------------------------------------+
|   VISA debug info (in ELF/DWARF format)                                  |
+--------------------------------------------------------------------------+
|   GenISA debug info (empty for newest drivers)                           |
+--------------------------------------------------------------------------+
```
The code to parse debug information may look like this:
```cpp
// Previously grabbed symbols for exact program/module and device
std::vector<uint8_t> symbols;

const uint8_t* ptr = symbols.data();
const SProgramDebugDataHeaderIGC* header =
  reinterpret_cast<const SProgramDebugDataHeaderIGC*>(ptr);
ptr += sizeof(SProgramDebugDataHeaderIGC);

for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
  const SKernelDebugDataHeaderIGC* kernel_header =
    reinterpret_cast<const SKernelDebugDataHeaderIGC*>(ptr);
  ptr += sizeof(SKernelDebugDataHeaderIGC);

  const char* kernel_name = reinterpret_cast<const char*>(ptr);
  unsigned kernel_name_size_aligned = sizeof(uint32_t) *
    (1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
  ptr += kernel_name_size_aligned;

  if (kernel_header->SizeVisaDbgInBytes > 0) {
    // Parse the binary block [ptr, ptr + kernel_header->SizeVisaDbgInBytes)
    // as a blob in standard ELF/DWARF format
  }

  // Should be zero for newest drivers
  assert(kernel_header->SizeGenIsaDbgInBytes == 0);

  ptr += kernel_header->SizeVisaDbgInBytes;
  ptr += kernel_header->SizeGenIsaDbgInBytes;
}
```

## Usage Details
- refer to the IGC [header](https://github.com/intel/intel-graphics-compiler/blob/master/IGC/AdaptorOCL/ocl_igc_shared/executable_format/program_debug_data.h) to learn more on debug symbols layout

## Samples
- [OpenCL(TM) Debug Info](../../samples/cl_debug_info)
- [Level Zero Debug Info](../../samples/ze_debug_info)