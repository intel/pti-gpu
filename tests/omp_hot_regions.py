import os
import subprocess
import sys

import omp_gemm
import utils

def config(path):
  e = utils.add_env(None, "CXX", "icpc")
  p = subprocess.Popen(["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."], env = e,\
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
    items = line.split("|")
    if len(items) < 6 or line.find("Region") != -1:
      continue
    region_id = int(items[1].strip(), 16)
    region_type = items[2].strip()
    call_count = int(items[3].strip())
    time = float(items[4].strip())
    if not region_id or not region_type or call_count <= 0:
      return False
    total_time += time
  if total_time <= 0:
    return False
  return True

def run(path, option):
  app_folder = utils.get_sample_build_path("omp_gemm")
  app_file = os.path.join(app_folder, "omp_gemm")
  e = utils.add_env(None, "OMP_TOOL_LIBRARIES", "./libomp_hot_regions.so")
  p = subprocess.Popen([app_file, option, "1024", "1"], env = e,\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stderr
  if stdout.find(" CORRECT") == -1:
    return stdout
  if stdout.find("Job is successfully completed") == -1:
    return stdout
  if option == "gpu" and stdout.find("Target") == -1:
    return stdout
  if option == "cpu" and stdout.find("Parallel") == -1:
    return stdout
  if not parse(stdout):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("omp_hot_regions")
  log = omp_gemm.main(option)
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
  log = main(option)
  if log:
    print(log)