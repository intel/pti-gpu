import os
import subprocess
import sys

import utils

if sys.platform == 'win32':
  cmake_generator = "NMake Makefiles"
  file_extention = ".exe"
  file_name_prefix = ""
  make = ["nmake"]
else:
  cmake_generator = "Unix Makefiles"
  file_extention = ""
  file_name_prefix = "./"
  make = ["make"]


def config(path):
  cmake = ["cmake", "-G", cmake_generator,\
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

def parse(stdout):
  count = 0
  for line in stdout.split("\n"):
    if line.find("Samples collected:") == 0:
      items = line.split()
      if len(items) != 7:
        break
      count = int(items[2])
      break
  if count > 0:
    return True
  return False

def run(path):
  command = [file_name_prefix + "cl_gemm_inst" + file_extention,\
    "1024", "1"]
  stdout, stderr = utils.run_process(command, path)
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  if not parse(stdout):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("cl_gemm_inst")
  log = config(path)
  if log:
    return log
  log = build(path)
  if log:
    return log
  log = run(path)
  if log:
    return log

if __name__ == "__main__":
  log = main(None)
  if log:
    print(log)