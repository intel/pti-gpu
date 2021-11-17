import os
import subprocess
import sys

from samples import cl_gemm
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

def run(path):
  app_folder = utils.get_sample_executable_path("cl_gemm")
  app_file = os.path.join(app_folder, "cl_gemm" + file_extention)
  command = [file_name_prefix + "cl_debug_info" + file_extention,\
    app_file, "gpu", "1024", "1"]
  stdout, stderr = utils.run_process(command, path)
  if not stdout:
    return "stdout is empty"
  if not stderr:
    return "stderr is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stderr.find("__kernel") == -1 or stderr.find("for") == -1:
    return stderr
  if stderr.find("add") == -1 or stderr.find("mov") == -1 or stderr.find("send") == -1:
    return stderr
  return None

def main(option):
  path = utils.get_sample_build_path("cl_debug_info")
  log = cl_gemm.main("gpu")
  if log:
    return log
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