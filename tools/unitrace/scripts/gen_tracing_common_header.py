#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re

from parse_l0_headers import find_extension_headers, get_extension_function_names

# Function scans ze_api.h file from given path (l0_path) and 
# generates list of l0 API of interest
def get_func_list(file_list):
  l0_func_list = []
  l0_rt_func_list = []
  for f in file_list:
    for line in f.readlines():
      if line.find("ZE_APICALL") != -1:
        items = []
        is_rt_func = False
        if line.find("ze_pfn") != -1:
          items = line.split("ze_pfn")
        elif line.find("zer_pfn") != -1:
          items = line.split("zer_pfn")
          is_rt_func = True
        if len(items) == 2 and items[1].find("Cb_t") != -1:
          items = items[1].split("Cb_t")
          if len(items) == 2:
            if is_rt_func:
              l0_rt_func_list.append(items[0].strip())
            else:
              l0_func_list.append(items[0].strip())
  return l0_func_list, l0_rt_func_list

def get_l0_api_list(l0_path):
  file_list = []
  l0_file_path = os.path.join(l0_path, "ze_api.h")
  l0_exp_path = os.path.join(l0_path, "layers", "zel_tracing_register_cb.h")
  try:
    l0_file = open(l0_file_path, "rt")
    file_list.append(l0_file)
    l0_exp_file = open(l0_exp_path, "rt")
    file_list.append(l0_exp_file)
    l0_func_list = []
    l0_rt_func_list = []
    l0_func_list, l0_rt_func_list = get_func_list(file_list)
  finally:
    l0_file.close()
    l0_exp_file.close()
  return l0_func_list, l0_rt_func_list

def get_extension_api_list(l0_path):
  func_list = []
  for header_path in find_extension_headers(l0_path):
    func_list.extend(get_extension_function_names(header_path))
  return func_list

def get_ocl_api_list(ocl_path):
  if (not os.path.exists(ocl_path)):
    return []

  ocl_file_path = os.path.join(ocl_path, "tracing_types.h")
  ocl_file = open(ocl_file_path, "rt")
  func_list = []
  cl_function_id_found = False
  for line in ocl_file.readlines():
    line = line.strip()
    if cl_function_id_found == True and line.find("CL_FUNCTION_COUNT") == -1:
      if line.startswith("CL_FUNCTION_"):
        api_name = line[len("CL_FUNCTION_"):line.find("=",len("CL_FUNCTION_"))]
        func_list.append(api_name.strip())
    elif line.find("ClFunctionId") != -1 or line.find("_cl_function_id") != -1:
      cl_function_id_found = True
  return func_list
# =================== generate common header =======================

# Func generates l0 enums from the list
def gen_l0_enums(out_file, l0_func_list):
  out_file.write("  L0StartTracingId,\n")
  for func in l0_func_list:
    out_file.write('  ' +func+'TracingId,\n')
  out_file.write("  L0EndTracingId,\n")

def gen_extension_enums(out_file, ext_func_list):
  out_file.write("  ExtStartTracingId,\n")
  for func in ext_func_list:
    out_file.write('  ' +func+'TracingId,\n')
  out_file.write("  ExtEndTracingId,\n")

def get_ocl_enums(out_file, ocl_func_list):
  out_file.write("  OCLStartTracingId,\n")
  if len(ocl_func_list) > 0:
    # first entry needs special care
    out_file.write('  ' +ocl_func_list[0]+'TracingId = OCLStartTracingId,\n')
    index = 1
    while index < len(ocl_func_list):
      out_file.write('  ' +ocl_func_list[index]+'TracingId,\n')
      index = index + 1
  out_file.write("  OCLEndTracingId,\n")

# generate enums
def gen_enums(out_file, l0_func_list, ext_func_list, ocl_func_list):
  # header
  out_file.write("typedef enum {\n")
  # common enums
  out_file.write("  UnknownTracingId,\n")
  out_file.write("  DummyTracingId,\n")
  out_file.write("  ZeKernelTracingId,\n")

  #l0 enums
  gen_l0_enums(out_file, l0_func_list)

  #extension enums (graph APIs, etc.)
  gen_extension_enums(out_file, ext_func_list)

  #opencl enum
  get_ocl_enums(out_file, ocl_func_list)
  out_file.write("  ClKernelTracingId,\n")
  out_file.write("  OpenClTracingId,\n")

  #xpti api id
  out_file.write("  XptiTracingId,\n")

  #itt api id
  out_file.write("  IttTracingId,\n")

  #footer
  out_file.write("} API_TRACING_ID;\n")

