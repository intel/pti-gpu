import os
import subprocess
import sys

import ze_gemm
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
  app_folder = utils.get_sample_build_path("ze_gemm")
  app_file = os.path.join(app_folder, "ze_gemm")
  p = subprocess.Popen(["./ze_debug_info", app_file, "1024", "1"],
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stderr
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stdout.find("Job is successfully completed") == -1:
    return stdout
  if stdout.find("__kernel") == -1 or stdout.find("for") == -1:
    return stdout
  if stdout.find("add") == -1 or stdout.find("mov") == -1 or stdout.find("send") == -1:
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("ze_debug_info")
  log = ze_gemm.main(None)
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