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
  count_platforms = 0
  count_devices = 0
  if option == "-a":
    lines = output.split("\n")
    for line in lines:
      if line.find("Number of  platforms") != -1:
        items = line.split(" ")
        count_platforms = int(items[-1])
      if line.find("Number of devices") != -1:
        items = line.split(" ")
        count_devices += int(items[-1])
  elif option == "-l":
    lines = output.split("\n")
    for line in lines:
      if line.find("Platform") != -1:
        count_platforms += 1
      if line.find("Device") != -1:
        count_devices += 1
  else:
    return False
  if count_platforms == 0 or count_devices == 0:
      return False
  if count_platforms > count_devices:
    return False
  return True

def run(path, option):
  p = subprocess.Popen(["./dpc_info", option],\
    cwd = path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  stdout, stderr = utils.run_process(p)
  if stderr:
    return stdout
  if not parse(stdout, option):
    return stdout
  return None

def main(option):
    path = utils.get_sample_build_path("dpc_info")
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
    option = "-a"
    if len(sys.argv) > 1 and sys.argv[1] == "-l":
        option = "-l"
    log = main(option)
    if log:
        print(log)