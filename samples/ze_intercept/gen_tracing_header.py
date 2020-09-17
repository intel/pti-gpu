import os
import sys

import parse_ze_headers

def gen_header(f):
  f.write("//==============================================================\n")
  f.write("// Copyright (C) 2020 Intel Corporation\n")
  f.write("//\n")
  f.write("// SPDX-License-Identifier: MIT\n")
  f.write("//==============================================================\n")
  f.write("\n")
  f.write("/* This file is auto-generated based on current version of Level Zero */\n")
  f.write("\n")
  f.write("#pragma once\n")
  f.write("\n")
  f.write("#include <chrono>\n")
  f.write("#include <iostream>\n")
  f.write("\n")
  f.write("#include <assert.h>\n")
  f.write("\n")
  f.write("#include <level_zero/zet_api.h>\n")
  f.write("\n")
  f.write("#include \"ze_intercept.h\"\n")
  f.write("\n")
  f.write("namespace ze_tracing {\n")
  f.write("\n")  

def gen_footer(f):
  f.write("} // namespace ze_tracing")

def gen_enum_out(f, enum_map, enum_name, param_name):
  assert enum_name in enum_map
  f.write("    switch (" + param_name + ") {\n")
  for name, value in enum_map[enum_name].items():
    f.write("      case " + value + ":\n")
    f.write("        std::cerr << \"" + name + "\";\n")
    f.write("        break;\n")
  f.write("      default:\n")
  f.write("        std::cerr << \"<UNKNOWN>\";\n")
  f.write("        break;\n")
  f.write("    }\n")

def gen_result_converter(f, enum_map):
  assert "ze_result_t" in enum_map
  f.write("const char* GetResultString(unsigned result) {\n")
  f.write("  switch (result) {\n")
  for name, value in enum_map["ze_result_t"].items():
    f.write("    case " + value + ":\n")
    f.write("      return \"" + name + "\";\n")
  f.write("    default:\n")
  f.write("      break;\n")
  f.write("  }\n")
  f.write("  return \"<UNKNOWN>\";\n")
  f.write("}\n")
  f.write("\n")

def gen_callback_body_enter(f, func, param_map, enum_map):
  f.write("  ZeIntercept* intercept = reinterpret_cast<ZeIntercept*>(global_user_data);\n")
  f.write("  if (intercept->CheckOption(ZEI_CALL_LOGGING)) {\n")
  f.write("    std::cerr << \">>>> \";\n")
  f.write("    if (intercept->CheckOption(ZEI_CALL_LOGGING_TIMESTAMPS)) {\n")
  f.write("      std::cerr << \"[\" << intercept->GetTimestamp() << \"] \";\n")
  f.write("    }\n")
  f.write("    std::cerr << \"" + func + "\" << ")
  params = param_map[func]
  if len(params) > 0:
    f.write("\":\"")
  f.write(";\n")
  for name, type in params:
    if type == "ze_ipc_mem_handle_t" or type == "ze_ipc_event_pool_handle_t":
      f.write("    std::cerr << \" " + name + " = \" << (params->p" + name + ");\n")
    else:
      f.write("    std::cerr << \" " + name + " = \" << *(params->p" + name + ");\n")
      if type in enum_map:
        f.write("    std::cerr << \" (\";\n")
        gen_enum_out(f, enum_map, type, "*(params->p" + name + ")")
        f.write("    std::cerr << \")\";\n")
  f.write("    std::cerr << std::endl;\n")
  f.write("  }\n")
  f.write("  if (intercept->CheckOption(ZEI_HOST_TIMING)) {\n")
  f.write("    std::chrono::time_point<std::chrono::steady_clock>* start =\n")
  f.write("      reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(\n")
  f.write("          instance_user_data);\n")
  f.write("    *start = std::chrono::steady_clock::now();\n")
  f.write("  }\n")


