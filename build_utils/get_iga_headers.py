import sys
import os

import build_utils

url = "https://github.com/intel/intel-graphics-compiler.git"
commit = "d5bef0c991c41e03a567187eb78fe35b6c116847"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_iga_headers.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "iga")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "intel-graphics-compiler")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "visa")
  src_path = os.path.join(src_path, "iga")
  src_path = os.path.join(src_path, "IGALibrary")
  src_path = os.path.join(src_path, "api")

  build_utils.copy(src_path, dst_path,
    ["iga.h",
     "iga_types_ext.hpp",
     "iga_types_swsb.hpp",
     "iga_bxml_ops.hpp",
     "iga_bxml_enums.hpp",
     "kv.h",
     "kv.hpp"])

if __name__ == "__main__":
  main()