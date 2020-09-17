import os
import subprocess
import sys

import cl_gemm
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

def parse(output):
  lines = output.split("\n")
  total_percent = 0.0
  for line in lines:
    if line.find("[INFO]") != -1:
      continue
    if line.find("[") != 0:
      continue
    items = line.split("]")
    if len(items) == 2:
      if items[0].strip() and items[1].strip():
        value = items[0].lstrip("[").rstrip("%").strip()
        if value != "-":
          percent = float(items[0].lstrip("[").rstrip("%").strip())
          if percent < 0:
            return False
          total_percent += percent
  if total_percent <= 0 or total_percent > 100:
    return False
  return True

def run(path, option):
  if option == "cl":
    app_folder = utils.get_sample_build_path("cl_gemm")
    app_file = os.path.join(app_folder, "cl_gemm")
    p = subprocess.Popen(["./gpu_perfmon_read", app_file, "gpu", "1024", "1"],\
      cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  else:
    app_folder = utils.get_sample_build_path("ze_gemm")
    app_file = os.path.join(app_folder, "ze_gemm")
    p = subprocess.Popen(["./gpu_perfmon_read", app_file, "1024", "1"],\
      cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stderr
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stdout.find("Job is successfully completed") == -1:
    return stdout
  if stdout.find("add") == -1 or stdout.find("mov") == -1 or stdout.find("send") == -1:
    return stdout
  if not parse(stdout):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("gpu_perfmon_read")
  if option == "cl":
    log = cl_gemm.main("gpu")
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
  option = "cl"
  if len(sys.argv) > 1 and sys.argv[1] == "ze":
    option = "ze"
  log = main(option)
  if log:
    print(log)