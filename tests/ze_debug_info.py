import os
import subprocess
import sys

import ze_gemm
import dpc_gemm
import utils

def config(path):
  p = subprocess.Popen(["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  p.wait()
  stdout, stderr = utils.run_process(p)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  p = subprocess.Popen(["make"], cwd = path,\
    stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  p.wait()
  stdout, stderr = utils.run_process(p)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def run(path, option):
  if option == "dpc":
    app_folder = utils.get_sample_build_path("dpc_gemm")
    app_file = os.path.join(app_folder, "dpc_gemm")
    p = subprocess.Popen(["./ze_debug_info", app_file, "gpu", "1024", "1"],
      cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  else:
    app_folder = utils.get_sample_build_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    p = subprocess.Popen(["./ze_debug_info", app_file, "1024", "1"],
      cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if not stderr:
    return stdout
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stderr.find("for") == -1:
    return stderr
  if stderr.find("add") == -1 or stderr.find("mov") == -1 or stderr.find("send") == -1:
    return stderr
  return None

def main(option):
  path = utils.get_sample_build_path("ze_debug_info")
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