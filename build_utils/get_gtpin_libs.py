import sys
import os

import build_utils
from get_gtpin import get_gtpin

def main(argv):
  if len(argv) < 3:
    print("Usage: python get_gtpin_libs.py <lib_path> <build_path>")
    return -1

  dst_path = argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  build_path = argv[2]

  platform = sys.platform

  res = get_gtpin(build_path)
  if res != 0:
    return res

  if platform.startswith('win32'):
    gtpin_libs = ["gtpin.lib"]
    gtpin_dlls = ["ged.dll",
                  "gtpin.dll",
                  "gtpin_core.dll",
                  "iga_wrapper.dll"]
  elif platform.startswith('linux'):
    gtpin_libs = ["libged.so",
                  "libgtpin_core.so",
                  "libgtpin.so",
                  "libiga_wrapper.so"]
    gtpin_dlls = []
  else:
    print("Platform not supported: ", platform)
    return -2

  src_lib_path = os.path.join(build_path, "Profilers", "Lib", "intel64")

  build_utils.copy(src_lib_path, dst_path, gtpin_libs)
  build_utils.copy(src_lib_path, build_path, gtpin_dlls)

  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv))
