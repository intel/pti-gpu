//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <mutex>
#include <set>

#include "elf_parser.h"
#include "gen_symbols_decoder.h"
#include "igc_binary_decoder.h"
#include "utils.h"
#include "ze_utils.h"

struct SourceLine {
  uint32_t number;
  std::string text;
};

zet_tracer_exp_handle_t tracer = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./ze_debug_info[.exe] <application> <args>" <<
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
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static std::vector<SourceLine> ReadSourceFile(const std::string& file_name) {
  std::string file_path = utils::GetExecutablePath() + file_name;
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return std::vector<SourceLine>();
  }

  std::vector<SourceLine> line_list;

  uint32_t number = 1;
  std::string text;
  while (std::getline(file, text)) {
    line_list.push_back({number, text});
    ++number;
  }

  return line_list;
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

static void OnExitKernelCreate(ze_kernel_create_params_t *params,
                               ze_result_t result,
                               void *global_user_data,
                               void **instance_user_data) {
  if (result != ZE_RESULT_SUCCESS) {
    return;
  }

  ze_result_t status = ZE_RESULT_SUCCESS;

  ze_module_handle_t module = *(params->phModule);
  PTI_ASSERT(module != nullptr);

  const ze_kernel_desc_t* desc = *(params->pdesc);
  PTI_ASSERT(desc != nullptr);

  const char* kernel_name = desc->pKernelName;
  PTI_ASSERT(kernel_name != nullptr);

  size_t native_binary_size = 0;
  status = zeModuleGetNativeBinary(module, &native_binary_size, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<uint8_t> native_binary(native_binary_size);
  status = zeModuleGetNativeBinary(
      module, &native_binary_size, native_binary.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  PTI_ASSERT(native_binary.size() < (std::numeric_limits<uint32_t>::max)());
  ElfParser elf_parser(native_binary.data(),
                       static_cast<uint32_t>(native_binary.size()));
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

  size_t debug_info_size = 0;
  status = zetModuleGetDebugInfo(
      module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
      &debug_info_size, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  if (debug_info_size == 0) {
    std::cout << "[WARNING] Unable to find kernel symbols" << std::endl;
    return;
  }

  std::vector<uint8_t> debug_info(debug_info_size);
  status = zetModuleGetDebugInfo(
      module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
      &debug_info_size, debug_info.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  GenSymbolsDecoder symbols_decoder(debug_info);
  std::vector<std::string> file_names =
    symbols_decoder.GetFileNames(kernel_name);
  if (file_names.size() == 0) {
    std::cout << "[WARNING] Unable to find source files" << std::endl;
    return;
  }

  size_t target_file_id = file_names.size();
  for (size_t i = 0; i < file_names.size(); ++i) {
    if (file_names[i].find(".cl") != std::string::npos) {
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
    std::cout << "[WARNING] Unable to decode kernel symbols" << std::endl;
    return;
  }

  std::vector<SourceLine> line_list =
    ReadSourceFile(file_names[target_file_id]);
  if (line_list.size() == 0) {
    std::cout << "[WARNING] Unable to find source files" << std::endl;
    return;
  }

  std::string prologue = "======== " + std::string(kernel_name) +
    " ========";
  std::string epilogue(prologue.size(), '=');
  std::cout << prologue << std::endl;
  PrintResults(line_list, instruction_list, line_info);
  std::cout << epilogue << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_device_handle_t device = nullptr;
  ze_driver_handle_t driver = nullptr;
  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU, device, driver);
  if (device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target device" << std::endl;
    return;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);
  PTI_ASSERT(context != nullptr);

  zet_tracer_exp_desc_t tracer_desc = {
      ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, nullptr};
  status = zetTracerExpCreate(context, &tracer_desc, &tracer);
  if (status != ZE_RESULT_SUCCESS) {
    std::cout <<
      "[WARNING] Unable to create Level Zero tracer for target driver" <<
      std::endl;
    return;
  }

  zet_core_callbacks_t epilogue_callbacks{};
  epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;

  status = zetTracerExpSetEpilogues(tracer, &epilogue_callbacks);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zetTracerExpSetEnabled(tracer, true);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

void DisableProfiling() {
  if (tracer != nullptr) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(tracer, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zetTracerExpDestroy(tracer);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }
}