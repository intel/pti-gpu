#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import platform
import os
import sys
import shutil
import subprocess

url = "https://github.com/intel/ittapi.git"

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

def build(build_dir, install_path):
  curr_wrkng_dir = os.getcwd()
  itt_dir = build_dir + "/ittapi"
  os.chdir(itt_dir)
  cmake_flags = ""
  if (install_path != ""):
    cmake_flags += "-DCMAKE_INSTALL_PREFIX={} ".format(install_path)
  if platform.system() == 'Windows':
    os.system("cmake -G \"NMake Makefiles\" {} .".format(cmake_flags))
    os.system("nmake")
    if (install_path != ""):
      os.system("nmake install")
    shutil.copyfile("./bin/libittnotify.lib", build_dir + "/libittnotify.lib")
  else :
    os.system("cmake {} .".format(cmake_flags))
    os.system("make")
    if (install_path != ""):
      os.system("make install")
    shutil.copyfile("./bin/libittnotify.a", build_dir + "/libittnotify.a")

  # Restore back the location
  os.chdir(curr_wrkng_dir)

def main():
  if len(sys.argv) < 4:
    print("Usage: python get_itt.py <include_path> <build_path> <commit_hash> [<cmake flags>]")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "ittheaders")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "ittapi")

  commit = sys.argv[3]
  clone(url, commit, clone_path)

  if len(sys.argv) > 4:
    install_path = sys.argv[4]
  else:
    install_path = ""

  src_path = os.path.join(clone_path, "src")
  src_path = os.path.join(src_path, "ittnotify")
  copy(src_path, dst_path, ["disable_warnings.h", "ittnotify_config.h",
    "ittnotify_static.c", "ittnotify_static.h", "ittnotify_types.h"])

  src_path = os.path.join(clone_path, "include")
  copy(src_path, dst_path, ["ittnotify.h"])

  dst_path = os.path.join(dst_path, "legacy")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  src_path = os.path.join(src_path, "legacy")
  copy(src_path, dst_path, ["ittnotify.h"])

  # build ittnotify
  build(sys.argv[2], install_path)

if __name__ == "__main__":
  main()