# Generates list of supported API names
def gen_api_name_list(out_file, l0_func_list, l0_rt_func_list, ext_func_list, ocl_func_list):
  #header
  out_file.write("\n\n static std::string tracing_api_name [ ] = {\n")
  
  #generate l0 names first
  for func in l0_func_list:
    out_file.write("  \"ze"+func+"\",\n")

  #generate l0 runtime names
  for func in l0_rt_func_list:
    out_file.write("  \"zer"+func+"\",\n")

  #generate extension api names
  for func in ext_func_list:
    out_file.write("  \""+func+"\",\n")

  #generate ocl names
  for func in ocl_func_list:
    out_file.write("  \""+func+"\",\n")  
  
  #footer
  out_file.write("};\n")


def gen_symbol_func(file_handle):
  file_handle.write("\n#include \"cl_intel_ext.h\"\n")
  file_handle.write("\n\nstatic std::string get_symbol(API_TRACING_ID id){\n")
  file_handle.write("    constexpr size_t tracing_api_name_size = sizeof(tracing_api_name) / sizeof(tracing_api_name[0]);\n")
  file_handle.write("    if (id == UnknownTracingId || static_cast<size_t>(id) >= tracing_api_name_size) {\n")
  file_handle.write("      return \"UnknownAPIName\";\n")
  file_handle.write("    } else if ((id > L0StartTracingId) && (id < L0EndTracingId)) {\n")
  file_handle.write("      // L0+RT APIs: array starts at index 0\n")
  file_handle.write("      auto index = id - L0StartTracingId - 1;\n")
  file_handle.write("      return tracing_api_name[index];\n")
  file_handle.write("    } else if ((id > ExtStartTracingId) && (id < ExtEndTracingId)) {\n")
  file_handle.write("      // Extension APIs: array starts after L0+RT APIs\n")
  file_handle.write("      constexpr auto l0_count = L0EndTracingId - L0StartTracingId - 1;\n")
  file_handle.write("      auto index = l0_count + (id - ExtStartTracingId - 1);\n")
  file_handle.write("      return tracing_api_name[index];\n")
  file_handle.write("    } else if ((id >= OCLStartTracingId) && (id < OCLEndTracingId)) {\n")
  file_handle.write("      // OCL APIs: array starts after L0+RT and Extension APIs\n")
  file_handle.write("      constexpr auto l0_count = L0EndTracingId - L0StartTracingId - 1;\n")
  file_handle.write("      constexpr auto ext_count = ExtEndTracingId - ExtStartTracingId - 1;\n")
  file_handle.write("      auto index = l0_count + ext_count + (id - OCLStartTracingId);\n")
  file_handle.write("      return tracing_api_name[index];\n")
  file_handle.write("    } else if (((cl_ext_api_id)id >= ClExtApiStart) && ((cl_ext_api_id)id < ClExtApiEnd)) {\n")
  file_handle.write("      return cl_ext_api[id - ClExtApiStart];\n")
  file_handle.write("    } else {\n")
  file_handle.write("      // Error: never come here\n")
  file_handle.write("      return \"UnknownAPIName\";\n")
  file_handle.write("    }\n")
  file_handle.write("  }\n")

def main():
  if len(sys.argv) < 4:
    print("Usage: python gen_tracing_common_header.py <output_include_path> <include_path> <ocl_path>")
    sys.exit(1) # Non-zero indicates error

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  dst_file_path = os.path.join(dst_path, "common_header.gen")
  if (os.path.isfile(dst_file_path)):
    os.remove(dst_file_path)

  dst_file = open(dst_file_path, "wt")

  l0_path = sys.argv[2]
  if not os.path.exists(l0_path):
    print(f"Error: Level Zero include path does not exist: {l0_path}")
    sys.exit(1)
  l0_api_list, rt_l0_api_list = get_l0_api_list(l0_path)
  ext_api_list = get_extension_api_list(l0_path)

  ocl_path = sys.argv[3]
  if ocl_path and not os.path.exists(ocl_path):
    print(f"Warning: OpenCL include path does not exist: {ocl_path}")
    sys.exit(1)
  ocl_api_list = get_ocl_api_list(ocl_path)

  dst_file.write("#ifndef PTI_TOOLS_COMMON_H_\n")
  dst_file.write("#define PTI_TOOLS_COMMON_H_\n\n")

  gen_enums(dst_file, l0_api_list + rt_l0_api_list, ext_api_list, ocl_api_list)
  gen_api_name_list(dst_file, l0_api_list, rt_l0_api_list, ext_api_list, ocl_api_list)
  gen_symbol_func(dst_file)

  dst_file.write("#endif //PTI_TOOLS_COMMON_H_\n")
  dst_file.close()

if __name__ == "__main__":
  main()
