import sys
import os

import build_utils
from get_gtpin import get_gtpin

def main(argv):
  if len(argv) != 3:
    print("Usage: python get_gtpin_headers.py <include_path> <build_path>")
    return -1

  dst_path = argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  build_path = argv[2]

  res = get_gtpin(build_path)
  if res != 0:
    return res

  src_gtpin_headers_path = os.path.join(build_path, "Profilers", "Include", "api")
  gtpin_headers = os.listdir(src_gtpin_headers_path)
  dst_gtpin_path = os.path.join(dst_path, "api")
  os.makedirs(dst_gtpin_path, exist_ok=True)
  build_utils.copy(src_gtpin_headers_path, dst_gtpin_path, gtpin_headers)

  src_ged_headers_path = os.path.join(build_path, "Profilers", "Include", "ged", "intel64")
  ged_headers = os.listdir(src_ged_headers_path)
  dst_ged_path = os.path.join(dst_path, "ged", "intel64")
  os.makedirs(dst_ged_path, exist_ok=True)
  build_utils.copy(src_ged_headers_path, dst_ged_path, ged_headers)

if __name__ == "__main__":
  sys.exit(main(sys.argv))
