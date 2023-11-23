import sys
import os

import build_utils
from get_gtpin import get_gtpin

def main(argv):
  if len(argv) < 4:
    print("Usage: python get_gtpin_libs.py <lib_path> <build_path> <util_name>")
    return -1

  dst_path = argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  build_path = argv[2]
  util_name = argv[3]

  res = get_gtpin(build_path)
  if res != 0:
    return res

  src_util_path = os.path.join(build_path, "Profilers", "Examples", "utils")

  util_files = [
    util_name + '.cpp',
    util_name + '.h'
  ]

  build_utils.copy(src_util_path, dst_path, util_files)
  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv))
