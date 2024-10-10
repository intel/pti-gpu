//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_
#define PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_

#include <level_zero/layers/zel_tracing_api.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "elf_parser.h"
#include "gen_binary_decoder.h"
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
  std::vector<SourceMapping> line_info_list;
  std::unordered_map<uint32_t, SourceFileInfo> source_info_list;
};

using KernelDebugInfoMap = std::map<std::string, KernelDebugInfo>;

class ZeDebugInfoCollector {
 public:  // User Interface
  static ZeDebugInfoCollector* Create() {
    ZeDebugInfoCollector* collector = new ZeDebugInfoCollector();
    PTI_ASSERT(collector != nullptr);

    ze_result_t status = ZE_RESULT_SUCCESS;
    zel_tracer_desc_t tracer_desc = {ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
    zel_tracer_handle_t tracer = nullptr;
    status = zelTracerCreate(&tracer_desc, &tracer);
    if (status != ZE_RESULT_SUCCESS) {
      std::cerr << "[WARNING] Unable to create Level Zero tracer" << std::endl;
      delete collector;
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

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

    const auto& source_info_list = kernel_debug_info.source_info_list;
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
      std::cerr << "=== File: " << source_info.second.file_name.c_str() << " ===" << std::endl;

      const std::vector<SourceLine> line_list = source_info.second.source_line_list;
      PTI_ASSERT(line_list.size() > 0);

      // Print instructions with no corresponding source line
      for (size_t l = 0; l < line_info.size(); ++l) {
        if (line_info[l].line == 0) {
          uint64_t start_address = line_info[l].address;
          uint64_t end_address =
              (l + 1 < line_info.size()) ? line_info[l + 1].address : last_instruction_address;

          for (auto instruction : instruction_list) {
            if (instruction.offset >= start_address && instruction.offset < end_address &&
                source_info.second.file_id == line_info[l].file_id) {
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
      for (const auto& line : line_list) {
        std::cerr << "[" << std::setw(5) << std::setfill(' ') << std::dec << line.number << "] "
                  << line.text << std::endl;

        for (size_t l = 0; l < line_info.size(); ++l) {
          if (line_info[l].line == line.number) {
            uint64_t start_address = line_info[l].address;
            uint64_t end_address =
                (l + 1 < line_info.size()) ? line_info[l + 1].address : last_instruction_address;

            for (auto instruction : instruction_list) {
              if (instruction.offset >= start_address && instruction.offset < end_address &&
                  source_info.second.file_id == line_info[l].file_id) {
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

  ~ZeDebugInfoCollector() {
    if (tracer_ != nullptr) {
      ze_result_t status = zelTracerDestroy(tracer_);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetEnabled(tracer_, false);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  const KernelDebugInfoMap& GetKernelDebugInfoMap() const { return kernel_debug_info_map_; }

 private:  // Implementation Details
  ZeDebugInfoCollector() {}

  void EnableTracing(zel_tracer_handle_t tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    zet_core_callbacks_t epilogue_callbacks{};
    epilogue_callbacks.Kernel.pfnCreateCb = OnExitKernelCreate;

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zelTracerSetEpilogues(tracer_, &epilogue_callbacks);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    status = zelTracerSetEnabled(tracer_, true);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  }

  void AddKernel(std::string name, const std::vector<Instruction>& instruction_list,
                 const std::vector<SourceMapping>& line_info_list,
                 const std::unordered_map<uint32_t, SourceFileInfo>& source_info_list) {
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

 private:  // Callbacks
  static void OnExitKernelCreate(ze_kernel_create_params_t* params, ze_result_t result,
                                 void* global_user_data, void** instance_user_data) {
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

    size_t debug_info_size = 0;
    status = zetModuleGetDebugInfo(module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF, &debug_info_size,
                                   nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    if (debug_info_size == 0) {
      std::cerr << "[WARNING] Unable to find kernel symbols" << std::endl;
      return;
    }

    std::vector<uint8_t> debug_info(debug_info_size);
    status = zetModuleGetDebugInfo(module, ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF, &debug_info_size,
                                   debug_info.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    pti_result res;

    elf_parser_handle_t parserHandle = nullptr;
    res = ptiElfParserCreate(debug_info.data(), static_cast<uint32_t>(debug_info.size()),
                             &parserHandle);
    if (res != PTI_SUCCESS || parserHandle == nullptr) {
      std::cerr << "[WARNING] : Cannot create elf parser" << std::endl;
      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);
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

      std::unordered_map<uint32_t, SourceFileInfo> source_info_list;
      for (const auto& line : line_info_list) {
        if (source_info_list.find(line.file_id) != source_info_list.end()) {
          continue;
        }
        std::filesystem::path fullpath =
            std::filesystem::path(std::string(line.file_path)) / line.file_name;
        std::vector<SourceLine> line_list = ReadSourceFile(fullpath);
        if (line_list.size() == 0) {
          std::cerr << "[WARNING] : Unable to find target source file for kernel: '" << kernel_name
                    << "' : " << std::string(line.file_path) + line.file_name << std::endl;
          continue;
        }

        PTI_ASSERT(line.file_id < (std::numeric_limits<uint32_t>::max)());

        source_info_list[line.file_id] = {static_cast<uint32_t>(line.file_id), line.file_name,
                                          line_list};
      }

      if (source_info_list.size() == 0) {
        std::cerr << "[WARNING] : Unable to find kernel source files for kernel: " << kernel_name
                  << std::endl;
        res = ptiElfParserDestroy(&parserHandle);
        PTI_ASSERT(res == PTI_SUCCESS);
        PTI_ASSERT(parserHandle == nullptr);
        return;
      }

      ZeDebugInfoCollector* collector = reinterpret_cast<ZeDebugInfoCollector*>(global_user_data);
      PTI_ASSERT(collector != nullptr);
      collector->AddKernel(kernel_name, instruction_list, line_info_list, source_info_list);

      res = ptiElfParserDestroy(&parserHandle);
      PTI_ASSERT(res == PTI_SUCCESS);
      PTI_ASSERT(parserHandle == nullptr);

      break;
    }
  }

 private:
  zel_tracer_handle_t tracer_ = nullptr;

  std::mutex lock_;
  KernelDebugInfoMap kernel_debug_info_map_;
};

#endif  // PTI_SAMPLES_ZE_DEBUG_INFO_ZE_DEBUG_INFO_COLLECTOR_H_
