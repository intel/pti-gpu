import os
import signal
import sys
import subprocess
import time

# Per-test deadline (time.monotonic() value) set by run.py; None means no limit.
_deadline = None

class TestTimeout(Exception):
  pass

def set_deadline(seconds):
  global _deadline
  _deadline = (time.monotonic() + seconds) if seconds and seconds > 0 else None

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

def get_sample_executable_path(name, build_type = ""):
  path = get_sample_build_path(name)
  assert os.path.exists(path)
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

def _kill(p):
  if sys.platform != 'win32':
    try:
      os.killpg(p.pid, signal.SIGKILL)
      return
    except OSError:
      pass
  p.kill()

def run_process(command, path, environ = None):
  shell = True if sys.platform == 'win32' else False
  timeout = None
  if _deadline is not None:
    timeout = _deadline - time.monotonic()
    if timeout <= 0:
      raise TestTimeout(" ".join(command))
  # A new session lets a timeout kill the whole process tree (e.g. make, or a
  # tool that launches the application), not only the direct child.
  new_session = (sys.platform != 'win32')
  p = subprocess.Popen(command, cwd = path, shell = shell,\
    env = environ, stdout = subprocess.PIPE, stderr = subprocess.PIPE, encoding = "latin-1", text = True,\
    start_new_session = new_session)
  try:
    stdout, stderr = p.communicate(timeout = timeout)
  except subprocess.TimeoutExpired:
    _kill(p)
    p.communicate()
    raise TestTimeout(" ".join(command))

  return stdout, stderr
