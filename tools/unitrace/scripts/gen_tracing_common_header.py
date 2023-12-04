#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re

# Function scans ze_api.h file from given path (l0_path) and 
# generates list of l0 API of interest
def get_l0_api_list(l0_path):
  l0_file_path = os.path.join(l0_path, "ze_api.h")
  l0_file = open(l0_file_path, "rt")
  func_list = []
  for line in l0_file.readlines():
    if line.find("ZE_APICALL") != -1 and line.find("ze_pfn") != -1:
      items = line.split("ze_pfn")
      assert len(items) == 2
      assert items[1].find("Cb_t") != -1
      items = items[1].split("Cb_t")
      assert len(items) == 2
      func_list.append(items[0].strip())
  l0_file.close()
  return func_list

# =================== generate common header =======================
# func generates internal tracing enums
def gen_internal_enums(out_file):
  out_file.write("  InternalStartTracingId,\n")
  out_file.write('  DepTracingId,\n')
  out_file.write("  InternalEndTracingId,\n")

# Func generates l0 enums from the list
def gen_l0_enums(out_file, l0_func_list):
  out_file.write("  L0StartTracingId,\n")
  for func in l0_func_list:
    out_file.write('  ' +func+'TracingId,\n')
  out_file.write("  L0EndTracingId,\n")

# generate enums
def gen_enums(out_file, l0_func_list):
  # header
  out_file.write("typedef enum {\n")
  # common enums
  out_file.write("  UnknownTracingId,\n")  
  out_file.write("  ZeKernelTracingId,\n")
  # Internal enums
  gen_internal_enums(out_file)

  #l0 enums
  gen_l0_enums(out_file, l0_func_list)

  #opencl enum
  out_file.write("  ClKernelTracingId,\n");
  out_file.write("  OpenClTracingId,\n");

  #xpti api id
  out_file.write("  XptiTracingId,\n");

  #itt api id
  out_file.write("  IttTracingId,\n");

  #footer
  out_file.write("} API_TRACING_ID;")

# Generates list of supported API names 
def gen_api_name_list(out_file, func_list):
  #header
  out_file.write("\n\n static std::string tracing_api_name [ ] = {\n")
  
  #generate l0 names first
  for func in func_list:
    out_file.write("  \"ze"+func+"\",\n")
  
  #footer
  out_file.write("};\n")


def gen_symbol_func(file_handle):
  file_handle.write("\n\nstatic std::string get_symbol(API_TRACING_ID id){\n")
  file_handle.write("    if (id == UnknownTracingId) {\n")
  file_handle.write("      return \"UnknownAPIName\";\n")
  file_handle.write("    } else if (id > InternalStartTracingId && id < InternalEndTracingId) {\n")
  file_handle.write("      return \"dep\";\n")
  file_handle.write("    } else if (id > L0StartTracingId && id < L0EndTracingId) {\n")
  file_handle.write("      auto index = id - L0StartTracingId;\n")
  file_handle.write("      return tracing_api_name[index-1];\n")
  file_handle.write("    } else {\n")
  file_handle.write("      // Error: never come here\n")
  file_handle.write("      return \"UnknownAPIName\";\n")
  file_handle.write("    }\n")
  file_handle.write("  }\n")

def main():
  if len(sys.argv) < 3:
    print("Usage: python gen_tracing_common_header.py <output_include_path> <include_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  dst_file_path = os.path.join(dst_path, "common_header.gen")
  if (os.path.isfile(dst_file_path)):
    os.remove(dst_file_path)

  dst_file = open(dst_file_path, "wt")

  l0_path = sys.argv[2]
  l0_api_list = get_l0_api_list(l0_path)

  dst_file.write("#ifndef PTI_TOOLS_COMMON_H_\n")
  dst_file.write("#define PTI_TOOLS_COMMON_H_\n\n")

  gen_enums(dst_file, l0_api_list)
  gen_api_name_list(dst_file, l0_api_list)
  gen_symbol_func(dst_file)

  dst_file.write("#endif //PTI_TOOLS_COMMON_H_\n")
  dst_file.close()

if __name__ == "__main__":
  main()
