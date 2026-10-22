import sys
import os

import build_utils

url = "https://github.com/intel/intel-graphics-compiler.git"

def main():
  # Usage: python get_iga_headers.py <include_path> <build_path> <iga_commit>
  # The IGA commit is passed in (from Unitrace's CMake) rather than hardcoded, so
  # the pinned revision lives in one place: tools/unitrace/CMakeLists.txt.
  if len(sys.argv) < 4:
    print("Usage: python get_iga_headers.py <include_path> <build_path> <iga_commit>")
    return

  include_path = sys.argv[1]
  if (not os.path.exists(include_path)):
    os.mkdir(include_path)
  dst_path = os.path.join(include_path, "iga")
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

  # GFXCORE_FAMILY and PRODUCT_FAMILY, the values IGC records in a binary's notes.
  igc_dst_path = os.path.join(include_path, "igc")
  if (not os.path.exists(igc_dst_path)):
    os.mkdir(igc_dst_path)
  igc_src_path = os.path.join(clone_path, "inc", "common")
  build_utils.copy(igc_src_path, igc_dst_path, ["igfxfmid.h"])

if __name__ == "__main__":
  main()
