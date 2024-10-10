import importlib
import os
import re
import shutil
import sys
import traceback

import utils

samples = [["cl_gemm", "gpu", "cpu"],
           ["cl_gemm_inst", None],
           ["cl_gemm_itt", "gpu", "cpu"],
           ["cl_debug_info", None],
           ["cl_gpu_metrics", None],
           ["cl_gpu_query", None],
           ["cl_hot_functions", "cpu", "gpu", "dpc", "omp"],
           ["cl_hot_kernels", "cpu", "gpu", "dpc", "omp"],
           ["gpu_perfmon_read", "cl", "ze", "dpc"],
           ["gpu_perfmon_set", None],
           ["ze_info", "-a", "-l"],
           ["ze_sysman", None],
           ["ze_gemm", None],
           ["ze_debug_info", "gpu", "dpc"],
           ["ze_hot_functions", "gpu", "dpc", "omp"],
           ["ze_hot_kernels", "gpu", "dpc", "omp"],
           ["ze_metric_info", None],
           ["ze_metric_query", "gpu", "dpc"],
           ["ze_metric_streamer", "gpu", "dpc"],
           ["omp_gemm", "gpu", "cpu"],
           ["omp_hot_regions", "gpu", "cpu"],
           ["dpc_gemm", "gpu", "cpu", "host"],
           ["dpc_info", "-a", "-l"]]

tools = [["instcount", "cl", "ze", "dpc"],
         ["memaccess", "cl", "ze", "dpc"],
         ["gpuinfo", "-l", "-i", "-m"],
         ["sysmon", "-p", "-l", "-d"],
         ["unitrace-build", "none"],
         ["unitrace-test",
          "-c", "-h", "-d", "-t", "-s",
          "--chrome-call-logging",
          "--chrome-device-logging",
          "--chrome-kernel-logging",
          "--chrome-sycl-logging",
          "--conditional-collection"],
         ["onetrace",
          "-c", "-h", "-d", "-v", "-t", "-s",
          "--demangle",
          "--kernels-per-tile",
          "--chrome-call-logging",
          "--chrome-device-timeline",
          "--chrome-kernel-timeline",
          "--chrome-device-stages",
          "--conditional-collection",
          "cl", "ze", "omp"],
         ["cl_tracer",
          "-c", "-h", "-d", "-v", "-t", "-s",
          "--demangle",
          "--chrome-call-logging",
          "--chrome-device-timeline",
          "--chrome-kernel-timeline",
          "--chrome-device-stages",
          "--conditional-collection",
          "gpu", "dpc", "omp"],
         ["ze_tracer",
          "-c", "-h", "-d", "-v", "-t", "-s",
          "--demangle",
          "--kernels-per-tile",
          "--chrome-call-logging",
          "--chrome-device-timeline",
          "--chrome-kernel-timeline",
          "--chrome-device-stages",
          "--conditional-collection",
          "dpc", "omp"],
        ["oneprof",
         "-i", "-m", "-k", "-a", "-q", "cl", "ze", "omp"]]

def remove_python_cache(path):
  files = os.listdir(path)
  for file in files:
    if file.endswith(".pyc"):
      os.remove(os.path.join(path, file))

  path = os.path.join(path, "__pycache__")
  if os.path.exists(path):
    shutil.rmtree(path)

def clean():
  for sample in samples:
    path = utils.get_sample_build_path(sample[0])
    if os.path.exists(path):
      shutil.rmtree(path)

  for tool in tools:
    path = utils.get_tool_build_path(tool[0])
    if os.path.exists(path):
      shutil.rmtree(path)

  remove_python_cache(utils.get_build_utils_path())
  remove_python_cache(utils.get_script_path())
  remove_python_cache(os.path.join(utils.get_script_path(), "samples"))
  remove_python_cache(os.path.join(utils.get_script_path(), "tools"))

  for root, subdirs, files in os.walk(utils.get_root_path()):
    for file in files:
      if file.endswith(".log"):
        os.remove(os.path.join(root, file))

def test(f, name, option, istool = False):
  if istool:
    if (option and (option != "none")):
      sys.stdout.write("Running " + name + " (" + option + ")...")
    else:
      sys.stdout.write("Running " + name + "...")
  else:
    if option:
      sys.stdout.write("Running sample " + name + " (" + option + ")...")
    else:
      sys.stdout.write("Running sample " + name + "...")
  sys.stdout.flush()

  if istool:
    module = importlib.import_module("tools." + name)
  else:
    module = importlib.import_module("samples." + name)
  log = module.main(option)

  if (log is not None):
    sys.stdout.write(log)
    sys.stdout.write("FAILED\n")
    if (option is not None):
      f.write("======= " + name + " (" + option + ") =======\n")
    else:
      f.write("======= " + name + " =======\n")
    f.write(log)
    f.write("\n")
    return False
  else:
    sys.stdout.write("PASSED\n")
    return True

def main():
  try:
    tmpl = ".+"
    for i in range(1, len(sys.argv) - 1):
      if sys.argv[i] == "-s":
        tmpl = sys.argv[i + 1]

    for i in range(1, len(sys.argv)):
      if sys.argv[i] == "-c":
        clean()
        return 0

    f = open("stderr.log", "wt")

    tests_passed = 0
    tests_failed = 0

    for sample in samples:
      name = sample[0]
      if re.search(tmpl, name) == None:
        continue
      for i in range(1, len(sample)):
        if test(f, name, sample[i]):
          tests_passed += 1
        else:
          tests_failed += 1

    for tool in tools:
      name = tool[0]
      if re.search(tmpl, name) == None:
        continue
      for i in range(1, len(tool)):
        if test(f, name, tool[i], True):
          tests_passed += 1
        else:
          tests_failed += 1

    f.close()

    print("PASSED: " + str(tests_passed) + " / FAILED: " + str(tests_failed))

    return 0 if tests_failed == 0 else  1

  except:
    print(traceback.format_exc())
    return 1

if __name__ == "__main__":
  sys.exit( main() )
