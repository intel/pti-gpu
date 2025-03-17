#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re

def get_func_list(f, func_list):
  f.seek(0)
  for line in f.readlines():
    if line.find("ZE_APICALL") != -1 and line.find("ze_pfn") != -1:
      items = line.split("ze_pfn")
      assert len(items) == 2
      assert items[1].find("Cb_t") != -1
      items = items[1].split("Cb_t")
      assert len(items) == 2
      func_list.append("ze" + items[0].strip())

def gen_loader(f, func_list):
  f.write("public:\n");
  for func in func_list:
    reg_fname = "zelTracer" + func[2:] + "RegisterCallback"
    f.write("  decltype(&" + reg_fname + ") " + reg_fname + "_ = nullptr;\n");

  f.write("private:\n");
  f.write("  void init() {\n");
  for func in func_list:
    reg_fname = "zelTracer" + func[2:] + "RegisterCallback"
    f.write("  LEVEL_ZERO_LOADER_GET_SYMBOL(" + reg_fname + ");\n");
  f.write("}\n");

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

  l0_path = sys.argv[2]
  l0_file_path = os.path.join(l0_path, "ze_api.h")

  func_list = []

  l0_file = open(l0_file_path, "rt")
  get_func_list(l0_file, func_list)

  l0_exp_path = os.path.join(l0_path, "layers", "zel_tracing_register_cb.h")
  l0_exp_file = open(l0_exp_path, "rt")
  get_func_list(l0_exp_file, func_list)

  gen_loader(dst_loader_file, func_list)

  l0_exp_file.close()
  l0_file.close()
  dst_loader_file.close()

if __name__ == "__main__":
  main()
