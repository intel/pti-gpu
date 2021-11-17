import os
import subprocess
import sys

import utils

if sys.platform == 'win32':
  file_extention = ".exe"
  file_name_prefix = ""
  make = ["cmake", "--build", ".", "--config", utils.get_build_flag()]
else:
  file_extention = ""
  file_name_prefix = "./"
  make = ["make"]

def config(path):
  cmake = ["cmake",\
    "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)
  if stderr and stderr.find("CMake Error") != -1:
    return stderr
  return None

def build(path):
  stdout, stderr = utils.run_process(make, path)
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
  command = [file_name_prefix + "dpc_info" + file_extention, option]
  stdout, stderr = utils.run_process(command,\
    utils.get_sample_executable_path("dpc_info", utils.get_build_flag()))
  if stderr:
    return stderr
  if not stdout:
    return "stdout is empty"
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