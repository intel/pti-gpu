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
  if option == "-p":
    lines = output.split("\n")
    total_devices = 0
    for line in lines:
      if line.find("GPU") != -1:
        total_devices += 1
    if total_devices < 1:
      return False
  elif option == "-l":
    count_drivers = 0
    count_devices = 0
    lines = output.split("\n")
    for line in lines:
      if line.find("Driver") != -1:
        count_drivers += 1
      if line.find("Device") != -1:
        count_devices += 1
  return True

def run(path, option):
  p = subprocess.Popen(["./sysmon", option],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if not parse(stdout, option):
    return stdout
  return None

def main(option):
  path = utils.get_tool_build_path("sysmon")
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
  option = "-p"
  if len(sys.argv) > 1 and sys.argv[1] == "-l":
    option = "-l"
  log = main(option)
  if log:
    print(log)