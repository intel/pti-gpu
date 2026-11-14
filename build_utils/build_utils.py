import fcntl
import os
import sys
import shutil
import subprocess

def clone(url, commit, clone_path):
  # Several header scripts may clone the same repository into the same path
  # (e.g. get_iga_headers.py and get_igc_headers.py) and run concurrently in a
  # parallel build. Serialize them so nobody copies from a half-done clone.
  with open(clone_path.rstrip(os.sep) + ".lock", "w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    if (not os.path.exists(clone_path)):
      subprocess.call(["git", "clone", url, clone_path])
      subprocess.call(["git", "checkout", commit], cwd = os.path.abspath(clone_path))

def copy(src_path, dst_path, file_list):
  for file in file_list:
    dst_file = os.path.join(dst_path, file)
    if os.path.isfile(dst_file):
      os.remove(dst_file)

    src_file = os.path.join(src_path, file)
    assert os.path.isfile(src_file)

    shutil.copy(src_file, dst_file)
