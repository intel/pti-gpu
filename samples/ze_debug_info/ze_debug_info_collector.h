//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_
#define PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_

#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <vector>

#include <level_zero/zet_api.h>

#include "elf_parser.h"
#include "gen_symbols_decoder.h"
#include "igc_binary_decoder.h"
#include "utils.h"
#include "ze_utils.h"

struct SourceLine {
  uint32_t number;
  std::string text;
};

struct SourceFileInfo {
  uint32_t file_id;
  std::string file_name;
  std::vector<SourceLine> source_line_list;
};

struct KernelDebugInfo {
  std::vector<Instruction> instruction_list;
  std::vector<LineInfo> line_info_list;
  std::vector<SourceFileInfo> source_info_list;
};

using KernelDebugInfoMap = std::map<std::string, KernelDebugInfo>;

class ZeDebugInfoCollector {
 public: // User Interface
  static ZeDebugInfoCollector* Create(ze_driver_handle_t driver) {
    PTI_ASSERT(driver != nullptr);

    ze_context_handle_t context = utils::ze::GetContext(driver);
    PTI_ASSERT(context != nullptr);

    ZeDebugInfoCollector* collector = new ZeDebugInfoCollector(context);
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_exp_desc_t tracer_desc = {
        ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zet_tracer_exp_handle_t tracer = nullptr;
    status = zetTracerExpCreate(context, &tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr <<
        "[WARNING] Unable to create Level Zero tracer for target context" <<
        std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  static void InstructionCallback(int32_t offset, void* data) {}

  static void PrintKernelDebugInfo(
      std::string kernel_name,
      const KernelDebugInfo& kernel_debug_info,
      decltype(InstructionCallback)* callback = InstructionCallback,
      void* callback_data = nullptr) {
    PTI_ASSERT(!kernel_name.empty());

    std::cerr << "===== Kernel: " << kernel_name << " =====" << std::endl;

    const std::vector<Instruction>& instruction_list =
      kernel_debug_info.instruction_list;
    PTI_ASSERT(instruction_list.size() > 0);

    int last_instruction_address = instruction_list.back().offset;

    const std::vector<LineInfo>& line_info =
      kernel_debug_info.line_info_list;
    PTI_ASSERT(line_info.size() > 0);

    const std::vector<SourceFileInfo>& source_info_list =
      kernel_debug_info.source_info_list;
    PTI_ASSERT(source_info_list.size() > 0);

    // Print instructions with no corresponding file
    std::cerr << "=== File: Unknown ===" << std::endl;
    for (auto& instruction : instruction_list) {
      bool found = false;

      for (size_t l = 0; l < line_info.size(); ++l) {
        int start_address = line_info[l].address;
        int end_address = (l + 1 < line_info.size()) ?
                          line_info[l + 1].address :
                          last_instruction_address;

        if (instruction.offset >= start_address &&
            instruction.offset < end_address) {
          found = true;
          break;
        }
      }

      if (!found) {
        std::cerr << "\t\t[" << "0x" << std::setw(5) <<
          std::setfill('0') << std::hex << std::uppercase <<
          instruction.offset << "] " << instruction.text;
        callback(instruction.offset, callback_data);
        std::cerr << std::endl;
      }
    }

    // Print info per file
    for (auto& source_info : source_info_list) {
      std::cerr << "=== File: " << source_info.file_name.c_str() <<
        " ===" << std::endl;

      const std::vector<SourceLine> line_list = source_info.source_line_list;
      PTI_ASSERT(line_list.size() > 0);

      // Print instructions with no corresponding source line
      for (size_t l = 0; l < line_info.size(); ++l) {
        if (line_info[l].line == 0) {
          int start_address = line_info[l].address;
          int end_address = (l + 1 < line_info.size()) ?
                            line_info[l + 1].address :
                            last_instruction_address;

          for (auto instruction : instruction_list) {
            if (instruction.offset >= start_address &&
                instruction.offset < end_address &&
                source_info.file_id == line_info[l].file) {
              std::cerr << "\t\t[" << "0x" << std::setw(5) <<
                std::setfill('0') << std::hex << std::uppercase <<
                instruction.offset << "] " << instruction.text;
              callback(instruction.offset, callback_data);
              std::cerr << std::endl;
            }
          }
        }
      }

      // Print instructions for corresponding source line
      for (auto line : line_list) {
        std::cerr << "[" << std::setw(5) << std::setfill(' ') << std::dec <<
          line.number << "] " << line.text << std::endl;

        for (size_t l = 0; l < line_info.size(); ++l) {
          if (line_info[l].line == line.number) {
            int start_address = line_info[l].address;
            int end_address = (l + 1 < line_info.size()) ?
                              line_info[l + 1].address :
                              last_instruction_address;

            for (auto instruction : instruction_list) {
              if (instruction.offset >= start_address &&
                  instruction.offset < end_address &&
                  source_info.file_id == line_info[l].file) {
                std::cerr << "\t\t[" << "0x" << std::setw(5) <<
                  std::setfill('0') << std::hex << std::uppercase <<
                  instruction.offset << "] " << instruction.text;
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

  ~ZeDebugInfoCollector() {
    ze_result_t status = ZE_RESULT_SUCCESS;

    if (tracer_ != nullptr) {
      status = zetTracerExpDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }

    PTI_ASSERT(context_ != nullptr);
    status = zeContextDestroy(context_);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  const KernelDebugInfoMap& GetKernelDebugInfoMap() const {
    return kernel_debug_info_map_;
  }

 private: // Implementation Details
  ZeDebugInfoCollector(ze_context_handle_t context) : context_(context) {}

  void EnableTracing(zet_tracer_exp_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t epilogue_callbacks{};
    epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerExpSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zetTracerExpSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddKernel(std::string name,
                 const std::vector<Instruction>& instruction_list,
                 const std::vector<LineInfo>& line_info_list,
                 const std::vector<SourceFileInfo>& source_info_list) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(instruction_list.size() > 0);
    PTI_ASSERT(line_info_list.size() > 0);
    PTI_ASSERT(source_info_list.size() > 0);

    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_debug_info_map_.count(name) == 0);
    kernel_debug_info_map_[name] = {instruction_list, line_info_list, source_info_list};
  }

  static std::vector<SourceLine> ReadSourceFile(const std::string& file_path) {
    std::string abs_path = file_path;
    if (abs_path[0] == '.') {
      abs_path = utils::GetExecutablePath() + abs_path;
    }

    std::ifstream file(abs_path);
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

 private: // Callbacks
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
      std::cerr << "[WARNING] Unable to get GEN binary" << std::endl;
      return;
    }

    IgcBinaryDecoder binary_decoder(igc_binary);
    std::vector<Instruction> instruction_list =
      binary_decoder.Disassemble(kernel_name);
    if (instruction_list.size() == 0) {
      std::cerr << "[WARNING] Unable to decode kernel binary" << std::endl;
      return;
    }

    size_t debug_info_size = 0;
    status = zetModuleGetDebugInfo(
        module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
        &debug_info_size, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (debug_info_size == 0) {
      std::cerr << "[WARNING] Unable to find kernel symbols" << std::endl;
      return;
    }

    std::vector<uint8_t> debug_info(debug_info_size);
    status = zetModuleGetDebugInfo(
        module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
        &debug_info_size, debug_info.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    GenSymbolsDecoder symbols_decoder(debug_info);
    std::vector<std::string> file_list =
      symbols_decoder.GetFileList(kernel_name);
    if (file_list.size() == 0) {
      std::cerr << "[WARNING] Unable to find source files" << std::endl;
      return;
    }

    std::vector<LineInfo> line_info_list =
      symbols_decoder.GetLineInfo(kernel_name);
    if (line_info_list.size() == 0) {
      std::cerr << "[WARNING] Unable to decode kernel line info" << std::endl;
      return;
    }

    std::vector<SourceFileInfo> source_info_list;
    for (size_t i = 0; i < file_list.size(); ++i) {
      std::vector<SourceLine> line_list = ReadSourceFile(file_list[i]);
      if (line_list.size() == 0) {
        std::cerr << "[WARNING] Unable to find target source file: " <<
          file_list[i] << std::endl;
        continue;
      }

      PTI_ASSERT(i + 1 < (std::numeric_limits<uint32_t>::max)());
      uint32_t file_id = static_cast<uint32_t>(i) + 1;
      source_info_list.push_back({file_id, file_list[i], line_list});
    }

    if (source_info_list.size() == 0) {
      std::cerr << "[WARNING] Unable to find kernel source files" << std::endl;
      return;
    }

    ZeDebugInfoCollector* collector =
      reinterpret_cast<ZeDebugInfoCollector*>(global_user_data);
    PTI_ASSERT(collector != nullptr);
    collector->AddKernel(kernel_name, instruction_list,
                         line_info_list, source_info_list);
  }

 private:
  zet_tracer_exp_handle_t tracer_ = nullptr;
  ze_context_handle_t context_ = nullptr;

  std::mutex lock_;
  KernelDebugInfoMap kernel_debug_info_map_;
};

#endif // PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_