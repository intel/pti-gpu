//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_DEBUG_INFO_CL_DEBUG_INFO_COLLECTOR_H_
#define PTI_SAMPLES_CL_DEBUG_INFO_CL_DEBUG_INFO_COLLECTOR_H_

#include <string.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "cl_api_tracer.h"
#include "cl_utils.h"
#include "elf_parser.h"
#include "gen_binary_decoder.h"

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL 0x4100

static const char* kDebugFlag = "-gline-tables-only";

struct SourceLine {
  uint64_t number;
  std::string text;
};

struct SourceFileInfo {
  uint64_t file_id;
  std::string file_name;
  std::vector<SourceLine> source_line_list;
};

struct KernelDebugInfo {
  std::vector<Instruction> instruction_list;
  std::vector<SourceMapping> line_info_list;
  std::vector<SourceFileInfo> source_info_list;
};

using KernelDebugInfoMap = std::map<std::string, KernelDebugInfo>;

class ClDebugInfoCollector {
 public:  // User Interface
  static ClDebugInfoCollector* Create(cl_device_id device) {
    PTI_ASSERT(device != nullptr);

    ClDebugInfoCollector* collector = new ClDebugInfoCollector(device);
    PTI_ASSERT(collector != nullptr);

    ClApiTracer* tracer = new ClApiTracer(device, Callback, collector);
    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer "
                << "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
      }
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ClDebugInfoCollector() {
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    bool disabled = tracer_->Disable();
    PTI_ASSERT(disabled);
  }

  const KernelDebugInfoMap& GetKernelDebugInfoMap() const { return kernel_debug_info_map_; }

  ClDebugInfoCollector(const ClDebugInfoCollector& copy) = delete;
  ClDebugInfoCollector& operator=(const ClDebugInfoCollector& copy) = delete;

  static void InstructionCallback(int32_t offset, void* data) {}

  static void PrintKernelDebugInfo(std::string kernel_name,
                                   const KernelDebugInfo& kernel_debug_info,
                                   decltype(InstructionCallback)* callback = InstructionCallback,
                                   void* callback_data = nullptr) {
    PTI_ASSERT(!kernel_name.empty());

    std::cerr << "===== Kernel: " << kernel_name << " =====" << std::endl;

    const std::vector<Instruction>& instruction_list = kernel_debug_info.instruction_list;
    PTI_ASSERT(instruction_list.size() > 0);

    uint64_t last_instruction_address = instruction_list.back().offset;

    const std::vector<SourceMapping>& line_info = kernel_debug_info.line_info_list;
    PTI_ASSERT(line_info.size() > 0);

    const std::vector<SourceFileInfo>& source_info_list = kernel_debug_info.source_info_list;
    PTI_ASSERT(source_info_list.size() > 0);

    // Print instructions with no corresponding file
    std::cerr << "=== File: Unknown ===" << std::endl;
    for (auto& instruction : instruction_list) {
      bool found = false;

      for (size_t l = 0; l < line_info.size(); ++l) {
        uint64_t start_address = line_info[l].address;
        uint64_t end_address =
            (l + 1 < line_info.size()) ? line_info[l + 1].address : last_instruction_address;

        if (instruction.offset >= start_address && instruction.offset < end_address) {
          found = true;
          break;
        }
      }

      if (!found) {
        std::cerr << "\t\t["
                  << "0x" << std::setw(5) << std::setfill('0') << std::hex << std::uppercase
                  << instruction.offset << "] " << instruction.text;
        callback(instruction.offset, callback_data);
        std::cerr << std::endl;
      }
    }

    // Print info per file
    for (auto& source_info : source_info_list) {
      std::cerr << "=== File: " << source_info.file_name.c_str() << " ===" << std::endl;

      const std::vector<SourceLine> line_list = source_info.source_line_list;
      PTI_ASSERT(line_list.size() > 0);

      // Print instructions with no corresponding source line
      for (size_t l = 0; l < line_info.size(); ++l) {
        if (line_info[l].line == 0) {
          uint64_t start_address = line_info[l].address;
          uint64_t end_address =
              (l + 1 < line_info.size()) ? line_info[l + 1].address : last_instruction_address;

          for (auto instruction : instruction_list) {
            if (instruction.offset >= start_address && instruction.offset < end_address &&
                source_info.file_id == line_info[l].file_id) {
              std::cerr << "\t\t["
                        << "0x" << std::setw(5) << std::setfill('0') << std::hex << std::uppercase
                        << instruction.offset << "] " << instruction.text;
              callback(instruction.offset, callback_data);
              std::cerr << std::endl;
            }
          }
        }
      }

      // Print instructions for corresponding source line
      for (auto line : line_list) {
        std::cerr << "[" << std::setw(5) << std::setfill(' ') << std::dec << line.number << "] "
                  << line.text << std::endl;

        for (size_t l = 0; l < line_info.size(); ++l) {
          if (line_info[l].line == line.number) {
            uint64_t start_address = line_info[l].address;
            uint64_t end_address =
                (l + 1 < line_info.size()) ? line_info[l + 1].address : last_instruction_address;

            for (auto instruction : instruction_list) {
              if (instruction.offset >= start_address && instruction.offset < end_address &&
                  source_info.file_id == line_info[l].file_id) {
                std::cerr << "\t\t["
                          << "0x" << std::setw(5) << std::setfill('0') << std::hex << std::uppercase
                          << instruction.offset << "] " << instruction.text;
                callback(instruction.offset, callback_data);
                std::cerr << std::endl;
              }
            }
          }
        }
      }
    }

    std::cerr << std::endl;
  }

