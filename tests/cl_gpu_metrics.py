import os
import subprocess
import sys

import cl_gemm
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

def run(path):
  app_folder = utils.get_sample_build_path("cl_gemm")
  app_file = os.path.join(app_folder, "cl_gemm")
  p = subprocess.Popen(["./cl_gpu_metrics", app_file, "gpu", "1024", "1"],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if not stdout:
    return stderr
  if stdout.find(" CORRECT") == -1:
    return stdout
  if not parse(stderr):
    return stderr
  return None

def main(option):
  path = utils.get_sample_build_path("cl_gpu_metrics")
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