import importlib
import os
import subprocess
import sys

import utils

samples = [["cl_gemm", "gpu"],
           ["ze_gemm", None],
           ["omp_gemm", "gpu"],
           ["dpc_gemm", "gpu"]]

def run(path, tooloption, app, option):
  app_folder = utils.get_sample_executable_path(app)
  app_file = os.path.join(app_folder, app)

  if (sys.platform == 'win32'):
    app_file += ".exe"

  if (option != None):
    if (sys.platform != 'win32'):
      command = ["./unitrace", "--opencl", tooloption, app_file, option, "1024", "1"]
    else:
      command = ["unitrace", "--opencl", tooloption, app_file, option, "1024", "1"]
  else:
    if (sys.platform != 'win32'):
      command = ["./unitrace", "--opencl", tooloption, app_file, "1024", "1"]
    else:
      command = ["unitrace", "--opencl", tooloption, app_file, "1024", "1"]
  stdout, stderr = utils.run_process(command, path)

  if (stdout is None):
    return "stdout is empty"
  
  if ('error' in stderr or 'ERROR' in stderr or 'Error' in stderr):
    print("======================== Test ================================")
    print(stdout)
    print(stderr)
    print("==============================================================")

  if (tooloption == "-t"):
    if (stderr is None):
      return "stderr is empty"

    occurrences = stderr.count("(end)\n")
    expected = 0
    if (app == "dpc_gemm"): 
      expected = 5
    elif (app == "ze_gemm"):
      expected = 6
    elif (app == "cl_gemm"):
      expected = 4
    elif (app == "omp_gemm"):
      expected = 17
        
      
    if (occurrences != expected): 
      if (option is None):
        log = " (" + app + " " + str(occurrences) + " captured but " + str(expected) + " expected) "
      else:
        log = " (" + app + " " + option + " " + str(occurrences) + " captured but " + str(expected) + " expected) "
      return log

  return None

def main(tooloption):
  path = utils.get_tool_build_path("unitrace")
  log = None
    
  for sample in samples:
    for i in range(1, len(sample)):
      module = importlib.import_module("samples." + sample[0])
      module.main(sample[i])
      runlog = run(path, tooloption, sample[0], sample[i])
      if runlog is not None:
        if (log is not None):
          log = log + runlog
        else:
          log = runlog

  return log


if __name__ == "__main__":
  if len(sys.argv) > 1:
    log = main(sys.argv[1])
    if log is not None:
      print(log)
