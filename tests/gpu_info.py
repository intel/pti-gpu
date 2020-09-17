import os
import subprocess
import sys

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

def parse(output, option):
  if option == "-d":
    lines = output.split("\n")
    total_values = 0
    for line in lines:
      if line.find("Device Information") != -1:
        continue
      items = line.strip().split(":")
      if len(items) == 2:
        if items[0].strip() and items[1].strip():
          total_values += 1
    if total_values < 1:
      return False
  elif option == "-m":
    lines = output.split("\n")
    total_groups = 0
    total_sets = 0
    total_metrics = 0
    for line in lines:
      if line.find("Metric Group") != -1:
        items = line.split(":")
        if len(items) == 2:
          if items[0].strip() and items[1].strip():
            total_groups += 1
      elif line.find("Metric Set") != -1:
        items = line.split(":")
        if len(items) == 2:
          if items[0].strip() and items[1].strip():
            total_sets += 1
      elif line.find("Metric") != -1:
        items = line.split(":")
        if len(items) == 2:
          if items[0].strip() and items[1].strip():
            subitems = items[1].split("/")
            if len(subitems) == 3:
              total_metrics += 1
    if total_groups == 0:
      return False
    if total_sets <= total_groups:
      return False
    if total_metrics <=  total_sets:
      return False
  else:
    return False

  return True

def run(path, option):
  p = subprocess.Popen(["./gpu_info", option],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stdout
  if stdout.find("Job is successfully completed") == -1:
    return stdout
  if not parse(stdout, option):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("gpu_info")
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
  option = "-d"
  if len(sys.argv) > 1 and sys.argv[1] == "-m":
    option = "-m"
  log = main(option)
  if log:
    print(log)