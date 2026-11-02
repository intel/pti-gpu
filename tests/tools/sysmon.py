import os
import re
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

# Default mode and -p print, for every GPU:
#   GPU <id>: <name>    PCI Bus: ...
#   ...
#   EU Count: <n>    Threads Per EU: <n>    EU SIMD Width: <n>    Total Memory(MB): ...
# The EU values must be positive numbers; "0" or "unknown" means sysmon could
# not read them (GSD-13969: "EU SIMD Width: 0" on BMG).
EU_TOPOLOGY = re.compile(
    r"EU Count: (\S+)\s+Threads Per EU: (\S+)\s+EU SIMD Width: (\S+)")

def parse_short_info(output):
  lines = output.split("\n")
  total_devices = 0
  topology = []
  for line in lines:
    if re.match(r"GPU \d+: ", line):
      total_devices += 1
    match = EU_TOPOLOGY.search(line)
    if match:
      topology.append(match.groups())
  if total_devices < 1 or len(topology) != total_devices:
    return False
  for values in topology:
    for value in values:
      if not value.isdigit() or int(value) < 1:
        return False
  return True

def parse(output, option):
  if option == "none" or option == "-p":
    return parse_short_info(output)
  elif option == "-l":
    count_drivers = 0
    count_devices = 0
    lines = output.split("\n")
    for line in lines:
      if line.find("Driver") != -1:
        count_drivers += 1
      if line.find("Device") != -1:
        count_devices += 1
    if count_drivers < 1:
      return False
    if count_devices < 1:
      return False
  elif option == "-d":
    lines = output.split("\n")
    total_eu_count = 0
    for line in lines:
      if line.find("Total EU Count") == 0:
        items = line.split(",")
        total_eu_count += int(items[1].strip())
    if total_eu_count < 1:
      return False
  return True

def run(path, option):
  command = ["./sysmon"] if option == "none" else ["./sysmon", option]
  stdout, stderr = utils.run_process(command, path)
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
  if len(sys.argv) > 1 and sys.argv[1] == "none":
    option = "none"
  elif len(sys.argv) > 1 and sys.argv[1] == "-l":
    option = "-l"
  elif len(sys.argv) > 1 and sys.argv[1] == "-d":
    option = "-d"
  log = main(option)
  if log:
    print(log)