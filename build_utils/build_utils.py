import os
import sys
import shutil
import subprocess

def clone(url, commit, clone_path):
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
