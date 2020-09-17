import sys
import os

import build_utils

url = "https://github.com/intel/ittapi.git"
commit = "8cd2618bbbc1f05657765948a745d46ebfa1f4a5"

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_itt.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "ITT")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  clone_path = sys.argv[2]
  clone_path = os.path.join(clone_path, "ittapi")
  build_utils.clone(url, commit, clone_path)

  src_path = os.path.join(clone_path, "src")
  src_path = os.path.join(src_path, "ittnotify")
  build_utils.copy(src_path, dst_path, ["disable_warnings.h", "ittnotify_config.h",
    "ittnotify_static.c", "ittnotify_static.h", "ittnotify_types.h"])
  
  src_path = os.path.join(clone_path, "include")
  build_utils.copy(src_path, dst_path, ["ittnotify.h"])

  dst_path = os.path.join(dst_path, "legacy")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  
  src_path = os.path.join(src_path, "legacy")
  build_utils.copy(src_path, dst_path, ["ittnotify.h"])

if __name__ == "__main__":
  main()