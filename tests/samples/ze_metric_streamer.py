import os
import subprocess
import sys

from samples import ze_gemm
from samples import dpc_gemm
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

def parse(output):
  lines = output.split("\n")
  total_time = 0.0
  for line in lines:
    items = line.split(",")
    if len(items) < 8 or line.find("Time (ns)") != -1:
      continue
    kernel_name = items[0].strip()
    call_count = int(items[1].strip())
    time = float(items[2].strip())
    eu_active = float(items[5].strip())
    eu_stall = float(items[6].strip())
    eu_idle = float(items[7].strip())
    if not kernel_name or call_count <= 0:
      return False
    if eu_active <= 0:
      return False
    if abs(eu_active + eu_stall + eu_idle - 100) >= 0.02:
        return False
    total_time += time
  if total_time <= 0:
    return False
  return True

def run(path, option):
  if option == "dpc":
    app_folder = utils.get_sample_executable_path("dpc_gemm")
    app_file = os.path.join(app_folder, "dpc_gemm")
    command = ["./ze_metric_streamer", app_file, "gpu", "1024", "1"]
  else:
    app_folder = utils.get_sample_executable_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    command = ["./ze_metric_streamer", app_file, "1024", "1"]
  stdout, stderr = utils.run_process(command, path)
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
  path = utils.get_sample_build_path("ze_metric_streamer")
  if option == "dpc":
    log = dpc_gemm.main("gpu")
    if log:
      return log
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
  option = "gpu"
  if len(sys.argv) > 1 and sys.argv[1] == "dpc":
    option = "dpc"
  log = main(option)
  if log:
    print(log)