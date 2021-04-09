import sys
import os

import build_utils

url = "https://github.com/intel/intel-graphics-compiler.git"
commit = "a1a451f633b0c8ba403e57ffb4ef2a5d898c0951"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_arch_header.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "igc")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "intel-graphics-compiler")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "inc")
  src_path = os.path.join(src_path, "common")

  build_utils.copy(src_path, dst_path, ["igfxfmid.h"])

if __name__ == "__main__":
  main()