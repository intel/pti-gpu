import os
import subprocess
import sys

from samples import dpc_gemm
from samples import cl_gemm
from samples import ze_gemm
import utils

def config(path):
  cmake = ["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  stdout, stderr = utils.run_process(["make"], path)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def parseTotal(output, countString=None):
  lines = output.split("\n")
  total_count = 0
  total_simd = 0
  for line in lines:
    if line.find("[INFO]") != -1:
      continue
    if line.find("[") != 0:
      continue
    if countString != None and countString not in line:
      continue
    items = line.split("]")
    if len(items) == 2:
      if items[0].strip() and items[1].strip():
        count_s = items[0].lstrip("[").strip()
        simd_s = '0'
        if '|' in count_s:
          simd_s = count_s.split("|")[1].strip()
          count_s = count_s.split("|")[0].strip()
        count = int(count_s)
        simd = int(simd_s)
        if count < 0:
          return False
        total_count += count
        total_simd += simd
  return total_count, total_simd

def parse(output):
  return False if parseTotal(output)[0] <= 0 else True

def getTestAppCommand(option, matrixSize, iterations):
  if option == "cl":
    app_folder = utils.get_sample_executable_path("cl_gemm")
    app_file = os.path.join(app_folder, "cl_gemm")
    return ["./instcount", app_file, "gpu", f"{matrixSize}", f"{iterations}"]
  elif option == "ze":
    app_folder = utils.get_sample_executable_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    return ["./instcount", app_file, f"{matrixSize}", f"{iterations}"]
  else:
    app_folder = utils.get_sample_executable_path("dpc_gemm")
    app_file = os.path.join(app_folder, "dpc_gemm")
    return ["./instcount", app_file, "gpu",  f"{matrixSize}", f"{iterations}"]

def isValidOutput(stdout, stderr):
  if not stdout:
    return "stdout is empty"
  if not stderr:
    return "stderr is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stderr.find("add") == -1 or stderr.find("mov") == -1 or stderr.find("send") == -1:
    return stderr
  if not parse(stderr):
    return stderr
  return None

def run(path, option):
  # Smoke test
  command = getTestAppCommand(option, 1024, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res

  # Correctness test
  # Test is based on relative results of instruction count. Test appplicaiton
  # has N^3 complexity. Correctness based on number of executed multiply instructions
  # for matrix sizes {1, 2, 4} is checked as: (-12*r1-r2+r4)/44==(4*r1-r2)/-4:)

  baseSize = 128
  command = getTestAppCommand(option, baseSize * 1, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  r1 = parseTotal(stderr, 'mad')

  command = getTestAppCommand(option, baseSize * 2, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  r2 = parseTotal(stderr, 'mad')

  command = getTestAppCommand(option, baseSize * 4, 1)
  stdout, stderr = utils.run_process(command, path)
  res = isValidOutput(stdout, stderr)
  if res != None: return res
  r4 = parseTotal(stderr, 'mad')

  if ((-12*r1[0] -r2[0] +r4[0])/44 != (4*r1[0] - r2[0])/-4 and 
      (-12*r1[1] -r2[1] +r4[1])/44 != (4*r1[1] - r2[1])/-4):
    return f"Correctness check failed: {r1[0] * 8} != {r2[0]}"

  return None

def main(option):
  path = utils.get_tool_build_path("instcount")
  if option == "cl":
    log = cl_gemm.main("gpu")
    if log:
      return log
  elif option == "ze":
    log = ze_gemm.main(None)
    if log:
      return log
  else:
    log = dpc_gemm.main("gpu")
    if log:
      return log
  log = config(path)
  if log:
    return log
  log = build(path)
  if log:
    return log
  log = run(path, option)
  if log:
    return log

if __name__ == "__main__":
  option = "cl"
  if len(sys.argv) > 1 and sys.argv[1] == "ze":
    option = "ze"
  if len(sys.argv) > 1 and sys.argv[1] == "dpc":
    option = "dpc"
  log = main(option)
  if log:
    print(log)