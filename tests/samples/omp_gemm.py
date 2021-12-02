import os
import subprocess
import sys

import utils

if sys.platform == 'win32':
  file_extention = ".exe"
  file_name_prefix = ""
else:
  file_extention = ""
  file_name_prefix = "./"


def config(path):
  if sys.platform != 'win32':
    cmake = ["cmake",\
      "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
    stdout, stderr = utils.run_process(cmake, path)
    if stderr and stderr.find("CMake Error") != -1:
      return stderr
  return None

def build(path):
  make = ""
  if sys.platform == 'win32':
    make += "icx.exe ../main.cc /Qopenmp /Qopenmp-targets=spir64" +\
      " -I../../../utils -o omp_gemm.exe"
    if utils.get_build_flag() == "Debug":
      make += " /Z7"
  else:
    make = ["make"]
  stdout, stderr = utils.run_process(make, path)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def run(path, option):
  command = [file_name_prefix + "omp_gemm" + file_extention,\
    option, "1024", "1"]
  stdout, stderr = utils.run_process(command, path)
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("omp_gemm")
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
  log = main(option)
  if log:
    print(log)