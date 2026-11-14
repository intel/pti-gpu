import importlib
import json
import os
import re
import shutil
import sys
import time
import traceback
from xml.sax.saxutils import escape, quoteattr

import utils

LOG_FILE = "stderr.log"
JSON_REPORT = "results.json"
JUNIT_REPORT = "junit.xml"

PASSED = "PASSED"
FAILED = "FAILED"
SKIPPED = "SKIPPED"
DEFAULT_TIMEOUT = 600 # seconds per test (sample/tool + option), 0 = no limit

samples = [["cl_gemm", "gpu", "cpu"],
           ["cl_gemm_inst", None],
           ["cl_gemm_itt", "gpu", "cpu"],
           ["cl_debug_info", None],
           ["cl_gpu_metrics", None],
           ["cl_gpu_query", None],
           ["cl_hot_functions", "cpu", "gpu", "dpc", "omp"],
           ["cl_hot_kernels", "cpu", "gpu", "dpc", "omp"],
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
           ["dpc_gemm", "gpu", "cpu"],
           ["dpc_info", "-a", "-l"]]

tools = [["instcount", "cl", "ze", "dpc"],
         ["memaccess", "cl", "ze", "dpc"],
         ["gpuinfo", "-l", "-i", "-m"],
         ["sysmon", "none", "-p", "-l", "-d"],
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

def clean(log_dir):
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

  # Remove only the files that run.py itself writes: in the current
  # directory (the default), the repository root and the log directory, if set.
  log_dirs = [os.getcwd(), utils.get_root_path()]
  if log_dir:
    log_dirs.append(log_dir)
  for path in log_dirs:
    for name in [LOG_FILE, JSON_REPORT, JUNIT_REPORT]:
      file_path = os.path.join(path, name)
      if os.path.isfile(file_path):
        os.remove(file_path)

def get_test_name(name, option):
  if option is not None and option != "none":
    return name + " (" + option + ")"
  return name

# Runs one sample/tool option. Returns a result dictionary whose "status" is
# PASSED, FAILED or SKIPPED, and "reason" holds the failure log or skip reason.
def run_test(f, name, option, istool = False, timeout = DEFAULT_TIMEOUT):
  if istool:
    sys.stdout.write("Running " + get_test_name(name, option) + "...")
  else:
    sys.stdout.write("Running sample " + get_test_name(name, option) + "...")
  sys.stdout.flush()

  if istool:
    module = importlib.import_module("tools." + name)
  else:
    module = importlib.import_module("samples." + name)
  status = FAILED
  label = FAILED
  timed_out = False
  start = time.monotonic()
  utils.set_deadline(timeout)
  try:
    log = module.main(option)
    if log is None:
      status = label = PASSED
  except utils.TestTimeout as e:
    log = "Timeout after " + str(timeout) + " s while running: " + str(e) + "\n"
    label = "FAILED (timeout)"
    timed_out = True
  except utils.TestSkipped as e:
    log = str(e)
    status = SKIPPED
    label = "SKIPPED (" + log + ")"
  finally:
    utils.set_deadline(None)
  elapsed = time.monotonic() - start

  if status == FAILED:
    sys.stdout.write(log)
    if (option is not None):
      f.write("======= " + name + " (" + option + ") =======\n")
    else:
      f.write("======= " + name + " =======\n")
    f.write(log)
    f.write("\n")
  sys.stdout.write(label + "\n")

  return {"name": name, "option": option,
          "kind": "tool" if istool else "sample",
          "status": status, "timeout": timed_out,
          "time": round(elapsed, 3),
          "reason": log if status != PASSED else None}

# Kept for callers that only need pass/fail: True if the test passed or was
# skipped, False if it failed.
def test(f, name, option, istool = False, timeout = DEFAULT_TIMEOUT):
  return run_test(f, name, option, istool, timeout)["status"] != FAILED

def write_reports(results, log_dir):
  path = log_dir if log_dir else "."
  with open(os.path.join(path, JSON_REPORT), "wt") as f:
    json.dump({"passed": sum(r["status"] == PASSED for r in results),
               "failed": sum(r["status"] == FAILED for r in results),
               "skipped": sum(r["status"] == SKIPPED for r in results),
               "results": results}, f, indent = 2)
    f.write("\n")

  with open(os.path.join(path, JUNIT_REPORT), "wt") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
    f.write('<testsuite name="pti-gpu" tests="%d" failures="%d" skipped="%d" '
            'time="%.3f">\n' % (len(results),
                                sum(r["status"] == FAILED for r in results),
                                sum(r["status"] == SKIPPED for r in results),
                                sum(r["time"] for r in results)))
    for r in results:
      f.write('  <testcase classname=%s name=%s time="%.3f"' %
              (quoteattr(r["kind"] + "." + r["name"]),
               quoteattr(get_test_name(r["name"], r["option"])), r["time"]))
      if r["status"] == PASSED:
        f.write('/>\n')
        continue
      f.write('>\n')
      if r["status"] == SKIPPED:
        f.write('    <skipped message=%s/>\n' % quoteattr(r["reason"]))
      else:
        message = "timeout" if r["timeout"] else "failed"
        f.write('    <failure message="%s">%s</failure>\n' %
                (message, escape(r["reason"])))
      f.write('  </testcase>\n')
    f.write('</testsuite>\n')

def print_summary(results):
  passed = sum(r["status"] == PASSED for r in results)
  failed = [r for r in results if r["status"] == FAILED]
  skipped = [r for r in results if r["status"] == SKIPPED]

  # Keep this line unchanged: CI and scripts look for it.
  print("PASSED: " + str(passed) + " / FAILED: " + str(len(failed)))
  print("SKIPPED: " + str(len(skipped)))
  if failed:
    print("Failed tests:")
    for r in failed:
      print("  " + get_test_name(r["name"], r["option"]) +
            (" (timeout)" if r["timeout"] else ""))
  if skipped:
    print("Skipped tests:")
    for r in skipped:
      print("  " + get_test_name(r["name"], r["option"]) + ": " + r["reason"])

def main():
  try:
    tmpl = ".+"
    for i in range(1, len(sys.argv) - 1):
      if sys.argv[i] == "-s":
        tmpl = sys.argv[i + 1]

    # --log-dir <path> (or PTI_TEST_LOG_DIR): where stderr.log is written.
    # Default: the current directory, as before.
    log_dir = os.environ.get("PTI_TEST_LOG_DIR")
    timeout = DEFAULT_TIMEOUT
    for i in range(1, len(sys.argv) - 1):
      if sys.argv[i] == "--log-dir":
        log_dir = sys.argv[i + 1]
      elif sys.argv[i] == "--timeout":
        timeout = int(sys.argv[i + 1])

    for i in range(1, len(sys.argv)):
      if sys.argv[i] == "-c":
        clean(log_dir)
        return 0
      elif sys.argv[i] == "--skip-oa":
        # Skip every test that opens GPU performance (OA) metric streams.
        os.environ[utils.SKIP_OA_ENV] = "1"

    log_path = LOG_FILE
    if log_dir:
      os.makedirs(log_dir, exist_ok = True)
      log_path = os.path.join(log_dir, LOG_FILE)
    f = open(log_path, "wt")

    results = []

    for sample in samples:
      name = sample[0]
      if re.search(tmpl, name) == None:
        continue
      for i in range(1, len(sample)):
        results.append(run_test(f, name, sample[i], False, timeout))
        f.flush()

    for tool in tools:
      name = tool[0]
      if re.search(tmpl, name) == None:
        continue
      for i in range(1, len(tool)):
        results.append(run_test(f, name, tool[i], True, timeout))
        f.flush()

    f.close()

    write_reports(results, log_dir)
    print_summary(results)

    tests_failed = sum(r["status"] == FAILED for r in results)
    return 0 if tests_failed == 0 else  1

  except:
    print(traceback.format_exc())
    return 1

if __name__ == "__main__":
  sys.exit( main() )
