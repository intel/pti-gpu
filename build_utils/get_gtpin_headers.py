import sys
import os

import build_utils

def main():
  if len(sys.argv) < 3:
    print("Usage: python get_gtpin_headers.py <include_path> <build_path>")
    return

  dst_path = sys.argv[1]
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "GTPIN")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  
  build_path = sys.argv[2]
  gtpin_package = "external-gtpin-2.12-linux.tar.bz2"
  build_utils.download("https://software.intel.com/content/dam/develop/public/us/en/protected/" + gtpin_package, build_path)
  arch_file = os.path.join(build_path, gtpin_package)
  build_utils.unpack(arch_file, build_path)

  src_path = os.path.join(build_path, "Profilers")
  src_path = os.path.join(src_path, "Include")

  build_utils.copy(src_path, dst_path,
    ["callbacks.h",
     "client_knob.h",
     "ged_ops.h",
     "gtpin_deprecated_types.h",
     "gtpin.h",
     "gtpin.hpp",
     "gtpintool_types.h",
     "init.h",
     "kernel.h",
     "knob.h",
     "knob_defs.h",
     "send_exec_semantics.h"])

  dst_path = os.path.join(dst_path, "ged")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)
  dst_path = os.path.join(dst_path, "intel64")
  if (not os.path.exists(dst_path)):
    os.mkdir(dst_path)

  src_path = os.path.join(src_path, "ged")
  src_path = os.path.join(src_path, "intel64")

  build_utils.copy(src_path, dst_path,
    ["ged_basic_types.h",
     "ged_enumerations.h",
     "ged_enum_types.h",
     "ged.h",
     "ged_ins_field.h"])

if __name__ == "__main__":
  main()