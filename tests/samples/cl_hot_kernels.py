import os
import subprocess
import sys

from samples import cl_gemm
from samples import dpc_gemm
from samples import omp_gemm
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

def parse(output):
  lines = output.split("\n")
  total_time = 0
  for line in lines:
    items = line.split(",")
    if len(items) != 8 or line.find("Time (ns)") != -1:
      continue
    kernel_name = items[0].strip()
    call_count = int(items[1].strip())
    time = int(items[3].strip())
    if not kernel_name or call_count <= 0:
      return False
    total_time += time
  if total_time <= 0:
    return False
  return True

def run(path, option):
  environ = None
  if option == "dpc":
    app_folder = utils.get_sample_executable_path("dpc_gemm", utils.get_build_flag())
    app_file = os.path.join(app_folder, "dpc_gemm" + file_extention)
    option = "cpu"
    command = [file_name_prefix + "cl_hot_kernels" + file_extention,\
      app_file, option, "1024", "1"]
  elif option == "omp":
    app_folder = utils.get_sample_executable_path("omp_gemm")
    app_file = os.path.join(app_folder, "omp_gemm" + file_extention)
    option = "gpu"
    environ = utils.add_env(None, "LIBOMPTARGET_PLUGIN", "OPENCL")
    command = [file_name_prefix + "cl_hot_kernels" + file_extention,\
      app_file, option, "1024", "1"]
  else:
    app_folder = utils.get_sample_executable_path("cl_gemm")
    app_file = os.path.join(app_folder, "cl_gemm" + file_extention)
    command = [file_name_prefix + "cl_hot_kernels" + file_extention,\
      app_file, option, "1024", "1"]
  stdout, stderr = utils.run_process(command, path, environ)
  if not stdout:
    return "stdout is empty"
  if not stderr:
    return "stderr is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  if not parse(stderr):
    return stderr
  return None

def main(option):
  path = utils.get_sample_build_path("cl_hot_kernels")
  if option == "dpc":
    log = dpc_gemm.main("cpu")
    if log:
      return log
  elif option == "omp":
    log = omp_gemm.main("gpu")
    if log:
      return log
  else:
    log = cl_gemm.main(option)
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
  option = "gpu"
  if len(sys.argv) > 1 and sys.argv[1] == "cpu":
    option = "cpu"
  if len(sys.argv) > 1 and sys.argv[1] == "dpc":
    option = "dpc"
  if len(sys.argv) > 1 and sys.argv[1] == "omp":
    option = "omp"
  log = main(option)
  if log:
    print(log)