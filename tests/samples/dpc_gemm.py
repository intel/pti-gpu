import os
import subprocess
import sys

import utils

if sys.platform == 'win32':
  file_extention = ".exe"
  file_name_prefix = ""
  make = ["nmake"]
else:
  file_extention = ""
  file_name_prefix = "./"
  make = ["make"]

def config(path):
  if sys.platform == 'win32':
    cmake = ["cmake", "-G", "NMake Makefiles", \
      "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  else:
    cmake = ["cmake",\
      "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  stdout, stderr = utils.run_process(make, path)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def run(path, option):
  command = [file_name_prefix + "dpc_gemm" + file_extention,\
    option, "1024", "1"]
  stdout, stderr = utils.run_process(command,\
    utils.get_sample_executable_path("dpc_gemm", utils.get_build_flag()))
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("dpc_gemm")
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
  option = "gpu"
  if len(sys.argv) > 1 and sys.argv[1] == "cpu":
    option = "cpu"
  if len(sys.argv) > 1 and sys.argv[1] == "host":
    option = "host"
  log = main(option)
  if log:
    print(log)