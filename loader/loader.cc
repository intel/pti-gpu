//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "loader.h"

#include <iostream>
#include <vector>

#include <stdio.h>
#include <string.h>

#ifndef TOOL_NAME
#error "TOOL_NAME is not defined"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include "shared_library.h"
#include "utils.h"

static std::string GetLibFileName() {
#if defined(_WIN32)
  return std::string(TOSTRING(TOOL_NAME)) + ".dll";
#else
  return std::string("lib") + TOSTRING(TOOL_NAME) + ".so";
#endif
}

static bool IsFileExists(const char* file_name) {
  PTI_ASSERT(file_name != nullptr);
  FILE* file = nullptr;
#if defined(_WIN32)
  errno_t status = fopen_s(&file, file_name, "rb");
  if (status != 0) {
    file = nullptr;
  }
#else
  file = fopen(file_name, "rb");
#endif
  if (file != nullptr) {
    fclose(file);
    return true;
  }
  return false;
}

#if defined(_WIN32)
static bool CheckBitness(HANDLE parent, HANDLE child) {
  BOOL parentBitness64 = FALSE;
  IsWow64Process(parent, &parentBitness64);

  BOOL childBitness64 = FALSE;
  IsWow64Process(child, &childBitness64);

  if (parentBitness64 != childBitness64) {
    return false;
  }
  return true;
}
#endif

int main(int argc, char* argv[]) {
  std::string library_file_name = GetLibFileName();
  std::string executable_path = utils::GetExecutablePath();
  std::string library_file_path = executable_path + library_file_name;

  if (!IsFileExists(library_file_path.c_str())) {
    std::cout << "[ERROR] Failed to find " <<
      library_file_name << " near the loader" << std::endl;
    return 0;
  }

  SharedLibrary* lib = SharedLibrary::Create(library_file_path);
  if (lib == nullptr) {
    std::cout << "[ERROR] Failed to load " << library_file_name <<
      " library" << std::endl;
    return 0;
  }

  decltype(Usage)* usage = lib->GetSym<decltype(Usage)*>("Usage");
  if (usage == nullptr) {
    std::cout << "[ERROR] Failed to find Usage function in " <<
      library_file_name << std::endl;
    delete lib;
    return 0;
  }

  if (argc < 2) {
    usage();
    delete lib;
    return 0;
  }

  decltype(ParseArgs)* parse_args =
      lib->GetSym<decltype(ParseArgs)*>("ParseArgs");
  if (parse_args == nullptr) {
    std::cout << "[ERROR] Failed to find ParseArgs function in " <<
      library_file_name << std::endl;
    delete lib;
    return 0;
  }

  decltype(SetToolEnv)* set_tool_env =
    lib->GetSym<decltype(SetToolEnv)*>("SetToolEnv");
  if (set_tool_env == nullptr) {
    std::cout << "[ERROR] Failed to find SetToolEnv function in " <<
      library_file_name << std::endl;
    delete lib;
    return 0;
  }

  int app_index = parse_args(argc, argv);
  if (app_index <= 0 || app_index >= argc) {
    if (app_index >= argc) {
      std::cout << "[ERROR] Application to run is not specified" << std::endl;
    } else {
      std::cout << "[ERROR] Invalid command line" << std::endl;
    }
    usage();
    delete lib;
    return 0;
  }
  std::vector<char*> app_args;

  for (int i = app_index; i < argc; ++i) {
    app_args.push_back(argv[i]);
  }
  app_args.push_back(nullptr);

  set_tool_env();

#if defined(_WIN32)

  BOOL ok = FALSE;
  DWORD status = -1;

  std::string command_line = app_args[0];
  for (size_t i = 1; i < app_args.size() - 1; ++i) {
    command_line += " ";
    command_line += app_args[i];
  }

  decltype(Init)* init = lib->GetSym<decltype(Init)*>("Init");
  if (init == nullptr) {
    std::cout << "[ERROR] Failed to find Init function in " <<
      library_file_name << std::endl;
    delete lib;
    return 0;
  }

  PROCESS_INFORMATION pinfo = { 0 };
  STARTUPINFOA sinfo = { 0 };
  sinfo.cb = sizeof(sinfo);
  ok = CreateProcessA(
    nullptr, const_cast<char*>(command_line.c_str()),
    nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
    nullptr, &sinfo, &pinfo);
  if (!ok) {
    std::cout << "[ERROR] Failed to launch target application: " <<
      command_line.c_str() << std::endl;
    usage();
    delete lib;
    return 0;
  }

  if (CheckBitness(GetCurrentProcess(), pinfo.hProcess)) {
    void* library_path_memory =
      VirtualAllocEx(pinfo.hProcess, nullptr, library_file_path.size() + 1,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    PTI_ASSERT(library_path_memory != nullptr);

    ok = WriteProcessMemory(
      pinfo.hProcess, library_path_memory, library_file_path.c_str(),
      library_file_path.size() + 1, nullptr);
    PTI_ASSERT(ok);

    HMODULE kernel32_module = GetModuleHandleA("kernel32.dll");
    PTI_ASSERT(kernel32_module != nullptr);
    LPTHREAD_START_ROUTINE remote_routine =
      reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32_module, "LoadLibraryA"));
    PTI_ASSERT(remote_routine != nullptr);
    HANDLE load_thread = CreateRemoteThread(
      pinfo.hProcess, nullptr, 0, remote_routine,
      library_path_memory, 0, nullptr);
    PTI_ASSERT(load_thread != nullptr);

    status = WaitForSingleObject(load_thread, INFINITE);
    PTI_ASSERT(status == WAIT_OBJECT_0);
    ok = CloseHandle(load_thread);
    PTI_ASSERT(ok);
    ok = VirtualFreeEx(pinfo.hProcess, library_path_memory, 0, MEM_RELEASE);
    PTI_ASSERT(ok);

    HANDLE init_thread = CreateRemoteThread(
      pinfo.hProcess, nullptr, 0, init, nullptr, 0, nullptr);
    PTI_ASSERT(init_thread != nullptr);
    status = WaitForSingleObject(init_thread, INFINITE);
    PTI_ASSERT(status == WAIT_OBJECT_0);
    ok = CloseHandle(init_thread);
    PTI_ASSERT(ok);
  }

  status = ResumeThread(pinfo.hThread);
  PTI_ASSERT(status != -1);
  status = WaitForSingleObject(pinfo.hProcess, INFINITE);
  PTI_ASSERT(status == WAIT_OBJECT_0);

  ok = CloseHandle(pinfo.hThread);
  PTI_ASSERT(ok);
  ok = CloseHandle(pinfo.hProcess);
  PTI_ASSERT(ok);

#else

  utils::SetEnv("LD_PRELOAD", library_file_path.c_str());
  utils::SetEnv("PTI_ENABLE", "1");

  if (execvp(app_args[0], app_args.data())) {
    std::cout << "[ERROR] Failed to launch target application: " <<
      app_args[0] << std::endl;
    delete lib;
    return 0;
  }

#endif

  delete lib;
  return 0;
}