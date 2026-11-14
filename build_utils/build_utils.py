import os
import sys
import shutil
import stat
import subprocess
import time

try:
  import fcntl
except ImportError: # Windows
  fcntl = None
  import msvcrt

def lock_file(f):
  if fcntl is not None:
    fcntl.flock(f, fcntl.LOCK_EX)
    return
  # msvcrt.LK_LOCK gives up after about 10 s, and a clone can take longer.
  while True:
    try:
      msvcrt.locking(f.fileno(), msvcrt.LK_LOCK, 1)
      return
    except OSError:
      time.sleep(1)

def unlock_file(f):
  if fcntl is not None:
    fcntl.flock(f, fcntl.LOCK_UN)
  else:
    f.seek(0)
    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)

# Make git give up on a transfer that stays below 1 KB/s for 60 s, instead of
# hanging until the build (or a test timeout) kills it.
GIT_STALL_OPTIONS = ["-c", "http.lowSpeedLimit=1000", "-c", "http.lowSpeedTime=60"]

# Written into a clone once the requested commit is checked out completely.
CLONE_STAMP = ".pti-clone-commit"

def git(args, cwd):
  return subprocess.call(["git"] + GIT_STALL_OPTIONS + args, cwd = cwd) == 0

def remove_tree(path):
  def make_writable_and_retry(func, failed_path, exc_info):
    os.chmod(failed_path, stat.S_IWRITE) # read-only git objects on Windows
    func(failed_path)
  if os.path.exists(path):
    shutil.rmtree(path, onerror = make_writable_and_retry)

def is_cloned(clone_path, commit):
  try:
    with open(os.path.join(clone_path, CLONE_STAMP)) as f:
      return f.read().strip() == commit
  except OSError:
    return False

def clone(url, commit, clone_path):
  # Several header scripts may clone the same repository into the same path
  # (e.g. get_iga_headers.py and get_igc_headers.py) and run concurrently in a
  # parallel build. Serialize them so nobody copies from a half-done clone.
  with open(clone_path.rstrip(os.sep) + ".lock", "w") as lock:
    lock_file(lock)
    try:
      if is_cloned(clone_path, commit):
        return
      # A clone without the stamp is incomplete (e.g. an interrupted build) or
      # at another commit: start over.
      remove_tree(clone_path)

      # Fetch into a temporary directory and move it into place only when
      # complete. Try a shallow fetch of the commit first (fast), then fall
      # back to a full clone.
      tmp_path = clone_path.rstrip(os.sep) + ".partial"
      remove_tree(tmp_path)
      os.makedirs(tmp_path)
      ok = (git(["init", "-q"], tmp_path) and
            git(["fetch", "-q", "--depth", "1", url, commit], tmp_path) and
            git(["checkout", "-q", "FETCH_HEAD"], tmp_path))
      if not ok:
        remove_tree(tmp_path)
        ok = (git(["clone", url, tmp_path], None) and
              git(["checkout", commit], tmp_path))
      if not ok:
        remove_tree(tmp_path)
        raise RuntimeError("Failed to fetch " + url + " at " + commit +
                           " (check network and proxy settings)")

      with open(os.path.join(tmp_path, CLONE_STAMP), "w") as f:
        f.write(commit + "\n")
      os.rename(tmp_path, clone_path)
    finally:
      unlock_file(lock)

def copy(src_path, dst_path, file_list):
  for file in file_list:
    dst_file = os.path.join(dst_path, file)
    if os.path.isfile(dst_file):
      os.remove(dst_file)

    src_file = os.path.join(src_path, file)
    assert os.path.isfile(src_file)

    shutil.copy(src_file, dst_file)
