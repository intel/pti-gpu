# GEN Binary Decoding
## Overview
This chapter describes [Intel(R) Processor Graphics Assembler (IGA)](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary) library - a part of [Intel(R) Processor Graphics Compiler](https://github.com/intel/intel-graphics-compiler). The IGA is designed to decode and disassemble GEN binaries, including getting instruction properties, like opcode, instruction size, arguments, execution size, etc.

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- Intel(R) Processor Graphics Assembler (IGA) library [headers](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary/api)

**Needed Libraries**:
- Intel(R) Processor Graphics Assembler (IGA) [library](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary), can be installed as part of [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

## How To Use
An example below demonstrates how to split raw GEN binary into separate instructions and disassemble them using `KernelView` interface of IGA:
```cpp
#include <kv.hpp>

std::vector< std::pair<uint32_t, std::string> > Disassemble(
    const std::vector<uint8_t>& binary) {
  std::vector< std::pair<uint32_t, std::string> > instruction_list;

  KernelView kv(IGA_GEN9, binary.data(), binary.size(),
                iga::SWSB_ENCODE_MODE::SingleDistPipe);
  assert(kv.decodeSucceeded());

  char text[MAX_STR_SIZE] = { 0 };
  int32_t offset = 0, size = 0;
  while (true) {
    size = kv.getInstSize(offset);
    if (size == 0) {
      break;
    }

    size_t lenght = kv.getInstSyntax(offset, text, MAX_STR_SIZE);
    assert(lenght > 0);
    instruction_list.push_back(std::make_pair(offset, text));

    offset += size;
  }

  return instruction_list;
}
```

## Usage Details
- refer to the paper [Introduction to GEN Assembly](https://software.intel.com/en-us/articles/introduction-to-gen-assembly) to learn more on Intel(R) Processor Graphics instruction set
- look into Intel(R) Processor Graphics Assembler (IGA) library [headers](https://github.com/intel/intel-graphics-compiler/tree/master/visa/iga/IGALibrary/api) to learn more on GEN binary decoding/disassembling interfaces

## Samples
- [OpenCL(TM) Debug Info](../../samples/cl_debug_info)
- [Level Zero Debug Info](../../samples/ze_debug_info)
- [GPU PerfMon Read](../../samples/gpu_perfmon_read)

## Tools
- [Level Zero Tracer](../../tools/instcount)
