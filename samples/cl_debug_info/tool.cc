//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <string.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "cl_tracer.h"
#include "cl_utils.h"
#include "igc_binary_decoder.h"
#include "gen_symbols_decoder.h"

#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101
#define CL_PROGRAM_DEBUG_INFO_INTEL       0x4100

const char* kDebugFlag = "-gline-tables-only";

struct SourceLine {
  uint32_t number;
  std::string text;
};

ClTracer* tracer = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_debug_info[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {}

// Internal Tool Functionality ////////////////////////////////////////////////

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
  status = clGetProgramInfo(program, CL_PROGRAM_SOURCE, length,
                            source.data(), nullptr);
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

static std::vector<uint8_t> GetBinary(cl_kernel kernel,
                                      cl_device_id device) {
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
  status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
                            device_list.size() * sizeof(size_t),
                            binary_size_list.data(), nullptr);
  if (status != CL_SUCCESS || binary_size_list[target_id] == 0) {
    return std::vector<uint8_t>();
  }

  uint8_t** binary_list = new uint8_t* [device_list.size()];
  PTI_ASSERT(binary_list != nullptr);
  for (size_t i = 0; i < device_list.size(); ++i) {
    if (binary_size_list[i] > 0) {
      binary_list[i] = new uint8_t[binary_size_list[i]];
      PTI_ASSERT(binary_list[i] != nullptr);
    } else {
      binary_list[i] = nullptr;
    }
  }

  status = clGetProgramInfo(program, CL_PROGRAM_BINARIES,
                            device_list.size() * sizeof(uint8_t*),
                            binary_list, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  std::vector<uint8_t> binary(binary_size_list[target_id]);
  memcpy(binary.data(), binary_list[target_id],
         binary_size_list[target_id] * sizeof(uint8_t));

  for (size_t i = 0; i < device_list.size(); ++i) {
    delete[] binary_list[i];
  }
  delete[] binary_list;

  return binary;
}

static std::vector<uint8_t> GetDebugSymbols(cl_kernel kernel,
                                            cl_device_id device) {
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
                            device_list.size() * sizeof(size_t),
                            debug_size_list.data(), nullptr);
  if (status != CL_SUCCESS || debug_size_list[target_id] == 0) {
    return std::vector<uint8_t>();
  }

  uint8_t** debug_symbols_list = new uint8_t* [device_list.size()];
  PTI_ASSERT(debug_symbols_list != nullptr);
  for (size_t i = 0; i < device_list.size(); ++i) {
    if (debug_size_list[i] > 0) {
      debug_symbols_list[i] = new uint8_t[debug_size_list[i]];
      PTI_ASSERT(debug_symbols_list[i] != nullptr);
    } else {
      debug_symbols_list[i] = nullptr;
    }
  }

  status = clGetProgramInfo(program, CL_PROGRAM_DEBUG_INFO_INTEL,
                            device_list.size() * sizeof(uint8_t*),
                            debug_symbols_list, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  std::vector<uint8_t> debug_symbols(debug_size_list[target_id]);
  memcpy(debug_symbols.data(), debug_symbols_list[target_id],
         debug_size_list[target_id] * sizeof(uint8_t));

  for (size_t i = 0; i < device_list.size(); ++i) {
    delete[] debug_symbols_list[i];
  }
  delete[] debug_symbols_list;

  return debug_symbols;
}

static void PrintResults(const std::vector<SourceLine>& line_list,
                         const std::vector<Instruction>& instruction_list,
                         const std::vector<LineInfo>& line_info) {
  int last_instruction_address = instruction_list.back().offset;

  // Print instructions with no corresponding source line
  for (size_t i = 0; i < line_info.size(); ++i) {
    if (line_info[i].line == 0) {
      int start_address = line_info[i].address;
      int end_address = (i + 1 < line_info.size()) ?
        line_info[i + 1].address : last_instruction_address;

      for (auto instruction : instruction_list) {
        if (instruction.offset >= start_address &&
            instruction.offset < end_address) {
          std::cout << "\t\t[" << "0x" << std::setw(3) << std::setfill('0') <<
            std::hex << std::uppercase << instruction.offset << "] " <<
            instruction.text << std::endl;
        }
      }
    }
  }

  // Print instructions for corresponding source line
  for (auto line : line_list) {
    std::cout << "[" << std::setw(3) << std::setfill(' ') << std::dec <<
      line.number << "] " << line.text << std::endl;

    for (size_t i = 0; i < line_info.size(); ++i) {
      if (line_info[i].line == line.number) {
        int start_address = line_info[i].address;
        int end_address = (i + 1 < line_info.size()) ?
          line_info[i + 1].address : last_instruction_address;

        for (auto instruction : instruction_list) {
          if (instruction.offset >= start_address &&
              instruction.offset < end_address) {
            std::cout << "\t\t[" << "0x" << std::setw(3) <<
              std::setfill('0') << std::hex << std::uppercase <<
              instruction.offset << "] " << instruction.text << std::endl;
          }
        }
      }
    }
  }

  std::cout << "[INFO] Job is successfully completed" << std::endl;
}

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

  std::string* build_options =
    reinterpret_cast<std::string*>(data->correlationData[0]);
  if (build_options != nullptr) {
    delete build_options;
  }
}

