import os
import shutil
import subprocess

def clone(url, commit, clone_path):
  if (not os.path.exists(clone_path)):
    subprocess.check_call(["git", "clone", url, clone_path])
    subprocess.check_call(["git", "checkout", commit], cwd = os.path.abspath(clone_path))
  # Reject incomplete clones or stale checkouts instead of copying unpinned headers.
  expected = subprocess.check_output(
    ["git", "rev-parse", "--verify", commit + "^{commit}"],
    cwd=os.path.abspath(clone_path), text=True).strip()
  actual = subprocess.check_output(
    ["git", "rev-parse", "--verify", "HEAD"],
    cwd=os.path.abspath(clone_path), text=True).strip()
  if actual != expected:
    raise RuntimeError(f"{clone_path} is at {actual}, expected {commit} ({expected}); "
                       "remove the cached clone and retry")

def copy(src_path, dst_path, file_list):
  for file in file_list:
    dst_file = os.path.join(dst_path, file)
    if os.path.isfile(dst_file):
      os.remove(dst_file)

    src_file = os.path.join(src_path, file)
    if not os.path.isfile(src_file):
      raise FileNotFoundError(src_file)

    shutil.copy(src_file, dst_file)
