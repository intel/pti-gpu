#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re

# Parse Level Zero Headers ####################################################

STATE_NORMAL = 0
STATE_CONDITION = 1
STATE_SKIP = 2

def get_func_name(callback_struct_name):
  assert callback_struct_name.strip().find("ze_pfn") == 0
  assert callback_struct_name.strip().find("Cb_t") + len("Cb_t") == len(callback_struct_name.strip())
  body = callback_struct_name.strip()
  body = body.split("ze_pfn")[1]
  body = body.split("Cb_t")[0]
  return "ze" + body

def get_struct_range(lines, struct_name):
  start = -1
  for i in range(len(lines)):
    if lines[i].find(struct_name) != -1:
      start = i
      break
  assert start >= 0

  while lines[start].find("{") == -1:
    start +=1
  start += 1
  assert start < len(lines)

  end = start
  while lines[end].find("}") == -1:
    end += 1
  assert end < len(lines)

  return start, end

def get_callback_struct_map(f, struct_name):
  f.seek(0)
  lines = f.readlines()

  start, end = get_struct_range(lines, struct_name)

  struct_map = {}
  cond = ""
  state = STATE_NORMAL
  for i in range(start, end):
    if lines[i].find("#if") >= 0:
      items = lines[i].strip().split()
      assert len(items) == 2
      state = STATE_CONDITION
      cond = items[1].strip()
      continue
    elif lines[i].find("#else") >= 0:
      assert state == STATE_CONDITION
      state = STATE_SKIP
      continue
    elif lines[i].find("#endif") >= 0:
      assert state != STATE_NORMAL
      state = STATE_NORMAL
      cond = ""
      continue

    if state == STATE_SKIP:
      continue

    items = lines[i].strip().split()
    assert len(items) == 2
    type_name = items[0].strip()
    field_name = items[1].strip().strip(";")
    assert not (type_name in struct_map)
    struct_map[type_name] = (field_name, cond)

  return struct_map

def get_func_list(f):
  f.seek(0)
  func_list = []
  for line in f.readlines():
    if line.find("ZE_APICALL") != -1 and line.find("ze_pfn") != -1:
      items = line.split("ze_pfn")
      assert len(items) == 2
      assert items[1].find("Cb_t") != -1
      items = items[1].split("Cb_t")
      assert len(items) == 2
      func_list.append("ze" + items[0].strip())
  return func_list

def get_callback_group_map(f):
  group_map = {}

  base_map = get_callback_struct_map(f, "ze_callbacks_t")
  assert len(base_map) > 0

  for key, value in base_map.items():
    func_map = get_callback_struct_map(f, key)
    for fkey, fvalue in func_map.items():
      func_name = get_func_name(fkey)
      assert not (func_name in group_map)
      group_map[func_name] = (value, fvalue)

  return group_map

def get_param_struct_name(func_name):
  assert func_name[0] == 'z'
  func_name = 'Z' + func_name[1:]
  func_name = func_name.replace("CL", "Cl")
  func_name = func_name.replace("IPC", "Ipc")
  items = re.findall('[A-Z][^A-Z]*', func_name)
  assert len(items) > 1
  struct_name = ""
  for item in items:
    struct_name += item.lower() + "_"
  struct_name += "params_t"
  return struct_name

# Generate Callbacks ##########################################################

def gen_api(f, func_list, group_map):
  f.write("static void SetTracingAPIs(zel_tracer_handle_t tracer) {\n")
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
  f.write("  status = zelTracerSetPrologues(tracer, &prologue);\n")
  f.write("  PTI_ASSERT(status == ZE_RESULT_SUCCESS);\n")
  f.write("  status = zelTracerSetEpilogues(tracer, &epilogue);\n")
  f.write("  PTI_ASSERT(status == ZE_RESULT_SUCCESS);\n")
  f.write("}\n")
  f.write("\n")

def gen_enter_callback(f, func):
  f.write("  PTI_ASSERT(global_user_data != nullptr);\n")
  f.write("  ZeApiCollector* collector =\n")
  f.write("    reinterpret_cast<ZeApiCollector*>(global_user_data);\n")
  f.write("  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);\n")
  f.write("  start_time = collector->GetTimestamp();\n")

def gen_exit_callback(f, func):
  f.write("  PTI_ASSERT(global_user_data != nullptr);\n")
  f.write("  ZeApiCollector* collector =\n")
  f.write("    reinterpret_cast<ZeApiCollector*>(global_user_data);\n")
  f.write("  uint64_t end_time = collector->GetTimestamp();\n")
  f.write("\n")
  f.write("  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);\n")
  f.write("  PTI_ASSERT(start_time > 0);\n")
  f.write("  PTI_ASSERT(start_time < end_time);\n")
  f.write("  uint64_t time = end_time - start_time;\n")
  f.write("  collector->AddFunctionTime(\"" + func + "\", time);\n")

def gen_callbacks(f, func_list, group_map):
  for func in func_list:
    assert func in group_map
    group, callback = group_map[func]
    callback_name = callback[0]
    callback_cond = callback[1]
    if callback_cond:
      f.write("#if " + callback_cond + "\n")
    f.write("static void " + func + "OnEnter(\n")
    f.write("    " + get_param_struct_name(func) + "* params,\n")
    f.write("    ze_result_t result,\n")
    f.write("    void* global_user_data,\n")
    f.write("    void** instance_user_data) {\n")
    gen_enter_callback(f, func)
    f.write("}\n")
    f.write("\n")
    f.write("static void " + func + "OnExit(\n")
    f.write("    " + get_param_struct_name(func) + "* params,\n")
    f.write("    ze_result_t result,\n")
    f.write("    void* global_user_data,\n")
    f.write("    void** instance_user_data) {\n")
    gen_exit_callback(f, func)
    f.write("}\n")
    if callback_cond:
      f.write("#endif //" + callback_cond + "\n")
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
  func_list = get_func_list(l0_file)
  group_map = get_callback_group_map(l0_file)

  gen_callbacks(dst_file, func_list, group_map)
  gen_api(dst_file, func_list, group_map)

  l0_file.close()
  dst_file.close()

if __name__ == "__main__":
  main()