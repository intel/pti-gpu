import os
import subprocess
import sys

import utils

def config(path):
  cmake = ["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  stdout, stderr = utils.run_process(["make"], path)
  if stderr and stderr.lower().find("error") != -1:
    return stderr
  return None

def parse(output, option):
  count_drivers = 0
  count_devices = 0
  if option == "-a":
    number_of_drivers = 0
    lines = output.split("\n")
    for line in lines:
      if line.find("Number of drivers") != -1:
        items = line.split(" ")
        number_of_drivers = int(items[-1])
      if line.find("Number of devices") != -1:
        items = line.split(" ")
        count_devices += int(items[-1])
      if line.find("Driver") != -1:
        subline = list(filter(None, line.split(" ")))
        if len(subline) == 2 and subline[0] == "Driver":
          count_drivers += 1
    if(number_of_drivers != count_drivers):
      return False
  elif option == "-l":
    lines = output.split("\n")
    for line in lines:
      if line.find("Driver") != -1:
        count_drivers += 1
      if line.find("Device") != -1:
        count_devices += 1
  else:
    return False
  if count_drivers == 0 or count_devices == 0:
      return False
  if count_drivers > count_devices:
    return False
  return True

def run(path, option):
  command = ["./ze_info", option]
  stdout, stderr = utils.run_process(command, path)
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
  if not parse(stdout, option):
    return stdout
  return None

def main(option):
  path = utils.get_sample_build_path("ze_info")
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