 private:  // Implementation Details
  ClDebugInfoCollector(cl_device_id device) : device_(device) { PTI_ASSERT(device_ != nullptr); }

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    bool set = true;
    set = set && tracer_->SetTracingFunction(CL_FUNCTION_clBuildProgram);
    set = set && tracer_->SetTracingFunction(CL_FUNCTION_clCreateKernel);
    PTI_ASSERT(set);

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernel(std::string name, const std::vector<Instruction>& instruction_list,
                 const std::vector<SourceMapping>& line_info_list,
                 const std::vector<SourceFileInfo>& source_info_list) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(instruction_list.size() > 0);
    PTI_ASSERT(line_info_list.size() > 0);
    PTI_ASSERT(source_info_list.size() > 0);

    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_debug_info_map_.count(name) == 0);
    kernel_debug_info_map_[name] = {instruction_list, line_info_list, source_info_list};
  }

  static std::vector<SourceLine> GetSource(cl_kernel kernel) {
    PTI_ASSERT(kernel != nullptr);

    cl_program program = utils::cl::GetProgram(kernel);
    PTI_ASSERT(program != nullptr);

    cl_int status = CL_SUCCESS;
    size_t length = 0;
    status = clGetProgramInfo(program, CL_PROGRAM_SOURCE, 0, nullptr, &length);
    PTI_ASSERT(status == CL_SUCCESS);
    if (length == 0) {
      return std::vector<SourceLine>();
    }

    std::vector<char> source(length, '\0');
    status = clGetProgramInfo(program, CL_PROGRAM_SOURCE, length, source.data(), nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    std::vector<SourceLine> line_list;
    std::istringstream stream(source.data());
    uint32_t number = 1;
    std::string text;
    while (std::getline(stream, text)) {
      line_list.push_back({number, text});
      ++number;
    }

    return line_list;
  }

  static std::vector<uint8_t> GetBinary(cl_kernel kernel, cl_device_id device) {
    PTI_ASSERT(kernel != nullptr && device != nullptr);

    cl_program program = utils::cl::GetProgram(kernel);
    PTI_ASSERT(program != nullptr);

    std::vector<cl_device_id> device_list = utils::cl::GetDeviceList(program);
    PTI_ASSERT(device_list.size() > 0);

    size_t target_id = device_list.size();
    for (size_t i = 0; i < device_list.size(); ++i) {
      if (device_list[i] == device) {
        target_id = i;
        break;
      }
    }

    if (target_id == device_list.size()) {
      return std::vector<uint8_t>();
    }

    std::vector<size_t> binary_size_list(device_list.size(), 0);

    cl_int status = CL_SUCCESS;
    status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, device_list.size() * sizeof(size_t),
                              binary_size_list.data(), nullptr);
    if (status != CL_SUCCESS || binary_size_list[target_id] == 0) {
      return std::vector<uint8_t>();
    }

    std::vector<std::vector<uint8_t>> binary_list(device_list.size());
    std::vector<uint8_t*> binary_ptr_list(
        binary_list.size());  // just pointers to binary_list data, no memory allocation. Used in
                              // clGetProgramInfo interface as a void**

    for (size_t i = 0; i < binary_list.size(); ++i) {
      binary_list[i].resize(binary_size_list[i]);
      binary_ptr_list[i] = binary_list[i].data();
    }

    status = clGetProgramInfo(program, CL_PROGRAM_BINARIES, binary_list.size() * sizeof(uint8_t*),
                              reinterpret_cast<void**>(binary_ptr_list.data()), nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    std::vector<uint8_t> binary(binary_size_list[target_id]);
    memcpy(binary.data(), binary_list[target_id].data(),
           binary_size_list[target_id] * sizeof(uint8_t));

    return binary;
  }

  static std::vector<uint8_t> GetDebugSymbols(cl_kernel kernel, cl_device_id device) {
    PTI_ASSERT(kernel != nullptr && device != nullptr);

    cl_program program = utils::cl::GetProgram(kernel);
    PTI_ASSERT(program != nullptr);

    std::vector<cl_device_id> device_list = utils::cl::GetDeviceList(program);
    PTI_ASSERT(device_list.size() > 0);

    size_t target_id = device_list.size();
    for (size_t i = 0; i < device_list.size(); ++i) {
      if (device_list[i] == device) {
        target_id = i;
        break;
      }
    }

    if (target_id == device_list.size()) {
      return std::vector<uint8_t>();
    }

    std::vector<size_t> debug_size_list(device_list.size(), 0);

    cl_int status = CL_SUCCESS;
    status = clGetProgramInfo(program, CL_PROGRAM_DEBUG_INFO_SIZES_INTEL,
                              device_list.size() * sizeof(size_t), debug_size_list.data(), nullptr);
    if (status != CL_SUCCESS || debug_size_list[target_id] == 0) {
      return std::vector<uint8_t>();
    }

    std::vector<std::vector<uint8_t>> debug_symbols_list(device_list.size());
    std::vector<uint8_t*> debug_symbols_ptr_list(
        debug_symbols_list.size());  // just pointers to debug_symbols_list data, no memory
                                     // allocation. Used in clGetProgramInfo interface as a void**

    for (size_t i = 0; i < debug_symbols_list.size(); ++i) {
      debug_symbols_list[i].resize(debug_size_list[i]);
      debug_symbols_ptr_list[i] = debug_symbols_list[i].data();
    }

    status = clGetProgramInfo(program, CL_PROGRAM_DEBUG_INFO_INTEL,
                              debug_symbols_list.size() * sizeof(uint8_t*),
                              reinterpret_cast<void**>(debug_symbols_ptr_list.data()), nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    std::vector<uint8_t> debug_symbols(debug_size_list[target_id]);
    memcpy(debug_symbols.data(), debug_symbols_list[target_id].data(),
           debug_size_list[target_id] * sizeof(uint8_t));

    return debug_symbols;
  }

 private:  // Callbacks
  static void OnEnterBuildProgram(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clBuildProgram* params =
        reinterpret_cast<const cl_params_clBuildProgram*>(data->functionParams);

    const char* options = *(params->options);
    if (options != nullptr && strstr(options, kDebugFlag) != nullptr) {
      return;
    }

    std::string* build_options = new std::string(kDebugFlag);
    if (options != nullptr) {
      *build_options += " ";
      *build_options += options;
    }

    *(params->options) = build_options->c_str();
    data->correlationData[0] = reinterpret_cast<cl_ulong>(build_options);
  }

  static void OnExitBuildProgram(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    std::string* build_options = reinterpret_cast<std::string*>(data->correlationData[0]);
    if (build_options != nullptr) {
      delete build_options;
    }
  }

  static void OnExitCreateKernel(cl_callback_data* data, void* user_data) {
    PTI_ASSERT(data != nullptr);

    cl_kernel* kernel = reinterpret_cast<cl_kernel*>(data->functionReturnValue);
    if (*kernel == nullptr) {
      return;
    }

    ClDebugInfoCollector* collector = reinterpret_cast<ClDebugInfoCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);

    PTI_ASSERT(collector->device_ != nullptr);
    cl_device_id device = collector->device_;

    std::string kernel_name = utils::cl::GetKernelName(*kernel);
    std::string device_name = utils::cl::GetDeviceName(device);

    std::vector<uint8_t> debug_info = GetBinary(*kernel, device);
    if (debug_info.size() == 0) {
      std::cerr << "[WARNING] Kernel binaries are not found" << std::endl;
      return;
    }
    PTI_ASSERT(debug_info.size() < (std::numeric_limits<uint32_t>::max)());

    pti_result res;

    elf_parser_handle_t parserHandle = nullptr;
    res = ptiElfParserCreate(debug_info.data(), static_cast<uint32_t>(debug_info.size()),
                             &parserHandle);
    if (res != PTI_SUCCESS || parserHandle == nullptr) {
      std::cerr << "[WARNING] : Cannot create elf parser" << std::endl;
      return;
    }

    bool is_valid = false;
    res = ptiElfParserIsValid(parserHandle, &is_valid);
    if (res != PTI_SUCCESS || !is_valid) {
      std::cerr << "[WARNING] : Constructed Elf parser is not valid" << std::endl;
      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);
      return;
    }

    uint32_t kernel_num = 0;
    res = ptiElfParserGetKernelNames(parserHandle, 0, nullptr, &kernel_num);
    if (res != PTI_SUCCESS) {
      std::cerr << "Error: Failed to get kernel names" << std::endl;
      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);
      return;
    }

    if (kernel_num == 0) {
      std::cerr << "[WARNING] : No kernels found" << std::endl;
      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);
      return;
    }

    std::vector<const char*> kernel_names(kernel_num);

    res = ptiElfParserGetKernelNames(parserHandle, kernel_num, kernel_names.data(), nullptr);
    if (res != PTI_SUCCESS) {
      std::cerr << "Error: Failed to get kernel names" << std::endl;
      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);
      return;
    }

    for (uint32_t kernel_idx = 0; kernel_idx < kernel_names.size(); kernel_idx++) {
      if (kernel_name != std::string(kernel_names[kernel_idx])) {
        continue;
      }

      uint32_t binary_size = 0;
      const uint8_t* binary = nullptr;
      uint64_t kernel_address = 0;

      res = ptiElfParserGetBinaryPtr(parserHandle, kernel_idx, &binary, &binary_size,
                                     &kernel_address);
      if (res != PTI_SUCCESS || binary_size == 0) {
        std::cerr << "[WARNING] : Unable to get GEN binary for kernel: " << kernel_name
                  << std::endl;
        continue;
      }

      uint32_t gfx_core = 0;
      res = ptiElfParserGetGfxCore(parserHandle, &gfx_core);
      if (res != PTI_SUCCESS || gfx_core == 0) {
        std::cerr << "[WARNING] : Unable to get GEN binary version for kernel: " << kernel_name
                  << std::endl;
        continue;
      }

      GenBinaryDecoder decoder(binary, binary_size, GenBinaryDecoder::GfxCoreToIgaGen(gfx_core));
      if (!decoder.IsValid()) {
        std::cerr << "[WARNING] : Unable to create decoder for kernel: " << kernel_name
                  << std::endl;
        continue;
      }
      std::vector<Instruction> instruction_list = decoder.Disassemble();
      if (instruction_list.size() == 0) {
        std::cerr << "[WARNING] : Unable to decode kernel binary for kernel: " << kernel_name
                  << std::endl;
        continue;
      }
      /// Apply base addr to all instructions
      for (auto& instruction : instruction_list) {
        instruction.offset += kernel_address;
      }

      uint32_t mapping_num = 0;
      res = ptiElfParserGetSourceMapping(parserHandle, kernel_idx, 0, nullptr, &mapping_num);
      if (res != PTI_SUCCESS) {
        std::cerr << "[WARNING] : Failed to get source mapping for kernel ID: " << kernel_idx
                  << std::endl;
        continue;
      }

      std::vector<SourceMapping> line_info_list(mapping_num);
      res = ptiElfParserGetSourceMapping(parserHandle, kernel_idx, mapping_num,
                                         line_info_list.data(), nullptr);
      if (res != PTI_SUCCESS) {
        std::cerr << "[WARNING] : No source mapping found for kernel ID: " << kernel_idx
                  << std::endl;
        continue;
      }

      std::vector<std::string> file_list;
      uint32_t max_files = 0;
      for (const auto& line : line_info_list) {
        max_files = line.file_id > max_files ? line.file_id : max_files;
      }
      file_list.resize(max_files);
      for (const auto& line : line_info_list) {
        file_list[line.file_id - 1] = line.file_name;
      }

      std::vector<SourceFileInfo> source_info_list;
      for (size_t i = 0; i < file_list.size(); ++i) {
        if (file_list[i].size() > 0 &&
            file_list[i].find_last_of("0123456789") == file_list[i].size() - 1) {
          std::vector<SourceLine> line_list = GetSource(*kernel);
          if (line_list.size() == 0) {
            std::cerr << "[WARNING] Kernel sources are not found" << std::endl;
            continue;
          }

          PTI_ASSERT(i + 1 < (std::numeric_limits<uint32_t>::max)());
          uint32_t file_id = static_cast<uint32_t>(i) + 1;
          source_info_list.push_back({file_id, "Kernel Source", line_list});
          break;
        }
      }

      if (source_info_list.size() == 0) {
        std::cerr << "[WARNING] : Unable to find kernel source files for kernel: " << kernel_name
                  << std::endl;
        res = ptiElfParserDestroy(&parserHandle);
        PTI_ASSERT(res == PTI_SUCCESS);
        PTI_ASSERT(parserHandle == nullptr);
        return;
      }

      collector->AddKernel(kernel_name, instruction_list, line_info_list, source_info_list);

      break;
    }

    res = ptiElfParserDestroy(&parserHandle);
    PTI_ASSERT(res == PTI_SUCCESS);
    PTI_ASSERT(parserHandle == nullptr);
  }

  static void Callback(cl_function_id function, cl_callback_data* callback_data, void* user_data) {
    if (function == CL_FUNCTION_clBuildProgram) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterBuildProgram(callback_data);
      } else {
        OnExitBuildProgram(callback_data);
      }
    } else if (function == CL_FUNCTION_clCreateKernel) {
      if (callback_data->site == CL_CALLBACK_SITE_EXIT) {
        OnExitCreateKernel(callback_data, user_data);
      }
    }
  }

 private:  // Data
  ClApiTracer* tracer_ = nullptr;
  cl_device_id device_ = nullptr;

  std::mutex lock_;
  KernelDebugInfoMap kernel_debug_info_map_;
};

#endif  // PTI_SAMPLES_CL_DEBUG_INFO_CL_DEBUG_INFO_COLLECTOR_H_
