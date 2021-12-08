import os
import subprocess
import sys

from samples import dpc_gemm
from samples import omp_gemm
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

def run(path, option):
  if option == "dpc":
    app_folder = utils.get_sample_executable_path("dpc_gemm")
    app_file = os.path.join(app_folder, "dpc_gemm")
    command = ["./ze_tracer", "-h", "-d", "-t", app_file, "gpu", "1024", "1"]
  elif option == "omp":
    app_folder = utils.get_sample_executable_path("omp_gemm")
    app_file = os.path.join(app_folder, "omp_gemm")
    command = ["./ze_tracer", "-h", "-d", "-t", app_file, "gpu", "1024", "1"]
  elif option == "-v" or option == "--kernels-per-tile" or option == "--conditional-collection":
    app_folder = utils.get_sample_executable_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    command = ["./ze_tracer", "-d", option, app_file, "1024", "1"]
  else:
    app_folder = utils.get_sample_executable_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    command = ["./ze_tracer", option, app_file, "1024", "1"]
  stdout, stderr = utils.run_process(command, path)
  if not stdout:
    return "stdout is empty"
  if not stderr:
    return "stderr is empty"
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stderr.find("WARNING") != -1:
    return stderr
  return None

def main(option):
  path = utils.get_tool_build_path("ze_tracer")
  if option == "dpc":
    log = dpc_gemm.main("gpu")
  elif option == "omp":
    log = omp_gemm.main("gpu")
  else:
    log = ze_gemm.main(None)
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
  option = "-c"
  if len(sys.argv) > 1 and sys.argv[1] == "-h":
    option = "-h"
  if len(sys.argv) > 1 and sys.argv[1] == "-d":
    option = "-d"
  if len(sys.argv) > 1 and sys.argv[1] == "-t":
    option = "-t"
  if len(sys.argv) > 1 and sys.argv[1] == "-s":
    option = "-s"
  if len(sys.argv) > 1 and sys.argv[1] == "-v":
    option = "-v"
  if len(sys.argv) > 1 and sys.argv[1] == "--kernels-per-tile":
    option = "--kernels-per-tile"
  if len(sys.argv) > 1 and sys.argv[1] == "--chrome-call-logging":
    option = "--chrome-call-logging"
  if len(sys.argv) > 1 and sys.argv[1] == "--chrome-device-timeline":
    option = "--chrome-device-timeline"
  if len(sys.argv) > 1 and sys.argv[1] == "--chrome-kernel-timeline":
    option = "--chrome-kernel-timeline"
  if len(sys.argv) > 1 and sys.argv[1] == "--chrome-device-stages":
    option = "--chrome-device-stages"
  if len(sys.argv) > 1 and sys.argv[1] == "--conditional-collection":
    option = "--conditional-collection"
  if len(sys.argv) > 1 and sys.argv[1] == "dpc":
    option = "dpc"
  if len(sys.argv) > 1 and sys.argv[1] == "omp":
    option = "omp"
  log = main(option)
  if log:
    print(log)