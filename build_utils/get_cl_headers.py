import sys
import os

import build_utils

url = "https://github.com/KhronosGroup/OpenCL-Headers.git"
commit = "dcd5bede6859d26833cd85f0d6bbcee7382dc9b3"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_ocl_headers.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.makedirs(dst_path, exist_ok=True)
  dst_path = os.path.join(dst_path, "CL")
  if (not os.path.exists(dst_path)):
    os.makedirs(dst_path, exist_ok=True)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "OpenCL-Headers")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "CL")
  build_utils.copy(src_path, dst_path, ["cl.h", "cl_gl.h", "cl_version.h", "cl_platform.h"])

if __name__ == "__main__":
  main()
