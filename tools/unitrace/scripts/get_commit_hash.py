#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import subprocess
import sys
import re

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_commit_hash.py <directory> <file_name>")
    return

  dir_path = sys.argv[1]
  if (not os.path.exists(dir_path)):
    os.mkdir(dir_path)

  file_path = os.path.join(dir_path, sys.argv[2])
  if (os.path.isfile(file_path)):
    os.remove(file_path)

  of = open(file_path, "wt")

  of.write("#ifndef PTI_TOOLS_UNITRACE_COMMIT_HASH_H_\n")
  of.write("#define PTI_TOOLS_UNITRACE_COMMIT_HASH_H_\n\n")

  result = subprocess.run(['git', 'rev-parse', 'HEAD'], stdout=subprocess.PIPE, universal_newlines=True)
  of.write("#define COMMIT_HASH \"")
  of.write(result.stdout[0:len(result.stdout) - 1])
  of.write("\"\n\n")

  of.write("#endif /* PTI_TOOLS_UNITRACE_COMMIT_HASH_ */\n")
  of.close()

if __name__ == "__main__":
  main()
