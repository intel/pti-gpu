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

def parse(output):
  lines = output.split("\n")
  total_groups = 0
  total_metrics = 0
  for line in lines:
    if line.find("Metric Group") != -1:
      items = line.split(":")
      if len(items) == 2:
        if items[0].strip() and items[1].strip():
          subitems = items[1].split("/")
          if len(subitems) == 3:
            total_groups += 1
    elif line.find("Metric") != -1:
      items = line.split(":")
      if len(items) == 2:
        if items[0].strip() and items[1].strip():
          subitems = items[1].split("/")
          if len(subitems) == 4:
            total_metrics += 1
  if total_groups == 0:
    return False
  if total_metrics <= total_groups:
    return False
  return True

def run(path):
  p = subprocess.Popen(["./ze_metric_info"],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if not parse(stdout):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("ze_metric_info")
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