static void OnExitCreateKernel(cl_callback_data* data, void* user_data) {
  PTI_ASSERT(data != nullptr);

  cl_kernel* kernel =
    reinterpret_cast<cl_kernel*>(data->functionReturnValue);
  if (*kernel == nullptr) {
    return;
  }

  PTI_ASSERT(user_data != nullptr);
  cl_device_id device = reinterpret_cast<cl_device_id>(user_data);

  std::string kernel_name = utils::cl::GetKernelName(*kernel);
  std::string device_name = utils::cl::GetDeviceName(device);

  std::vector<SourceLine> line_list = GetSource(*kernel);
  if (line_list.size() == 0) {
    std::cout << "[WARNING] Kernel sources are not found" << std::endl;
    return;
  }

  std::vector<uint8_t> binary = GetBinary(*kernel, device);
  if (binary.size() == 0) {
    std::cout << "[WARNING] Kernel binaries are not found" << std::endl;
    return;
  }
  
  std::vector<uint8_t> symbols = GetDebugSymbols(*kernel, device);
  if (symbols.size() == 0) {
    std::cout << "[WARNING] Kernel symbols are not found" << std::endl;
    return;
  }

  PTI_ASSERT(binary.size() < (std::numeric_limits<uint32_t>::max)());
  ElfParser elf_parser(binary.data(),
                       static_cast<uint32_t>(binary.size()));
  std::vector<uint8_t> igc_binary = elf_parser.GetGenBinary();
  if (igc_binary.size() == 0) {
    std::cout << "[WARNING] Unable to get GEN binary" << std::endl;
    return;
  }

  IgcBinaryDecoder binary_decoder(igc_binary);
  std::vector<Instruction> instruction_list =
    binary_decoder.Disassemble(kernel_name);
  if (instruction_list.size() == 0) {
    std::cout << "[WARNING] Unable to decode kernel binary" << std::endl;
    return;
  }

  GenSymbolsDecoder symbols_decoder(symbols);
  std::vector<std::string> file_names =
    symbols_decoder.GetFileNames(kernel_name);
  if (file_names.size() == 0) {
    std::cout << "[WARNING] Unable to find source files" << std::endl;
    return;
  }

  size_t target_file_id = file_names.size();
  for (size_t i = 0; i < file_names.size(); ++i) {
    if (file_names[i].find_first_not_of("0123456789") ==
        std::string::npos) {
      target_file_id = i;
    }
  }

  if (target_file_id == file_names.size()) {
    std::cout << "[WARNING] Unable to find source files" << std::endl;
    return;
  }

  PTI_ASSERT(target_file_id < (std::numeric_limits<uint32_t>::max)());
  std::vector<LineInfo> line_info = symbols_decoder.GetLineInfo(
      kernel_name, static_cast<uint32_t>(target_file_id) + 1);
  if (line_info.size() == 0) {
    std::cout << "[WARNING] Unable to find kernel symbols" << std::endl;
    return;
  }

  std::string prologue = "======== " + kernel_name + " ========";
  std::string epilogue(prologue.size(), '=');
  std::cout << prologue << std::endl;
  PrintResults(line_list, instruction_list, line_info);
  std::cout << epilogue << std::endl;
}

static void Callback(cl_function_id function,
                     cl_callback_data* callback_data,
                     void* user_data) {
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

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cout << "[WARNING] Unable to find target GPU device for tracing" <<
      std::endl;
    return;
  }

  tracer = new ClTracer(device, Callback, device);
  if (tracer == nullptr || !tracer->IsValid()) {
    std::cout << "[WARNING] Unable to create OpenCL tracer " <<
      "for target device" << std::endl;
    if (tracer != nullptr) {
      delete tracer;
      tracer = nullptr;
    }
    return;
  }

  bool set = true;
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clBuildProgram);
  set = set && tracer->SetTracingFunction(CL_FUNCTION_clCreateKernel);
  PTI_ASSERT(set);

  bool enabled = tracer->Enable();
  PTI_ASSERT(enabled);
}

void DisableProfiling() {
  if (tracer != nullptr) {
    bool disabled = tracer->Disable();
    PTI_ASSERT(disabled);
    delete tracer;
  }
}