import sys
import os

import build_utils

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_gtpin_libs.py <lib_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  
  build_path = sys.argv[2]
  root_path = build_utils.get_root(sys.argv[2])
  gtpin_path = os.path.join(root_path, "gtpin")
  arch_file = os.path.join(gtpin_path, "external-gtpin-2.11.4-linux.tar.bz2")
  build_utils.unpack(arch_file, build_path)

  src_path = os.path.join(build_path, "Profilers")
  src_path = os.path.join(src_path, "Lib")
  src_path = os.path.join(src_path, "intel64")

  build_utils.copy(src_path, dst_path,
    ["libgcc_s.so.1",
     "libged.so",
     "libgtpin.so",
     "libgtpin_core.so",
     "libiga_wrapper.so",
     "libstdc++.so.6"])

if __name__ == "__main__":
  main()