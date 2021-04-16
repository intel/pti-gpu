import os
import subprocess
import sys

from samples import cl_gemm
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

def run(path):
  app_folder = utils.get_sample_build_path("cl_gemm")
  app_file = os.path.join(app_folder, "cl_gemm")
  p = subprocess.Popen(["./cl_debug_info", app_file, "gpu", "1024", "1"],
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
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