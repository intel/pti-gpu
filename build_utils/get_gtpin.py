import sys
import os

import build_utils

def get_gtpin(build_path):
  platform = sys.platform
  if platform.startswith('win32'):
    gtpin_package = "external-release-gtpin-3.4-win.zip"
    download_link = "https://downloadmirror.intel.com/777295/"
  elif platform.startswith('linux'):
    gtpin_package = "external-release-gtpin-3.4-linux.tar.xz"
    download_link = "https://downloadmirror.intel.com/777295/"
  else:
    print("Platform not supported: ", platform)
    return -2

  build_utils.download(download_link + gtpin_package, build_path)
  arch_file = os.path.join(build_path, gtpin_package)
  build_utils.unpack(arch_file, build_path)

  return 0

def main(argv):
  if len(argv) < 2:
    print("Usage: python get_gtpin.py <build_path>")
    return -1
  return get_gtpin(argv[1])

if __name__ == "__main__":
  sys.exit(main(sys.argv))
