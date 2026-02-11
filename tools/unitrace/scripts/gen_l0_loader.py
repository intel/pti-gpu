#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys

from parse_l0_headers import find_extension_headers, get_extension_function_names


def get_func_list(file_path, func_list):
  f = open(file_path, "rt")
  for line in f.readlines():
    if line.find("ZE_APICALL") != -1:
      items = []
      is_rt_func = False
      if line.find("ze_pfn") != -1:
        items = line.split("ze_pfn")
      elif line.find("zer_pfn") != -1:
        is_rt_func = True
        items = line.split("zer_pfn")
      if len(items) == 2 and items[1].find("Cb_t") != -1:
        items = items[1].split("Cb_t")
        if len(items) == 2:
          if is_rt_func:
            func_list.append("zer" + items[0].strip())
          else:
            func_list.append("ze" + items[0].strip())
  f.close()

def get_api_func_list(file_path, func_list):
  f = open(file_path, "rt")
  api_line = False
  for line in f.readlines():
    if line.find("ZE_APICALL") != -1:
      # Check if function name is on the same line (format: ZE_APIEXPORT ze_result_t ZE_APICALL funcName(...))
      parts = line.split("ZE_APICALL")
      if len(parts) == 2 and "(" in parts[1]:
        # Function name is on the same line after ZE_APICALL
        items = parts[1].split("(")
        if len(items) >= 2:
          func_list.append(items[0].strip())
        api_line = False
      else:
        # Function name is on the next line
        api_line = True
    elif api_line:
      api_line = False
      items = line.split("(")
      assert len(items) == 2
      func_list.append(items[0].strip())
  f.close()

def gen_loader(f, register_func_list, api_func_list, ext_func_list):
  f.write("public:\n")
  for func in register_func_list:
    offset = 2
    if func.find("zer") == 0:
      offset = 3
    reg_fname = "zelTracer" + func[offset:] + "RegisterCallback"
    f.write("  decltype(&" + reg_fname + ") " + reg_fname + "_ = nullptr;\n")
    f.write("  decltype(&" + func + ") " + func + "_ = nullptr;\n")
  for func in api_func_list:
    f.write("  decltype(&" + func + ") " + func + "_ = nullptr;\n")

  # Extension function pointers - these are populated by interception, not from loader
  f.write("\n  // Extension function pointers (populated via InterceptExtensionApi)\n")
  for func in ext_func_list:
    f.write("  decltype(&" + func + ") " + func + "_ = nullptr;\n")

  f.write("private:\n")
  f.write("  void init() {\n")
  for func in register_func_list:
    offset = 2
    if func.find("zer") == 0:
      offset = 3
    reg_fname = "zelTracer" + func[offset:] + "RegisterCallback"
    f.write("  LEVEL_ZERO_LOADER_GET_SYMBOL(" + reg_fname + ");\n")
    f.write("  LEVEL_ZERO_LOADER_GET_SYMBOL(" + func + ");\n")
  for func in api_func_list:
    f.write("  LEVEL_ZERO_LOADER_GET_SYMBOL(" + func + ");\n")
  # Note: Extension functions are NOT loaded here - they come via interception
  f.write("}\n")

def main():
  if len(sys.argv) < 3:
    print("Usage: python gen_l0_loader.py <output_include_path> <l0_include_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  dst_loader_file_path = os.path.join(dst_path, "l0_loader.gen")
  if (os.path.isfile(dst_loader_file_path)):
    os.remove(dst_loader_file_path)

  dst_loader_file = open(dst_loader_file_path, "wt")

  register_func_list = []
  api_func_list = []
  ext_func_list = []

  l0_path = sys.argv[2]
  get_func_list(os.path.join(l0_path, "ze_api.h"), register_func_list)
  get_func_list(os.path.join(l0_path, "layers", "zel_tracing_register_cb.h"), register_func_list)
  get_api_func_list(os.path.join(l0_path, "zet_api.h"), api_func_list)
  get_api_func_list(os.path.join(l0_path, "zes_api.h"), api_func_list)
  get_api_func_list(os.path.join(l0_path, "layers", "zel_tracing_api.h"), api_func_list)

  # Extension functions go into a separate list - they're not loaded from the library
  for header_path in find_extension_headers(l0_path):
    ext_func_list.extend(get_extension_function_names(header_path))

  gen_loader(dst_loader_file, register_func_list, api_func_list, ext_func_list)

  dst_loader_file.close()

if __name__ == "__main__":
  main()
