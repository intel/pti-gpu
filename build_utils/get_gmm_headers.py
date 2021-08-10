import sys
import os

import build_utils

url = "https://github.com/intel/gmmlib.git"
commit = "9253eb8b949515d2a5030f99163a35567fe81838"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_gmm_headers.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "igdgmm")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "inc")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "common")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "gmmlib")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "Source")
  src_path = os.path.join(src_path, "inc")
  src_path = os.path.join(src_path, "common")

  build_utils.copy(src_path, dst_path, ["igfxfmid.h"])

if __name__ == "__main__":
  main()