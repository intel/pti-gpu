import sys
import os

import build_utils

url = "https://github.com/intel/intel-graphics-compiler.git"
commit = "d5bef0c991c41e03a567187eb78fe35b6c116847"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_igc_headers.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "igc")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "ocl_igc_shared")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "executable_format")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "intel-graphics-compiler")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "IGC")
  src_path = os.path.join(src_path, "AdaptorOCL")
  src_path = os.path.join(src_path, "ocl_igc_shared")
  src_path = os.path.join(src_path, "executable_format")

  build_utils.copy(src_path, dst_path, ["program_debug_data.h", "patch_list.h"])

if __name__ == "__main__":
  main()