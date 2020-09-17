import os
import sys
import re

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
  f.write("#include <level_zero/zet_api.h>\n")
  f.write("\n")
  f.write("namespace ze_tracing {\n")
  f.write("\n")
  f.write("typedef enum _callback_site_t {\n")
  f.write("  ZE_CALLBACK_SITE_ENTER,\n")
  f.write("  ZE_CALLBACK_SITE_EXIT\n")
  f.write("} callback_site_t;\n")
  f.write("\n")
  f.write("typedef struct _callback_data_t {\n")
  f.write("  callback_site_t site;\n")
  f.write("  uint64_t *correlation_data;\n")
  f.write("  const char *function_name;\n")
  f.write("  const void *function_params;\n")
  f.write("  ze_result_t function_return_value;\n")
  f.write("} callback_data_t;\n")
  f.write("\n")  

def gen_footer(f):
  f.write("} // namespace ze_tracing")

def gen_func_list(f, func_list, group_map):
  f.write("typedef enum _function_id_t {\n")
  i = 0
  for func in func_list:
    assert func in group_map
    group, callback = group_map[func]
    callback_name = callback[0]
    callback_cond = callback[1]
    if callback_cond:
      f.write("#if " + callback_cond + "\n")
    f.write("  ZE_FUNCTION_" + func + " = " + str(i) + ",\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
    i += 1
  f.write("  ZE_FUNCTION_COUNT = " + str(i) + "\n")
  f.write("} function_id_t;\n")
  f.write("\n")

def gen_callback_type(f):
  f.write("typedef void (*tracing_callback_t)(\n")
  f.write("  function_id_t fid, callback_data_t* callback_data, void* user_data);\n")
  f.write("\n")

def gen_global_data_type(f):
  f.write("typedef struct _global_data_t{\n")
  f.write("  tracing_callback_t callback = nullptr;\n")
  f.write("  void* user_data = nullptr;\n")
  f.write("} global_data_t;\n")
  f.write("\n")

def gen_callback_body(f, func, enter):
  f.write("  global_data_t* data =\n")
  f.write("    reinterpret_cast<global_data_t*>(global_user_data);\n")
  f.write("  PTI_ASSERT(data != nullptr);\n")
  f.write("\n")
  f.write("  callback_data_t callback_data = {\n")
  if enter:
    f.write("    ZE_CALLBACK_SITE_ENTER,\n")
  else:
    f.write("    ZE_CALLBACK_SITE_EXIT,\n")
  f.write("    reinterpret_cast<uint64_t*>(instance_user_data),\n")
  f.write("    \"" + func + "\",\n")
  f.write("    reinterpret_cast<void*>(params),\n")
  f.write("    result };\n")
  f.write("\n")
  f.write("  data->callback(ZE_FUNCTION_" + func +",\n")
  f.write("                &callback_data, data->user_data);\n")

def gen_callbacks(f, func_list, group_map):
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
    gen_callback_body(f, func, True)
    f.write("}\n")
    f.write("\n")
    f.write("void " + func + "OnExit(\n")
    f.write("    " + parse_ze_headers.get_param_struct_name(func) + "* params,\n")
    f.write("    ze_result_t result,\n")
    f.write("    void* global_user_data,\n")
    f.write("    void** instance_user_data) {\n")
    gen_callback_body(f, func, False)
    f.write("}\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
    f.write("\n")

def gen_api(f, func_list, group_map):
  f.write("void SetTracingFunctions(zet_tracer_exp_handle_t tracer,\n")
  f.write("                         std::set<function_id_t> functions) {\n")
  f.write("  zet_core_callbacks_t prologue = {};\n")
  f.write("  zet_core_callbacks_t epilogue = {};\n")
  f.write("\n")
  f.write("  for (auto fid : functions) {\n")
  f.write("    switch (fid) {\n")
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
    f.write("      case ZE_FUNCTION_" + func + ":\n")
    f.write("        prologue." + group_name + "." + callback_name + " = " + func + "OnEnter;\n")
    f.write("        epilogue." + group_name + "." + callback_name + " = " + func + "OnExit;\n")
    f.write("        break;\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
  f.write("      default:\n")
  f.write("        break;\n")
  f.write("    }\n")
  f.write("  }\n")
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
  gen_header(dst_file)

  l0_path = sys.argv[2]
  l0_file_path = os.path.join(l0_path, "ze_api.h")
  
  l0_file = open(l0_file_path, "rt")
  func_list = parse_ze_headers.get_func_list(l0_file)
  group_map = parse_ze_headers.get_callback_group_map(l0_file)

  gen_func_list(dst_file, func_list, group_map)
  gen_callback_type(dst_file)
  gen_global_data_type(dst_file)
  gen_callbacks(dst_file, func_list, group_map)
  gen_api(dst_file, func_list, group_map)
  gen_footer(dst_file)

  l0_file.close()
  dst_file.close()

if __name__ == "__main__":
  main()