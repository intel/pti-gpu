import sys
import os

import build_utils

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_gtpin_libs_legacy.py <lib_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  
  build_path = sys.argv[2]
  if sys.platform == 'win32':
    gtpin_package = "external-gtpin-2.19-win.zip"
    download_link = "https://downloadmirror.intel.com/686382/"
  else:
    gtpin_package = "external-gtpin-2.19-linux.tar.xz"
    download_link = "https://downloadmirror.intel.com/686383/"
  build_utils.download(download_link + gtpin_package, build_path)
  arch_file = os.path.join(build_path, gtpin_package)
  build_utils.unpack(arch_file, build_path)

  src_path = os.path.join(build_path, "Profilers")
  src_path = os.path.join(src_path, "Lib")
  src_path = os.path.join(src_path, "intel64")

  gtpin_libs = ["gtpin.lib"]\
    if sys.platform == 'win32' else\
    ["libgcc_s.so.1",
     "libged.so",
     "libgtpin.so",
     "libgtpin_core.so",
     "libiga_wrapper.so",
     "libstdc++.so.6"]

  build_utils.copy(src_path, dst_path, gtpin_libs)

  if sys.platform == 'win32':
    gtpin_dlls = [
      "gtpin.dll",
      "ged.dll",
      "gtpin_core.dll",
      "iga_wrapper.dll"]

    build_utils.copy(src_path, build_path, gtpin_dlls)

if __name__ == "__main__":
  main()