def gen_callback_body_exit(f, func):
  f.write("  uint64_t duration = 0;\n")
  f.write("  ZeIntercept* intercept = reinterpret_cast<ZeIntercept*>(global_user_data);\n")
  f.write("  if (intercept->CheckOption(ZEI_HOST_TIMING)) {\n")
  f.write("    std::chrono::time_point<std::chrono::steady_clock>* start =\n")
  f.write("      reinterpret_cast<std::chrono::time_point<std::chrono::steady_clock>*>(\n")
  f.write("          instance_user_data);\n")
  f.write("    std::chrono::duration<uint64_t, std::nano> time = std::chrono::steady_clock::now() - *start;\n")
  f.write("    duration = time.count();\n")
  f.write("    intercept->AddHostTime(\"" + func + "\", duration);\n")
  f.write("  }\n")
  f.write("  if (intercept->CheckOption(ZEI_CALL_LOGGING)) {\n")
  f.write("    std::cerr << \"<<<< \";\n")
  f.write("    if (intercept->CheckOption(ZEI_CALL_LOGGING_TIMESTAMPS)) {\n")
  f.write("      std::cerr << \"[\" << intercept->GetTimestamp() << \"] \";\n")
  f.write("    }\n")
  f.write("    std::cerr << \"" + func + "\";\n")
  f.write("    if (intercept->CheckOption(ZEI_HOST_TIMING)) {\n")
  f.write("      std::cerr << \" [\" << duration << \" ns]\";\n")
  f.write("    }\n")
  f.write("    std::cerr << \" -> \" << GetResultString(result) << \n")
  f.write("      \" (\" << result << \")\" << std::endl;\n")
  f.write("  }\n")

def gen_callbacks(f, func_list, group_map, param_map, enum_map):
  for func in func_list:
    assert func in group_map
    group, callback = group_map[func]
    callback_name = callback[0]
    callback_cond = callback[1]
    if callback_cond:
      f.write("#if " + callback_cond + "\n")
    f.write("void " + func + "OnEnter(\n")
    f.write("    " + parse_ze_headers.get_param_struct_name(func) + "* params,\n")
    f.write("    ze_result_t result,\n")
    f.write("    void* global_user_data,\n")
    f.write("    void** instance_user_data) {\n")
    gen_callback_body_enter(f, func, param_map, enum_map)
    f.write("}\n")
    f.write("\n")
    f.write("void " + func + "OnExit(\n")
    f.write("    " + parse_ze_headers.get_param_struct_name(func) + "* params,\n")
    f.write("    ze_result_t result,\n")
    f.write("    void* global_user_data,\n")
    f.write("    void** instance_user_data) {\n")
    gen_callback_body_exit(f, func)
    f.write("}\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
    f.write("\n")

def gen_api(f, func_list, group_map):
  f.write("void SetTracingFunctions(zet_tracer_exp_handle_t tracer) {\n")
  f.write("  zet_core_callbacks_t prologue = {};\n")
  f.write("  zet_core_callbacks_t epilogue = {};\n")
  f.write("\n")
  for func in func_list:
    assert func in group_map
    group, callback = group_map[func]
    group_name = group[0]
    group_cond = group[1]
    assert not group_cond
    callback_name = callback[0]
    callback_cond = callback[1]
    if callback_cond:
      f.write("#if " + callback_cond + "\n")
    f.write("  prologue." + group_name + "." + callback_name + " = " + func + "OnEnter;\n")
    f.write("  epilogue." + group_name + "." + callback_name + " = " + func + "OnExit;\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
  f.write("\n")
  f.write("  ze_result_t status = ZE_RESULT_SUCCESS;\n")
  f.write("  status = zetTracerExpSetPrologues(tracer, &prologue);\n")
  f.write("  PTI_ASSERT(status == ZE_RESULT_SUCCESS);\n")
  f.write("  status = zetTracerExpSetEpilogues(tracer, &epilogue);\n")
  f.write("  PTI_ASSERT(status == ZE_RESULT_SUCCESS);\n")
  f.write("}\n")
  f.write("\n")

def main():
  if len(sys.argv) < 3:
    print("Usage: python gen_tracing_header.py <output_include_path> <l0_include_path>")
    return
  
  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  dst_file_path = os.path.join(dst_path, "tracing.gen")
  if (os.path.isfile(dst_file_path)):
    os.remove(dst_file_path)

  dst_file = open(dst_file_path, "wt")

  l0_path = sys.argv[2]
  l0_file_path = os.path.join(l0_path, "ze_api.h")
  
  l0_file = open(l0_file_path, "rt")
  func_list = parse_ze_headers.get_func_list(l0_file)
  group_map = parse_ze_headers.get_callback_group_map(l0_file)
  param_map = parse_ze_headers.get_param_map(l0_file)
  enum_map = parse_ze_headers.get_enum_map(l0_path)

  gen_header(dst_file)
  gen_result_converter(dst_file, enum_map)
  gen_callbacks(dst_file, func_list, group_map, param_map, enum_map)
  gen_api(dst_file, func_list, group_map)

  gen_footer(dst_file)

  l0_file.close()
  dst_file.close()

if __name__ == "__main__":
  main()