import os
import sys

def get_script_path():
  path, script = os.path.split(os.path.realpath(__file__))
  return path

def get_root_path():
  head, tail = os.path.split(get_script_path())
  return head

def get_sample_build_path(name):
  head, tail = os.path.split(get_script_path())
  path = os.path.join(head, "samples")
  path = os.path.join(path, name)
  assert os.path.exists(path)
  path = os.path.join(path, "build")
  if not os.path.exists(path):
    os.mkdir(path)
  return path

def get_tool_build_path(name):
  head, tail = os.path.split(get_script_path())
  path = os.path.join(head, "tools")
  path = os.path.join(path, name)
  assert os.path.exists(path)
  path = os.path.join(path, "build")
  if not os.path.exists(path):
    os.mkdir(path)
  return path

def get_build_utils_path():
  head, tail = os.path.split(get_script_path())
  path = os.path.join(head, "build_utils")
  return path

def get_build_flag():
  build_flag = "Release"
  for i in range(1, len(sys.argv)):
    if sys.argv[i] == "-d":
      build_flag = "Debug"
  return build_flag

def add_env(env, name, val):
  if env:
    custom_env = env
  else:
    custom_env = os.environ.copy()
  custom_env[name] = val
  return custom_env

def run_process(p):
  stdout, stderr = p.communicate()
  if sys.version_info.major > 2:
    if stderr:
      stderr = str(stderr, "utf-8")
    if stdout:
      stdout = str(stdout, "utf-8")
  return stdout, stderr