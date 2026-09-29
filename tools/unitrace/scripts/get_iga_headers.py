import sys
import os

# This script lives under tools/unitrace/scripts, not next to build_utils.py, so
# add the shared build_utils directory to the import path before importing it.
sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "build_utils"))

import build_utils

url = "https://github.com/intel/intel-graphics-compiler.git"

def main():
  # Usage: python get_iga_headers.py <include_path> <build_path> <iga_commit>
  # The IGA commit is passed in (from Unitrace's CMake) rather than hardcoded, so
  # the pinned revision lives in one place: tools/unitrace/CMakeLists.txt.
  if len(sys.argv) < 4:
    print("Usage: python get_iga_headers.py <include_path> <build_path> <iga_commit>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "iga")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "intel-graphics-compiler")

  commit = sys.argv[3]
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "visa")
  src_path = os.path.join(src_path, "iga")
  src_path = os.path.join(src_path, "IGALibrary")
  src_path = os.path.join(src_path, "api")

  # Only the C API headers reachable from iga.h.
  build_utils.copy(src_path, dst_path,
    ["iga.h",
     "iga_types_ext.hpp",
     "iga_types_swsb.hpp",
     "iga_bxml_ops.hpp",
     "iga_bxml_enums.hpp"])

if __name__ == "__main__":
  main()
