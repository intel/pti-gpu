#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

"""
Shared utilities for parsing Level Zero headers.

This module provides common parsing functions used by multiple code generators:
- gen_l0_loader.py
- gen_tracing_callbacks.py
"""

import os
import re
from typing import List, NamedTuple


class FunctionParam(NamedTuple):
  """Represents a function parameter with type and name."""
  type: str
  name: str


class FunctionDecl(NamedTuple):
  """Represents a function declaration."""
  name: str
  return_type: str
  params: List[FunctionParam]


def parse_function_declaration(line: str) -> FunctionDecl:
  """Parse a function declaration line.

  Handles both single-line and accumulated multi-line declarations.
  Format: ZE_APIEXPORT ze_result_t ZE_APICALL funcName(params);
  """
  # Remove ZE_APIEXPORT and ZE_APICALL
  line = line.replace("ZE_APIEXPORT", "").replace("ZE_APICALL", "").strip()

  # Extract return type and function signature
  match = re.match(r'(\w+)\s+(\w+)\s*\((.*)\)\s*;?\s*$', line, re.DOTALL)
  if not match:
    return None

  return_type = match.group(1)
  func_name = match.group(2)
  params_str = match.group(3).strip()

  params = _parse_params(params_str)
  return FunctionDecl(func_name, return_type, params)


def _parse_params(params_str: str) -> List[FunctionParam]:
  """Parse function parameters string into list of FunctionParam."""
  params = []
  if not params_str or params_str == "void":
    return params

  # Split by comma, handling nested parentheses
  param_list = []
  depth = 0
  current = ""
  for char in params_str:
    if char == '(':
      depth += 1
    elif char == ')':
      depth -= 1
    elif char == ',' and depth == 0:
      param_list.append(current.strip())
      current = ""
      continue
    current += char
  if current.strip():
    param_list.append(current.strip())

  for param in param_list:
    param = param.strip()
    if not param:
      continue
    # Split into type and name - last word is the name
    parts = param.rsplit(None, 1)
    if len(parts) == 2:
      param_type = parts[0].strip()
      param_name = parts[1].strip()
      # Handle pointer that might be attached to name
      while param_name.startswith('*'):
        param_type += '*'
        param_name = param_name[1:]
      params.append(FunctionParam(param_type, param_name))

  return params


def get_extension_functions(file_path: str) -> List[FunctionDecl]:
  """Parse an extension header file and extract all function declarations.

  Args:
    file_path: Path to the zex_*.h header file.

  Returns:
    List of FunctionDecl for each extension function found.
  """
  functions = []

  if not os.path.exists(file_path):
    return functions

  with open(file_path, "rt") as f:
    content = f.read()

  # Remove all doxygen comments (///< ...) before processing
  content = re.sub(r'///<[^\n]*', '', content)
  # Remove regular comments (// ...)
  content = re.sub(r'//[^\n]*', '', content)

  lines = content.split('\n')
  i = 0
  while i < len(lines):
    line = lines[i]

    # Skip typedefs and pfn declarations
    if "typedef" in line or "pfn" in line.lower():
      i += 1
      continue

    if "ZE_APICALL" in line:
      # Accumulate the full declaration if it spans multiple lines
      full_decl = line.strip()
      while ';' not in full_decl and ')' not in full_decl and i + 1 < len(lines):
        i += 1
        next_line = lines[i].strip()
        if next_line:  # Skip empty lines
          full_decl += " " + next_line

      # Handle closing paren on separate line
      while ';' not in full_decl and i + 1 < len(lines):
        i += 1
        next_line = lines[i].strip()
        if next_line:
          full_decl += " " + next_line
        if ';' in full_decl or ')' in full_decl:
          break

      # Clean up whitespace
      full_decl = ' '.join(full_decl.split())

      func = parse_function_declaration(full_decl)
      if func and func.name.startswith("ze"):
        functions.append(func)
    i += 1

  return functions


def get_extension_function_names(file_path: str) -> List[str]:
  """Parse an extension header and return just the function names.

  This is a lighter-weight alternative to get_extension_functions()
  when you only need the names.
  """
  return [func.name for func in get_extension_functions(file_path)]


def find_extension_headers(l0_include_path: str) -> List[str]:
  """Find all extension header files in the driver_experimental directory.

  Args:
    l0_include_path: Path to level_zero include directory.

  Returns:
    List of full paths to zex_*.h header files.
  """
  exp_path = os.path.join(l0_include_path, "driver_experimental")
  header_files = []

  if os.path.exists(exp_path):
    for filename in os.listdir(exp_path):
      if filename.startswith("zex_") and filename.endswith(".h"):
        # Skip common headers that don't define functions
        if filename in ["zex_common.h"]:
          continue
        header_files.append(os.path.join(exp_path, filename))

  return header_files


def get_param_struct_field_name(param: FunctionParam) -> str:
  """Get the field name for a parameter in a params struct. adds 'p' prefix

  Example: phGraph -> pphGraph
  """
  return "p" + param.name


def camel_to_snake(name: str) -> str:
  """Convert CamelCase to snake_case.

  Example: GraphCreateExp -> graph_create_exp
  """
  result = ""
  for i, char in enumerate(name):
    if char.isupper() and i > 0:
      result += "_" + char.lower()
    else:
      result += char.lower()
  return result


def get_param_struct_name(func_name: str) -> str:
  """Generate params struct name for extension function.

  Example: zeGraphCreateExp -> ze_graph_create_exp_params_t
  """
  name = func_name
  if name.startswith("ze"):
    name = name[2:]  # Remove "ze" prefix

  return "ze_" + camel_to_snake(name) + "_params_t"
