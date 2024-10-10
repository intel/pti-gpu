import importlib
import os
import subprocess
import sys

import utils

def config(path):
  if (sys.platform != 'win32'):
    cmake = ["cmake",\
      "-DBUILD_WITH_MPI=0", "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  else:
    cmake = ["cmake",\
      "-G", "NMake Makefiles", "-DBUILD_WITH_MPI=0", "-DCMAKE_BUILD_TYPE=" + utils.get_build_flag(), ".."]
  stdout, stderr = utils.run_process(cmake, path)

  if stderr and stderr.find("CMake Error") != -1:
    print("======================== CMake ===============================")
    print(stdout)
    print(stderr)
    print("==============================================================")

    return stderr

  return None

def build(path):
  if (sys.platform != 'win32'):
    stdout, stderr = utils.run_process(["make"], path)
  else:
    stdout, stderr = utils.run_process(["nmake"], path)

  if stderr and stderr.lower().find("error") != -1:
    print("======================== Build ===============================")
    print(stdout)
    print(stderr)
    print("==============================================================")

    return stderr

  return None

def main(tooloption):
  path = utils.get_tool_build_path("unitrace")

  log = config(path)
  if log is not None:
    return log

  log = build(path)
  return log
    
if __name__ == "__main__":
  if len(sys.argv) > 1:
    log = main(sys.argv[1])
    if log is not None:
      print(log)
